# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1162/1252 lines (92.81%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `/* range() formats the float variant of its max-array-size ValueError with libc` |
|         - |    8 | ` * snprintf and parses numeric strings with libc strtod — the byte-exact-floats` |
|         - |    9 | ` * rule (see builtin_math.c): SyBufferFormat/SyStrToReal are not correctly` |
|         - |   10 | ` * rounded at extreme magnitudes. */` |
|         - |   11 | `#include <stdio.h>  /* snprintf */` |
|         - |   12 | `#include <stdlib.h> /* strtod */` |
|         - |   13 | `/* This file implement generic hashmaps known as 'array' in the PHP world */` |
|         - |   14 | `/* HASHMAP_INT_NODE / HASHMAP_BLOB_NODE (node key types) are declared in ph7int.h` |
|         - |   15 | ` * alongside ph7_hashmap_node so name-forwarding builtins can classify keys. */` |
|         - |   16 | `/* HASHMAP_NODE_FOREIGN_OBJ (node control flag) is declared in ph7int.h too. */` |
|         - |   17 | `/*` |
|         - |   18 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|         - |   19 | ` */` |
|  12823779 |   20 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   21 | `{` |
|  12823784 |   22 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|  12823784 |   23 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   24 | `}` |
|         - |   25 | `/*` |
|         - |   26 | ` * Default hash function for string/BLOB keys.` |
|         - |   27 | ` */` |
|   6896592 |   28 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   29 | `{` |
|   6896597 |   30 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   31 | `	unsigned char *zEnd;` |
|   6896597 |   32 | `	sxu32 nH = 5381;` |
|   6896597 |   33 | `	zEnd = &zIn[nLen];` |
|   8103229 |   34 | `	for(;;){` |
|  16244862 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  11283154 |   36 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   9953706 |   37 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   9702135 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   39 | `	}` |
|   6896597 |   40 | `	return nH;` |
|         5 |   41 | `}` |
|         - |   42 | `/*` |
|         - |   43 | ` * The hash a map ACTUALLY uses, spelled so the compiler can inline the body.` |
|         - |   44 | ` *` |
|         - |   45 | ` * Every map in the engine is built by PH7_NewHashmap(pVm,0,0) -- all 28 of its call` |
|         - |   46 | ` * sites pass no hash functions -- so xIntHash is IntHash and xBlobHash is BinHash in` |
|         - |   47 | ` * every array a PHP program can reach. Through the pointer, each of the 244 million` |
|         - |   48 | ` * array subscript reads on the ecosystem gate's phpcs step paid a real indirect call` |
|         - |   49 | ` * to reach three shifts (an int key) or a four-byte loop (a string key), and a body` |
|         - |   50 | ` * behind a pointer the compiler cannot resolve is a body it cannot inline.` |
|         - |   51 | ` *` |
|         - |   52 | ` * The test keeps the pointer meaningful rather than deleting it: a map built with its` |
|         - |   53 | ` * own hash still uses it, and the branch is perfectly predicted because nothing in the` |
|         - |   54 | ` * tree takes the other arm. This is NOT the devirtualization PERF.md §5 warns about --` |
|         - |   55 | ` * that one was SyHash's xHash, where BOTH implementations are live (SyStrHash folds` |
|         - |   56 | ` * case, SyBinHash does not) and the branch it added was a real one.` |
|         - |   57 | ` */` |
|         - |   58 | `#define HASHMAP_INT_HASH(pMap,iKey) \` |
|         - |   59 | `	( (pMap)->xIntHash == IntHash ? IntHash(iKey) : (pMap)->xIntHash(iKey) )` |
|         - |   60 | `#define HASHMAP_BLOB_HASH(pMap,pKey,nLen) \` |
|         - |   61 | `	( (pMap)->xBlobHash == BinHash ? BinHash(pKey,nLen) : (pMap)->xBlobHash(pKey,nLen) )` |
|         - |   62 | `/*` |
|         - |   63 | ` * Return the total number of entries in a given hashmap.` |
|         - |   64 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   65 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   66 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   67 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   68 | ` */` |
|      1869 |   69 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   70 | `{` |
|      1874 |   71 | `	sxi64 iCount = 0;` |
|      1874 |   72 | `	if( !bRecursive ){` |
|      1700 |   73 | `		iCount = pMap->nEntry;` |
|       847 |   74 | `	}else{` |
|         - |   75 | `		/* Recursive hashmap walk */` |
|       175 |   76 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|         - |   77 | `		ph7_value *pElem;` |
|       175 |   78 | `		sxu32 n = 0;` |
|         - |   79 | `		/* Mark this map as being counted */` |
|       175 |   80 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|       215 |   81 | `		for(;;){` |
|       431 |   82 | `			if( n >= pMap->nEntry ){` |
|       175 |   83 | `				break;` |
|         - |   84 | `			}` |
|         - |   85 | `			/* Point to the element value */` |
|       257 |   86 | `			pElem = (ph7_value *)PH7_MemObjAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|       257 |   87 | `			if( pElem ){` |
|       257 |   88 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|       151 |   89 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|       151 |   90 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|         - |   91 | `						/* Cycle detected — skip this entry */` |
|         3 |   92 | `						if( pCycleDetected ){` |
|         3 |   93 | `							*pCycleDetected = TRUE;` |
|         1 |   94 | `						}` |
|         2 |   95 | `					}else{` |
|       149 |   96 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|         - |   97 | `					}` |
|        75 |   98 | `				}` |
|       128 |   99 | `			}` |
|         - |  100 | `			/* Point to the next entry */` |
|       257 |  101 | `			pEntry = pEntry->pNext;` |
|       257 |  102 | `			++n;` |
|         1 |  103 | `		}` |
|         - |  104 | `		/* Clear the counting flag */` |
|       175 |  105 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|         - |  106 | `		/* Update count */` |
|       175 |  107 | `		iCount += pMap->nEntry;` |
|         - |  108 | `	}` |
|      1874 |  109 | `	return iCount;` |
|         5 |  110 | `}` |
|         - |  111 | `/*` |
|         - |  112 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  115 | ` */` |
|   8039762 |  116 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  117 | `{` |
|         - |  118 | `	ph7_hashmap_node *pNode;` |
|         - |  119 | `	/* Allocate a new node */` |
|   8039767 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   8039767 |  121 | `	if( pNode == 0 ){` |
|       ! 0 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|         - |  124 | `	/* Zero the stucture */` |
|   8039767 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  126 | `	/* Fill in the structure */` |
|   8039767 |  127 | `	pNode->pMap  = &(*pMap);` |
|   8039767 |  128 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   8039767 |  129 | `	pNode->nHash = nHash;` |
|   8039767 |  130 | `	pNode->xKey.iKey = iKey;` |
|   8039767 |  131 | `	pNode->nValIdx  = nValIdx;` |
|   8039767 |  132 | `	return pNode;` |
|   4019231 |  133 | `}` |
|         - |  134 | `/*` |
|         - |  135 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  136 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  137 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  138 | ` */` |
|   3663183 |  139 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	ph7_hashmap_node *pNode;` |
|         - |  142 | `	/* Allocate a new node */` |
|   3663188 |  143 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3663188 |  144 | `	if( pNode == 0 ){` |
|       ! 0 |  145 | `		return 0;` |
|         - |  146 | `	}` |
|         - |  147 | `	/* Zero the stucture */` |
|   3663188 |  148 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  149 | `	/* Fill in the structure */` |
|   3663188 |  150 | `	pNode->pMap  = &(*pMap);` |
|   3663188 |  151 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   3663188 |  152 | `	pNode->nHash = nHash;` |
|   3663188 |  153 | `	if( nKeyLen <= sizeof(pNode->zKey) ){` |
|         - |  154 | `		/* The key lives in the node -- see HASHMAP_NODE_INLINE_KEY. LOCKED\|STATIC is` |
|         - |  155 | `		 * what SyBlobInitFromBuf marks it, which is exactly right here: the buffer is` |
|         - |  156 | `		 * not the allocator's to grow and not SyBlobRelease's to free, and the key is` |
|         - |  157 | `		 * written once, right below, by an append that is guaranteed to fit. */` |
|   3646147 |  158 | `		SyBlobInitFromBuf(&pNode->xKey.sKey,pNode->zKey,sizeof(pNode->zKey));` |
|   1820409 |  159 | `	}else{` |
|     17046 |  160 | `		SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|         - |  161 | `	}` |
|   3663188 |  162 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   3663188 |  163 | `	pNode->nValIdx = nValIdx;` |
|   3663188 |  164 | `	return pNode;` |
|   1828924 |  165 | `}` |
|         - |  166 | `/*` |
|         - |  167 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  168 | ` */` |
|  11702945 |  169 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  170 | `{` |
|         - |  171 | `	/* Link */` |
|  11702950 |  172 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   6020487 |  173 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   6020487 |  174 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   3008161 |  175 | `	}` |
|  11702950 |  176 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  177 | `	/* Link to the map list */` |
|  11702950 |  178 | `	if( pMap->pFirst == 0 ){` |
|   2355987 |  179 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  180 | `		/* Point to the first inserted node */` |
|   2355987 |  181 | `		pMap->pCur = pNode;` |
|   1177711 |  182 | `	}else{` |
|   9346968 |  183 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  184 | `	}` |
|  11702950 |  185 | `	if( pMap->pActiveSteps ){` |
|         - |  186 | `		/* Re-arm any live foreach cursor parked past the end: php's by-ref` |
|         - |  187 | `		 * foreach iterates the LIVE array, so an element appended while the` |
|         - |  188 | `		 * loop stands on the last node (worklist idiom), or after the body` |
|         - |  189 | `		 * emptied the map, is still visited. A registered step with a NULL` |
|         - |  190 | `		 * cursor is always mid-loop — natural exhaustion unregisters before` |
|         - |  191 | `		 * the loop ends. */` |
|         - |  192 | `		ph7_foreach_step *pStep;` |
|        34 |  193 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        18 |  194 | `			if( pStep->pCursor == 0 ){` |
|        14 |  195 | `				pStep->pCursor = pNode;` |
|         6 |  196 | `			}` |
|        10 |  197 | `		}` |
|         8 |  198 | `	}` |
|  11702950 |  199 | `	++pMap->nEntry;` |
|  11702950 |  200 | `}` |
|         - |  201 | `/*` |
|         - |  202 | ` * Unlink a node from the hashmap.` |
|         - |  203 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  204 | ` */` |
|     12509 |  205 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  206 | `{` |
|     12514 |  207 | `	ph7_hashmap *pMap = pNode->pMap;` |
|     12514 |  208 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  209 | `	/* Unlink from the corresponding bucket */` |
|     12514 |  210 | `	if( pNode->pPrevCollide == 0 ){` |
|     10638 |  211 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      5318 |  212 | `	}else{` |
|      1881 |  213 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  214 | `	}` |
|     12514 |  215 | `	if( pNode->pNextCollide ){` |
|      9014 |  216 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      4501 |  217 | `	}` |
|     12514 |  218 | `	if( pMap->pFirst == pNode ){` |
|       869 |  219 | `		pMap->pFirst = pNode->pPrev;` |
|       432 |  220 | `	}` |
|     12514 |  221 | `	if( pMap->pCur == pNode ){` |
|         - |  222 | `		/* Advance the node cursor */` |
|       867 |  223 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       431 |  224 | `	}` |
|     12514 |  225 | `	if( pMap->pActiveSteps ){` |
|         - |  226 | `		/* Advance any live foreach cursor parked on this node (delete during` |
|         - |  227 | `		 * live-map iteration: by-ref foreach, $GLOBALS, snapshot fallbacks). */` |
|         - |  228 | `		ph7_foreach_step *pStep;` |
|        29 |  229 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        15 |  230 | `			if( pStep->pCursor == pNode ){` |
|         3 |  231 | `				pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|         1 |  232 | `			}` |
|         8 |  233 | `		}` |
|         7 |  234 | `	}` |
|         - |  235 | `	/* Unlink from the map list */` |
|     12514 |  236 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|     12514 |  237 | `	if( bRestore ){` |
|         - |  238 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      1655 |  239 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  240 | `		/* Restore to the freelist — but only if this node was the LAST holder. A` |
|         - |  241 | `` 		 * node's own value can be held by a name too (`$r = &$a[0]; unset($a);` `` |
|         - |  242 | `		 * must leave $r reading 7, not destroy it), and a FOREIGN node may be the` |
|         - |  243 | `		 * last thing holding a slot whose frame is already gone (that frame left it` |
|         - |  244 | `		 * standing for this very node), which nothing else would ever free. */` |
|      1655 |  245 | `		PH7_VmReleaseUnheldSlot(pVm,pNode->nValIdx);` |
|       825 |  246 | `	}` |
|     12514 |  247 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|     11640 |  248 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      5813 |  249 | `	}` |
|     12514 |  250 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|     12514 |  251 | `	pMap->nEntry--;` |
|     12514 |  252 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  253 | `		/* Free the hash-bucket */` |
|       251 |  254 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       251 |  255 | `		pMap->apBucket = 0;` |
|       251 |  256 | `		pMap->nSize = 0;` |
|       251 |  257 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       123 |  258 | `	}` |
|     12514 |  259 | `}` |
|         - |  260 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  261 | `/* Buckets an array's FIRST table is given. Sixteen pointers -- 152 bytes with the` |
|         - |  262 | ` * allocator's block header -- was the opening bid for every array in the program,` |
|         - |  263 | ` * and most arrays are three elements: 69,952 of them were live at the peak of a` |
|         - |  264 | ` * 711-file lint run, 10.8 MB of bucket table for far less than that in entries.` |
|         - |  265 | ` * Eight still holds HASHMAP_FILL_FACTOR * 8 = 24 entries before the first rehash,` |
|         - |  266 | ` * which covers the overwhelming majority of arrays a program builds. */` |
|         - |  267 | `#define HASHMAP_FIRST_BUCKETS 8` |
|         - |  268 | `/*` |
|         - |  269 | ` * Grow the hash-table and rehash all entries.` |
|         - |  270 | ` */` |
|  11702945 |  271 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  272 | `{` |
|  11702950 |  273 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   2381594 |  274 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  275 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   2381594 |  276 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  277 | `		sxu32 nBucket;` |
|         - |  278 | `		sxu32 n;` |
|   2381594 |  279 | `		if( nNew < 1 ){` |
|   2355987 |  280 | `			nNew = HASHMAP_FIRST_BUCKETS;` |
|   1177706 |  281 | `		}` |
|         - |  282 | `		/* Allocate a new bucket */` |
|   2381594 |  283 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   2381594 |  284 | `		if( apNew == 0 ){` |
|       ! 0 |  285 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  286 | `				return SXERR_MEM; /* Fatal */` |
|         - |  287 | `			}` |
|         - |  288 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  289 | `			return SXRET_OK;` |
|         - |  290 | `		}` |
|         - |  291 | `		/* Zero the table */` |
|   2381594 |  292 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  293 | `		/* Reflect the change */` |
|   2381594 |  294 | `		pMap->apBucket = apNew;` |
|   2381594 |  295 | `		pMap->nSize = nNew;` |
|   2381594 |  296 | `		if( apOld == 0 ){` |
|         - |  297 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   2355987 |  298 | `			return SXRET_OK;` |
|         - |  299 | `		}` |
|         - |  300 | `		/* Rehash old entries */` |
|     25612 |  301 | `		pEntry = pMap->pFirst;` |
|     25612 |  302 | `		n = 0;` |
|   3505130 |  303 | `		for( ;; ){` |
|   7011868 |  304 | `			if( n >= pMap->nEntry ){` |
|     25612 |  305 | `				break;` |
|         - |  306 | `			}` |
|         - |  307 | `			/* Clear the old collision link */` |
|   6986261 |  308 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  309 | `			/* Link to the new bucket */` |
|   6986261 |  310 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   6986261 |  311 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   5376029 |  312 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   5376029 |  313 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2687732 |  314 | `			}` |
|   6986261 |  315 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  316 | `			/* Point to the next entry */` |
|   6986261 |  317 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   6986261 |  318 | `			n++;` |
|         5 |  319 | `		}` |
|         - |  320 | `		/* Free the old table */` |
|     25612 |  321 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|     12794 |  322 | `	}` |
|   9346968 |  323 | `	return SXRET_OK;` |
|   5848150 |  324 | `}` |
|         - |  325 | `/*` |
|         - |  326 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  327 | ` * hashmap.` |
|         - |  328 | ` */` |
|   8039762 |  329 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  330 | `{` |
|         - |  331 | `	ph7_hashmap_node *pNode;` |
|         - |  332 | `	sxu32 nIdx;` |
|         - |  333 | `	sxu32 nHash;` |
|         - |  334 | `	sxi32 rc;` |
|   8039767 |  335 | `	if( !isForeign ){` |
|         - |  336 | `		ph7_value *pObj;` |
|         - |  337 | `		ph7_value sSafeVal;` |
|         - |  338 | `		/* Snapshot the source BEFORE reserving. This guarded PH7_ReserveMemObj` |
|         - |  339 | `		 * MOVING pVm->aMemObj under a pValue that points into the pool (e.g.` |
|         - |  340 | `		 * get_defined_vars/func_get_args/get_class_vars/get_object_vars all pass` |
|         - |  341 | `		 * a pool slot). Redundant since P1 -- the pool's segments are fixed, so a` |
|         - |  342 | `		 * slot's address never moves. Left for the harvest sweep (PERF.md P1). */` |
|   8039625 |  343 | `		if( pValue ){` |
|   8039547 |  344 | `			sSafeVal = *pValue;` |
|   8039547 |  345 | `			pValue = &sSafeVal;` |
|   4019116 |  346 | `		}` |
|         - |  347 | `		/* Reserve a ph7_value for the value */` |
|   8039625 |  348 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   8039625 |  349 | `		if( pObj == 0 ){` |
|       ! 0 |  350 | `			return SXERR_MEM;` |
|         - |  351 | `		}` |
|   8039625 |  352 | `		if( pValue ){` |
|         - |  353 | `			/* Duplicate the value */` |
|   8039547 |  354 | `			PH7_MemObjStore(pValue,pObj);` |
|   4019116 |  355 | `		}` |
|   8039625 |  356 | `		nIdx = pObj->nIdx;` |
|   4019160 |  357 | `	}else{` |
|       145 |  358 | `		nIdx = nRefIdx;` |
|         - |  359 | `	}` |
|         - |  360 | `	/* Hash the key */` |
|   8039767 |  361 | `	nHash = HASHMAP_INT_HASH(pMap,iKey);` |
|         - |  362 | `	/* Allocate a new int node */` |
|   8039767 |  363 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   8039767 |  364 | `	if( pNode == 0 ){` |
|       ! 0 |  365 | `		return SXERR_MEM;` |
|         - |  366 | `	}` |
|   8039767 |  367 | `	if( isForeign ){` |
|         - |  368 | `		/* Mark as a foregin entry */` |
|       145 |  369 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        71 |  370 | `	}` |
|         - |  371 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   8039767 |  372 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   8039767 |  373 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  374 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  375 | `		return rc;` |
|         - |  376 | `	}` |
|         - |  377 | `	/* Perform the insertion */` |
|   8039767 |  378 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|   8039767 |  379 | `	if( iKey > pMap->iMaxIntKey ){` |
|   6447791 |  380 | `		pMap->iMaxIntKey = iKey; /* the auto-index scan's only reason to run */` |
|   3223369 |  381 | `	}` |
|         - |  382 | `	/* Install in the reference table */` |
|   8039767 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  384 | `	/* All done */` |
|   8039767 |  385 | `	return SXRET_OK;` |
|   4019231 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  389 | ` * hashmap.` |
|         - |  390 | ` */` |
|   3663183 |  391 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  392 | `{` |
|         - |  393 | `	ph7_hashmap_node *pNode;` |
|         - |  394 | `	sxu32 nHash;` |
|         - |  395 | `	sxu32 nIdx;` |
|         - |  396 | `	sxi32 rc;` |
|   3663188 |  397 | `	if( !isForeign ){` |
|         - |  398 | `		ph7_value *pObj;` |
|         - |  399 | `		ph7_value sSafeVal;` |
|         - |  400 | `		/* Snapshot the source BEFORE reserving. This guarded PH7_ReserveMemObj` |
|         - |  401 | `		 * MOVING pVm->aMemObj under a pValue that points into the pool (e.g.` |
|         - |  402 | `		 * get_defined_vars/func_get_args/get_class_vars/get_object_vars all pass` |
|         - |  403 | `		 * a pool slot). Redundant since P1 -- the pool's segments are fixed, so a` |
|         - |  404 | `		 * slot's address never moves. Left for the harvest sweep (PERF.md P1). */` |
|   3583671 |  405 | `		if( pValue ){` |
|   3583521 |  406 | `			sSafeVal = *pValue;` |
|   3583521 |  407 | `			pValue = &sSafeVal;` |
|   1789166 |  408 | `		}` |
|         - |  409 | `		/* Reserve a ph7_value for the value */` |
|   3583671 |  410 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3583671 |  411 | `		if( pObj == 0 ){` |
|       ! 0 |  412 | `			return SXERR_MEM;` |
|         - |  413 | `		}` |
|   3583671 |  414 | `		if( pValue ){` |
|         - |  415 | `			/* Duplicate the value */` |
|   3583521 |  416 | `			PH7_MemObjStore(pValue,pObj);` |
|   1789166 |  417 | `		}` |
|   3583671 |  418 | `		nIdx = pObj->nIdx;` |
|   1789246 |  419 | `	}else{` |
|     79522 |  420 | `		nIdx = nRefIdx;` |
|         - |  421 | `	}` |
|         - |  422 | `	/* Hash the key */` |
|   3663188 |  423 | `	nHash = HASHMAP_BLOB_HASH(pMap,pKey,nKeyLen);` |
|         - |  424 | `	/* Allocate a new blob node */` |
|   3663188 |  425 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   3663188 |  426 | `	if( pNode == 0 ){` |
|       ! 0 |  427 | `		return SXERR_MEM;` |
|         - |  428 | `	}` |
|   3663188 |  429 | `	if( isForeign ){` |
|         - |  430 | `		/* Mark as a foregin entry */` |
|     79522 |  431 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     39678 |  432 | `	}` |
|         - |  433 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3663188 |  434 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3663188 |  435 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  436 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  437 | `		return rc;` |
|         - |  438 | `	}` |
|         - |  439 | `	/* Perform the insertion */` |
|   3663188 |  440 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  441 | `	/* Install in the reference table */` |
|   3663188 |  442 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  443 | `	/* All done */` |
|   3663188 |  444 | `	return SXRET_OK;` |
|   1828924 |  445 | `}` |
|         - |  446 | `/*` |
|         - |  447 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  448 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  449 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  450 | ` */` |
|   5157144 |  451 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(` |
|         - |  452 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  453 | `	sxi64 iKey,                /* lookup key */` |
|         - |  454 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  455 | `	)` |
|         5 |  456 | `{` |
|         - |  457 | `	ph7_hashmap_node *pNode;` |
|         - |  458 | `	sxu32 nHash;` |
|   5157149 |  459 | `	if( pMap->nEntry < 1 ){` |
|         - |  460 | `		/* Don't bother hashing,there is no entry anyway */` |
|    400452 |  461 | `		return SXERR_NOTFOUND;` |
|         - |  462 | `	}` |
|         - |  463 | `	/* Hash the key first */` |
|   4756702 |  464 | `	nHash = HASHMAP_INT_HASH(pMap,iKey);` |
|         - |  465 | `	/* Point to the appropriate bucket */` |
|   4756702 |  466 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  467 | `	/* Perform the lookup */` |
|  56602379 |  468 | `	for(;;){` |
| 113204486 |  469 | `		if( pNode == 0 ){` |
|   2942306 |  470 | `			break;` |
|         - |  471 | `		}` |
| 110262180 |  472 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 110260531 |  473 | `			&& pNode->nHash == nHash` |
|  56036808 |  474 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  475 | `				/* Node found */` |
|   1814401 |  476 | `				if( ppNode ){` |
|   1814355 |  477 | `					*ppNode = pNode;` |
|    907165 |  478 | `				}` |
|   1814401 |  479 | `				return SXRET_OK;` |
|         - |  480 | `		}` |
|         - |  481 | `		/* Follow the collision link */` |
| 108447789 |  482 | `		pNode = pNode->pNextCollide;` |
|         5 |  483 | `	}` |
|         - |  484 | `	/* No such entry */` |
|   2942306 |  485 | `	return SXERR_NOTFOUND;` |
|   2578547 |  486 | `}` |
|         - |  487 | `/*` |
|         - |  488 | ` * Can this key POSSIBLY be one of php's numeric array keys? A yes still has to be` |
|         - |  489 | ` * confirmed by HashmapIsIntKey; a no is final, and it is the answer for nearly every` |
|         - |  490 | ` * array read a program makes -- 'code', 'content', 'type'. php's rule can only say yes` |
|         - |  491 | ` * for a key that starts with a digit or with '-', so one byte settles it INLINE, where` |
|         - |  492 | ` * the full rule is an out-of-line call that re-derives that same first byte before it` |
|         - |  493 | ` * does anything else. 121 million string-key subscript reads asked it on the phpcs step` |
|         - |  494 | ` * of record.` |
|         - |  495 | ` */` |
|   3994008 |  496 | `static int HashmapKeyMayBeInt(SyBlob *pKey)` |
|         5 |  497 | `{` |
|   3994013 |  498 | `	const unsigned char *zIn = (const unsigned char *)SyBlobData(pKey);` |
|   3994013 |  499 | `	if( SyBlobLength(pKey) < 1 ){` |
|       227 |  500 | `		return FALSE;` |
|         - |  501 | `	}` |
|   3993791 |  502 | `	return ( zIn[0] == '-' \|\| (zIn[0] >= '0' && zIn[0] <= '9') ) ? TRUE : FALSE;` |
|   1994277 |  503 | `}` |
|         - |  504 | `/*` |
|         - |  505 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  506 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  507 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  508 | ` */` |
|   3995933 |  509 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(` |
|         - |  510 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  511 | `	const void *pKey,           /* Lookup key */` |
|         - |  512 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  513 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  514 | `	)` |
|         5 |  515 | `{` |
|         - |  516 | `	ph7_hashmap_node *pNode;` |
|         - |  517 | `	sxu32 nHash;` |
|   3995938 |  518 | `	if( pMap->nEntry < 1 ){` |
|         - |  519 | `		/* Don't bother hashing,there is no entry anyway */` |
|    762529 |  520 | `		return SXERR_NOTFOUND;` |
|         - |  521 | `	}` |
|         - |  522 | `	/* Hash the key first */` |
|   3233414 |  523 | `	nHash = HASHMAP_BLOB_HASH(pMap,pKey,nKeyLen);` |
|         - |  524 | `	/* Point to the appropriate bucket */` |
|   3233414 |  525 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  526 | `	/* Perform the lookup */` |
|   2589478 |  527 | `	for(;;){` |
|   5192752 |  528 | `		if( pNode == 0 ){` |
|   3031780 |  529 | `			break;` |
|         - |  530 | `		}` |
|   2160972 |  531 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|   2158159 |  532 | `			&& pNode->nHash == nHash` |
|   1174609 |  533 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen ){` |
|         - |  534 | `				/* The bytes, compared INLINE. SyMemcmp is the same loop (it is` |
|         - |  535 | `				 * SX_MACRO_FAST_CMP either way, PERF.md §5) plus a call and two` |
|         - |  536 | `				 * null tests, and the keys here are four or five bytes: the call` |
|         - |  537 | `				 * costs more than the comparison it makes, tens of millions of` |
|         - |  538 | `				 * times a run. */` |
|    202435 |  539 | `				sxi32 rcCmp = 0;` |
|    202435 |  540 | `				if( nKeyLen > 0 ){` |
|    388856 |  541 | `					SX_MACRO_FAST_CMP(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen,rcCmp);` |
|    101135 |  542 | `				}` |
|    202435 |  543 | `				if( rcCmp == 0 ){` |
|         - |  544 | `					/* Node found */` |
|    201639 |  545 | `					if( ppNode ){` |
|    201605 |  546 | `						*ppNode = pNode;` |
|    100748 |  547 | `					}` |
|    201639 |  548 | `					return SXRET_OK;` |
|         - |  549 | `				}` |
|       398 |  550 | `		}` |
|         - |  551 | `		/* Follow the collision link */` |
|   1959343 |  552 | `		pNode = pNode->pNextCollide;` |
|         5 |  553 | `	}` |
|         - |  554 | `	/* No such entry */` |
|   3031780 |  555 | `	return SXERR_NOTFOUND;` |
|   1995231 |  556 | `}` |
|         - |  557 | `/*` |
|         - |  558 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  559 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  560 | ` */` |
|      3272 |  561 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  562 | `{` |
|      3277 |  563 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|      3277 |  564 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  565 | `	const char *zDigit;` |
|      3277 |  566 | `	int isNeg = FALSE, nDigit;` |
|      3277 |  567 | `	if( zIn >= zEnd ){` |
|       ! 0 |  568 | `		return FALSE;` |
|         - |  569 | `	}` |
|         - |  570 | `	/* php's rule (_zend_handle_numeric_str_ex), byte for byte:` |
|         - |  571 | `	 *   - a leading '-' is allowed, a leading '+' is NOT ('+1' stays a` |
|         - |  572 | `	 *     string key)` |
|         - |  573 | `	 *   - after the sign, a leading '0' disqualifies the key unless the` |
|         - |  574 | `	 *     WHOLE key is the single digit "0" -- so "00", "01", "-0" and` |
|         - |  575 | `	 *     "-01" all stay string keys. The length is measured over the key,` |
|         - |  576 | `	 *     sign included, which is what makes "-0" fail.` |
|         - |  577 | `	 * PH7 tested the leading zero BEFORE skipping the sign and accepted '+',` |
|         - |  578 | `	 * so $a['-0'], $a['+0'], $a['+1'] and $a['-01'] canonicalised onto the` |
|         - |  579 | `	 * integer keys 0/0/1/-1 -- silently COLLIDING with a genuine 0/1/-1 entry` |
|         - |  580 | `	 * ($a = ['-0'=>'a','0'=>'d'] kept one element where php keeps two) and` |
|         - |  581 | `	 * carrying the wrong key through array_keys/array_flip/json_decode/` |
|         - |  582 | `	 * serialize/array_count_values alike. */` |
|      3277 |  583 | `	if( zIn[0] == '-' && &zIn[1] < zEnd ){` |
|       114 |  584 | `		isNeg = TRUE;` |
|       114 |  585 | `		zIn++;` |
|        55 |  586 | `	}` |
|      3277 |  587 | `	if( zIn < zEnd && zIn[0] == '0' && SyBlobLength(pKey) > 1 ){` |
|         - |  588 | `		/* Leading zero: octal-looking, signed zero, or just padded */` |
|       161 |  589 | `		return FALSE;` |
|         - |  590 | `	}` |
|      3119 |  591 | `	zDigit = zIn;` |
|      3790 |  592 | `	for(;;){` |
|      7585 |  593 | `		if( zIn >= zEnd ){` |
|      2921 |  594 | `			break;` |
|         - |  595 | `		}` |
|      4669 |  596 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  597 | `			/* Key does not look like a decimal number */` |
|       203 |  598 | `			return FALSE;` |
|         - |  599 | `		}` |
|      4471 |  600 | `		zIn++;` |
|         5 |  601 | `	}` |
|         - |  602 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  603 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  604 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  605 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|      2921 |  606 | `	nDigit = (int)(zEnd - zDigit);` |
|      2921 |  607 | `	if( nDigit < 1 ){` |
|         - |  608 | `		/* A lone "-" (the digit loop rejects it first; kept defensive) */` |
|       ! 0 |  609 | `		return FALSE;` |
|         - |  610 | `	}` |
|      2940 |  611 | `	if( nDigit > 19 \|\|` |
|      1476 |  612 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|        22 |  613 | `		return FALSE;` |
|         - |  614 | `	}` |
|      2901 |  615 | `	return TRUE;` |
|      1641 |  616 | `}` |
|         - |  617 | `/*` |
|         - |  618 | ` * TRUE when this key value lands on an INTEGER key — the same fold HashmapLookup` |
|         - |  619 | ` * and HashmapInsert perform below, exposed so a DIAGNOSTIC can name the key the` |
|         - |  620 | ` * way the lookup saw it rather than the way it was written ($a["10"] misses the` |
|         - |  621 | `` * integer key 10, so php's warning says `Undefined array key 10`, unquoted).`` |
|         - |  622 | ` * A non-integer key is left as a STRING with its blob ready to print — including` |
|         - |  623 | ` * the NULL key, which folds to "" exactly as the lookup folds it.` |
|         - |  624 | ` */` |
|       166 |  625 | `PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey)` |
|         5 |  626 | `{` |
|       171 |  627 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|       135 |  628 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  629 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  630 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  631 | `		}` |
|       135 |  632 | `		return ( HashmapKeyMayBeInt(&pKey->sBlob) && HashmapIsIntKey(&pKey->sBlob) ) ? TRUE : FALSE;` |
|         - |  633 | `	}` |
|         - |  634 | `	/* int / float / BOOL all reach an integer key ($a[false] is $a[0]) */` |
|        38 |  635 | `	return TRUE;` |
|        88 |  636 | `}` |
|         - |  637 | `/*` |
|         - |  638 | ` * Check if a given key exists in the given hashmap.` |
|         - |  639 | ` * Write a pointer to the target node on success.` |
|         - |  640 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  641 | ` */` |
|   2136244 |  642 | `static sxi32 HashmapLookup(` |
|         - |  643 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  644 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  645 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  646 | `	)` |
|         5 |  647 | `{` |
|   2136249 |  648 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  649 | `	sxi32 rc;` |
|   2136249 |  650 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    333301 |  651 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  652 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|        58 |  653 | `			PH7_MemObjToString(&(*pKey));` |
|        27 |  654 | `		}` |
|    333301 |  655 | `		if( !HashmapKeyMayBeInt(&pKey->sBlob) \|\| !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  656 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  657 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  658 | `			 * to an integer lookup for key 0. */` |
|    330949 |  659 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    330949 |  660 | `			goto result;` |
|         - |  661 | `		}` |
|      1176 |  662 | `	}` |
|         - |  663 | `	/* Perform an int lookup */` |
|   1805305 |  664 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  665 | `		/* Force an integer cast */` |
|      2383 |  666 | `		PH7_MemObjToInteger(pKey);` |
|      1189 |  667 | `	}` |
|         - |  668 | `	/* Perform an int lookup */` |
|   1805305 |  669 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   1068197 |  670 | `result:` |
|   2136249 |  671 | `	if( rc == SXRET_OK ){` |
|         - |  672 | `		/* Node found */` |
|   2001922 |  673 | `		if( ppNode ){` |
|   2001851 |  674 | `			*ppNode = pNode;` |
|   1000869 |  675 | `		}` |
|   2001922 |  676 | `		return SXRET_OK;` |
|         - |  677 | `	}` |
|         - |  678 | `	/* No such entry */` |
|    134332 |  679 | `	return SXERR_NOTFOUND;` |
|   1068052 |  680 | `}` |
|         - |  681 | `/*` |
|         - |  682 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  683 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  684 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  685 | ` * via HashmapAppendIndexBusy.` |
|         - |  686 | ` */` |
|         - |  687 | `/*` |
|         - |  688 | ` * Walk the auto-index past any slot that is already taken. Only a key ABOVE the` |
|         - |  689 | ` * one that moved the index can be in the way, so the whole walk is skipped` |
|         - |  690 | ` * unless the map has held one (iMaxIntKey): the scan's first step is a hash` |
|         - |  691 | ``  * lookup that MISSES, and paying it on every int-keyed insert made `$a[$i]=$i` `` |
|         - |  692 | `` * six times slower than `$a[]=$i` over the same keys.`` |
|         - |  693 | ` */` |
|   3352706 |  694 | `static void HashmapSkipReservedIndex(ph7_hashmap *pMap)` |
|         5 |  695 | `{` |
|   3352711 |  696 | `	if( pMap->iNextIdx > pMap->iMaxIntKey ){` |
|   3352643 |  697 | `		return;` |
|         - |  698 | `	}` |
|        96 |  699 | `	while( pMap->iNextIdx < SXI64_HIGH` |
|        71 |  700 | `	    && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  701 | `		pMap->iNextIdx++;` |
|       ! 0 |  702 | `	}` |
|   1676319 |  703 | `}` |
|   3353815 |  704 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  705 | `{` |
|   3353820 |  706 | `	if( !pMap->bIntKeySeen ){` |
|         - |  707 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|    407550 |  708 | `		pMap->bIntKeySeen = 1;` |
|    407550 |  709 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|    407550 |  710 | `		HashmapSkipReservedIndex(&(*pMap));` |
|    407550 |  711 | `		return;` |
|         - |  712 | `	}` |
|   2946275 |  713 | `	if( iKey >= pMap->iNextIdx ){` |
|   2945166 |  714 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  715 | `		/* Make sure the automatic index is not reserved */` |
|   2945166 |  716 | `		HashmapSkipReservedIndex(&(*pMap));` |
|   1472552 |  717 | `	}` |
|   1676897 |  718 | `}` |
|         - |  719 | `/*` |
|         - |  720 | ` * Insert a value under an int key it KEEPS, and move the auto-index on.` |
|         - |  721 | ` *` |
|         - |  722 | ` * Every builtin that rebuilds an array PRESERVING keys lands here — array_chunk,` |
|         - |  723 | ` * array_slice, array_filter, array_reverse, array_unique, the whole diff and` |
|         - |  724 | `` * intersect family, array_pad, and the `+` union — and none of them moved the`` |
|         - |  725 | `` * index, so the rebuilt array's next `$a[] = v` went back to 0 even though an`` |
|         - |  726 | ` * integer key was already in it. php's nNextFreeElement is one past the largest` |
|         - |  727 | `` * integer key COPIED, whichever call built the array; twig's `batch` filter pads`` |
|         - |  728 | `` * a chunk by appending to it and wrote a `0` key into a row whose last key was`` |
|         - |  729 | ` * 123. The advance is the same one PH7_HashmapInsert's own int-key path makes,` |
|         - |  730 | ` * so putting it beside the insert is what stops the two drifting again.` |
|         - |  731 | ` */` |
|   3353815 |  732 | `static sxi32 HashmapKeepIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,` |
|         - |  733 | `	sxu32 nRefIdx,int isForeign)` |
|         5 |  734 | `{` |
|   3353820 |  735 | `	sxi32 rc = HashmapInsertIntKey(&(*pMap),iKey,pValue,nRefIdx,isForeign);` |
|   3353820 |  736 | `	if( rc == SXRET_OK ){` |
|   3353820 |  737 | `		HashmapAdvanceAutoIndex(&(*pMap),iKey);` |
|   1676892 |  738 | `	}` |
|   3353820 |  739 | `	return rc;` |
|         5 |  740 | `}` |
|         - |  741 | `/*` |
|         - |  742 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  743 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  744 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  745 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  746 | ` */` |
|   4685955 |  747 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  748 | `{` |
|   4685960 |  749 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|        10 |  750 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|        10 |  751 | `		return TRUE;` |
|         - |  752 | `	}` |
|   4685952 |  753 | `	return FALSE;` |
|   2342343 |  754 | `}` |
|         - |  755 | `/*` |
|         - |  756 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  757 | ` * hashmap.` |
|         - |  758 | ` * If a node with the given key already exists in the database` |
|         - |  759 | ` * then this function overwrite the old value.` |
|         - |  760 | ` */` |
|  11608126 |  761 | `PH7_PRIVATE sxi32 HashmapInsert(` |
|         - |  762 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  763 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  764 | `	ph7_value *pVal    /* Node value */` |
|         - |  765 | `	)` |
|         5 |  766 | `{` |
|  11608131 |  767 | `	ph7_hashmap_node *pNode = 0;` |
|  11608131 |  768 | `	sxi32 rc = SXRET_OK;` |
|  11608131 |  769 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|   3581062 |  770 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  771 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  772 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  773 | `			 * path and filed it under 0). */` |
|        15 |  774 | `			PH7_MemObjToString(&(*pKey));` |
|         6 |  775 | `		}` |
|   3581062 |  776 | `		if( HashmapKeyMayBeInt(&pKey->sBlob) && HashmapIsIntKey(&pKey->sBlob) ){` |
|       531 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|         - |  779 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  780 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  781 | `		 * overwriting nothing and bumping the auto-index). */` |
|   5368214 |  782 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   1787678 |  783 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  784 | `				/* Overwrite the old value */` |
|         - |  785 | `				ph7_value *pElem;` |
|      1312 |  786 | `				pElem = (ph7_value *)PH7_MemObjAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      1312 |  787 | `				if( pElem ){` |
|      1312 |  788 | `					if( pVal ){` |
|      1312 |  789 | `						PH7_MemObjStore(pVal,pElem);` |
|       658 |  790 | `					}else{` |
|         - |  791 | `						/* Nullify the entry */` |
|       ! 0 |  792 | `						PH7_MemObjToNull(pElem);` |
|         - |  793 | `					}` |
|       653 |  794 | `				}` |
|      1312 |  795 | `				return SXRET_OK;` |
|         - |  796 | `		}` |
|   3579229 |  797 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  798 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  799 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       190 |  800 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  801 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  802 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  803 | `				return SXRET_OK;` |
|         - |  804 | `			}` |
|       283 |  805 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       186 |  806 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        93 |  807 | `				pVal,SXU32_HIGH);` |
|         - |  808 | `		}` |
|         - |  809 | `		/* Perform a blob-key insertion */` |
|   3579043 |  810 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   3579043 |  811 | `		return rc;` |
|         - |  812 | `	}` |
|   4014193 |  813 | `IntKey:` |
|   8027600 |  814 | `	if( pKey ){` |
|   3341767 |  815 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  816 | `			/* Force an integer cast */` |
|       553 |  817 | `			PH7_MemObjToInteger(pKey);` |
|       274 |  818 | `		}` |
|   3341767 |  819 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  820 | `			/* Overwrite the old value */` |
|         - |  821 | `			ph7_value *pElem;` |
|      2114 |  822 | `			pElem = (ph7_value *)PH7_MemObjAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      2114 |  823 | `			if( pElem ){` |
|      2114 |  824 | `				if( pVal ){` |
|      2114 |  825 | `					PH7_MemObjStore(pVal,pElem);` |
|      1060 |  826 | `				}else{` |
|         - |  827 | `					/* Nullify the entry */` |
|       ! 0 |  828 | `					PH7_MemObjToNull(pElem);` |
|         - |  829 | `				}` |
|      1056 |  830 | `			}` |
|      2114 |  831 | `			return SXRET_OK;` |
|         - |  832 | `		}` |
|   3339657 |  833 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  834 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  835 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  836 | `			char zKey[24];` |
|         3 |  837 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  838 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  839 | `		}` |
|         - |  840 | `		/* Perform a 64-bit-int-key insertion */` |
|   3339655 |  841 | `		rc = HashmapKeepIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   1669810 |  842 | `	}else{` |
|   4685838 |  843 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  844 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  845 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  846 | `		}` |
|   4685836 |  847 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|        10 |  848 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  849 | `		}` |
|         - |  850 | `		/* Assign an automatic index */` |
|   4685828 |  851 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   4685828 |  852 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   4685826 |  853 | `			++pMap->iNextIdx;` |
|   2342271 |  854 | `		}` |
|         - |  855 | `	}` |
|         - |  856 | `	/* Insertion result */` |
|   8025478 |  857 | `	return rc;` |
|   5800822 |  858 | `}` |
|         - |  859 | `/*` |
|         - |  860 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  861 | ` * hashmap.` |
|         - |  862 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  863 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  864 | ` * The insertion by reference is triggered when the following` |
|         - |  865 | ` * expression is encountered.` |
|         - |  866 | ` * $var = 10;` |
|         - |  867 | ` *  $a = array(&var);` |
|         - |  868 | ` * OR` |
|         - |  869 | ` *  $a[] =& $var;` |
|         - |  870 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  871 | ` * over it's contents.` |
|         - |  872 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  873 | ` * removed when the foreign ph7_value is unset.` |
|         - |  874 | ` * Example:` |
|         - |  875 | ` *  $var = 10;` |
|         - |  876 | ` *  $a[] =& $var;` |
|         - |  877 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  878 | ` *  //Unset the foreign ph7_value now` |
|         - |  879 | ` *  unset($var);` |
|         - |  880 | ` *  echo count($a); //0` |
|         - |  881 | ` * Note that this is a PH7 eXtension.` |
|         - |  882 | ` * Refer to the official documentation for more information.` |
|         - |  883 | ` * If a node with the given key already exists in the database` |
|         - |  884 | ` * then this function overwrite the old value.` |
|         - |  885 | ` */` |
|     79661 |  886 | `static sxi32 HashmapInsertByRef(` |
|         - |  887 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  888 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  889 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  890 | `	)` |
|         5 |  891 | `{` |
|     79666 |  892 | `	ph7_hashmap_node *pNode = 0;` |
|     79666 |  893 | `	sxi32 rc = SXRET_OK;` |
|     79666 |  894 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|     79530 |  895 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  896 | ``			/* Force a string cast. NULL casts to "": `$a[null] =& $x` binds under the`` |
|         - |  897 | `			 * EMPTY STRING key, symmetric with HashmapInsert (the by-value path). */` |
|         3 |  898 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  899 | `		}` |
|     79530 |  900 | `		if( HashmapKeyMayBeInt(&pKey->sBlob) && HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  901 | `			goto IntKey;` |
|         - |  902 | `		}` |
|         - |  903 | ``		/* An empty key is a REAL key: `$a[""] =& $x` binds (and OVERWRITES an existing`` |
|         - |  904 | `		 * "" element) under "", it does NOT auto-index. The legacy path turned "" into` |
|         - |  905 | ``		 * the next integer slot — `$a[""] =& $x` filed under 0 and a second write added`` |
|         - |  906 | `		 * a duplicate rather than rebinding. A genuine auto-index caller passes` |
|         - |  907 | `		 * pKey == 0 (a literal null pointer), handled at IntKey below, never a "" blob. */` |
|    119209 |  908 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     39681 |  909 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  910 | `				/* Overwrite */` |
|         8 |  911 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         8 |  912 | `				pNode->nValIdx = nRefIdx;` |
|         - |  913 | `				/* Install in the reference table */` |
|         8 |  914 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         8 |  915 | `				return SXRET_OK;` |
|         - |  916 | `		}` |
|         - |  917 | `		/* Perform a blob-key insertion */` |
|     79522 |  918 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     79522 |  919 | `		return rc;` |
|         - |  920 | `	}` |
|        68 |  921 | `IntKey:` |
|       141 |  922 | `	if( pKey ){` |
|        16 |  923 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  924 | `			/* Force an integer cast */` |
|         3 |  925 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  926 | `		}` |
|        16 |  927 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  928 | `			/* Overwrite */` |
|         5 |  929 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         5 |  930 | `			pNode->nValIdx = nRefIdx;` |
|         - |  931 | `			/* Install in the reference table */` |
|         5 |  932 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         5 |  933 | `			return SXRET_OK;` |
|         - |  934 | `		}` |
|         - |  935 | `		/* Perform a 64-bit-int-key insertion */` |
|        12 |  936 | `		rc = HashmapKeepIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|         7 |  937 | `	}else{` |
|       127 |  938 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  939 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  940 | `		}` |
|         - |  941 | `		/* Assign an automatic index */` |
|       127 |  942 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|       127 |  943 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|       127 |  944 | `			++pMap->iNextIdx;` |
|        62 |  945 | `		}` |
|         - |  946 | `	}` |
|         - |  947 | `	/* Insertion result */` |
|       137 |  948 | `	return rc;` |
|     39755 |  949 | `}` |
|         - |  950 | `/*` |
|         - |  951 | ` * Is this element a php REFERENCE — that is, does the value it points at have a` |
|         - |  952 | ` * holder BESIDES this node?` |
|         - |  953 | ` *` |
|         - |  954 | ` * php's answer is a refcount: an element is a reference while at least two things` |
|         - |  955 | ` * share the value, and the marker (and the shared-through-a-copy behaviour that` |
|         - |  956 | ` * goes with it) disappears with the second-to-last holder. Every node is filed in` |
|         - |  957 | ` * its own slot's reference record, so "another holder" is simply a holder count of` |
|         - |  958 | `` * two or more — a name bound to the value (`$r = &$a[1]`), a second array node`` |
|         - |  959 | `` * (`$b = [&$x]; $c = [&$x]`), or the variable a FOREIGN node points at.`` |
|         - |  960 | ` *` |
|         - |  961 | `` * Reading the FOREIGN flag instead answered "was this element created by `&`",`` |
|         - |  962 | `` * which stops being true the moment the other side goes away: `$v = 10; $a = [&$v];`` |
|         - |  963 | `` * unset($v);` left the element marked `&int(10)` where php says `int(10)`, and — the`` |
|         - |  964 | ` * half that was not cosmetic — a COPY of that array still shared the slot, so` |
|         - |  965 | `` * `$j = $i; $j[0] = 99;` wrote through to `$i[0]`.`` |
|         - |  966 | ` */` |
|   1354907 |  967 | `PH7_PRIVATE int PH7_HashmapNodeIsRef(ph7_hashmap_node *pNode)` |
|         5 |  968 | `{` |
|   1354912 |  969 | `	return PH7_VmSlotHolderCount(pNode->pMap->pVm,pNode->nValIdx) >= 2;` |
|         5 |  970 | `}` |
|         - |  971 | `/*` |
|         - |  972 | ` * Extract node value.` |
|         - |  973 | ` */` |
|   6855711 |  974 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  975 | `{` |
|         - |  976 | `	/* Point to the desired object */` |
|         - |  977 | `	ph7_value *pObj;` |
|   6855716 |  978 | `	pObj = (ph7_value *)PH7_MemObjAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   6855716 |  979 | `	return pObj;` |
|         5 |  980 | `}` |
|         - |  981 | `/*` |
|         - |  982 | ` * Insert a node in the given hashmap.` |
|         - |  983 | ` * If a node with the given key already exists in the database` |
|         - |  984 | ` * then this function overwrite the old value.` |
|         - |  985 | ` */` |
|      6037 |  986 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  987 | `{` |
|         - |  988 | `	ph7_value *pObj;` |
|         - |  989 | `	sxi32 rc;` |
|         - |  990 | `	/* Extract the node value */` |
|      6042 |  991 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|      6042 |  992 | `	if( pObj == 0 ){` |
|       ! 0 |  993 | `		return SXERR_EMPTY;` |
|         - |  994 | `	}` |
|      6042 |  995 | `	if( PH7_HashmapNodeIsRef(&(*pNode)) ){` |
|         - |  996 | `		/* A referenced element keeps its reference through the copy (php: array_slice()` |
|         - |  997 | ``		 * of an array holding `$r = &$a[1]` still var_dumps that element as &int(2)).`` |
|         - |  998 | `		 * Same rule HashmapDuplicateNode applies for array_merge()/spread. */` |
|         3 |  999 | `		sxu32 nRefIdx = pNode->nValIdx;` |
|         - | 1000 | `		ph7_value sKey;` |
|         3 | 1001 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         3 | 1002 | `			if( !bPreserve ){` |
|         3 | 1003 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - | 1004 | `			}` |
|       ! 0 | 1005 | `			PH7_MemObjInitFromInt(pMap->pVm,&sKey,pNode->xKey.iKey);` |
|       ! 0 | 1006 | `		}else{` |
|       ! 0 | 1007 | `			if( !bPreserve ){` |
|       ! 0 | 1008 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - | 1009 | `			}` |
|       ! 0 | 1010 | `			PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       ! 0 | 1011 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|       ! 0 | 1012 | `				SyBlobLength(&pNode->xKey.sKey));` |
|         - | 1013 | `		}` |
|       ! 0 | 1014 | `		rc = HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|       ! 0 | 1015 | `		PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1016 | `		return rc;` |
|         - | 1017 | `	}` |
|         - | 1018 | `	/* Preserve key */` |
|      6040 | 1019 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - | 1020 | `		/* Int64 key */` |
|      5611 | 1021 | `		if( !bPreserve ){` |
|         - | 1022 | `			/* Assign an automatic index */` |
|       527 | 1023 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       266 | 1024 | `		}else{` |
|      5089 | 1025 | `			rc = HashmapKeepIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - | 1026 | `		}` |
|      2811 | 1027 | `	}else{` |
|         - | 1028 | `		/* Blob key */` |
|       433 | 1029 | `		if( !bPreserve ){` |
|         - | 1030 | `			/* treat it like an automatically-indexed element, drop the` |
|         - | 1031 | `			 * original string key entirely */` |
|        44 | 1032 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        23 | 1033 | `		}else{` |
|       580 | 1034 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|       189 | 1035 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - | 1036 | `		}` |
|         - | 1037 | `	}` |
|      6040 | 1038 | `	return rc;` |
|      3022 | 1039 | `}` |
|         - | 1040 | `/*` |
|         - | 1041 | ` * Compare two node values.` |
|         - | 1042 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - | 1043 | ` * or < 0 if pRight is greater than pLeft.` |
|         - | 1044 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - | 1045 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - | 1046 | ` * documenation.` |
|         - | 1047 | ` */` |
|     71129 | 1048 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 | 1049 | `{` |
|         - | 1050 | `	ph7_value sObj1,sObj2;` |
|         - | 1051 | `	sxi32 rc;` |
|     71134 | 1052 | `	if( pLeft == pRight ){` |
|         - | 1053 | `		/*` |
|         - | 1054 | `		 * Same node.Refer to the sort() implementation defined` |
|         - | 1055 | `		 * below for more information on this sceanario.` |
|         - | 1056 | `		 */` |
|       ! 0 | 1057 | `		return 0;` |
|         - | 1058 | `	}` |
|         - | 1059 | `	/* Do the comparison */` |
|     71134 | 1060 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71134 | 1061 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71134 | 1062 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71134 | 1063 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71134 | 1064 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71134 | 1065 | `	PH7_MemObjRelease(&sObj1);` |
|     71134 | 1066 | `	PH7_MemObjRelease(&sObj2);` |
|     71134 | 1067 | `	return rc;` |
|     35568 | 1068 | `}` |
|         - | 1069 | `/*` |
|         - | 1070 | ` * Rehash a node with a 64-bit integer key.` |
|         - | 1071 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - | 1072 | ` */` |
|     27320 | 1073 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 | 1074 | `{` |
|     27325 | 1075 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - | 1076 | `	sxu32 nBucket;` |
|         - | 1077 | `	/* Remove old collision links */` |
|     27325 | 1078 | `	if( pEntry->pPrevCollide ){` |
|     19975 | 1079 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      9908 | 1080 | `	}else{` |
|      7355 | 1081 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - | 1082 | `	}` |
|     27325 | 1083 | `	if( pEntry->pNextCollide ){` |
|      2112 | 1084 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1039 | 1085 | `	}` |
|     27325 | 1086 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 1087 | `	/* Compute the new hash */` |
|     27325 | 1088 | `	pEntry->nHash = HASHMAP_INT_HASH(pMap,pMap->iNextIdx);` |
|     27325 | 1089 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     27325 | 1090 | `	if( pMap->iNextIdx > pMap->iMaxIntKey ){` |
|        13 | 1091 | `		pMap->iMaxIntKey = pMap->iNextIdx;` |
|         6 | 1092 | `	}` |
|     27325 | 1093 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - | 1094 | `	/* Link to the new bucket */` |
|     27325 | 1095 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     27325 | 1096 | `	if( pMap->apBucket[nBucket] ){` |
|     20579 | 1097 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|     10213 | 1098 | `	}` |
|     27325 | 1099 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     27325 | 1100 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - | 1101 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - | 1102 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - | 1103 | `	 * the no-overflow invariant uniform). */` |
|     27325 | 1104 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     27325 | 1105 | `		pMap->iNextIdx++;` |
|     13642 | 1106 | `	}` |
|     27325 | 1107 | `}` |
|         - | 1108 | `/*` |
|         - | 1109 | ` * Perform a linear search on a given hashmap.` |
|         - | 1110 | ` * Write a pointer to the target node on success.` |
|         - | 1111 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1112 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - | 1113 | ` * for more information.` |
|         - | 1114 | ` */` |
|     48775 | 1115 | `PH7_PRIVATE int HashmapFindValue(` |
|         - | 1116 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1117 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - | 1118 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - | 1119 | `	int bStrict      /* TRUE for strict comparison */` |
|         - | 1120 | `	)` |
|         5 | 1121 | `{` |
|         - | 1122 | `	ph7_hashmap_node *pEntry;` |
|         - | 1123 | `	ph7_value sVal,*pVal;` |
|         - | 1124 | `	ph7_value sNeedle;` |
|         - | 1125 | `	sxi32 rc;` |
|         - | 1126 | `	sxu32 n;` |
|         - | 1127 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     48780 | 1128 | `	pEntry = pMap->pFirst;` |
|     48780 | 1129 | `	n = pMap->nEntry;` |
|     48780 | 1130 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     48780 | 1131 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|    125818 | 1132 | `	for(;;){` |
|    251987 | 1133 | `		if( n < 1 ){` |
|      1423 | 1134 | `			break;` |
|         - | 1135 | `		}` |
|         - | 1136 | `		/* Extract node value */` |
|    250569 | 1137 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    250569 | 1138 | `		if( pVal ){` |
|         - | 1139 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - | 1140 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - | 1141 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - | 1142 | `			 * so null needles/values take the same path as everything else` |
|         - | 1143 | `			 * (the historical null-to-null shortcut here made` |
|         - | 1144 | `			 * in_array(null, [""]) false where php says true). */` |
|    250569 | 1145 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    250569 | 1146 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    250569 | 1147 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    250569 | 1148 | `			PH7_MemObjRelease(&sVal);` |
|    250569 | 1149 | `			PH7_MemObjRelease(&sNeedle);` |
|    250569 | 1150 | `			if( rc == 0 ){` |
|     47362 | 1151 | `				if( ppNode ){` |
|       ! 0 | 1152 | `					*ppNode = pEntry;` |
|       ! 0 | 1153 | `				}` |
|         - | 1154 | `				/* Match found*/` |
|     47362 | 1155 | `				return SXRET_OK;` |
|         - | 1156 | `			}` |
|    101441 | 1157 | `		}` |
|         - | 1158 | `		/* Point to the next entry */` |
|    203212 | 1159 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    203212 | 1160 | `		n--;` |
|         5 | 1161 | `	}` |
|         - | 1162 | `	/* No such entry */` |
|      1423 | 1163 | `	return SXERR_NOTFOUND;` |
|     24382 | 1164 | `}` |
|         - | 1165 | `/*` |
|         - | 1166 | ` * The element comparison array_diff()/array_intersect() and their _assoc pair` |
|         - | 1167 | ` * use, which is NOT the engine's value comparison: php's manual defines all four` |
|         - | 1168 | ` * as` |
|         - | 1169 | ` *     (string)$elem1 === (string)$elem2` |
|         - | 1170 | ` * a PURE string comparison — not numeric-string aware, so array_diff(["10"],` |
|         - | 1171 | ` * ["1e1"]) keeps "10" — where PHL used to call PH7_MemObjCmp with bStrict. That` |
|         - | 1172 | ` * made no int ever match its own decimal string, so array_diff([1,2,3],` |
|         - | 1173 | ` * ["1","2"]) answered the whole first array instead of [2=>3].` |
|         - | 1174 | ` *` |
|         - | 1175 | ` * bUserVisible picks the coercion: TRUE emits php's user-visible diagnostics (an` |
|         - | 1176 | ` * ARRAY element warns "Array to string conversion", an object with no` |
|         - | 1177 | ` * __toString() throws the catchable "could not be converted to string" Error,` |
|         - | 1178 | ` * reported through *pRc so the builtin answers the throw instead of a result),` |
|         - | 1179 | ` * FALSE renders silently. The two diff families need different answers there:` |
|         - | 1180 | ` * the _assoc pair converts LAZILY, only when a key matched, so it coerces` |
|         - | 1181 | ` * user-visibly right here; array_diff/array_intersect convert every element of` |
|         - | 1182 | ` * every input array up front (php sorts them), so those pre-pass with` |
|         - | 1183 | ` * HashmapStringifyElems and compare silently afterwards — which is what makes` |
|         - | 1184 | ` * the warning COUNT and the "throws even though an earlier element matched"` |
|         - | 1185 | ` * behaviour come out php-exact.` |
|         - | 1186 | ` *` |
|         - | 1187 | ` * Both operands are coerced on COPIES: these are live array elements, and a` |
|         - | 1188 | ` * diff must not rewrite the caller's array.` |
|         - | 1189 | ` */` |
|     48845 | 1190 | `PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc)` |
|         5 | 1191 | `{` |
|         - | 1192 | `	ph7_value sA,sB;` |
|     48850 | 1193 | `	int bEq = FALSE;` |
|         - | 1194 | `	sxi32 rc;` |
|     48850 | 1195 | `	*pRc = SXRET_OK;` |
|         - | 1196 | `	/* Two fast paths that need no rendering at all, because each type's string` |
|         - | 1197 | `	 * form is canonical and injective: two STRINGS already ARE their string form,` |
|         - | 1198 | `	 * and two INTS are string-equal exactly when they are equal. Without them` |
|         - | 1199 | `	 * array_diff() over a pair of integer ranges formatted both operands of every` |
|         - | 1200 | `	 * one of its O(n*m) comparisons (~4x slower than the strict compare it` |
|         - | 1201 | `	 * replaced). A value carrying MEMOBJ_INT alongside MEMOBJ_REAL is an integral` |
|         - | 1202 | `	 * FLOAT, whose "1" can equal an int's — the mask sends it down the slow path` |
|         - | 1203 | `	 * rather than comparing rVal-derived iVal, and bools/null/resources likewise. */` |
|     48850 | 1204 | `	if( (pA->iFlags & MEMOBJ_STRING) && (pB->iFlags & MEMOBJ_STRING) ){` |
|     50745 | 1205 | `		return SyBlobLength(&pA->sBlob) == SyBlobLength(&pB->sBlob)` |
|     52968 | 1206 | `		    && ( SyBlobLength(&pA->sBlob) == 0` |
|      4513 | 1207 | `		      \|\| SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),` |
|      4570 | 1208 | `		                  SyBlobLength(&pA->sBlob)) == 0 );` |
|         - | 1209 | `	}` |
|       390 | 1210 | `	if( (pA->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|       369 | 1211 | `	 && (pB->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT ){` |
|       200 | 1212 | `		return pA->x.iVal == pB->x.iVal;` |
|         - | 1213 | `	}` |
|       194 | 1214 | `	PH7_MemObjInit(pA->pVm,&sA);` |
|       194 | 1215 | `	PH7_MemObjInit(pA->pVm,&sB);` |
|       194 | 1216 | `	PH7_MemObjLoad(pA,&sA);` |
|       194 | 1217 | `	PH7_MemObjLoad(pB,&sB);` |
|       194 | 1218 | `	rc = bUserVisible ? PH7_MemObjToStringUV(&sA) : PH7_MemObjToString(&sA);` |
|       194 | 1219 | `	if( rc == SXRET_OK ){` |
|       194 | 1220 | `		rc = bUserVisible ? PH7_MemObjToStringUV(&sB) : PH7_MemObjToString(&sB);` |
|        96 | 1221 | `	}` |
|       194 | 1222 | `	if( rc != SXRET_OK ){` |
|         3 | 1223 | `		*pRc = rc;` |
|       193 | 1224 | `	}else if( SyBlobLength(&sA.sBlob) == SyBlobLength(&sB.sBlob) ){` |
|       254 | 1225 | `		bEq = SyBlobLength(&sA.sBlob) == 0` |
|       170 | 1226 | `		   \|\| SyMemcmp(SyBlobData(&sA.sBlob),SyBlobData(&sB.sBlob),SyBlobLength(&sA.sBlob)) == 0;` |
|        85 | 1227 | `	}` |
|       194 | 1228 | `	PH7_MemObjRelease(&sA);` |
|       194 | 1229 | `	PH7_MemObjRelease(&sB);` |
|       194 | 1230 | `	return bEq;` |
|     25873 | 1231 | `}` |
|         - | 1232 | `/*` |
|         - | 1233 | ` * Run the USER-VISIBLE string coercion over every element of pMap once, in` |
|         - | 1234 | ` * insertion order, discarding the result: php's array_diff/array_intersect sort` |
|         - | 1235 | ` * each input array, which converts every element exactly once, so this is where` |
|         - | 1236 | ` * their "Array to string conversion" warnings and their not-stringable-object` |
|         - | 1237 | ` * Error come from. Doing it as a pre-pass is what lets` |
|         - | 1238 | ` * array_diff([1,2],[1,new P()]) throw the way php's does even though the first` |
|         - | 1239 | ` * element already matched. Returns the throw status, SXRET_OK otherwise.` |
|         - | 1240 | ` */` |
|       286 | 1241 | `PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap)` |
|         5 | 1242 | `{` |
|       291 | 1243 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       291 | 1244 | `	sxu32 n = pMap->nEntry;` |
|      2736 | 1245 | `	while( n > 0 && pEntry ){` |
|      2458 | 1246 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|      2458 | 1247 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1248 | `			ph7_value sTmp;` |
|         - | 1249 | `			sxi32 rc;` |
|       246 | 1250 | `			PH7_MemObjInit(pMap->pVm,&sTmp);` |
|       246 | 1251 | `			PH7_MemObjLoad(pVal,&sTmp);` |
|       246 | 1252 | `			rc = PH7_MemObjToStringUV(&sTmp);` |
|       246 | 1253 | `			PH7_MemObjRelease(&sTmp);` |
|       246 | 1254 | `			if( rc != SXRET_OK ){` |
|         9 | 1255 | `				return rc;` |
|         - | 1256 | `			}` |
|       118 | 1257 | `		}` |
|      2450 | 1258 | `		pEntry = pEntry->pPrev; /* Reverse link — insertion order */` |
|      2450 | 1259 | `		n--;` |
|         5 | 1260 | `	}` |
|       283 | 1261 | `	return SXRET_OK;` |
|       148 | 1262 | `}` |
|         - | 1263 | `/*` |
|         - | 1264 | ` * Perform a linear search on a given hashmap, comparing values the way` |
|         - | 1265 | ` * array_diff()/array_intersect() do (see HashmapValueStrEq). Writes a pointer to` |
|         - | 1266 | ` * the target node on success; SXERR_NOTFOUND otherwise, with *pRc carrying the` |
|         - | 1267 | ` * status of a coercion that threw.` |
|         - | 1268 | ` */` |
|      1255 | 1269 | `PH7_PRIVATE int HashmapFindStringValue(` |
|         - | 1270 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1271 | `	ph7_value *pNeedle,  /* Lookup value */` |
|         - | 1272 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success */` |
|         - | 1273 | `	sxi32 *pRc           /* OUT: coercion status */` |
|         - | 1274 | `	)` |
|         5 | 1275 | `{` |
|      1260 | 1276 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      1260 | 1277 | `	sxu32 n = pMap->nEntry;` |
|      1260 | 1278 | `	*pRc = SXRET_OK;` |
|     49205 | 1279 | `	while( n > 0 && pEntry ){` |
|     48726 | 1280 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|     48726 | 1281 | `		if( pVal ){` |
|     48726 | 1282 | `			if( HashmapValueStrEq(pNeedle,pVal,/*bUserVisible*/0,pRc) ){` |
|       781 | 1283 | `				if( ppNode ){` |
|       ! 0 | 1284 | `					*ppNode = pEntry;` |
|       ! 0 | 1285 | `				}` |
|       781 | 1286 | `				return SXRET_OK;` |
|         - | 1287 | `			}` |
|     47950 | 1288 | `			if( *pRc != SXRET_OK ){` |
|       ! 0 | 1289 | `				return SXERR_NOTFOUND;` |
|         - | 1290 | `			}` |
|     25398 | 1291 | `		}` |
|     47950 | 1292 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     47950 | 1293 | `		n--;` |
|         5 | 1294 | `	}` |
|       483 | 1295 | `	return SXERR_NOTFOUND;` |
|       645 | 1296 | `}` |
|         - | 1297 | `/*` |
|         - | 1298 | ` * Compare two hashmaps.` |
|         - | 1299 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1300 | ` * Note on array comparison operators.` |
|         - | 1301 | ` *  According to the PHP language reference manual.` |
|         - | 1302 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1303 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1304 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1305 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1306 | ` *                          order and of the same types.` |
|         - | 1307 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1308 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1309 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1310 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1311 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1312 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1313 | ` * <?php` |
|         - | 1314 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1315 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1316 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1317 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1318 | ` * var_dump($c);` |
|         - | 1319 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1320 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1321 | ` * var_dump($c);` |
|         - | 1322 | ` * ?>` |
|         - | 1323 | ` * When executed, this script will print the following:` |
|         - | 1324 | ` * Union of $a and $b:` |
|         - | 1325 | ` * array(3) {` |
|         - | 1326 | ` *  ["a"]=>` |
|         - | 1327 | ` *  string(5) "apple"` |
|         - | 1328 | ` *  ["b"]=>` |
|         - | 1329 | ` * string(6) "banana"` |
|         - | 1330 | ` *  ["c"]=>` |
|         - | 1331 | ` * string(6) "cherry"` |
|         - | 1332 | ` * }` |
|         - | 1333 | ` * Union of $b and $a:` |
|         - | 1334 | ` * array(3) {` |
|         - | 1335 | ` * ["a"]=>` |
|         - | 1336 | ` * string(4) "pear"` |
|         - | 1337 | ` * ["b"]=>` |
|         - | 1338 | ` * string(10) "strawberry"` |
|         - | 1339 | ` * ["c"]=>` |
|         - | 1340 | ` * string(6) "cherry"` |
|         - | 1341 | ` * }` |
|         - | 1342 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1343 | ` */` |
|       427 | 1344 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1345 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1346 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1347 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1348 | `	)` |
|         5 | 1349 | `{` |
|         - | 1350 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1351 | `	sxi32 rc;` |
|         - | 1352 | `	sxu32 n;` |
|       432 | 1353 | `	if( pLeft == pRight ){` |
|         - | 1354 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1355 | `		 * Unlike the zend engine.` |
|         - | 1356 | `		 */` |
|         9 | 1357 | `		return 0;` |
|         - | 1358 | `	}` |
|       424 | 1359 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1360 | `		/* Must have the same number of entries */` |
|        24 | 1361 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1362 | `	}` |
|       402 | 1363 | `	if( bStrict ){` |
|         - | 1364 | `		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the` |
|         - | 1365 | `		 * same key/value pairs, with identical key types, in the same insertion` |
|         - | 1366 | `		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per` |
|         - | 1367 | `		 * this file's forward-iteration convention) in lockstep and compare each` |
|         - | 1368 | `		 * position's key then value. (Loose '==' below stays order-insensitive,` |
|         - | 1369 | `		 * matching each left key by lookup into the right map.) */` |
|       334 | 1370 | `		ph7_hashmap_node *pLs = pLeft->pFirst;` |
|       334 | 1371 | `		ph7_hashmap_node *pRs = pRight->pFirst;` |
|      9132 | 1372 | `		for( n = pLeft->nEntry ; n > 0 ; n-- ){` |
|         - | 1373 | `			/* Keys must match in type and value at this position */` |
|      8823 | 1374 | `			if( pLs->iType != pRs->iType ){` |
|         5 | 1375 | `				return 1;` |
|         - | 1376 | `			}` |
|      8819 | 1377 | `			if( pLs->iType == HASHMAP_INT_NODE ){` |
|      8572 | 1378 | `				if( pLs->xKey.iKey != pRs->xKey.iKey ){` |
|         3 | 1379 | `					return 1;` |
|         - | 1380 | `				}` |
|      4297 | 1381 | `			}else{` |
|       251 | 1382 | `				SyBlob *pLk = &pLs->xKey.sKey;` |
|       251 | 1383 | `				SyBlob *pRk = &pRs->xKey.sKey;` |
|       247 | 1384 | `				if( SyBlobLength(pLk) != SyBlobLength(pRk)` |
|       251 | 1385 | `				 \|\| (SyBlobLength(pLk) > 0` |
|       247 | 1386 | `				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){` |
|         7 | 1387 | `					return 1;` |
|         - | 1388 | `				}` |
|         - | 1389 | `			}` |
|         - | 1390 | `			/* Values must be strictly identical */` |
|      8811 | 1391 | `			if( HashmapNodeCmp(pLs,pRs,TRUE) != 0 ){` |
|         9 | 1392 | `				return 1;` |
|         - | 1393 | `			}` |
|      8803 | 1394 | `			pLs = pLs->pPrev; /* Reverse link = insertion order */` |
|      8803 | 1395 | `			pRs = pRs->pPrev;` |
|      4412 | 1396 | `		}` |
|       314 | 1397 | `		return 0; /* Same pairs, same order */` |
|         - | 1398 | `	}` |
|         - | 1399 | `	/* Point to the first inserted entry of the left hashmap */` |
|        71 | 1400 | `	pLe = pLeft->pFirst;` |
|        71 | 1401 | `	pRe = 0; /* cc warning */` |
|         - | 1402 | `	/* Perform the comparison */` |
|        71 | 1403 | `	n = pLeft->nEntry;` |
|       294 | 1404 | `	for(;;){` |
|       607 | 1405 | `		if( n < 1 ){` |
|        36 | 1406 | `			break;` |
|         - | 1407 | `		}` |
|       573 | 1408 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1409 | `			/* Int key */` |
|        55 | 1410 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        29 | 1411 | `		}else{` |
|       519 | 1412 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1413 | `			/* Blob key */` |
|       519 | 1414 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1415 | `		}` |
|       573 | 1416 | `		if( rc != SXRET_OK ){` |
|         - | 1417 | `			/* No such entry in the right side */` |
|        29 | 1418 | `			return 1;` |
|         - | 1419 | `		}` |
|       545 | 1420 | `		rc = 0;` |
|       545 | 1421 | `		if( bStrict ){` |
|         - | 1422 | `			/* Make sure,the keys are of the same type */` |
|       ! 0 | 1423 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1424 | `				rc = 1;` |
|       ! 0 | 1425 | `			}` |
|       ! 0 | 1426 | `		}` |
|       545 | 1427 | `		if( !rc ){` |
|         - | 1428 | `			/* Compare nodes */` |
|       545 | 1429 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|       263 | 1430 | `		}` |
|       545 | 1431 | `		if( rc != 0 ){` |
|         - | 1432 | `			/* Nodes key/value differ */` |
|         9 | 1433 | `			return rc;` |
|         - | 1434 | `		}` |
|         - | 1435 | `		/* Point to the next entry */` |
|       538 | 1436 | `		pLe = pLe->pPrev; /* Reverse link */` |
|       538 | 1437 | `		n--;` |
|         2 | 1438 | `	}` |
|        36 | 1439 | `	return 0; /* Hashmaps are equals */` |
|       216 | 1440 | `}` |
|         - | 1441 | `/*` |
|         - | 1442 | ` * Duplicate a hashmap node.` |
|         - | 1443 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1444 | ` */` |
|   1344012 | 1445 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1446 | `	ph7_hashmap *pDest,` |
|         - | 1447 | `	ph7_hashmap_node *pEntry,` |
|         - | 1448 | `	ph7_value *pVal,` |
|         - | 1449 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1450 | `	)` |
|         5 | 1451 | `{` |
|         - | 1452 | `	ph7_value sSafeVal;` |
|         - | 1453 | `	ph7_value sKey;` |
|         - | 1454 | `	sxi32 rc;` |
|         - | 1455 |  |
|   1344017 | 1456 | `	if( PH7_HashmapNodeIsRef(&(*pEntry)) ){` |
|         - | 1457 | ``		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points`` |
|         - | 1458 | `		 * at an outside slot) or, the case PH7 missed, an element somebody took a` |
|         - | 1459 | ``		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through`` |
|         - | 1460 | `		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep` |
|         - | 1461 | ``		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */`` |
|        28 | 1462 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|        28 | 1463 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        13 | 1464 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|        13 | 1465 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|        13 | 1466 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|        13 | 1467 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 1468 | `		}else{` |
|        16 | 1469 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         7 | 1470 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|        13 | 1471 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1472 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1473 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1474 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1475 | `			}else{ /* Dup: preserve the int key */` |
|        10 | 1476 | `				rc = HashmapKeepIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1477 | `			}` |
|         - | 1478 | `		}` |
|        28 | 1479 | `		return rc;` |
|         - | 1480 | `	}` |
|   1343991 | 1481 | `	sSafeVal = *pVal;` |
|         - | 1482 |  |
|   1343991 | 1483 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1484 | `		/* Blob key insertion */` |
|      7656 | 1485 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      7656 | 1486 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      7656 | 1487 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      7656 | 1488 | `		PH7_MemObjRelease(&sKey);` |
|      3823 | 1489 | `	}else{` |
|         - | 1490 | `		/* Int key */` |
|   1336340 | 1491 | `		if( iAction == 0 ){ /* Merge */` |
|   1327269 | 1492 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    672699 | 1493 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        34 | 1494 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        34 | 1495 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        34 | 1496 | `			PH7_MemObjRelease(&sKey);` |
|        18 | 1497 | `		}else{ /* Dup */` |
|      9044 | 1498 | `			rc = HashmapKeepIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1499 | `		}` |
|         - | 1500 | `	}` |
|   1343991 | 1501 | `	return rc;` |
|    671996 | 1502 | `}` |
|         - | 1503 | `/*` |
|         - | 1504 | ` * Merge two hashmaps.` |
|         - | 1505 | ` * Note on the merge process` |
|         - | 1506 | ` * According to the PHP language reference manual.` |
|         - | 1507 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1508 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1509 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1510 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1511 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1512 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1513 | ` *  keys starting from zero in the result array.` |
|         - | 1514 | ` */` |
|      3974 | 1515 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1516 | `{` |
|         - | 1517 | `	ph7_hashmap_node *pEntry;` |
|         - | 1518 | `	ph7_value *pVal;` |
|         - | 1519 | `	sxi32 rc;` |
|         - | 1520 | `	sxu32 n;` |
|      3979 | 1521 | `	if( pSrc == pDest ){` |
|         - | 1522 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1523 | `		 * Unlike the zend engine.` |
|         - | 1524 | `		 */` |
|       ! 0 | 1525 | `		return SXRET_OK;` |
|         - | 1526 | `	}` |
|         - | 1527 | `	/* Point to the first inserted entry in the source */` |
|      3979 | 1528 | `	pEntry = pSrc->pFirst;` |
|         - | 1529 | `	/* Perform the merge */` |
|   1331465 | 1530 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1531 | `		/* Extract the node value */` |
|   1327491 | 1532 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   1327491 | 1533 | `		if( pVal ){` |
|         - | 1534 | `			/* Make a local copy of the value.` |
|         - | 1535 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1536 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1537 | `			 * to the old pool.` |
|         - | 1538 | `			 */` |
|   1327491 | 1539 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    663739 | 1540 | `		}else{` |
|       ! 0 | 1541 | `			rc = SXRET_OK;` |
|         - | 1542 | `		}` |
|   1327491 | 1543 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1544 | `			return rc;` |
|         - | 1545 | `		}` |
|         - | 1546 | `		/* Point to the next entry */` |
|   1327491 | 1547 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    663739 | 1548 | `	}` |
|      3979 | 1549 | `	return SXRET_OK;` |
|      1992 | 1550 | `}` |
|         - | 1551 | `/*` |
|         - | 1552 | ` * Overwrite entries with the same key.` |
|         - | 1553 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1554 | ` *  According to the PHP language reference manual.` |
|         - | 1555 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1556 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1557 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1558 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1559 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1560 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1561 | ` *  overwriting the previous values.` |
|         - | 1562 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1563 | ` *  by whatever type is in the second array.` |
|         - | 1564 | ` */` |
|        38 | 1565 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1566 | `{` |
|         - | 1567 | `	ph7_hashmap_node *pEntry;` |
|         - | 1568 | `	ph7_value *pVal;` |
|         - | 1569 | `	sxi32 rc;` |
|         - | 1570 | `	sxu32 n;` |
|        40 | 1571 | `	if( pSrc == pDest ){` |
|         - | 1572 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1573 | `		 * Unlike the zend engine.` |
|         - | 1574 | `		 */` |
|       ! 0 | 1575 | `		return SXRET_OK;` |
|         - | 1576 | `	}` |
|         - | 1577 | `	/* Point to the first inserted entry in the source */` |
|        40 | 1578 | `	pEntry = pSrc->pFirst;` |
|         - | 1579 | `	/* Perform the merge */` |
|        88 | 1580 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1581 | `		/* Extract the node value */` |
|        50 | 1582 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        50 | 1583 | `		if( pVal ){` |
|        50 | 1584 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        26 | 1585 | `		}else{` |
|       ! 0 | 1586 | `			rc = SXRET_OK;` |
|         - | 1587 | `		}` |
|        50 | 1588 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1589 | `			return rc;` |
|         - | 1590 | `		}` |
|         - | 1591 | `		/* Point to the next entry */` |
|        50 | 1592 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        26 | 1593 | `	}` |
|        40 | 1594 | `	return SXRET_OK;` |
|        21 | 1595 | `}` |
|         - | 1596 | `/*` |
|         - | 1597 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1598 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1599 | ` */` |
|     13352 | 1600 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1601 | `{` |
|         - | 1602 | `	ph7_hashmap_node *pEntry;` |
|         - | 1603 | `	ph7_value *pVal;` |
|         - | 1604 | `	sxi32 rc;` |
|         - | 1605 | `	sxu32 n;` |
|     13357 | 1606 | `	if( pSrc == pDest ){` |
|         - | 1607 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1608 | `		 * Unlike the zend engine.` |
|         - | 1609 | `		 */` |
|       ! 0 | 1610 | `		return SXRET_OK;` |
|         - | 1611 | `	}` |
|         - | 1612 | `	/* Point to the first inserted entry in the source */` |
|     13357 | 1613 | `	pEntry = pSrc->pFirst;` |
|         - | 1614 | `	/* Perform the duplication */` |
|     29835 | 1615 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1616 | `		/* Extract the node value */` |
|     16483 | 1617 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     16483 | 1618 | `		if( pVal ){` |
|     16483 | 1619 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      8238 | 1620 | `		}else{` |
|       ! 0 | 1621 | `			rc = SXRET_OK;` |
|         - | 1622 | `		}` |
|     16483 | 1623 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1624 | `			return rc;` |
|         - | 1625 | `		}` |
|         - | 1626 | `		/* Point to the next entry */` |
|     16483 | 1627 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8238 | 1628 | `	}` |
|     13357 | 1629 | `	return SXRET_OK;` |
|      6672 | 1630 | `}` |
|         - | 1631 | `/*` |
|         - | 1632 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1633 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1634 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1635 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1636 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1637 | ` * this instead of PH7_HashmapDup.` |
|         - | 1638 | ` */` |
|        12 | 1639 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1640 | `{` |
|         - | 1641 | `	ph7_hashmap_node *pEntry;` |
|         - | 1642 | `	ph7_value *pVal;` |
|         - | 1643 | `	sxi32 rc;` |
|         - | 1644 | `	sxu32 n;` |
|        13 | 1645 | `	if( pSrc == pDest ){` |
|       ! 0 | 1646 | `		return SXRET_OK;` |
|         - | 1647 | `	}` |
|        13 | 1648 | `	pEntry = pSrc->pFirst;` |
|       939 | 1649 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1650 | `		/* Extract the node value (resolves foreign references) */` |
|       927 | 1651 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       926 | 1652 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       597 | 1653 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1654 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1655 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1656 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1657 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1658 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1659 | `			pVal = 0;` |
|         2 | 1660 | `		}` |
|       927 | 1661 | `		if( pVal ){` |
|       923 | 1662 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1378 | 1663 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       459 | 1664 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       460 | 1665 | `			}else{` |
|         5 | 1666 | `				rc = HashmapKeepIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1667 | `			}` |
|       923 | 1668 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1669 | `				return rc;` |
|         - | 1670 | `			}` |
|       461 | 1671 | `		}` |
|         - | 1672 | `		/* Point to the next entry */` |
|       927 | 1673 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       464 | 1674 | `	}` |
|        13 | 1675 | `	return SXRET_OK;` |
|         7 | 1676 | `}` |
|         - | 1677 | `/*` |
|         - | 1678 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1679 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1680 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1681 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1682 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1683 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1684 | ` * iterate-a-snapshot semantic.` |
|         - | 1685 | ` */` |
|        48 | 1686 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1687 | `{` |
|         - | 1688 | `	ph7_foreach_step *pStep;` |
|        51 | 1689 | `	sxi32 nRef = 0;` |
|        99 | 1690 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        51 | 1691 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1692 | `			nRef++;` |
|        21 | 1693 | `		}` |
|        27 | 1694 | `	}` |
|        51 | 1695 | `	return nRef;` |
|         3 | 1696 | `}` |
|         - | 1697 | `/*` |
|         - | 1698 | ` * Copy-on-write separation for arrays.` |
|         - | 1699 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1700 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1701 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1702 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1703 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1704 | ` * on the live map the loop is walking, like php.` |
|         - | 1705 | ` */` |
|    551764 | 1706 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1707 | `{` |
|    551769 | 1708 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1709 | `	ph7_hashmap *pNew;` |
|         - | 1710 | `	ph7_value *pBacking;` |
|         - | 1711 | `	sxu32 nValIdx;` |
|         - | 1712 | `	int bValueInPool;` |
|    551769 | 1713 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    551769 | 1714 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1715 | `		/* Sole owner, no separation needed */` |
|    541025 | 1716 | `		return pMap;` |
|         - | 1717 | `	}` |
|     10749 | 1718 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1719 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1720 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1721 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       536 | 1722 | `		return pMap;` |
|         - | 1723 | `	}` |
|         - | 1724 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1725 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1726 | `	 * frame is popped. */` |
|     10215 | 1727 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|     10189 | 1728 | `		pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pValue->nIdx);` |
|     10184 | 1729 | `		if( pBacking && pBacking != pValue` |
|      9566 | 1730 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      8953 | 1731 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1732 | `			/* Undo the stack ref to reveal true sharing count */` |
|      8911 | 1733 | `			pMap->iRef--;` |
|      8911 | 1734 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1735 | `				/* After undoing stack ref, sole owner — no separation */` |
|      8359 | 1736 | `				pMap->iRef++;` |
|      8359 | 1737 | `				return pMap;` |
|         - | 1738 | `			}` |
|       556 | 1739 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|       556 | 1740 | `			if( pNew == 0 ){` |
|       ! 0 | 1741 | `				pMap->iRef++;` |
|       ! 0 | 1742 | `				return pMap;` |
|         - | 1743 | `			}` |
|       556 | 1744 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1745 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1746 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1747 | `				pMap->iRef++;` |
|       ! 0 | 1748 | `				return pMap;` |
|         - | 1749 | `			}` |
|       556 | 1750 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|       556 | 1751 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1752 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1753 | `			 * used to grow — and therefore reallocate (move) — pVm->aMemObj and` |
|         - | 1754 | `			 * invalidate the pBacking pointer captured above; the stale pointer` |
|         - | 1755 | `			 * was a hard SIGSEGV once the table was big enough to be mmap-backed.` |
|         - | 1756 | `			 * Redundant since P1 -- the pool's segments are fixed, so a slot's` |
|         - | 1757 | `			 * address never moves. Left for the harvest sweep (PERF.md P1). */` |
|       556 | 1758 | `			pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pValue->nIdx);` |
|       556 | 1759 | `			if( pBacking ){` |
|       556 | 1760 | `				pBacking->x.pOther = pNew;` |
|       276 | 1761 | `			}` |
|         - | 1762 | `			/* Update the stack value to match */` |
|       556 | 1763 | `			pValue->x.pOther = pNew;` |
|       556 | 1764 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|       556 | 1765 | `			return pNew;` |
|         - | 1766 | `		}` |
|       639 | 1767 | `	}` |
|         - | 1768 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1769 | `	 * directly into pVm->aMemObj, and PH7_HashmapDup below reserves a memory` |
|         - | 1770 | `	 * object per duplicated entry — which used to reallocate (move) the table and` |
|         - | 1771 | `	 * leave such a pValue dangling, so the slot identity is captured here and the` |
|         - | 1772 | `	 * write-back re-resolves from the index. Redundant since P1 -- the pool's` |
|         - | 1773 | `	 * segments are fixed, so a slot's address never moves. Left for the harvest` |
|         - | 1774 | `	 * sweep (PERF.md P1). */` |
|      1309 | 1775 | `	nValIdx = pValue->nIdx;` |
|      1948 | 1776 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      1304 | 1777 | `		&& (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      1309 | 1778 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      1309 | 1779 | `	if( pNew == 0 ){` |
|         - | 1780 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1781 | `		return pMap;` |
|         - | 1782 | `	}` |
|      1309 | 1783 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1784 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1785 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1786 | `		return pMap;` |
|         - | 1787 | `	}` |
|      1309 | 1788 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      1309 | 1789 | `	pMap->iRef--;` |
|      1309 | 1790 | `	if( bValueInPool ){` |
|         - | 1791 | `		/* Re-resolve pValue's slot. Redundant since P1 (see above): the dup can no` |
|         - | 1792 | `		 * longer move it. Left for the harvest sweep (PERF.md P1). */` |
|      1241 | 1793 | `		pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nValIdx);` |
|      1241 | 1794 | `		if( pValue == 0 ){` |
|       ! 0 | 1795 | `			return pNew;` |
|         - | 1796 | `		}` |
|       618 | 1797 | `	}` |
|      1309 | 1798 | `	pValue->x.pOther = pNew;` |
|      1309 | 1799 | `	return pNew;` |
|    275742 | 1800 | `}` |
|         - | 1801 | `/*` |
|         - | 1802 | ` * Perform the union of two hashmaps.` |
|         - | 1803 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1804 | ` * with a variable holding an array as follows:` |
|         - | 1805 | ` * <?php` |
|         - | 1806 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1807 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1808 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1809 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1810 | ` * var_dump($c);` |
|         - | 1811 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1812 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1813 | ` * var_dump($c);` |
|         - | 1814 | ` * ?>` |
|         - | 1815 | ` * When executed, this script will print the following:` |
|         - | 1816 | ` * Union of $a and $b:` |
|         - | 1817 | ` * array(3) {` |
|         - | 1818 | ` *  ["a"]=>` |
|         - | 1819 | ` *  string(5) "apple"` |
|         - | 1820 | ` *  ["b"]=>` |
|         - | 1821 | ` * string(6) "banana"` |
|         - | 1822 | ` *  ["c"]=>` |
|         - | 1823 | ` * string(6) "cherry"` |
|         - | 1824 | ` * }` |
|         - | 1825 | ` * Union of $b and $a:` |
|         - | 1826 | ` * array(3) {` |
|         - | 1827 | ` * ["a"]=>` |
|         - | 1828 | ` * string(4) "pear"` |
|         - | 1829 | ` * ["b"]=>` |
|         - | 1830 | ` * string(10) "strawberry"` |
|         - | 1831 | ` * ["c"]=>` |
|         - | 1832 | ` * string(6) "cherry"` |
|         - | 1833 | ` * }` |
|         - | 1834 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1835 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1836 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1837 | ` */` |
|      5977 | 1838 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1839 | `{` |
|         - | 1840 | `	ph7_hashmap_node *pEntry;` |
|      5982 | 1841 | `	sxi32 rc = SXRET_OK;` |
|         - | 1842 | `	ph7_value *pObj;` |
|         - | 1843 | `	sxu32 n;` |
|      5982 | 1844 | `	if( pLeft == pRight ){` |
|         - | 1845 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1846 | `		 * Unlike the zend engine.` |
|         - | 1847 | `		 */` |
|       ! 0 | 1848 | `		return SXRET_OK;` |
|         - | 1849 | `	}` |
|         - | 1850 | `	/* Perform the union */` |
|      5982 | 1851 | `	pEntry = pRight->pFirst;` |
|      6045 | 1852 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1853 | `		/* Make sure the given key does not exists in the left array */` |
|        67 | 1854 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1855 | `			/* BLOB key */` |
|        27 | 1856 | `			if( SXRET_OK !=` |
|        23 | 1857 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        23 | 1858 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        23 | 1859 | `					if( pObj ){` |
|        23 | 1860 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1861 | `						/* Perform the insertion */` |
|        23 | 1862 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1863 | `							&sSafeVal,0,FALSE);` |
|        23 | 1864 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1865 | `							return rc;` |
|         - | 1866 | `						}` |
|         9 | 1867 | `					}` |
|         9 | 1868 | `			}` |
|        15 | 1869 | `		}else{` |
|         - | 1870 | `			/* INT key */` |
|        42 | 1871 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        21 | 1872 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        21 | 1873 | `				if( pObj ){` |
|        21 | 1874 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1875 | `					/* Perform the insertion */` |
|        21 | 1876 | `					rc = HashmapKeepIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        21 | 1877 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1878 | `						return rc;` |
|         - | 1879 | `					}` |
|        10 | 1880 | `				}` |
|        10 | 1881 | `			}` |
|         - | 1882 | `		}` |
|         - | 1883 | `		/* Point to the next entry */` |
|        67 | 1884 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 1885 | `	}` |
|      5982 | 1886 | `	return SXRET_OK;` |
|      2989 | 1887 | `}` |
|         - | 1888 | `/*` |
|         - | 1889 | ` * Allocate a new hashmap.` |
|         - | 1890 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1891 | ` */` |
|   4739313 | 1892 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1893 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1894 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1895 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1896 | `	)` |
|         5 | 1897 | `{` |
|         - | 1898 | `	ph7_hashmap *pMap;` |
|         - | 1899 | `	/* Allocate a new instance */` |
|   4739318 | 1900 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   4739318 | 1901 | `	if( pMap == 0 ){` |
|       ! 0 | 1902 | `		return 0;` |
|         - | 1903 | `	}` |
|         - | 1904 | `	/* Zero the structure */` |
|   4739318 | 1905 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1906 | `	/* Fill in the structure */` |
|   4739318 | 1907 | `	pMap->pVm = &(*pVm);` |
|   4739318 | 1908 | `	pMap->iRef = 1;` |
|         - | 1909 | `	/* Default hash functions */` |
|   4739318 | 1910 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   4739318 | 1911 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   4739318 | 1912 | `	return pMap;` |
|   2369300 | 1913 | `}` |
|         - | 1914 | `/*` |
|         - | 1915 | ` * Install superglobals in the given virtual machine.` |
|         - | 1916 | ` * Note on superglobals.` |
|         - | 1917 | ` *  According to the PHP language reference manual.` |
|         - | 1918 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1919 | `*   Description` |
|         - | 1920 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1921 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1922 | `*   global $variable; to access them within functions or methods.` |
|         - | 1923 | `*   These superglobal variables are:` |
|         - | 1924 | `*    $GLOBALS` |
|         - | 1925 | `*    $_SERVER` |
|         - | 1926 | `*    $_GET` |
|         - | 1927 | `*    $_POST` |
|         - | 1928 | `*    $_FILES` |
|         - | 1929 | `*    $_COOKIE` |
|         - | 1930 | `*    $_SESSION` |
|         - | 1931 | `*    $_REQUEST` |
|         - | 1932 | `*    $_ENV` |
|         - | 1933 | `*/` |
|      5635 | 1934 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1935 | `{` |
|         - | 1936 | `	static const char * azSuper[] = {` |
|         - | 1937 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1938 | `		"_GET",      /* $_GET */` |
|         - | 1939 | `		"_POST",     /* $_POST */` |
|         - | 1940 | `		"_FILES",    /* $_FILES */` |
|         - | 1941 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1942 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1943 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1944 | `		"_ENV",      /* $_ENV */` |
|         - | 1945 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1946 | `		"argv"       /* $argv */` |
|         - | 1947 | `	};` |
|         - | 1948 | `	ph7_hashmap *pMap;` |
|         - | 1949 | `	ph7_value *pObj;` |
|         - | 1950 | `	SyString *pFile;` |
|         - | 1951 | `	sxi32 rc;` |
|         - | 1952 | `	sxu32 n;` |
|         - | 1953 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      5640 | 1954 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      5640 | 1955 | `	if( pMap == 0 ){` |
|       ! 0 | 1956 | `		return SXERR_MEM;` |
|         - | 1957 | `	}` |
|      5640 | 1958 | `	pVm->pGlobal = pMap;` |
|         - | 1959 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      5640 | 1960 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      5640 | 1961 | `	if( pObj == 0 ){` |
|       ! 0 | 1962 | `		return SXERR_MEM;` |
|         - | 1963 | `	}` |
|      5640 | 1964 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1965 | `	/* Record object index */` |
|      5640 | 1966 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1967 | `	/* Install the special $GLOBALS array */` |
|      5640 | 1968 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      5640 | 1969 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1970 | `		return rc;` |
|         - | 1971 | `	}` |
|      5640 | 1972 | `	PH7_VmSuperNote(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1);` |
|         - | 1973 | `	/* Install superglobals now */` |
|     61990 | 1974 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1975 | `		ph7_value *pSuper;` |
|         - | 1976 | `		/* Request an empty array */` |
|     56355 | 1977 | `		pSuper = ph7_new_array(&(*pVm));` |
|     56355 | 1978 | `		if( pSuper == 0 ){` |
|       ! 0 | 1979 | `			return SXERR_MEM;` |
|         - | 1980 | `		}` |
|         - | 1981 | `		/* Install */` |
|     56355 | 1982 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     56355 | 1983 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1984 | `			return rc;` |
|         - | 1985 | `		}` |
|         - | 1986 | `		/* Release the value now it have been installed */` |
|     56355 | 1987 | `		ph7_release_value(&(*pVm),pSuper);` |
|     28135 | 1988 | `	}` |
|         - | 1989 | `	/* Set some $_SERVER entries */` |
|      5640 | 1990 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1991 | `	/*` |
|         - | 1992 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1993 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1994 | `	 */` |
|     11275 | 1995 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1996 | `		"SCRIPT_FILENAME",` |
|      2813 | 1997 | `		pFile ? pFile->zString : ":Memory:",` |
|      5635 | 1998 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1999 | `		);` |
|         - | 2000 | `	/* All done,all super-global are installed now */` |
|      5640 | 2001 | `	return SXRET_OK;` |
|      2818 | 2002 | `}` |
|         - | 2003 | `/*` |
|         - | 2004 | ` * Release a hashmap.` |
|         - | 2005 | ` */` |
|   4558510 | 2006 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 2007 | `{` |
|         - | 2008 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   4558515 | 2009 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 2010 | `	sxu32 n;` |
|   4558515 | 2011 | `	if( pMap == pVm->pGlobal ){` |
|         - | 2012 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 2013 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 2014 | `		return SXRET_OK;` |
|         - | 2015 | `	}` |
|   4558515 | 2016 | `	if( pMap->pActiveSteps ){` |
|         - | 2017 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 2018 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 2019 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 2020 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 2021 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 2022 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 2023 | `		ph7_foreach_step *pStep;` |
|        17 | 2024 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 2025 | `			pStep->pCursor = 0;` |
|         5 | 2026 | `		}` |
|         4 | 2027 | `	}` |
|         - | 2028 | `	/* Start the release process */` |
|   4558515 | 2029 | `	n = 0;` |
|   4558515 | 2030 | `	pEntry = pMap->pFirst;` |
|   8021539 | 2031 | `	for(;;){` |
|  16049082 | 2032 | `		if( n >= pMap->nEntry ){` |
|   4558515 | 2033 | `			break;` |
|         - | 2034 | `		}` |
|  11490572 | 2035 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 2036 | `		/* Remove the reference from the foreign table */` |
|  11490572 | 2037 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|         - | 2038 | `		/* Restore the ph7_value to the free list if this node was its last holder` |
|         - | 2039 | `		 * (PH7_HashmapUnlinkNode explains both halves) */` |
|  11490572 | 2040 | `		PH7_VmReleaseUnheldSlot(pVm,pEntry->nValIdx);` |
|         - | 2041 | `		/* Release the node */` |
|  11490572 | 2042 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   3506482 | 2043 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   1751133 | 2044 | `		}` |
|  11490572 | 2045 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 2046 | `		/* Point to the next entry */` |
|  11490572 | 2047 | `		pEntry = pNext;` |
|  11490572 | 2048 | `		n++;` |
|         5 | 2049 | `	}` |
|   4558515 | 2050 | `	if( pMap->nEntry > 0 ){` |
|         - | 2051 | `		/* Release the hash bucket */` |
|   2321701 | 2052 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   1160628 | 2053 | `	}` |
|   4558515 | 2054 | `	if( FreeDS ){` |
|         - | 2055 | `		/* Free the whole instance -- and stop the collector's root buffer naming` |
|         - | 2056 | `		 * memory that is going back to the pool. */` |
|   4558505 | 2057 | `		PH7_GcForget(pVm,(void *)pMap,1);` |
|   4558505 | 2058 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   2278997 | 2059 | `	}else{` |
|         - | 2060 | `		/* Keep the instance but reset it's fields */` |
|        12 | 2061 | `		pMap->apBucket = 0;` |
|        12 | 2062 | `		pMap->iNextIdx = 0;` |
|        12 | 2063 | `	pMap->bIntKeySeen = 0;` |
|        12 | 2064 | `		pMap->nEntry = pMap->nSize = 0;` |
|        12 | 2065 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 2066 | `	}` |
|   4558515 | 2067 | `	return SXRET_OK;` |
|   2279002 | 2068 | `}` |
|         - | 2069 | `/*` |
|         - | 2070 | ` * Decrement the reference count of a given hashmap.` |
|         - | 2071 | ` * If the count reaches zero which mean no more variables` |
|         - | 2072 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 2073 | ` */` |
|   9942457 | 2074 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 2075 | `{` |
|   9942462 | 2076 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 2077 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|   9942462 | 2078 | `	pMap->iRef--;` |
|   9942462 | 2079 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   4558407 | 2080 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   7663003 | 2081 | `	}else if( pMap->iRef >= 1 ){` |
|         - | 2082 | `		/* Still held -- but by whom? A drop that does NOT reach zero is the only` |
|         - | 2083 | `		 * event that can strand a cycle, so it is what the collector buffers. */` |
|   5384060 | 2084 | `		PH7_GcPossibleRoot(pVm,(void *)pMap,1);` |
|   2691334 | 2085 | `	}` |
|   9942462 | 2086 | `}` |
|         - | 2087 | `/*` |
|         - | 2088 | ` * Check if a given key exists in the given hashmap.` |
|         - | 2089 | ` * Write a pointer to the target node on success.` |
|         - | 2090 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 2091 | ` */` |
|   2142196 | 2092 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 2093 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 2094 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 2095 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 2096 | `	)` |
|         5 | 2097 | `{` |
|         - | 2098 | `	sxi32 rc;` |
|   2142201 | 2099 | `	if( pMap->nEntry < 1 ){` |
|         - | 2100 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 2101 | `		 */` |
|      5957 | 2102 | `		return SXERR_NOTFOUND;` |
|         - | 2103 | `	}` |
|   2136249 | 2104 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   2136249 | 2105 | `	return rc;` |
|   1071029 | 2106 | `}` |
|         - | 2107 | `/*` |
|         - | 2108 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 2109 | ` * hashmap.` |
|         - | 2110 | ` * If a node with the given key already exists in the database` |
|         - | 2111 | ` * then this function overwrite the old value.` |
|         - | 2112 | ` */` |
|  10280214 | 2113 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 2114 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2115 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2116 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 2117 | `	)` |
|         5 | 2118 | `{` |
|         - | 2119 | `	sxi32 rc;` |
|         - | 2120 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 2121 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 2122 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 2123 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|  10280219 | 2124 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|  10280219 | 2125 | `	return rc;` |
|         5 | 2126 | `}` |
|         - | 2127 | `/*` |
|         - | 2128 | ` * Insert (or overwrite) an entry under a RAW string key: php's zend_hash_update()` |
|         - | 2129 | ` * as opposed to the zend_symtable_update() every array subscript goes through, so` |
|         - | 2130 | ` * a numeric-looking NAME is kept as a string key instead of folding to the integer` |
|         - | 2131 | ` * one. php reaches for this where a name comes from OUTSIDE the language — the` |
|         - | 2132 | ``  * session store's `7\|i:1;` really does become a string key "7" that no `$_SESSION[7]` `` |
|         - | 2133 | `` * or `$_SESSION["7"]` can then reach. Only such a name-carrying reader should use it.`` |
|         - | 2134 | ` */` |
|      3330 | 2135 | `PH7_PRIVATE sxi32 PH7_HashmapInsertRawKey(` |
|         - | 2136 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 2137 | `	const char *zKey,    /* Raw key bytes */` |
|         - | 2138 | `	sxu32 nKey,          /* Key length */` |
|         - | 2139 | `	ph7_value *pVal      /* Node value */` |
|         - | 2140 | `	)` |
|         4 | 2141 | `{` |
|      3334 | 2142 | `	ph7_hashmap_node *pNode = 0;` |
|      3334 | 2143 | `	if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),zKey,nKey,&pNode) && pNode ){` |
|        27 | 2144 | `		ph7_value *pElem = (ph7_value *)PH7_MemObjAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        27 | 2145 | `		if( pElem ){` |
|        27 | 2146 | `			if( pVal ){` |
|        27 | 2147 | `				PH7_MemObjStore(pVal,pElem);` |
|        14 | 2148 | `			}else{` |
|       ! 0 | 2149 | `				PH7_MemObjToNull(pElem);` |
|         - | 2150 | `			}` |
|        13 | 2151 | `		}` |
|        27 | 2152 | `		return SXRET_OK;` |
|         - | 2153 | `	}` |
|      3308 | 2154 | `	return HashmapInsertBlobKey(&(*pMap),zKey,nKey,&(*pVal),0,FALSE);` |
|      1669 | 2155 | `}` |
|         - | 2156 | `/*` |
|         - | 2157 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 2158 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 2159 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 2160 | ` * This is the same routine that backs array_merge().` |
|         - | 2161 | ` */` |
|       664 | 2162 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         3 | 2163 | `{` |
|       667 | 2164 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         3 | 2165 | `}` |
|         - | 2166 | `/*` |
|         - | 2167 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 2168 | ` * hashmap.` |
|         - | 2169 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 2170 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 2171 | ` * The insertion by reference is triggered when the following` |
|         - | 2172 | ` * expression is encountered.` |
|         - | 2173 | ` * $var = 10;` |
|         - | 2174 | ` *  $a = array(&var);` |
|         - | 2175 | ` * OR` |
|         - | 2176 | ` *  $a[] =& $var;` |
|         - | 2177 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 2178 | ` * over it's contents.` |
|         - | 2179 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 2180 | ` * removed when the foreign ph7_value is unset.` |
|         - | 2181 | ` * Example:` |
|         - | 2182 | ` *  $var = 10;` |
|         - | 2183 | ` *  $a[] =& $var;` |
|         - | 2184 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 2185 | ` *  //Unset the foreign ph7_value now` |
|         - | 2186 | ` *  unset($var);` |
|         - | 2187 | ` *  echo count($a); //0` |
|         - | 2188 | ` * Note that this is a PH7 eXtension.` |
|         - | 2189 | ` * Refer to the official documentation for more information.` |
|         - | 2190 | ` * If a node with the given key already exists in the database` |
|         - | 2191 | ` * then this function overwrite the old value.` |
|         - | 2192 | ` */` |
|     79641 | 2193 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 2194 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2195 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2196 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 2197 | `	)` |
|         5 | 2198 | `{` |
|         - | 2199 | `	sxi32 rc;` |
|     79646 | 2200 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 2201 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 2202 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 2203 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 2204 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 2205 | `		return PH7_ABORT;` |
|         - | 2206 | `	}` |
|     79646 | 2207 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     79646 | 2208 | `	return rc;` |
|     39745 | 2209 | `}` |
|         - | 2210 | `/*` |
|         - | 2211 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 2212 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 2213 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 2214 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 2215 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 2216 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 2217 | ` */` |
|     40334 | 2218 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2219 | `{` |
|     40339 | 2220 | `	pStep->pCursor = pMap->pFirst;` |
|     40339 | 2221 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     40339 | 2222 | `	pMap->pActiveSteps = pStep;` |
|     40339 | 2223 | `}` |
|         - | 2224 | `/*` |
|         - | 2225 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 2226 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 2227 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 2228 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 2229 | ` */` |
|     40292 | 2230 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2231 | `{` |
|     40297 | 2232 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     40297 | 2233 | `	while( *ppLink ){` |
|     40297 | 2234 | `		if( *ppLink == pStep ){` |
|     40297 | 2235 | `			*ppLink = pStep->pNextActive;` |
|     40297 | 2236 | `			pStep->pNextActive = 0;` |
|     40297 | 2237 | `			return;` |
|         - | 2238 | `		}` |
|       ! 0 | 2239 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 2240 | `	}` |
|     20119 | 2241 | `}` |
|         - | 2242 | `/*` |
|         - | 2243 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 2244 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 2245 | ` * return NULL.` |
|         - | 2246 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 2247 | ` */` |
|      1089 | 2248 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         5 | 2249 | `{` |
|      1094 | 2250 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|      1094 | 2251 | `	if( pCur == 0 ){` |
|         - | 2252 | `		/* End of the list,return null */` |
|       571 | 2253 | `		return 0;` |
|         - | 2254 | `	}` |
|         - | 2255 | `	/* Advance the node cursor */` |
|       528 | 2256 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       528 | 2257 | `	return pCur;` |
|       522 | 2258 | `}` |
|         - | 2259 | `/*` |
|         - | 2260 | ` * Extract a node value.` |
|         - | 2261 | ` */` |
|   2581091 | 2262 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2263 | `{` |
|   2581096 | 2264 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|   2581096 | 2265 | `	if( pEntry ){` |
|   2581096 | 2266 | `		if( bStore ){` |
|   1740825 | 2267 | `			PH7_MemObjStore(pEntry,pValue);` |
|    870030 | 2268 | `		}else{` |
|    840276 | 2269 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2270 | `		}` |
|   1289864 | 2271 | `	}else{` |
|       ! 0 | 2272 | `		PH7_MemObjRelease(pValue);` |
|         - | 2273 | `	}` |
|   2581096 | 2274 | `}` |
|         - | 2275 | `/*` |
|         - | 2276 | ` * Extract a node key.` |
|         - | 2277 | ` */` |
|   1642668 | 2278 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2279 | `{` |
|         - | 2280 | `	/* Fill with the current key */` |
|   1642673 | 2281 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   1611392 | 2282 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        63 | 2283 | `			SyBlobRelease(&pKey->sBlob);` |
|        31 | 2284 | `		}` |
|   1611392 | 2285 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   1611392 | 2286 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|    805558 | 2287 | `	}else{` |
|     31286 | 2288 | `		SyBlobReset(&pKey->sBlob);` |
|     31286 | 2289 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     31286 | 2290 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2291 | `	}` |
|   1642673 | 2292 | `}` |
|         - | 2293 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2294 | `/*` |
|         - | 2295 | ` * Store the address of nodes value in the given container.` |
|         - | 2296 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2297 | ` * defined in 'builtin.c' for more information.` |
|         - | 2298 | ` */` |
|        28 | 2299 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         2 | 2300 | `{` |
|        30 | 2301 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2302 | `	ph7_value *pValue;` |
|         - | 2303 | `	sxu32 n;` |
|         - | 2304 | `	/* Initialize the container */` |
|        30 | 2305 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        84 | 2306 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2307 | `		/* Extract node value */` |
|        56 | 2308 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        56 | 2309 | `		if( pValue ){` |
|        56 | 2310 | `			SySetPut(pOut,(const void *)&pValue);` |
|        27 | 2311 | `		}` |
|         - | 2312 | `		/* Point to the next entry */` |
|        56 | 2313 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 2314 | `	}` |
|         - | 2315 | `	/* Total inserted entries */` |
|        30 | 2316 | `	return (int)SySetUsed(pOut);` |
|         2 | 2317 | `}` |
|         - | 2318 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2319 | `/*` |
|         - | 2320 | ` * Table of hashmap functions.` |
|         - | 2321 | ` */` |
|         - | 2322 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 2323 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 2324 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 2325 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 2326 | `	{"count",             ph7_hashmap_count },` |
|         - | 2327 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 2328 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 2329 | `	{"key_exists",        ph7_hashmap_key_exists },` |
|         - | 2330 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 2331 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 2332 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 2333 | `	{"array_unshift",     ph7_hashmap_unshift },` |
|         - | 2334 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 2335 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 2336 | `	{"max",               ph7_hashmap_max     },` |
|         - | 2337 | `	{"min",               ph7_hashmap_min     },` |
|         - | 2338 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 2339 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 2340 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 2341 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 2342 | `	{"array_merge_recursive", ph7_hashmap_merge_recursive },` |
|         - | 2343 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 2344 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 2345 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 2346 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 2347 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 2348 | `	{"array_udiff_assoc", ph7_hashmap_udiff_assoc },` |
|         - | 2349 | `	{"array_udiff_uassoc",ph7_hashmap_udiff_uassoc },` |
|         - | 2350 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 2351 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 2352 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 2353 | `	{"array_diff_ukey",   ph7_hashmap_diff_ukey },` |
|         - | 2354 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 2355 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 2356 | `	{"array_intersect_uassoc", ph7_hashmap_intersect_uassoc},` |
|         - | 2357 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 2358 | `	{"array_uintersect_assoc", ph7_hashmap_uintersect_assoc},` |
|         - | 2359 | `	{"array_uintersect_uassoc", ph7_hashmap_uintersect_uassoc},` |
|         - | 2360 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 2361 | `	{"array_multisort",   ph7_hashmap_multisort },` |
|         - | 2362 | `	{"array_intersect_ukey",  ph7_hashmap_intersect_ukey},` |
|         - | 2363 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 2364 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 2365 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 2366 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 2367 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 2368 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 2369 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 2370 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 2371 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 2372 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 2373 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 2374 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 2375 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 2376 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 2377 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 2378 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 2379 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 2380 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 2381 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 2382 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 2383 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 2384 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 2385 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 2386 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 2387 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 2388 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 2389 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 2390 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 2391 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 2392 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 2393 | `	{"natsort",           ph7_hashmap_natsort },` |
|         - | 2394 | `	{"natcasesort",       ph7_hashmap_natsort },` |
|         - | 2395 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 2396 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 2397 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 2398 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 2399 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 2400 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 2401 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 2402 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 2403 | `	{"range",             ph7_hashmap_range   },` |
|         - | 2404 | `	{"current",           ph7_hashmap_current },` |
|         - | 2405 | `	{"each",              ph7_hashmap_each    },` |
|         - | 2406 | `	{"pos",               ph7_hashmap_current },` |
|         - | 2407 | `	{"next",              ph7_hashmap_next    },` |
|         - | 2408 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 2409 | `	{"end",               ph7_hashmap_end     },` |
|         - | 2410 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 2411 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 2412 | `};` |
|         - | 2413 | `/*` |
|         - | 2414 | ` * Register the built-in hashmap functions defined above.` |
|         - | 2415 | ` */` |
|      5619 | 2416 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 2417 | `{` |
|         - | 2418 | `	sxu32 n;` |
|    505715 | 2419 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    500096 | 2420 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    249650 | 2421 | `	}` |
|      5624 | 2422 | `}` |
|         - | 2423 | `/*` |
|         - | 2424 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 2425 | ` * the BLOB given as the first argument.` |
|         - | 2426 | ` * This function is typically invoked when the user issue a call to` |
|         - | 2427 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 2428 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 2429 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 2430 | ` */` |
|         - | 2431 | `/*` |
|         - | 2432 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 2433 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 2434 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 2435 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 2436 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 2437 | ` *` |
|         - | 2438 | ` * bProp says the entries are an object's PROPERTIES: php then reads each key` |
|         - | 2439 | ``  * through zend_unmangle_property_name, so "\0C\0p" prints as `["p":"C":private]` `` |
|         - | 2440 | `` * and "\0*\0p" as `["p":protected]`. That is how a get_debug_info handler (and a`` |
|         - | 2441 | ` * userland __debugInfo()) labels a non-public slot, and it is the ONLY place the` |
|         - | 2442 | `` * decode happens — `var_dump((array)$obj)` shows the mangled key raw.`` |
|         - | 2443 | ` */` |
|      2095 | 2444 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth,int bProp)` |
|         5 | 2445 | `{` |
|      2100 | 2446 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2447 | `	ph7_value *pObj;` |
|      2100 | 2448 | `	sxu32 n = 0;` |
|         - | 2449 | `	int isRef;` |
|      2100 | 2450 | `	sxi32 rc = SXRET_OK;` |
|         - | 2451 | `	int i;` |
|      3444 | 2452 | `	for(;;){` |
|      6894 | 2453 | `		if( n >= pMap->nEntry ){` |
|      2100 | 2454 | `			break;` |
|         - | 2455 | `		}` |
|      4799 | 2456 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 2457 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 2458 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|      4799 | 2459 | `		isRef = PH7_HashmapNodeIsRef(pEntry);` |
|      4799 | 2460 | `		if( ShowType ){` |
|         - | 2461 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 2462 | `			 * on the next line at the same indent (php). */` |
|     11695 | 2463 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|      8149 | 2464 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      4077 | 2465 | `			}` |
|      3551 | 2466 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|      2351 | 2467 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|      1178 | 2468 | `			}else{` |
|         - | 2469 | `				SyString sCls,sNm;` |
|      1205 | 2470 | `				if( bProp && PH7_UnmangleAttrName((const char *)SyBlobData(&pEntry->xKey.sKey),` |
|       155 | 2471 | `					SyBlobLength(&pEntry->xKey.sKey),&sCls,&sNm) ){` |
|       312 | 2472 | `					SyBlobFormat(&(*pOut),"[\"%z\"",&sNm);` |
|       312 | 2473 | `					if( sCls.nByte > 0 ){` |
|        85 | 2474 | `						if( sCls.zString[0] == '*' ){` |
|         7 | 2475 | `							SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|         4 | 2476 | `						}else{` |
|        79 | 2477 | `							SyBlobFormat(&(*pOut),":\"%z\":private",&sCls);` |
|         - | 2478 | `						}` |
|        42 | 2479 | `					}` |
|       312 | 2480 | `					SyBlobAppend(&(*pOut),"]=>",sizeof("]=>")-1);` |
|       157 | 2481 | `				}else{` |
|      1340 | 2482 | `					SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|       445 | 2483 | `						SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2484 | `				}` |
|         - | 2485 | `			}` |
|      3551 | 2486 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      3551 | 2487 | `			if( pObj ){` |
|      3551 | 2488 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|      3551 | 2489 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2490 | `					break;` |
|         - | 2491 | `				}` |
|      1773 | 2492 | `			}` |
|      1778 | 2493 | `		}else{` |
|         - | 2494 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 2495 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 2496 | `			 * php's extra blank line. References carry no marker. */` |
|      8485 | 2497 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      7237 | 2498 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      3621 | 2499 | `			}` |
|      1253 | 2500 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       547 | 2501 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|       276 | 2502 | `			}else{` |
|         - | 2503 | `				SyString sCls,sNm;` |
|       711 | 2504 | `				if( bProp && PH7_UnmangleAttrName((const char *)SyBlobData(&pEntry->xKey.sKey),` |
|       196 | 2505 | `					SyBlobLength(&pEntry->xKey.sKey),&sCls,&sNm) ){` |
|       396 | 2506 | `					SyBlobFormat(&(*pOut),"[%z",&sNm);` |
|       396 | 2507 | `					if( sCls.nByte > 0 ){` |
|        47 | 2508 | `						if( sCls.zString[0] == '*' ){` |
|         3 | 2509 | `							SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|         2 | 2510 | `						}else{` |
|        45 | 2511 | `							SyBlobFormat(&(*pOut),":%z:private",&sCls);` |
|         - | 2512 | `						}` |
|        23 | 2513 | `					}` |
|       396 | 2514 | `					SyBlobAppend(&(*pOut),"] => ",sizeof("] => ")-1);` |
|       200 | 2515 | `				}else{` |
|       474 | 2516 | `					SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|       157 | 2517 | `						SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2518 | `				}` |
|         - | 2519 | `			}` |
|      1248 | 2520 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       700 | 2521 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|       146 | 2522 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|       146 | 2523 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       146 | 2524 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2525 | `					break;` |
|         - | 2526 | `				}` |
|        75 | 2527 | `			}else{` |
|      1111 | 2528 | `				if( pObj ){` |
|      1111 | 2529 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       553 | 2530 | `				}` |
|      1111 | 2531 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 2532 | `			}` |
|         - | 2533 | `		}` |
|         - | 2534 | `		/* Point to the next entry */` |
|      4799 | 2535 | `		n++;` |
|      4799 | 2536 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         5 | 2537 | `	}` |
|      2100 | 2538 | `	return rc;` |
|         5 | 2539 | `}` |
|      1923 | 2540 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         5 | 2541 | `{` |
|         - | 2542 | `	sxi32 rc;` |
|         - | 2543 | `	int i;` |
|      1928 | 2544 | `	if( nDepth > 31 ){` |
|         - | 2545 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 2546 | `		/* Nesting limit reached */` |
|       ! 0 | 2547 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 2548 | `		return SXERR_LIMIT;` |
|         - | 2549 | `	}` |
|      1928 | 2550 | `	if( ShowType ){` |
|         - | 2551 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 2552 | `		 * newline (a nested array is itself an entry value line). */` |
|      1610 | 2553 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|      1610 | 2554 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1610 | 2555 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth,0);` |
|      2154 | 2556 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       548 | 2557 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       276 | 2558 | `		}` |
|      1610 | 2559 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      1610 | 2560 | `		return rc;` |
|         - | 2561 | `	}` |
|         - | 2562 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       323 | 2563 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|      1491 | 2564 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      1172 | 2565 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       588 | 2566 | `	}` |
|       323 | 2567 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       323 | 2568 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth,0);` |
|      1491 | 2569 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      1172 | 2570 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       588 | 2571 | `	}` |
|       323 | 2572 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       323 | 2573 | `	return rc;` |
|       966 | 2574 | `}` |
|         - | 2575 | `/*` |
|         - | 2576 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 2577 | ` * retrieved entry.` |
|         - | 2578 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 2579 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 2580 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 2581 | ` * a value different from PH7_OK.` |
|         - | 2582 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 2583 | ` */` |
|     67350 | 2584 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 2585 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2586 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 2587 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 2588 | `	)` |
|         5 | 2589 | `{` |
|         - | 2590 | `	ph7_hashmap_node *pEntry;` |
|         - | 2591 | `	ph7_value sKey,sValue;` |
|         - | 2592 | `	sxi32 rc;` |
|         - | 2593 | `	sxu32 n;` |
|         - | 2594 | `	/* Initialize walker parameter */` |
|     67355 | 2595 | `	rc = SXRET_OK;` |
|     67355 | 2596 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     67355 | 2597 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     67355 | 2598 | `	n = pMap->nEntry;` |
|     67355 | 2599 | `	pEntry = pMap->pFirst;` |
|         - | 2600 | `	/* Start the iteration process */` |
|    254147 | 2601 | `	for(;;){` |
|    508585 | 2602 | `		if( n < 1 ){` |
|     67271 | 2603 | `			break;` |
|         - | 2604 | `		}` |
|         - | 2605 | `		/* Extract a copy of the key and a copy the current value */` |
|    441319 | 2606 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    441319 | 2607 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 2608 | `		/* Invoke the user callback */` |
|    441319 | 2609 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 2610 | `		/* Release the copy of the key and the value */` |
|    441319 | 2611 | `		PH7_MemObjRelease(&sKey);` |
|    441319 | 2612 | `		PH7_MemObjRelease(&sValue);` |
|    441319 | 2613 | `		if( rc != PH7_OK ){` |
|         - | 2614 | `			/* Callback request an operation abort */` |
|        88 | 2615 | `			return SXERR_ABORT;` |
|         - | 2616 | `		}` |
|         - | 2617 | `		/* Point to the next entry */` |
|    441235 | 2618 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    441235 | 2619 | `		n--;` |
|         5 | 2620 | `	}` |
|         - | 2621 | `	/* All done */` |
|     67271 | 2622 | `	return SXRET_OK;` |
|     33655 | 2623 | `}` |
|         - | 2624 |  |
