# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1185/1275 lines (92.94%)

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
|  12963400 |   20 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   21 | `{` |
|  12963405 |   22 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|  12963405 |   23 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   24 | `}` |
|         - |   25 | `/*` |
|         - |   26 | ` * Default hash function for string/BLOB keys.` |
|         - |   27 | ` */` |
|   7522567 |   28 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   29 | `{` |
|   7522572 |   30 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   31 | `	unsigned char *zEnd;` |
|   7522572 |   32 | `	sxu32 nH = 5381;` |
|   7522572 |   33 | `	zEnd = &zIn[nLen];` |
|   8784799 |   34 | `	for(;;){` |
|  17609929 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  12274774 |   36 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  10918106 |   37 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  10481259 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   39 | `	}` |
|   7522572 |   40 | `	return nH;` |
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
|         - |   54 | ` * tree takes the other arm. This is NOT the devirtualization that was measured and rejected --` |
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
|      2369 |   69 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   70 | `{` |
|      2374 |   71 | `	sxi64 iCount = 0;` |
|      2374 |   72 | `	if( !bRecursive ){` |
|      2200 |   73 | `		iCount = pMap->nEntry;` |
|      1095 |   74 | `	}else{` |
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
|      2374 |  109 | `	return iCount;` |
|         5 |  110 | `}` |
|         - |  111 | `/*` |
|         - |  112 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |  113 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  114 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  115 | ` */` |
|   8162798 |  116 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  117 | `{` |
|         - |  118 | `	ph7_hashmap_node *pNode;` |
|         - |  119 | `	/* Allocate a new node */` |
|   8162803 |  120 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   8162803 |  121 | `	if( pNode == 0 ){` |
|       ! 0 |  122 | `		return 0;` |
|         - |  123 | `	}` |
|         - |  124 | `	/* Zero the stucture */` |
|   8162803 |  125 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  126 | `	/* Fill in the structure */` |
|   8162803 |  127 | `	pNode->pMap  = &(*pMap);` |
|   8162803 |  128 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   8162803 |  129 | `	pNode->nHash = nHash;` |
|   8162803 |  130 | `	pNode->xKey.iKey = iKey;` |
|   8162803 |  131 | `	pNode->nValIdx  = nValIdx;` |
|   8162803 |  132 | `	return pNode;` |
|   4080704 |  133 | `}` |
|         - |  134 | `/*` |
|         - |  135 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  136 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  137 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  138 | ` */` |
|   4100185 |  139 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  140 | `{` |
|         - |  141 | `	ph7_hashmap_node *pNode;` |
|         - |  142 | `	/* Allocate a new node */` |
|   4100190 |  143 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   4100190 |  144 | `	if( pNode == 0 ){` |
|       ! 0 |  145 | `		return 0;` |
|         - |  146 | `	}` |
|         - |  147 | `	/* Zero the stucture */` |
|   4100190 |  148 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  149 | `	/* Fill in the structure */` |
|   4100190 |  150 | `	pNode->pMap  = &(*pMap);` |
|   4100190 |  151 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   4100190 |  152 | `	pNode->nHash = nHash;` |
|   4100190 |  153 | `	if( nKeyLen <= sizeof(pNode->zKey) ){` |
|         - |  154 | `		/* The key lives in the node -- see HASHMAP_NODE_INLINE_KEY. LOCKED\|STATIC is` |
|         - |  155 | `		 * what SyBlobInitFromBuf marks it, which is exactly right here: the buffer is` |
|         - |  156 | `		 * not the allocator's to grow and not SyBlobRelease's to free, and the key is` |
|         - |  157 | `		 * written once, right below, by an append that is guaranteed to fit. */` |
|   4080705 |  158 | `		SyBlobInitFromBuf(&pNode->xKey.sKey,pNode->zKey,sizeof(pNode->zKey));` |
|   2037516 |  159 | `	}else{` |
|     19490 |  160 | `		SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|         - |  161 | `	}` |
|   4100190 |  162 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   4100190 |  163 | `	pNode->nValIdx = nValIdx;` |
|   4100190 |  164 | `	return pNode;` |
|   2047253 |  165 | `}` |
|         - |  166 | `/*` |
|         - |  167 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  168 | ` */` |
|  12262983 |  169 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  170 | `{` |
|         - |  171 | `	/* Link */` |
|  12262988 |  172 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   6139263 |  173 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   6139263 |  174 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   3067458 |  175 | `	}` |
|  12262988 |  176 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  177 | `	/* Link to the map list */` |
|  12262988 |  178 | `	if( pMap->pFirst == 0 ){` |
|   2721047 |  179 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  180 | `		/* Point to the first inserted node */` |
|   2721047 |  181 | `		pMap->pCur = pNode;` |
|   1360205 |  182 | `	}else{` |
|   9541946 |  183 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  184 | `	}` |
|  12262988 |  185 | `	if( pMap->pActiveSteps ){` |
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
|  12262988 |  199 | `	++pMap->nEntry;` |
|  12262988 |  200 | `}` |
|         - |  201 | `/*` |
|         - |  202 | ` * Unlink a node from the hashmap.` |
|         - |  203 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  204 | ` */` |
|     16806 |  205 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  206 | `{` |
|     16811 |  207 | `	ph7_hashmap *pMap = pNode->pMap;` |
|     16811 |  208 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  209 | `	/* Unlink from the corresponding bucket */` |
|     16811 |  210 | `	if( pNode->pPrevCollide == 0 ){` |
|     14566 |  211 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      7257 |  212 | `	}else{` |
|      2250 |  213 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  214 | `	}` |
|     16811 |  215 | `	if( pNode->pNextCollide ){` |
|      9613 |  216 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      4836 |  217 | `	}` |
|     16811 |  218 | `	if( pMap->pFirst == pNode ){` |
|      3201 |  219 | `		pMap->pFirst = pNode->pPrev;` |
|      1598 |  220 | `	}` |
|     16811 |  221 | `	if( pMap->pCur == pNode ){` |
|         - |  222 | `		/* Advance the node cursor */` |
|      3199 |  223 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      1597 |  224 | `	}` |
|     16811 |  225 | `	if( pMap->pActiveSteps ){` |
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
|     16811 |  236 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|     16811 |  237 | `	if( bRestore ){` |
|         - |  238 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      5647 |  239 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  240 | `		/* Restore to the freelist — but only if this node was the LAST holder. A` |
|         - |  241 | `` 		 * node's own value can be held by a name too (`$r = &$a[0]; unset($a);` `` |
|         - |  242 | `		 * must leave $r reading 7, not destroy it), and a FOREIGN node may be the` |
|         - |  243 | `		 * last thing holding a slot whose frame is already gone (that frame left it` |
|         - |  244 | `		 * standing for this very node), which nothing else would ever free. */` |
|      5647 |  245 | `		PH7_VmReleaseUnheldSlot(pVm,pNode->nValIdx);` |
|      2821 |  246 | `	}` |
|     16811 |  247 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|     11947 |  248 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      5966 |  249 | `	}` |
|     16811 |  250 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|     16811 |  251 | `	pMap->nEntry--;` |
|     16811 |  252 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  253 | `		/* Free the hash-bucket */` |
|      1679 |  254 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      1679 |  255 | `		pMap->apBucket = 0;` |
|      1679 |  256 | `		pMap->nSize = 0;` |
|      1679 |  257 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       837 |  258 | `	}` |
|     16811 |  259 | `}` |
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
|  12262983 |  271 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  272 | `{` |
|  12262988 |  273 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   2748223 |  274 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  275 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   2748223 |  276 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  277 | `		sxu32 nBucket;` |
|         - |  278 | `		sxu32 n;` |
|   2748223 |  279 | `		if( nNew < 1 ){` |
|   2721047 |  280 | `			nNew = HASHMAP_FIRST_BUCKETS;` |
|   1360200 |  281 | `		}` |
|         - |  282 | `		/* Allocate a new bucket */` |
|   2748223 |  283 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   2748223 |  284 | `		if( apNew == 0 ){` |
|       ! 0 |  285 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  286 | `				return SXERR_MEM; /* Fatal */` |
|         - |  287 | `			}` |
|         - |  288 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  289 | `			return SXRET_OK;` |
|         - |  290 | `		}` |
|         - |  291 | `		/* Zero the table */` |
|   2748223 |  292 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  293 | `		/* Reflect the change */` |
|   2748223 |  294 | `		pMap->apBucket = apNew;` |
|   2748223 |  295 | `		pMap->nSize = nNew;` |
|   2748223 |  296 | `		if( apOld == 0 ){` |
|         - |  297 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   2721047 |  298 | `			return SXRET_OK;` |
|         - |  299 | `		}` |
|         - |  300 | `		/* Rehash old entries */` |
|     27181 |  301 | `		pEntry = pMap->pFirst;` |
|     27181 |  302 | `		n = 0;` |
|   3559482 |  303 | `		for( ;; ){` |
|   7120765 |  304 | `			if( n >= pMap->nEntry ){` |
|     27181 |  305 | `				break;` |
|         - |  306 | `			}` |
|         - |  307 | `			/* Clear the old collision link */` |
|   7093589 |  308 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  309 | `			/* Link to the new bucket */` |
|   7093589 |  310 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   7093589 |  311 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   5421800 |  312 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   5421800 |  313 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2710501 |  314 | `			}` |
|   7093589 |  315 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  316 | `			/* Point to the next entry */` |
|   7093589 |  317 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   7093589 |  318 | `			n++;` |
|         5 |  319 | `		}` |
|         - |  320 | `		/* Free the old table */` |
|     27181 |  321 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|     13578 |  322 | `	}` |
|   9541946 |  323 | `	return SXRET_OK;` |
|   6127952 |  324 | `}` |
|         - |  325 | `/*` |
|         - |  326 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  327 | ` * hashmap.` |
|         - |  328 | ` */` |
|   8162798 |  329 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  330 | `{` |
|         - |  331 | `	ph7_hashmap_node *pNode;` |
|         - |  332 | `	sxu32 nIdx;` |
|         - |  333 | `	sxu32 nHash;` |
|         - |  334 | `	sxi32 rc;` |
|   8162803 |  335 | `	if( !isForeign ){` |
|         - |  336 | `		ph7_value *pObj;` |
|         - |  337 | `		ph7_value sSafeVal;` |
|         - |  338 | `		/* Snapshot the source BEFORE reserving. This guarded PH7_ReserveMemObj` |
|         - |  339 | `		 * MOVING pVm->aMemObj under a pValue that points into the pool (e.g.` |
|         - |  340 | `		 * get_defined_vars/func_get_args/get_class_vars/get_object_vars all pass` |
|         - |  341 | `		 * a pool slot). Redundant since P1 -- the pool's segments are fixed, so a` |
|         - |  342 | `		 * slot's address never moves. Left for the harvest sweep. */` |
|   8162619 |  343 | `		if( pValue ){` |
|   8162537 |  344 | `			sSafeVal = *pValue;` |
|   8162537 |  345 | `			pValue = &sSafeVal;` |
|   4080566 |  346 | `		}` |
|         - |  347 | `		/* Reserve a ph7_value for the value */` |
|   8162619 |  348 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   8162619 |  349 | `		if( pObj == 0 ){` |
|       ! 0 |  350 | `			return SXERR_MEM;` |
|         - |  351 | `		}` |
|   8162619 |  352 | `		if( pValue ){` |
|         - |  353 | `			/* Duplicate the value */` |
|   8162537 |  354 | `			PH7_MemObjStore(pValue,pObj);` |
|   4080566 |  355 | `		}` |
|   8162619 |  356 | `		nIdx = pObj->nIdx;` |
|   4080612 |  357 | `	}else{` |
|       188 |  358 | `		nIdx = nRefIdx;` |
|         - |  359 | `	}` |
|         - |  360 | `	/* Hash the key */` |
|   8162803 |  361 | `	nHash = HASHMAP_INT_HASH(pMap,iKey);` |
|         - |  362 | `	/* Allocate a new int node */` |
|   8162803 |  363 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   8162803 |  364 | `	if( pNode == 0 ){` |
|       ! 0 |  365 | `		return SXERR_MEM;` |
|         - |  366 | `	}` |
|   8162803 |  367 | `	if( isForeign ){` |
|         - |  368 | `		/* Mark as a foregin entry */` |
|       188 |  369 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        92 |  370 | `	}` |
|         - |  371 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   8162803 |  372 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   8162803 |  373 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  374 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  375 | `		return rc;` |
|         - |  376 | `	}` |
|         - |  377 | `	/* Perform the insertion */` |
|   8162803 |  378 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|   8162803 |  379 | `	if( iKey > pMap->iMaxIntKey ){` |
|   6550147 |  380 | `		pMap->iMaxIntKey = iKey; /* the auto-index scan's only reason to run */` |
|   3274481 |  381 | `	}` |
|         - |  382 | `	/* Install in the reference table */` |
|   8162803 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  384 | `	/* All done */` |
|   8162803 |  385 | `	return SXRET_OK;` |
|   4080704 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  389 | ` * hashmap.` |
|         - |  390 | ` */` |
|   4100185 |  391 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  392 | `{` |
|         - |  393 | `	ph7_hashmap_node *pNode;` |
|         - |  394 | `	sxu32 nHash;` |
|         - |  395 | `	sxu32 nIdx;` |
|         - |  396 | `	sxi32 rc;` |
|   4100190 |  397 | `	if( !isForeign ){` |
|         - |  398 | `		ph7_value *pObj;` |
|         - |  399 | `		ph7_value sSafeVal;` |
|         - |  400 | `		/* Snapshot the source BEFORE reserving. This guarded PH7_ReserveMemObj` |
|         - |  401 | `		 * MOVING pVm->aMemObj under a pValue that points into the pool (e.g.` |
|         - |  402 | `		 * get_defined_vars/func_get_args/get_class_vars/get_object_vars all pass` |
|         - |  403 | `		 * a pool slot). Redundant since P1 -- the pool's segments are fixed, so a` |
|         - |  404 | `		 * slot's address never moves. Left for the harvest sweep. */` |
|   4007177 |  405 | `		if( pValue ){` |
|   4007021 |  406 | `			sSafeVal = *pValue;` |
|   4007021 |  407 | `			pValue = &sSafeVal;` |
|   2000760 |  408 | `		}` |
|         - |  409 | `		/* Reserve a ph7_value for the value */` |
|   4007177 |  410 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   4007177 |  411 | `		if( pObj == 0 ){` |
|       ! 0 |  412 | `			return SXERR_MEM;` |
|         - |  413 | `		}` |
|   4007177 |  414 | `		if( pValue ){` |
|         - |  415 | `			/* Duplicate the value */` |
|   4007021 |  416 | `			PH7_MemObjStore(pValue,pObj);` |
|   2000760 |  417 | `		}` |
|   4007177 |  418 | `		nIdx = pObj->nIdx;` |
|   2000843 |  419 | `	}else{` |
|     93018 |  420 | `		nIdx = nRefIdx;` |
|         - |  421 | `	}` |
|         - |  422 | `	/* Hash the key */` |
|   4100190 |  423 | `	nHash = HASHMAP_BLOB_HASH(pMap,pKey,nKeyLen);` |
|         - |  424 | `	/* Allocate a new blob node */` |
|   4100190 |  425 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   4100190 |  426 | `	if( pNode == 0 ){` |
|       ! 0 |  427 | `		return SXERR_MEM;` |
|         - |  428 | `	}` |
|   4100190 |  429 | `	if( isForeign ){` |
|         - |  430 | `		/* Mark as a foregin entry */` |
|     93018 |  431 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     46410 |  432 | `	}` |
|         - |  433 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   4100190 |  434 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   4100190 |  435 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  436 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  437 | `		return rc;` |
|         - |  438 | `	}` |
|         - |  439 | `	/* Perform the insertion */` |
|   4100190 |  440 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  441 | `	/* Install in the reference table */` |
|   4100190 |  442 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  443 | `	/* All done */` |
|   4100190 |  444 | `	return SXRET_OK;` |
|   2047253 |  445 | `}` |
|         - |  446 | `/*` |
|         - |  447 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  448 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  449 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  450 | ` */` |
|   5174827 |  451 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(` |
|         - |  452 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  453 | `	sxi64 iKey,                /* lookup key */` |
|         - |  454 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  455 | `	)` |
|         5 |  456 | `{` |
|         - |  457 | `	ph7_hashmap_node *pNode;` |
|         - |  458 | `	sxu32 nHash;` |
|   5174832 |  459 | `	if( pMap->nEntry < 1 ){` |
|         - |  460 | `		/* Don't bother hashing,there is no entry anyway */` |
|    402992 |  461 | `		return SXERR_NOTFOUND;` |
|         - |  462 | `	}` |
|         - |  463 | `	/* Hash the key first */` |
|   4771845 |  464 | `	nHash = HASHMAP_INT_HASH(pMap,iKey);` |
|         - |  465 | `	/* Point to the appropriate bucket */` |
|   4771845 |  466 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  467 | `	/* Perform the lookup */` |
|  56610204 |  468 | `	for(;;){` |
| 113220021 |  469 | `		if( pNode == 0 ){` |
|   2945207 |  470 | `			break;` |
|         - |  471 | `		}` |
| 110274814 |  472 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 110273165 |  473 | `			&& pNode->nHash == nHash` |
|  56049303 |  474 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  475 | `				/* Node found */` |
|   1826643 |  476 | `				if( ppNode ){` |
|   1826597 |  477 | `					*ppNode = pNode;` |
|    913286 |  478 | `				}` |
|   1826643 |  479 | `				return SXRET_OK;` |
|         - |  480 | `		}` |
|         - |  481 | `		/* Follow the collision link */` |
| 108448181 |  482 | `		pNode = pNode->pNextCollide;` |
|         5 |  483 | `	}` |
|         - |  484 | `	/* No such entry */` |
|   2945207 |  485 | `	return SXERR_NOTFOUND;` |
|   2587389 |  486 | `}` |
|         - |  487 | `/*` |
|         - |  488 | ` * Can this key POSSIBLY be one of php's numeric array keys? A yes still has to be` |
|         - |  489 | ` * confirmed by HashmapIsIntKey; a no is final, and it is the answer for nearly every` |
|         - |  490 | ` * array read a program makes -- 'code', 'content', 'type'. php's rule can only say yes` |
|         - |  491 | ` * for a key that starts with a digit or with '-', so one byte settles it INLINE, where` |
|         - |  492 | ` * the full rule is an out-of-line call that re-derives that same first byte before it` |
|         - |  493 | ` * does anything else. 121 million string-key subscript reads asked it on the phpcs step` |
|         - |  494 | ` * of record.` |
|         - |  495 | ` */` |
|   4529515 |  496 | `static int HashmapKeyMayBeInt(SyBlob *pKey)` |
|         5 |  497 | `{` |
|   4529520 |  498 | `	const unsigned char *zIn = (const unsigned char *)SyBlobData(pKey);` |
|   4529520 |  499 | `	if( SyBlobLength(pKey) < 1 ){` |
|       242 |  500 | `		return FALSE;` |
|         - |  501 | `	}` |
|   4529282 |  502 | `	return ( zIn[0] == '-' \|\| (zIn[0] >= '0' && zIn[0] <= '9') ) ? TRUE : FALSE;` |
|   2261853 |  503 | `}` |
|         - |  504 | `/*` |
|         - |  505 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  506 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  507 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  508 | ` */` |
|   4530224 |  509 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(` |
|         - |  510 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  511 | `	const void *pKey,           /* Lookup key */` |
|         - |  512 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  513 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  514 | `	)` |
|         5 |  515 | `{` |
|         - |  516 | `	ph7_hashmap_node *pNode;` |
|         - |  517 | `	sxu32 nHash;` |
|   4530229 |  518 | `	if( pMap->nEntry < 1 ){` |
|         - |  519 | `		/* Don't bother hashing,there is no entry anyway */` |
|   1107847 |  520 | `		return SXERR_NOTFOUND;` |
|         - |  521 | `	}` |
|         - |  522 | `	/* Hash the key first */` |
|   3422387 |  523 | `	nHash = HASHMAP_BLOB_HASH(pMap,pKey,nKeyLen);` |
|         - |  524 | `	/* Point to the appropriate bucket */` |
|   3422387 |  525 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  526 | `	/* Perform the lookup */` |
|   2721513 |  527 | `	for(;;){` |
|   5457504 |  528 | `		if( pNode == 0 ){` |
|   3130976 |  529 | `			break;` |
|         - |  530 | `		}` |
|   2326528 |  531 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|   2323713 |  532 | `			&& pNode->nHash == nHash` |
|   1302078 |  533 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen ){` |
|         - |  534 | `				/* The bytes, compared INLINE. SyMemcmp is the same loop (it is` |
|         - |  535 | `				 * SX_MACRO_FAST_CMP either way) plus a call and two` |
|         - |  536 | `				 * null tests, and the keys here are four or five bytes: the call` |
|         - |  537 | `				 * costs more than the comparison it makes, tens of millions of` |
|         - |  538 | `				 * times a run. */` |
|    292214 |  539 | `				sxi32 rcCmp = 0;` |
|    292214 |  540 | `				if( nKeyLen > 0 ){` |
|    568829 |  541 | `					SX_MACRO_FAST_CMP(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen,rcCmp);` |
|    146022 |  542 | `				}` |
|    292214 |  543 | `				if( rcCmp == 0 ){` |
|         - |  544 | `					/* Node found */` |
|    291416 |  545 | `					if( ppNode ){` |
|    291382 |  546 | `						*ppNode = pNode;` |
|    145634 |  547 | `					}` |
|    291416 |  548 | `					return SXRET_OK;` |
|         - |  549 | `				}` |
|       399 |  550 | `		}` |
|         - |  551 | `		/* Follow the collision link */` |
|   2035122 |  552 | `		pNode = pNode->pNextCollide;` |
|         5 |  553 | `	}` |
|         - |  554 | `	/* No such entry */` |
|   3130976 |  555 | `	return SXERR_NOTFOUND;` |
|   2262199 |  556 | `}` |
|         - |  557 | `/*` |
|         - |  558 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  559 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  560 | ` */` |
|      4712 |  561 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  562 | `{` |
|      4717 |  563 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|      4717 |  564 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  565 | `	const char *zDigit;` |
|      4717 |  566 | `	int isNeg = FALSE, nDigit;` |
|      4717 |  567 | `	if( zIn >= zEnd ){` |
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
|      4717 |  583 | `	if( zIn[0] == '-' && &zIn[1] < zEnd ){` |
|       116 |  584 | `		isNeg = TRUE;` |
|       116 |  585 | `		zIn++;` |
|        56 |  586 | `	}` |
|      4717 |  587 | `	if( zIn < zEnd && zIn[0] == '0' && SyBlobLength(pKey) > 1 ){` |
|         - |  588 | `		/* Leading zero: octal-looking, signed zero, or just padded */` |
|       164 |  589 | `		return FALSE;` |
|         - |  590 | `	}` |
|      4557 |  591 | `	zDigit = zIn;` |
|      5228 |  592 | `	for(;;){` |
|     10461 |  593 | `		if( zIn >= zEnd ){` |
|      4253 |  594 | `			break;` |
|         - |  595 | `		}` |
|      6213 |  596 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  597 | `			/* Key does not look like a decimal number */` |
|       308 |  598 | `			return FALSE;` |
|         - |  599 | `		}` |
|      5909 |  600 | `		zIn++;` |
|         5 |  601 | `	}` |
|         - |  602 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  603 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  604 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  605 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|      4253 |  606 | `	nDigit = (int)(zEnd - zDigit);` |
|      4253 |  607 | `	if( nDigit < 1 ){` |
|         - |  608 | `		/* A lone "-" (the digit loop rejects it first; kept defensive) */` |
|       ! 0 |  609 | `		return FALSE;` |
|         - |  610 | `	}` |
|      4272 |  611 | `	if( nDigit > 19 \|\|` |
|      2142 |  612 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|        22 |  613 | `		return FALSE;` |
|         - |  614 | `	}` |
|      4233 |  615 | `	return TRUE;` |
|      2361 |  616 | `}` |
|         - |  617 | `/*` |
|         - |  618 | ` * TRUE when this key value lands on an INTEGER key — the same fold HashmapLookup` |
|         - |  619 | ` * and HashmapInsert perform below, exposed so a DIAGNOSTIC can name the key the` |
|         - |  620 | ` * way the lookup saw it rather than the way it was written ($a["10"] misses the` |
|         - |  621 | `` * integer key 10, so php's warning says `Undefined array key 10`, unquoted).`` |
|         - |  622 | ` * A non-integer key is left as a STRING with its blob ready to print — including` |
|         - |  623 | ` * the NULL key, which folds to "" exactly as the lookup folds it.` |
|         - |  624 | ` */` |
|       182 |  625 | `PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey)` |
|         5 |  626 | `{` |
|       187 |  627 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|       151 |  628 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  629 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  630 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  631 | `		}` |
|       151 |  632 | `		return ( HashmapKeyMayBeInt(&pKey->sBlob) && HashmapIsIntKey(&pKey->sBlob) ) ? TRUE : FALSE;` |
|         - |  633 | `	}` |
|         - |  634 | `	/* int / float / BOOL all reach an integer key ($a[false] is $a[0]) */` |
|        38 |  635 | `	return TRUE;` |
|        96 |  636 | `}` |
|         - |  637 | `/*` |
|         - |  638 | ` * Check if a given key exists in the given hashmap.` |
|         - |  639 | ` * Write a pointer to the target node on success.` |
|         - |  640 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  641 | ` */` |
|   2245655 |  642 | `static sxi32 HashmapLookup(` |
|         - |  643 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  644 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  645 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  646 | `	)` |
|         5 |  647 | `{` |
|   2245660 |  648 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  649 | `	sxi32 rc;` |
|   2245660 |  650 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    431802 |  651 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  652 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|        57 |  653 | `			PH7_MemObjToString(&(*pKey));` |
|        27 |  654 | `		}` |
|    431802 |  655 | `		if( !HashmapKeyMayBeInt(&pKey->sBlob) \|\| !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  656 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  657 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  658 | `			 * to an integer lookup for key 0. */` |
|    428130 |  659 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    428130 |  660 | `			goto result;` |
|         - |  661 | `		}` |
|      1836 |  662 | `	}` |
|         - |  663 | `	/* Perform an int lookup */` |
|   1817535 |  664 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  665 | `		/* Force an integer cast */` |
|      3703 |  666 | `		PH7_MemObjToInteger(pKey);` |
|      1849 |  667 | `	}` |
|         - |  668 | `	/* Perform an int lookup */` |
|   1817535 |  669 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   1122908 |  670 | `result:` |
|   2245660 |  671 | `	if( rc == SXRET_OK ){` |
|         - |  672 | `		/* Node found */` |
|   2102945 |  673 | `		if( ppNode ){` |
|   2102874 |  674 | `			*ppNode = pNode;` |
|   1051378 |  675 | `		}` |
|   2102945 |  676 | `		return SXRET_OK;` |
|         - |  677 | `	}` |
|         - |  678 | `	/* No such entry */` |
|    142720 |  679 | `	return SXERR_NOTFOUND;` |
|   1122752 |  680 | `}` |
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
|   3357739 |  694 | `static void HashmapSkipReservedIndex(ph7_hashmap *pMap)` |
|         5 |  695 | `{` |
|   3357744 |  696 | `	if( pMap->iNextIdx > pMap->iMaxIntKey ){` |
|   3357674 |  697 | `		return;` |
|         - |  698 | `	}` |
|        99 |  699 | `	while( pMap->iNextIdx < SXI64_HIGH` |
|        73 |  700 | `	    && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  701 | `		pMap->iNextIdx++;` |
|       ! 0 |  702 | `	}` |
|   1678811 |  703 | `}` |
|   3360132 |  704 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  705 | `{` |
|   3360137 |  706 | `	if( !pMap->bIntKeySeen ){` |
|         - |  707 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|    410874 |  708 | `		pMap->bIntKeySeen = 1;` |
|    410874 |  709 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|    410874 |  710 | `		HashmapSkipReservedIndex(&(*pMap));` |
|    410874 |  711 | `		return;` |
|         - |  712 | `	}` |
|   2949268 |  713 | `	if( iKey >= pMap->iNextIdx ){` |
|   2946875 |  714 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  715 | `		/* Make sure the automatic index is not reserved */` |
|   2946875 |  716 | `		HashmapSkipReservedIndex(&(*pMap));` |
|   1473383 |  717 | `	}` |
|   1680055 |  718 | `}` |
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
|   3360132 |  732 | `static sxi32 HashmapKeepIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,` |
|         - |  733 | `	sxu32 nRefIdx,int isForeign)` |
|         5 |  734 | `{` |
|   3360137 |  735 | `	sxi32 rc = HashmapInsertIntKey(&(*pMap),iKey,pValue,nRefIdx,isForeign);` |
|   3360137 |  736 | `	if( rc == SXRET_OK ){` |
|   3360137 |  737 | `		HashmapAdvanceAutoIndex(&(*pMap),iKey);` |
|   1680050 |  738 | `	}` |
|   3360137 |  739 | `	return rc;` |
|         5 |  740 | `}` |
|         - |  741 | `/*` |
|         - |  742 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  743 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  744 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  745 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  746 | ` */` |
|   4802674 |  747 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  748 | `{` |
|   4802679 |  749 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|        10 |  750 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|        10 |  751 | `		return TRUE;` |
|         - |  752 | `	}` |
|   4802671 |  753 | `	return FALSE;` |
|   2400658 |  754 | `}` |
|         - |  755 | `/*` |
|         - |  756 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  757 | ` * hashmap.` |
|         - |  758 | ` * If a node with the given key already exists in the database` |
|         - |  759 | ` * then this function overwrite the old value.` |
|         - |  760 | ` */` |
|  12152912 |  761 | `PH7_PRIVATE sxi32 HashmapInsert(` |
|         - |  762 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  763 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  764 | `	ph7_value *pVal    /* Node value */` |
|         - |  765 | `	)` |
|         5 |  766 | `{` |
|  12152917 |  767 | `	ph7_hashmap_node *pNode = 0;` |
|  12152917 |  768 | `	sxi32 rc = SXRET_OK;` |
|  12152917 |  769 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|   4004556 |  770 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  771 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  772 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  773 | `			 * path and filed it under 0). */` |
|        15 |  774 | `			PH7_MemObjToString(&(*pKey));` |
|         6 |  775 | `		}` |
|   4004556 |  776 | `		if( HashmapKeyMayBeInt(&pKey->sBlob) && HashmapIsIntKey(&pKey->sBlob) ){` |
|       543 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|         - |  779 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  780 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  781 | `		 * overwriting nothing and bumping the auto-index). */` |
|   6003281 |  782 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   1999263 |  783 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  784 | `				/* Overwrite the old value */` |
|         - |  785 | `				ph7_value *pElem;` |
|      1424 |  786 | `				pElem = (ph7_value *)PH7_MemObjAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      1424 |  787 | `				if( pElem ){` |
|      1424 |  788 | `					if( pVal ){` |
|      1424 |  789 | `						PH7_MemObjStore(pVal,pElem);` |
|       714 |  790 | `					}else{` |
|         - |  791 | `						/* Nullify the entry */` |
|       ! 0 |  792 | `						PH7_MemObjToNull(pElem);` |
|         - |  793 | `					}` |
|       709 |  794 | `				}` |
|      1424 |  795 | `				return SXRET_OK;` |
|         - |  796 | `		}` |
|   4002599 |  797 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  798 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  799 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       206 |  800 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  801 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  802 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  803 | `				return SXRET_OK;` |
|         - |  804 | `			}` |
|       307 |  805 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       202 |  806 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|       101 |  807 | `				pVal,SXU32_HIGH);` |
|         - |  808 | `		}` |
|         - |  809 | `		/* Perform a blob-key insertion */` |
|   4002397 |  810 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   4002397 |  811 | `		return rc;` |
|         - |  812 | `	}` |
|   4074883 |  813 | `IntKey:` |
|   8148904 |  814 | `	if( pKey ){` |
|   3346368 |  815 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  816 | `			/* Force an integer cast */` |
|       565 |  817 | `			PH7_MemObjToInteger(pKey);` |
|       280 |  818 | `		}` |
|   3346368 |  819 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  820 | `			/* Overwrite the old value */` |
|         - |  821 | `			ph7_value *pElem;` |
|      2152 |  822 | `			pElem = (ph7_value *)PH7_MemObjAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      2152 |  823 | `			if( pElem ){` |
|      2152 |  824 | `				if( pVal ){` |
|      2152 |  825 | `					PH7_MemObjStore(pVal,pElem);` |
|      1079 |  826 | `				}else{` |
|         - |  827 | `					/* Nullify the entry */` |
|       ! 0 |  828 | `					PH7_MemObjToNull(pElem);` |
|         - |  829 | `				}` |
|      1075 |  830 | `			}` |
|      2152 |  831 | `			return SXRET_OK;` |
|         - |  832 | `		}` |
|   3344220 |  833 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  834 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  835 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  836 | `			char zKey[24];` |
|         3 |  837 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  838 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  839 | `		}` |
|         - |  840 | `		/* Perform a 64-bit-int-key insertion */` |
|   3344218 |  841 | `		rc = HashmapKeepIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   1672092 |  842 | `	}else{` |
|   4802541 |  843 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  844 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  845 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  846 | `		}` |
|   4802539 |  847 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|        10 |  848 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  849 | `		}` |
|         - |  850 | `		/* Assign an automatic index */` |
|   4802531 |  851 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   4802531 |  852 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   4802529 |  853 | `			++pMap->iNextIdx;` |
|   2400578 |  854 | `		}` |
|         - |  855 | `	}` |
|         - |  856 | `	/* Insertion result */` |
|   8146744 |  857 | `	return rc;` |
|   6073015 |  858 | `}` |
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
|     93183 |  886 | `static sxi32 HashmapInsertByRef(` |
|         - |  887 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  888 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  889 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  890 | `	)` |
|         5 |  891 | `{` |
|     93188 |  892 | `	ph7_hashmap_node *pNode = 0;` |
|     93188 |  893 | `	sxi32 rc = SXRET_OK;` |
|     93188 |  894 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|     93026 |  895 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  896 | ``			/* Force a string cast. NULL casts to "": `$a[null] =& $x` binds under the`` |
|         - |  897 | `			 * EMPTY STRING key, symmetric with HashmapInsert (the by-value path). */` |
|         3 |  898 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  899 | `		}` |
|     93026 |  900 | `		if( HashmapKeyMayBeInt(&pKey->sBlob) && HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  901 | `			goto IntKey;` |
|         - |  902 | `		}` |
|         - |  903 | ``		/* An empty key is a REAL key: `$a[""] =& $x` binds (and OVERWRITES an existing`` |
|         - |  904 | `		 * "" element) under "", it does NOT auto-index. The legacy path turned "" into` |
|         - |  905 | ``		 * the next integer slot — `$a[""] =& $x` filed under 0 and a second write added`` |
|         - |  906 | `		 * a duplicate rather than rebinding. A genuine auto-index caller passes` |
|         - |  907 | `		 * pKey == 0 (a literal null pointer), handled at IntKey below, never a "" blob. */` |
|    139437 |  908 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     46413 |  909 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  910 | `				/* Overwrite */` |
|         8 |  911 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         8 |  912 | `				pNode->nValIdx = nRefIdx;` |
|         - |  913 | `				/* Install in the reference table */` |
|         8 |  914 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         8 |  915 | `				return SXRET_OK;` |
|         - |  916 | `		}` |
|         - |  917 | `		/* Perform a blob-key insertion */` |
|     93018 |  918 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     93018 |  919 | `		return rc;` |
|         - |  920 | `	}` |
|        81 |  921 | `IntKey:` |
|       168 |  922 | `	if( pKey ){` |
|        26 |  923 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  924 | `			/* Force an integer cast */` |
|         3 |  925 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  926 | `		}` |
|        26 |  927 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  928 | `			/* Overwrite */` |
|         7 |  929 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         7 |  930 | `			pNode->nValIdx = nRefIdx;` |
|         - |  931 | `			/* Install in the reference table */` |
|         7 |  932 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         7 |  933 | `			return SXRET_OK;` |
|         - |  934 | `		}` |
|         - |  935 | `		/* Perform a 64-bit-int-key insertion */` |
|        20 |  936 | `		rc = HashmapKeepIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|        11 |  937 | `	}else{` |
|       144 |  938 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  939 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  940 | `		}` |
|         - |  941 | `		/* Assign an automatic index */` |
|       144 |  942 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|       144 |  943 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|       144 |  944 | `			++pMap->iNextIdx;` |
|        70 |  945 | `		}` |
|         - |  946 | `	}` |
|         - |  947 | `	/* Insertion result */` |
|       162 |  948 | `	return rc;` |
|     46500 |  949 | `}` |
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
|   1378943 |  967 | `PH7_PRIVATE int PH7_HashmapNodeIsRef(ph7_hashmap_node *pNode)` |
|         5 |  968 | `{` |
|   1378948 |  969 | `	return PH7_VmSlotHolderCount(pNode->pMap->pVm,pNode->nValIdx) >= 2;` |
|         5 |  970 | `}` |
|         - |  971 | `/*` |
|         - |  972 | ` * Does this node point at the very map it lives in?` |
|         - |  973 | ` *` |
|         - |  974 | `` * `$a[0] = &$a` leaves exactly that, and it is the one reference an element can hold`` |
|         - |  975 | `` * whose slot has no OTHER holder to count: once the name `$a` goes -- an unset, or the`` |
|         - |  976 | ` * return that ends the function that built it -- the cycle IS the only holder, so` |
|         - |  977 | ` * PH7_HashmapNodeIsRef correctly answers "nobody else refers to this value" and a COPY` |
|         - |  978 | ` * acting on that answer flattens the element into a value copy of the same map. That` |
|         - |  979 | ` * copy contains the same node, so reading it makes one more level, for ever: a` |
|         - |  980 | ` * by-reference walk never revisits the same array, php's recursion guards (a marker` |
|         - |  981 | `` * pair written through a reference, `count()`'s detector) never fire, and a data`` |
|         - |  982 | ` * provider that yields such an array hangs the interpreter instead of printing` |
|         - |  983 | `` * `*RECURSION*`. php keeps the element a reference regardless of the count, so the copy`` |
|         - |  984 | ` * shares the cycle and the walk comes back round.` |
|         - |  985 | ` */` |
|   1368675 |  986 | `static int HashmapNodeIsSelfCycle(ph7_hashmap_node *pNode)` |
|         5 |  987 | `{` |
|   1368680 |  988 | `	ph7_value *pVal = HashmapExtractNodeValue(&(*pNode));` |
|   1368885 |  989 | `	return pVal != 0 && (pVal->iFlags & MEMOBJ_HASHMAP) != 0` |
|   2053031 |  990 | `		&& (ph7_hashmap *)pVal->x.pOther == pNode->pMap;` |
|         5 |  991 | `}` |
|         - |  992 | `/*` |
|         - |  993 | ` * Extract node value.` |
|         - |  994 | ` */` |
|   8622685 |  995 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  996 | `{` |
|         - |  997 | `	/* Point to the desired object */` |
|         - |  998 | `	ph7_value *pObj;` |
|   8622690 |  999 | `	pObj = (ph7_value *)PH7_MemObjAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   8622690 | 1000 | `	return pObj;` |
|         5 | 1001 | `}` |
|         - | 1002 | `/*` |
|         - | 1003 | ` * Insert a node in the given hashmap.` |
|         - | 1004 | ` * If a node with the given key already exists in the database` |
|         - | 1005 | ` * then this function overwrite the old value.` |
|         - | 1006 | ` */` |
|      6291 | 1007 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 | 1008 | `{` |
|         - | 1009 | `	ph7_value *pObj;` |
|         - | 1010 | `	sxi32 rc;` |
|         - | 1011 | `	/* Extract the node value */` |
|      6296 | 1012 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|      6296 | 1013 | `	if( pObj == 0 ){` |
|       ! 0 | 1014 | `		return SXERR_EMPTY;` |
|         - | 1015 | `	}` |
|      6296 | 1016 | `	if( PH7_HashmapNodeIsRef(&(*pNode)) \|\| HashmapNodeIsSelfCycle(&(*pNode)) ){` |
|         - | 1017 | `		/* A referenced element keeps its reference through the copy (php: array_slice()` |
|         - | 1018 | ``		 * of an array holding `$r = &$a[1]` still var_dumps that element as &int(2)).`` |
|         - | 1019 | `		 * Same rule HashmapDuplicateNode applies for array_merge()/spread. */` |
|         3 | 1020 | `		sxu32 nRefIdx = pNode->nValIdx;` |
|         - | 1021 | `		ph7_value sKey;` |
|         3 | 1022 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         3 | 1023 | `			if( !bPreserve ){` |
|         3 | 1024 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - | 1025 | `			}` |
|       ! 0 | 1026 | `			PH7_MemObjInitFromInt(pMap->pVm,&sKey,pNode->xKey.iKey);` |
|       ! 0 | 1027 | `		}else{` |
|       ! 0 | 1028 | `			if( !bPreserve ){` |
|       ! 0 | 1029 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - | 1030 | `			}` |
|       ! 0 | 1031 | `			PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       ! 0 | 1032 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|       ! 0 | 1033 | `				SyBlobLength(&pNode->xKey.sKey));` |
|         - | 1034 | `		}` |
|       ! 0 | 1035 | `		rc = HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|       ! 0 | 1036 | `		PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1037 | `		return rc;` |
|         - | 1038 | `	}` |
|         - | 1039 | `	/* Preserve key */` |
|      6294 | 1040 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - | 1041 | `		/* Int64 key */` |
|      5845 | 1042 | `		if( !bPreserve ){` |
|         - | 1043 | `			/* Assign an automatic index */` |
|       617 | 1044 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       311 | 1045 | `		}else{` |
|      5233 | 1046 | `			rc = HashmapKeepIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - | 1047 | `		}` |
|      2928 | 1048 | `	}else{` |
|         - | 1049 | `		/* Blob key */` |
|       454 | 1050 | `		if( !bPreserve ){` |
|         - | 1051 | `			/* treat it like an automatically-indexed element, drop the` |
|         - | 1052 | `			 * original string key entirely */` |
|        44 | 1053 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        23 | 1054 | `		}else{` |
|       611 | 1055 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|       199 | 1056 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - | 1057 | `		}` |
|         - | 1058 | `	}` |
|      6294 | 1059 | `	return rc;` |
|      3149 | 1060 | `}` |
|         - | 1061 | `/*` |
|         - | 1062 | ` * Compare two node values.` |
|         - | 1063 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - | 1064 | ` * or < 0 if pRight is greater than pLeft.` |
|         - | 1065 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - | 1066 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - | 1067 | ` * documenation.` |
|         - | 1068 | ` */` |
|    108787 | 1069 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict,int iNest)` |
|         5 | 1070 | `{` |
|         - | 1071 | `	ph7_value sObj1,sObj2;` |
|         - | 1072 | `	sxi32 rc;` |
|    108792 | 1073 | `	if( pLeft == pRight ){` |
|         - | 1074 | `		/*` |
|         - | 1075 | `		 * Same node.Refer to the sort() implementation defined` |
|         - | 1076 | `		 * below for more information on this sceanario.` |
|         - | 1077 | `		 */` |
|       ! 0 | 1078 | `		return 0;` |
|         - | 1079 | `	}` |
|         - | 1080 | `	/* Do the comparison */` |
|    108792 | 1081 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|    108792 | 1082 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|    108792 | 1083 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|    108792 | 1084 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|    108792 | 1085 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,iNest);` |
|    108792 | 1086 | `	PH7_MemObjRelease(&sObj1);` |
|    108792 | 1087 | `	PH7_MemObjRelease(&sObj2);` |
|    108792 | 1088 | `	return rc;` |
|     54400 | 1089 | `}` |
|         - | 1090 | `/*` |
|         - | 1091 | ` * Rehash a node with a 64-bit integer key.` |
|         - | 1092 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - | 1093 | ` */` |
|     28762 | 1094 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 | 1095 | `{` |
|     28767 | 1096 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - | 1097 | `	sxu32 nBucket;` |
|         - | 1098 | `	/* Remove old collision links */` |
|     28767 | 1099 | `	if( pEntry->pPrevCollide ){` |
|     21140 | 1100 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|     10504 | 1101 | `	}else{` |
|      7632 | 1102 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - | 1103 | `	}` |
|     28767 | 1104 | `	if( pEntry->pNextCollide ){` |
|      2289 | 1105 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|      1119 | 1106 | `	}` |
|     28767 | 1107 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 1108 | `	/* Compute the new hash */` |
|     28767 | 1109 | `	pEntry->nHash = HASHMAP_INT_HASH(pMap,pMap->iNextIdx);` |
|     28767 | 1110 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     28767 | 1111 | `	if( pMap->iNextIdx > pMap->iMaxIntKey ){` |
|        13 | 1112 | `		pMap->iMaxIntKey = pMap->iNextIdx;` |
|         6 | 1113 | `	}` |
|     28767 | 1114 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - | 1115 | `	/* Link to the new bucket */` |
|     28767 | 1116 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     28767 | 1117 | `	if( pMap->apBucket[nBucket] ){` |
|     21805 | 1118 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|     10830 | 1119 | `	}` |
|     28767 | 1120 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     28767 | 1121 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - | 1122 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - | 1123 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - | 1124 | `	 * the no-overflow invariant uniform). */` |
|     28767 | 1125 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     28767 | 1126 | `		pMap->iNextIdx++;` |
|     14347 | 1127 | `	}` |
|     28767 | 1128 | `}` |
|         - | 1129 | `/*` |
|         - | 1130 | ` * Perform a linear search on a given hashmap.` |
|         - | 1131 | ` * Write a pointer to the target node on success.` |
|         - | 1132 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1133 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - | 1134 | ` * for more information.` |
|         - | 1135 | ` */` |
|     50395 | 1136 | `PH7_PRIVATE int HashmapFindValue(` |
|         - | 1137 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1138 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - | 1139 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - | 1140 | `	int bStrict      /* TRUE for strict comparison */` |
|         - | 1141 | `	)` |
|         5 | 1142 | `{` |
|         - | 1143 | `	ph7_hashmap_node *pEntry;` |
|         - | 1144 | `	ph7_value sVal,*pVal;` |
|         - | 1145 | `	ph7_value sNeedle;` |
|         - | 1146 | `	sxi32 rc;` |
|         - | 1147 | `	sxu32 n;` |
|         - | 1148 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     50400 | 1149 | `	pEntry = pMap->pFirst;` |
|     50400 | 1150 | `	n = pMap->nEntry;` |
|     50400 | 1151 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     50400 | 1152 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|    129650 | 1153 | `	for(;;){` |
|    259650 | 1154 | `		if( n < 1 ){` |
|      1427 | 1155 | `			break;` |
|         - | 1156 | `		}` |
|         - | 1157 | `		/* Extract node value */` |
|    258228 | 1158 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    258228 | 1159 | `		if( pVal ){` |
|         - | 1160 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - | 1161 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - | 1162 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - | 1163 | `			 * so null needles/values take the same path as everything else` |
|         - | 1164 | `			 * (the historical null-to-null shortcut here made` |
|         - | 1165 | `			 * in_array(null, [""]) false where php says true). */` |
|    258228 | 1166 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    258228 | 1167 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    258228 | 1168 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    258228 | 1169 | `			PH7_MemObjRelease(&sVal);` |
|    258228 | 1170 | `			PH7_MemObjRelease(&sNeedle);` |
|    258228 | 1171 | `			if( rc == 0 ){` |
|     48978 | 1172 | `				if( ppNode ){` |
|       ! 0 | 1173 | `					*ppNode = pEntry;` |
|       ! 0 | 1174 | `				}` |
|         - | 1175 | `				/* Match found*/` |
|     48978 | 1176 | `				return SXRET_OK;` |
|         - | 1177 | `			}` |
|    104463 | 1178 | `		}` |
|         - | 1179 | `		/* Point to the next entry */` |
|    209255 | 1180 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    209255 | 1181 | `		n--;` |
|         5 | 1182 | `	}` |
|         - | 1183 | `	/* No such entry */` |
|      1427 | 1184 | `	return SXERR_NOTFOUND;` |
|     25192 | 1185 | `}` |
|         - | 1186 | `/*` |
|         - | 1187 | ` * The element comparison array_diff()/array_intersect() and their _assoc pair` |
|         - | 1188 | ` * use, which is NOT the engine's value comparison: php's manual defines all four` |
|         - | 1189 | ` * as` |
|         - | 1190 | ` *     (string)$elem1 === (string)$elem2` |
|         - | 1191 | ` * a PURE string comparison — not numeric-string aware, so array_diff(["10"],` |
|         - | 1192 | ` * ["1e1"]) keeps "10" — where PHL used to call PH7_MemObjCmp with bStrict. That` |
|         - | 1193 | ` * made no int ever match its own decimal string, so array_diff([1,2,3],` |
|         - | 1194 | ` * ["1","2"]) answered the whole first array instead of [2=>3].` |
|         - | 1195 | ` *` |
|         - | 1196 | ` * bUserVisible picks the coercion: TRUE emits php's user-visible diagnostics (an` |
|         - | 1197 | ` * ARRAY element warns "Array to string conversion", an object with no` |
|         - | 1198 | ` * __toString() throws the catchable "could not be converted to string" Error,` |
|         - | 1199 | ` * reported through *pRc so the builtin answers the throw instead of a result),` |
|         - | 1200 | ` * FALSE renders silently. The two diff families need different answers there:` |
|         - | 1201 | ` * the _assoc pair converts LAZILY, only when a key matched, so it coerces` |
|         - | 1202 | ` * user-visibly right here; array_diff/array_intersect convert every element of` |
|         - | 1203 | ` * every input array up front (php sorts them), so those pre-pass with` |
|         - | 1204 | ` * HashmapStringifyElems and compare silently afterwards — which is what makes` |
|         - | 1205 | ` * the warning COUNT and the "throws even though an earlier element matched"` |
|         - | 1206 | ` * behaviour come out php-exact.` |
|         - | 1207 | ` *` |
|         - | 1208 | ` * Both operands are coerced on COPIES: these are live array elements, and a` |
|         - | 1209 | ` * diff must not rewrite the caller's array.` |
|         - | 1210 | ` */` |
|     48877 | 1211 | `PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc)` |
|         4 | 1212 | `{` |
|         - | 1213 | `	ph7_value sA,sB;` |
|     48881 | 1214 | `	int bEq = FALSE;` |
|         - | 1215 | `	sxi32 rc;` |
|     48881 | 1216 | `	*pRc = SXRET_OK;` |
|         - | 1217 | `	/* Two fast paths that need no rendering at all, because each type's string` |
|         - | 1218 | `	 * form is canonical and injective: two STRINGS already ARE their string form,` |
|         - | 1219 | `	 * and two INTS are string-equal exactly when they are equal. Without them` |
|         - | 1220 | `	 * array_diff() over a pair of integer ranges formatted both operands of every` |
|         - | 1221 | `	 * one of its O(n*m) comparisons (~4x slower than the strict compare it` |
|         - | 1222 | `	 * replaced). A value carrying MEMOBJ_INT alongside MEMOBJ_REAL is an integral` |
|         - | 1223 | `	 * FLOAT, whose "1" can equal an int's — the mask sends it down the slow path` |
|         - | 1224 | `	 * rather than comparing rVal-derived iVal, and bools/null/resources likewise. */` |
|     48881 | 1225 | `	if( (pA->iFlags & MEMOBJ_STRING) && (pB->iFlags & MEMOBJ_STRING) ){` |
|     50776 | 1226 | `		return SyBlobLength(&pA->sBlob) == SyBlobLength(&pB->sBlob)` |
|     53000 | 1227 | `		    && ( SyBlobLength(&pA->sBlob) == 0` |
|      4513 | 1228 | `		      \|\| SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),` |
|      4570 | 1229 | `		                  SyBlobLength(&pA->sBlob)) == 0 );` |
|         - | 1230 | `	}` |
|       390 | 1231 | `	if( (pA->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|       369 | 1232 | `	 && (pB->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT ){` |
|       201 | 1233 | `		return pA->x.iVal == pB->x.iVal;` |
|         - | 1234 | `	}` |
|       194 | 1235 | `	PH7_MemObjInit(pA->pVm,&sA);` |
|       194 | 1236 | `	PH7_MemObjInit(pA->pVm,&sB);` |
|       194 | 1237 | `	PH7_MemObjLoad(pA,&sA);` |
|       194 | 1238 | `	PH7_MemObjLoad(pB,&sB);` |
|       194 | 1239 | `	rc = bUserVisible ? PH7_MemObjToStringUV(&sA) : PH7_MemObjToString(&sA);` |
|       194 | 1240 | `	if( rc == SXRET_OK ){` |
|       194 | 1241 | `		rc = bUserVisible ? PH7_MemObjToStringUV(&sB) : PH7_MemObjToString(&sB);` |
|        96 | 1242 | `	}` |
|       194 | 1243 | `	if( rc != SXRET_OK ){` |
|         3 | 1244 | `		*pRc = rc;` |
|       193 | 1245 | `	}else if( SyBlobLength(&sA.sBlob) == SyBlobLength(&sB.sBlob) ){` |
|       254 | 1246 | `		bEq = SyBlobLength(&sA.sBlob) == 0` |
|       170 | 1247 | `		   \|\| SyMemcmp(SyBlobData(&sA.sBlob),SyBlobData(&sB.sBlob),SyBlobLength(&sA.sBlob)) == 0;` |
|        85 | 1248 | `	}` |
|       194 | 1249 | `	PH7_MemObjRelease(&sA);` |
|       194 | 1250 | `	PH7_MemObjRelease(&sB);` |
|       194 | 1251 | `	return bEq;` |
|     25888 | 1252 | `}` |
|         - | 1253 | `/*` |
|         - | 1254 | ` * Run the USER-VISIBLE string coercion over every element of pMap once, in` |
|         - | 1255 | ` * insertion order, discarding the result: php's array_diff/array_intersect sort` |
|         - | 1256 | ` * each input array, which converts every element exactly once, so this is where` |
|         - | 1257 | ` * their "Array to string conversion" warnings and their not-stringable-object` |
|         - | 1258 | ` * Error come from. Doing it as a pre-pass is what lets` |
|         - | 1259 | ` * array_diff([1,2],[1,new P()]) throw the way php's does even though the first` |
|         - | 1260 | ` * element already matched. Returns the throw status, SXRET_OK otherwise.` |
|         - | 1261 | ` */` |
|       298 | 1262 | `PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap)` |
|         4 | 1263 | `{` |
|       302 | 1264 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       302 | 1265 | `	sxu32 n = pMap->nEntry;` |
|      2785 | 1266 | `	while( n > 0 && pEntry ){` |
|      2495 | 1267 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|      2495 | 1268 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1269 | `			ph7_value sTmp;` |
|         - | 1270 | `			sxi32 rc;` |
|       246 | 1271 | `			PH7_MemObjInit(pMap->pVm,&sTmp);` |
|       246 | 1272 | `			PH7_MemObjLoad(pVal,&sTmp);` |
|       246 | 1273 | `			rc = PH7_MemObjToStringUV(&sTmp);` |
|       246 | 1274 | `			PH7_MemObjRelease(&sTmp);` |
|       246 | 1275 | `			if( rc != SXRET_OK ){` |
|         9 | 1276 | `				return rc;` |
|         - | 1277 | `			}` |
|       118 | 1278 | `		}` |
|      2487 | 1279 | `		pEntry = pEntry->pPrev; /* Reverse link — insertion order */` |
|      2487 | 1280 | `		n--;` |
|         4 | 1281 | `	}` |
|       294 | 1282 | `	return SXRET_OK;` |
|       153 | 1283 | `}` |
|         - | 1284 | `/*` |
|         - | 1285 | ` * Perform a linear search on a given hashmap, comparing values the way` |
|         - | 1286 | ` * array_diff()/array_intersect() do (see HashmapValueStrEq). Writes a pointer to` |
|         - | 1287 | ` * the target node on success; SXERR_NOTFOUND otherwise, with *pRc carrying the` |
|         - | 1288 | ` * status of a coercion that threw.` |
|         - | 1289 | ` */` |
|      1287 | 1290 | `PH7_PRIVATE int HashmapFindStringValue(` |
|         - | 1291 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1292 | `	ph7_value *pNeedle,  /* Lookup value */` |
|         - | 1293 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success */` |
|         - | 1294 | `	sxi32 *pRc           /* OUT: coercion status */` |
|         - | 1295 | `	)` |
|         4 | 1296 | `{` |
|      1291 | 1297 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      1291 | 1298 | `	sxu32 n = pMap->nEntry;` |
|      1291 | 1299 | `	*pRc = SXRET_OK;` |
|     49268 | 1300 | `	while( n > 0 && pEntry ){` |
|     48757 | 1301 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|     48757 | 1302 | `		if( pVal ){` |
|     48757 | 1303 | `			if( HashmapValueStrEq(pNeedle,pVal,/*bUserVisible*/0,pRc) ){` |
|       780 | 1304 | `				if( ppNode ){` |
|       ! 0 | 1305 | `					*ppNode = pEntry;` |
|       ! 0 | 1306 | `				}` |
|       780 | 1307 | `				return SXRET_OK;` |
|         - | 1308 | `			}` |
|     47981 | 1309 | `			if( *pRc != SXRET_OK ){` |
|       ! 0 | 1310 | `				return SXERR_NOTFOUND;` |
|         - | 1311 | `			}` |
|     25414 | 1312 | `		}` |
|     47981 | 1313 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     47981 | 1314 | `		n--;` |
|         4 | 1315 | `	}` |
|       515 | 1316 | `	return SXERR_NOTFOUND;` |
|       660 | 1317 | `}` |
|         - | 1318 | `/*` |
|         - | 1319 | ` * Compare two hashmaps.` |
|         - | 1320 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1321 | ` * Note on array comparison operators.` |
|         - | 1322 | ` *  According to the PHP language reference manual.` |
|         - | 1323 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1324 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1325 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1326 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1327 | ` *                          order and of the same types.` |
|         - | 1328 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1329 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1330 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1331 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1332 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1333 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1334 | ` * <?php` |
|         - | 1335 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1336 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1337 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1338 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1339 | ` * var_dump($c);` |
|         - | 1340 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1341 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1342 | ` * var_dump($c);` |
|         - | 1343 | ` * ?>` |
|         - | 1344 | ` * When executed, this script will print the following:` |
|         - | 1345 | ` * Union of $a and $b:` |
|         - | 1346 | ` * array(3) {` |
|         - | 1347 | ` *  ["a"]=>` |
|         - | 1348 | ` *  string(5) "apple"` |
|         - | 1349 | ` *  ["b"]=>` |
|         - | 1350 | ` * string(6) "banana"` |
|         - | 1351 | ` *  ["c"]=>` |
|         - | 1352 | ` * string(6) "cherry"` |
|         - | 1353 | ` * }` |
|         - | 1354 | ` * Union of $b and $a:` |
|         - | 1355 | ` * array(3) {` |
|         - | 1356 | ` * ["a"]=>` |
|         - | 1357 | ` * string(4) "pear"` |
|         - | 1358 | ` * ["b"]=>` |
|         - | 1359 | ` * string(10) "strawberry"` |
|         - | 1360 | ` * ["c"]=>` |
|         - | 1361 | ` * string(6) "cherry"` |
|         - | 1362 | ` * }` |
|         - | 1363 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1364 | ` *` |
|         - | 1365 | ` * This is the walk. PH7_HashmapCmp() below is the entry point every caller uses: it` |
|         - | 1366 | `` * holds the identity shortcut and php's recursion mark, so that every `return` here`` |
|         - | 1367 | `` * can stay a plain `return`.`` |
|         - | 1368 | ` */` |
|      3865 | 1369 | `static sxi32 HashmapCmpWalk(` |
|         - | 1370 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1371 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1372 | `	int bStrict,         /* TRUE for strict comparison */` |
|         - | 1373 | `	int iNest            /* Nesting depth counter */` |
|         - | 1374 | `	)` |
|         5 | 1375 | `{` |
|         - | 1376 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1377 | `	sxi32 rc;` |
|         - | 1378 | `	sxu32 n;` |
|      3870 | 1379 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1380 | `		/* Must have the same number of entries */` |
|        36 | 1381 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1382 | `	}` |
|      3838 | 1383 | `	if( bStrict ){` |
|         - | 1384 | `		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the` |
|         - | 1385 | `		 * same key/value pairs, with identical key types, in the same insertion` |
|         - | 1386 | `		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per` |
|         - | 1387 | `		 * this file's forward-iteration convention) in lockstep and compare each` |
|         - | 1388 | `		 * position's key then value. (Loose '==' below stays order-insensitive,` |
|         - | 1389 | `		 * matching each left key by lookup into the right map.) */` |
|      3018 | 1390 | `		ph7_hashmap_node *pLs = pLeft->pFirst;` |
|      3018 | 1391 | `		ph7_hashmap_node *pRs = pRight->pFirst;` |
|     22292 | 1392 | `		for( n = pLeft->nEntry ; n > 0 ; n-- ){` |
|         - | 1393 | `			/* Keys must match in type and value at this position */` |
|     19309 | 1394 | `			if( pLs->iType != pRs->iType ){` |
|         5 | 1395 | `				return 1;` |
|         - | 1396 | `			}` |
|     19305 | 1397 | `			if( pLs->iType == HASHMAP_INT_NODE ){` |
|     11982 | 1398 | `				if( pLs->xKey.iKey != pRs->xKey.iKey ){` |
|         3 | 1399 | `					return 1;` |
|         - | 1400 | `				}` |
|      6002 | 1401 | `			}else{` |
|      7327 | 1402 | `				SyBlob *pLk = &pLs->xKey.sKey;` |
|      7327 | 1403 | `				SyBlob *pRk = &pRs->xKey.sKey;` |
|      7323 | 1404 | `				if( SyBlobLength(pLk) != SyBlobLength(pRk)` |
|      7327 | 1405 | `				 \|\| (SyBlobLength(pLk) > 0` |
|      7323 | 1406 | `				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){` |
|         7 | 1407 | `					return 1;` |
|         - | 1408 | `				}` |
|         - | 1409 | `			}` |
|         - | 1410 | `			/* Values must be strictly identical */` |
|     19297 | 1411 | `			if( HashmapNodeCmp(pLs,pRs,TRUE,iNest+1) != 0 ){` |
|        21 | 1412 | `				return 1;` |
|         - | 1413 | `			}` |
|     19279 | 1414 | `			pLs = pLs->pPrev; /* Reverse link = insertion order */` |
|     19279 | 1415 | `			pRs = pRs->pPrev;` |
|      9650 | 1416 | `		}` |
|      2988 | 1417 | `		return 0; /* Same pairs, same order */` |
|         - | 1418 | `	}` |
|         - | 1419 | `	/* Point to the first inserted entry of the left hashmap */` |
|       824 | 1420 | `	pLe = pLeft->pFirst;` |
|       824 | 1421 | `	pRe = 0; /* cc warning */` |
|         - | 1422 | `	/* Perform the comparison */` |
|       824 | 1423 | `	n = pLeft->nEntry;` |
|       994 | 1424 | `	for(;;){` |
|      2008 | 1425 | `		if( n < 1 ){` |
|       684 | 1426 | `			break;` |
|         - | 1427 | `		}` |
|      1328 | 1428 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1429 | `			/* Int key */` |
|       810 | 1430 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|       407 | 1431 | `		}else{` |
|       519 | 1432 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1433 | `			/* Blob key */` |
|       519 | 1434 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1435 | `		}` |
|      1328 | 1436 | `		if( rc != SXRET_OK ){` |
|         - | 1437 | `			/* No such entry in the right side */` |
|        29 | 1438 | `			return 1;` |
|         - | 1439 | `		}` |
|      1300 | 1440 | `		rc = 0;` |
|      1300 | 1441 | `		if( bStrict ){` |
|         - | 1442 | `			/* Make sure,the keys are of the same type */` |
|       ! 0 | 1443 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1444 | `				rc = 1;` |
|       ! 0 | 1445 | `			}` |
|       ! 0 | 1446 | `		}` |
|      1300 | 1447 | `		if( !rc ){` |
|         - | 1448 | `			/* Compare nodes */` |
|      1300 | 1449 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict,iNest+1);` |
|       640 | 1450 | `		}` |
|      1300 | 1451 | `		if( rc != 0 ){` |
|         - | 1452 | `			/* Nodes key/value differ */` |
|       116 | 1453 | `			return rc;` |
|         - | 1454 | `		}` |
|         - | 1455 | `		/* Point to the next entry */` |
|      1188 | 1456 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      1188 | 1457 | `		n--;` |
|         4 | 1458 | `	}` |
|       684 | 1459 | `	return 0; /* Hashmaps are equals */` |
|      1935 | 1460 | `}` |
|      3897 | 1461 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1462 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1463 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1464 | `	int bStrict,         /* TRUE for strict comparison */` |
|         - | 1465 | `	int iNest            /* Nesting depth counter */` |
|         - | 1466 | `	)` |
|         5 | 1467 | `{` |
|         - | 1468 | `	sxi32 rc;` |
|      3902 | 1469 | `	if( pLeft == pRight ){` |
|         - | 1470 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1471 | `		 * Unlike the zend engine.` |
|         - | 1472 | `		 * php tests this FIRST too, above its own recursion guard, and that order is` |
|         - | 1473 | ``		 * observable: `$a[] = &$a; $a == $a` is TRUE, not a refusal.`` |
|         - | 1474 | `		 */` |
|        16 | 1475 | `		return 0;` |
|         - | 1476 | `	}` |
|      3888 | 1477 | `	if( (pLeft->iFlags & HASHMAP_COMPARING) \|\| iNest > PH7_CMP_MAX_DEPTH ){` |
|         - | 1478 | `		/* Either this map is its own descendant -- php's GC_IS_RECURSIVE(ht1) test, an` |
|         - | 1479 | `		 * ANCESTOR question that a depth counter cannot answer: a counter refuses data` |
|         - | 1480 | `		 * that is merely deep, and only notices a cycle once it has walked 31 levels of` |
|         - | 1481 | `		 * it -- or the finite-nesting backstop tripped. Both are php's catchable` |
|         - | 1482 | `		 * Error, recorded here and raised by whichever door onto the comparator can` |
|         - | 1483 | `		 * throw (an operator, a switch arm, a builtin's own context). */` |
|        19 | 1484 | `		PH7_CmpRefusalNesting(pLeft->pVm);` |
|        19 | 1485 | `		return 1;` |
|         - | 1486 | `	}` |
|      3870 | 1487 | `	pLeft->iFlags \|= HASHMAP_COMPARING;` |
|      3870 | 1488 | `	rc = HashmapCmpWalk(&(*pLeft),&(*pRight),bStrict,iNest);` |
|      3870 | 1489 | `	pLeft->iFlags &= ~HASHMAP_COMPARING;` |
|      3870 | 1490 | `	return rc;` |
|      1951 | 1491 | `}` |
|         - | 1492 | `/*` |
|         - | 1493 | ` * Duplicate a hashmap node.` |
|         - | 1494 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1495 | ` */` |
|   1362426 | 1496 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1497 | `	ph7_hashmap *pDest,` |
|         - | 1498 | `	ph7_hashmap_node *pEntry,` |
|         - | 1499 | `	ph7_value *pVal,` |
|         - | 1500 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1501 | `	)` |
|         5 | 1502 | `{` |
|         - | 1503 | `	ph7_value sSafeVal;` |
|         - | 1504 | `	ph7_value sKey;` |
|         - | 1505 | `	sxi32 rc;` |
|         - | 1506 |  |
|   1362431 | 1507 | `	if( PH7_HashmapNodeIsRef(&(*pEntry)) \|\| HashmapNodeIsSelfCycle(&(*pEntry)) ){` |
|         - | 1508 | ``		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points`` |
|         - | 1509 | `		 * at an outside slot) or, the case PH7 missed, an element somebody took a` |
|         - | 1510 | ``		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through`` |
|         - | 1511 | `		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep` |
|         - | 1512 | ``		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */`` |
|        46 | 1513 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|        46 | 1514 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        13 | 1515 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|        13 | 1516 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|        13 | 1517 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|        13 | 1518 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 1519 | `		}else{` |
|        34 | 1520 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         7 | 1521 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|        31 | 1522 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1523 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1524 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1525 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1526 | `			}else{ /* Dup: preserve the int key */` |
|        28 | 1527 | `				rc = HashmapKeepIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1528 | `			}` |
|         - | 1529 | `		}` |
|        46 | 1530 | `		return rc;` |
|         - | 1531 | `	}` |
|   1362387 | 1532 | `	sSafeVal = *pVal;` |
|         - | 1533 |  |
|   1362387 | 1534 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1535 | `		/* Blob key insertion */` |
|      8064 | 1536 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      8064 | 1537 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      8064 | 1538 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      8064 | 1539 | `		PH7_MemObjRelease(&sKey);` |
|      4026 | 1540 | `	}else{` |
|         - | 1541 | `		/* Int key */` |
|   1354328 | 1542 | `		if( iAction == 0 ){ /* Merge */` |
|   1343673 | 1543 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    682485 | 1544 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        34 | 1545 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        34 | 1546 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        34 | 1547 | `			PH7_MemObjRelease(&sKey);` |
|        18 | 1548 | `		}else{ /* Dup */` |
|     10628 | 1549 | `			rc = HashmapKeepIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1550 | `		}` |
|         - | 1551 | `	}` |
|   1362387 | 1552 | `	return rc;` |
|    681201 | 1553 | `}` |
|         - | 1554 | `/*` |
|         - | 1555 | ` * Merge two hashmaps.` |
|         - | 1556 | ` * Note on the merge process` |
|         - | 1557 | ` * According to the PHP language reference manual.` |
|         - | 1558 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1559 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1560 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1561 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1562 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1563 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1564 | ` *  keys starting from zero in the result array.` |
|         - | 1565 | ` */` |
|      4000 | 1566 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1567 | `{` |
|         - | 1568 | `	ph7_hashmap_node *pEntry;` |
|         - | 1569 | `	ph7_value *pVal;` |
|         - | 1570 | `	sxi32 rc;` |
|         - | 1571 | `	sxu32 n;` |
|      4005 | 1572 | `	if( pSrc == pDest ){` |
|         - | 1573 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1574 | `		 * Unlike the zend engine.` |
|         - | 1575 | `		 */` |
|       ! 0 | 1576 | `		return SXRET_OK;` |
|         - | 1577 | `	}` |
|         - | 1578 | `	/* Point to the first inserted entry in the source */` |
|      4005 | 1579 | `	pEntry = pSrc->pFirst;` |
|         - | 1580 | `	/* Perform the merge */` |
|   1347897 | 1581 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1582 | `		/* Extract the node value */` |
|   1343897 | 1583 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   1343897 | 1584 | `		if( pVal ){` |
|         - | 1585 | `			/* Make a local copy of the value.` |
|         - | 1586 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1587 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1588 | `			 * to the old pool.` |
|         - | 1589 | `			 */` |
|   1343897 | 1590 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    671942 | 1591 | `		}else{` |
|       ! 0 | 1592 | `			rc = SXRET_OK;` |
|         - | 1593 | `		}` |
|   1343897 | 1594 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1595 | `			return rc;` |
|         - | 1596 | `		}` |
|         - | 1597 | `		/* Point to the next entry */` |
|   1343897 | 1598 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    671942 | 1599 | `	}` |
|      4005 | 1600 | `	return SXRET_OK;` |
|      2005 | 1601 | `}` |
|         - | 1602 | `/*` |
|         - | 1603 | ` * Overwrite entries with the same key.` |
|         - | 1604 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1605 | ` *  According to the PHP language reference manual.` |
|         - | 1606 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1607 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1608 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1609 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1610 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1611 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1612 | ` *  overwriting the previous values.` |
|         - | 1613 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1614 | ` *  by whatever type is in the second array.` |
|         - | 1615 | ` */` |
|        38 | 1616 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1617 | `{` |
|         - | 1618 | `	ph7_hashmap_node *pEntry;` |
|         - | 1619 | `	ph7_value *pVal;` |
|         - | 1620 | `	sxi32 rc;` |
|         - | 1621 | `	sxu32 n;` |
|        40 | 1622 | `	if( pSrc == pDest ){` |
|         - | 1623 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1624 | `		 * Unlike the zend engine.` |
|         - | 1625 | `		 */` |
|       ! 0 | 1626 | `		return SXRET_OK;` |
|         - | 1627 | `	}` |
|         - | 1628 | `	/* Point to the first inserted entry in the source */` |
|        40 | 1629 | `	pEntry = pSrc->pFirst;` |
|         - | 1630 | `	/* Perform the merge */` |
|        88 | 1631 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1632 | `		/* Extract the node value */` |
|        50 | 1633 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        50 | 1634 | `		if( pVal ){` |
|        50 | 1635 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        26 | 1636 | `		}else{` |
|       ! 0 | 1637 | `			rc = SXRET_OK;` |
|         - | 1638 | `		}` |
|        50 | 1639 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1640 | `			return rc;` |
|         - | 1641 | `		}` |
|         - | 1642 | `		/* Point to the next entry */` |
|        50 | 1643 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        26 | 1644 | `	}` |
|        40 | 1645 | `	return SXRET_OK;` |
|        21 | 1646 | `}` |
|         - | 1647 | `/*` |
|         - | 1648 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1649 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1650 | ` */` |
|     14856 | 1651 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1652 | `{` |
|         - | 1653 | `	ph7_hashmap_node *pEntry;` |
|         - | 1654 | `	ph7_value *pVal;` |
|         - | 1655 | `	sxi32 rc;` |
|         - | 1656 | `	sxu32 n;` |
|     14861 | 1657 | `	if( pSrc == pDest ){` |
|         - | 1658 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1659 | `		 * Unlike the zend engine.` |
|         - | 1660 | `		 */` |
|       ! 0 | 1661 | `		return SXRET_OK;` |
|         - | 1662 | `	}` |
|         - | 1663 | `	/* Point to the first inserted entry in the source */` |
|     14861 | 1664 | `	pEntry = pSrc->pFirst;` |
|         - | 1665 | `	/* Perform the duplication */` |
|     33347 | 1666 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1667 | `		/* Extract the node value */` |
|     18491 | 1668 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     18491 | 1669 | `		if( pVal ){` |
|     18491 | 1670 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      9240 | 1671 | `		}else{` |
|       ! 0 | 1672 | `			rc = SXRET_OK;` |
|         - | 1673 | `		}` |
|     18491 | 1674 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1675 | `			return rc;` |
|         - | 1676 | `		}` |
|         - | 1677 | `		/* Point to the next entry */` |
|     18491 | 1678 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9240 | 1679 | `	}` |
|     14861 | 1680 | `	return SXRET_OK;` |
|      7422 | 1681 | `}` |
|         - | 1682 | `/*` |
|         - | 1683 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1684 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1685 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1686 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1687 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1688 | ` * this instead of PH7_HashmapDup.` |
|         - | 1689 | ` */` |
|        12 | 1690 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1691 | `{` |
|         - | 1692 | `	ph7_hashmap_node *pEntry;` |
|         - | 1693 | `	ph7_value *pVal;` |
|         - | 1694 | `	sxi32 rc;` |
|         - | 1695 | `	sxu32 n;` |
|        13 | 1696 | `	if( pSrc == pDest ){` |
|       ! 0 | 1697 | `		return SXRET_OK;` |
|         - | 1698 | `	}` |
|        13 | 1699 | `	pEntry = pSrc->pFirst;` |
|       951 | 1700 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1701 | `		/* Extract the node value (resolves foreign references) */` |
|       939 | 1702 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       938 | 1703 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       609 | 1704 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1705 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1706 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1707 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1708 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1709 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1710 | `			pVal = 0;` |
|         2 | 1711 | `		}` |
|       939 | 1712 | `		if( pVal ){` |
|       935 | 1713 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1396 | 1714 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       465 | 1715 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       466 | 1716 | `			}else{` |
|         5 | 1717 | `				rc = HashmapKeepIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1718 | `			}` |
|       935 | 1719 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1720 | `				return rc;` |
|         - | 1721 | `			}` |
|       467 | 1722 | `		}` |
|         - | 1723 | `		/* Point to the next entry */` |
|       939 | 1724 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       470 | 1725 | `	}` |
|        13 | 1726 | `	return SXRET_OK;` |
|         7 | 1727 | `}` |
|         - | 1728 | `/*` |
|         - | 1729 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1730 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1731 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1732 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1733 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1734 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1735 | ` * iterate-a-snapshot semantic.` |
|         - | 1736 | ` */` |
|        48 | 1737 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1738 | `{` |
|         - | 1739 | `	ph7_foreach_step *pStep;` |
|        51 | 1740 | `	sxi32 nRef = 0;` |
|        99 | 1741 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        51 | 1742 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1743 | `			nRef++;` |
|        21 | 1744 | `		}` |
|        27 | 1745 | `	}` |
|        51 | 1746 | `	return nRef;` |
|         3 | 1747 | `}` |
|         - | 1748 | `/*` |
|         - | 1749 | ` * Copy-on-write separation for arrays.` |
|         - | 1750 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1751 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1752 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1753 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1754 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1755 | ` * on the live map the loop is walking, like php.` |
|         - | 1756 | ` */` |
|    668734 | 1757 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1758 | `{` |
|    668739 | 1759 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1760 | `	ph7_hashmap *pNew;` |
|         - | 1761 | `	ph7_value *pBacking;` |
|         - | 1762 | `	sxu32 nValIdx;` |
|         - | 1763 | `	int bValueInPool;` |
|    668739 | 1764 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    668739 | 1765 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1766 | `		/* Sole owner, no separation needed */` |
|    655908 | 1767 | `		return pMap;` |
|         - | 1768 | `	}` |
|     12836 | 1769 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1770 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1771 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1772 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       651 | 1773 | `		return pMap;` |
|         - | 1774 | `	}` |
|         - | 1775 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1776 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1777 | `	 * frame is popped. */` |
|     12188 | 1778 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|     12162 | 1779 | `		pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pValue->nIdx);` |
|     12157 | 1780 | `		if( pBacking && pBacking != pValue` |
|     11516 | 1781 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|     10880 | 1782 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1783 | `			/* Undo the stack ref to reveal true sharing count */` |
|     10838 | 1784 | `			pMap->iRef--;` |
|     10838 | 1785 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1786 | `				/* After undoing stack ref, sole owner — no separation */` |
|     10252 | 1787 | `				pMap->iRef++;` |
|     10252 | 1788 | `				return pMap;` |
|         - | 1789 | `			}` |
|       590 | 1790 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|       590 | 1791 | `			if( pNew == 0 ){` |
|       ! 0 | 1792 | `				pMap->iRef++;` |
|       ! 0 | 1793 | `				return pMap;` |
|         - | 1794 | `			}` |
|       590 | 1795 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1796 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1797 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1798 | `				pMap->iRef++;` |
|       ! 0 | 1799 | `				return pMap;` |
|         - | 1800 | `			}` |
|       590 | 1801 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|       590 | 1802 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1803 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1804 | `			 * used to grow — and therefore reallocate (move) — pVm->aMemObj and` |
|         - | 1805 | `			 * invalidate the pBacking pointer captured above; the stale pointer` |
|         - | 1806 | `			 * was a hard SIGSEGV once the table was big enough to be mmap-backed.` |
|         - | 1807 | `			 * Redundant since P1 -- the pool's segments are fixed, so a slot's` |
|         - | 1808 | `			 * address never moves. Left for the harvest sweep. */` |
|       590 | 1809 | `			pBacking = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pValue->nIdx);` |
|       590 | 1810 | `			if( pBacking ){` |
|       590 | 1811 | `				pBacking->x.pOther = pNew;` |
|       293 | 1812 | `			}` |
|         - | 1813 | `			/* Update the stack value to match */` |
|       590 | 1814 | `			pValue->x.pOther = pNew;` |
|       590 | 1815 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|       590 | 1816 | `			return pNew;` |
|         - | 1817 | `		}` |
|       662 | 1818 | `	}` |
|         - | 1819 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1820 | `	 * directly into pVm->aMemObj, and PH7_HashmapDup below reserves a memory` |
|         - | 1821 | `	 * object per duplicated entry — which used to reallocate (move) the table and` |
|         - | 1822 | `	 * leave such a pValue dangling, so the slot identity is captured here and the` |
|         - | 1823 | `	 * write-back re-resolves from the index. Redundant since P1 -- the pool's` |
|         - | 1824 | `	 * segments are fixed, so a slot's address never moves. Left for the harvest` |
|         - | 1825 | `	 * sweep. */` |
|      1355 | 1826 | `	nValIdx = pValue->nIdx;` |
|      2017 | 1827 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      1350 | 1828 | `		&& (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      1355 | 1829 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      1355 | 1830 | `	if( pNew == 0 ){` |
|         - | 1831 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1832 | `		return pMap;` |
|         - | 1833 | `	}` |
|      1355 | 1834 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1835 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1836 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1837 | `		return pMap;` |
|         - | 1838 | `	}` |
|      1355 | 1839 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      1355 | 1840 | `	pMap->iRef--;` |
|      1355 | 1841 | `	if( bValueInPool ){` |
|         - | 1842 | `		/* Re-resolve pValue's slot. Redundant since P1 (see above): the dup can no` |
|         - | 1843 | `		 * longer move it. Left for the harvest sweep. */` |
|      1287 | 1844 | `		pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nValIdx);` |
|      1287 | 1845 | `		if( pValue == 0 ){` |
|       ! 0 | 1846 | `			return pNew;` |
|         - | 1847 | `		}` |
|       641 | 1848 | `	}` |
|      1355 | 1849 | `	pValue->x.pOther = pNew;` |
|      1355 | 1850 | `	return pNew;` |
|    334210 | 1851 | `}` |
|         - | 1852 | `/*` |
|         - | 1853 | ` * Perform the union of two hashmaps.` |
|         - | 1854 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1855 | ` * with a variable holding an array as follows:` |
|         - | 1856 | ` * <?php` |
|         - | 1857 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1858 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1859 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1860 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1861 | ` * var_dump($c);` |
|         - | 1862 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1863 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1864 | ` * var_dump($c);` |
|         - | 1865 | ` * ?>` |
|         - | 1866 | ` * When executed, this script will print the following:` |
|         - | 1867 | ` * Union of $a and $b:` |
|         - | 1868 | ` * array(3) {` |
|         - | 1869 | ` *  ["a"]=>` |
|         - | 1870 | ` *  string(5) "apple"` |
|         - | 1871 | ` *  ["b"]=>` |
|         - | 1872 | ` * string(6) "banana"` |
|         - | 1873 | ` *  ["c"]=>` |
|         - | 1874 | ` * string(6) "cherry"` |
|         - | 1875 | ` * }` |
|         - | 1876 | ` * Union of $b and $a:` |
|         - | 1877 | ` * array(3) {` |
|         - | 1878 | ` * ["a"]=>` |
|         - | 1879 | ` * string(4) "pear"` |
|         - | 1880 | ` * ["b"]=>` |
|         - | 1881 | ` * string(10) "strawberry"` |
|         - | 1882 | ` * ["c"]=>` |
|         - | 1883 | ` * string(6) "cherry"` |
|         - | 1884 | ` * }` |
|         - | 1885 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1886 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1887 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1888 | ` */` |
|      6425 | 1889 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1890 | `{` |
|         - | 1891 | `	ph7_hashmap_node *pEntry;` |
|      6430 | 1892 | `	sxi32 rc = SXRET_OK;` |
|         - | 1893 | `	ph7_value *pObj;` |
|         - | 1894 | `	sxu32 n;` |
|      6430 | 1895 | `	if( pLeft == pRight ){` |
|         - | 1896 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1897 | `		 * Unlike the zend engine.` |
|         - | 1898 | `		 */` |
|       ! 0 | 1899 | `		return SXRET_OK;` |
|         - | 1900 | `	}` |
|         - | 1901 | `	/* Perform the union */` |
|      6430 | 1902 | `	pEntry = pRight->pFirst;` |
|      6613 | 1903 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1904 | `		/* Make sure the given key does not exists in the left array */` |
|       188 | 1905 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1906 | `			/* BLOB key */` |
|       148 | 1907 | `			if( SXRET_OK !=` |
|       143 | 1908 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       144 | 1909 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       144 | 1910 | `					if( pObj ){` |
|       144 | 1911 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1912 | `						/* Perform the insertion */` |
|       144 | 1913 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1914 | `							&sSafeVal,0,FALSE);` |
|       144 | 1915 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1916 | `							return rc;` |
|         - | 1917 | `						}` |
|        69 | 1918 | `					}` |
|        69 | 1919 | `			}` |
|        76 | 1920 | `		}else{` |
|         - | 1921 | `			/* INT key */` |
|        42 | 1922 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        21 | 1923 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        21 | 1924 | `				if( pObj ){` |
|        21 | 1925 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1926 | `					/* Perform the insertion */` |
|        21 | 1927 | `					rc = HashmapKeepIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        21 | 1928 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1929 | `						return rc;` |
|         - | 1930 | `					}` |
|        10 | 1931 | `				}` |
|        10 | 1932 | `			}` |
|         - | 1933 | `		}` |
|         - | 1934 | `		/* Point to the next entry */` |
|       188 | 1935 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        96 | 1936 | `	}` |
|      6430 | 1937 | `	return SXRET_OK;` |
|      3212 | 1938 | `}` |
|         - | 1939 | `/*` |
|         - | 1940 | ` * Allocate a new hashmap.` |
|         - | 1941 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1942 | ` */` |
|   5114117 | 1943 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1944 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1945 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1946 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1947 | `	)` |
|         5 | 1948 | `{` |
|         - | 1949 | `	ph7_hashmap *pMap;` |
|         - | 1950 | `	/* Allocate a new instance */` |
|   5114122 | 1951 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   5114122 | 1952 | `	if( pMap == 0 ){` |
|       ! 0 | 1953 | `		return 0;` |
|         - | 1954 | `	}` |
|         - | 1955 | `	/* Zero the structure */` |
|   5114122 | 1956 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1957 | `	/* Fill in the structure */` |
|   5114122 | 1958 | `	pMap->pVm = &(*pVm);` |
|   5114122 | 1959 | `	pMap->iRef = 1;` |
|         - | 1960 | `	/* Default hash functions */` |
|   5114122 | 1961 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   5114122 | 1962 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   5114122 | 1963 | `	return pMap;` |
|   2556657 | 1964 | `}` |
|         - | 1965 | `/*` |
|         - | 1966 | ` * Install superglobals in the given virtual machine.` |
|         - | 1967 | ` * Note on superglobals.` |
|         - | 1968 | ` *  According to the PHP language reference manual.` |
|         - | 1969 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1970 | `*   Description` |
|         - | 1971 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1972 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1973 | `*   global $variable; to access them within functions or methods.` |
|         - | 1974 | `*   These superglobal variables are:` |
|         - | 1975 | `*    $GLOBALS` |
|         - | 1976 | `*    $_SERVER` |
|         - | 1977 | `*    $_GET` |
|         - | 1978 | `*    $_POST` |
|         - | 1979 | `*    $_FILES` |
|         - | 1980 | `*    $_COOKIE` |
|         - | 1981 | `*    $_SESSION` |
|         - | 1982 | `*    $_REQUEST` |
|         - | 1983 | `*    $_ENV` |
|         - | 1984 | `*/` |
|      6707 | 1985 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1986 | `{` |
|         - | 1987 | `	static const char * azSuper[] = {` |
|         - | 1988 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1989 | `		"_GET",      /* $_GET */` |
|         - | 1990 | `		"_POST",     /* $_POST */` |
|         - | 1991 | `		"_FILES",    /* $_FILES */` |
|         - | 1992 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1993 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1994 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1995 | `		"_ENV",      /* $_ENV */` |
|         - | 1996 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1997 | `		"argv"       /* $argv */` |
|         - | 1998 | `	};` |
|         - | 1999 | `	ph7_hashmap *pMap;` |
|         - | 2000 | `	ph7_value *pObj;` |
|         - | 2001 | `	SyString *pFile;` |
|         - | 2002 | `	sxi32 rc;` |
|         - | 2003 | `	sxu32 n;` |
|         - | 2004 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      6712 | 2005 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      6712 | 2006 | `	if( pMap == 0 ){` |
|       ! 0 | 2007 | `		return SXERR_MEM;` |
|         - | 2008 | `	}` |
|      6712 | 2009 | `	pVm->pGlobal = pMap;` |
|         - | 2010 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      6712 | 2011 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      6712 | 2012 | `	if( pObj == 0 ){` |
|       ! 0 | 2013 | `		return SXERR_MEM;` |
|         - | 2014 | `	}` |
|      6712 | 2015 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 2016 | `	/* Record object index */` |
|      6712 | 2017 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 2018 | `	/* Install the special $GLOBALS array */` |
|      6712 | 2019 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      6712 | 2020 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2021 | `		return rc;` |
|         - | 2022 | `	}` |
|      6712 | 2023 | `	PH7_VmSuperNote(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1);` |
|         - | 2024 | `	/* Install superglobals now */` |
|     73782 | 2025 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 2026 | `		ph7_value *pSuper;` |
|         - | 2027 | `		/* Request an empty array */` |
|     67075 | 2028 | `		pSuper = ph7_new_array(&(*pVm));` |
|     67075 | 2029 | `		if( pSuper == 0 ){` |
|       ! 0 | 2030 | `			return SXERR_MEM;` |
|         - | 2031 | `		}` |
|         - | 2032 | `		/* Install */` |
|     67075 | 2033 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     67075 | 2034 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2035 | `			return rc;` |
|         - | 2036 | `		}` |
|         - | 2037 | `		/* Release the value now it have been installed */` |
|     67075 | 2038 | `		ph7_release_value(&(*pVm),pSuper);` |
|     33485 | 2039 | `	}` |
|         - | 2040 | `	/* Set some $_SERVER entries */` |
|      6712 | 2041 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 2042 | `	/*` |
|         - | 2043 | `	 * 'SCRIPT_FILENAME'` |
|         - | 2044 | `	 * The absolute pathname of the currently executing script.` |
|         - | 2045 | `	 */` |
|     13419 | 2046 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 2047 | `		"SCRIPT_FILENAME",` |
|      3348 | 2048 | `		pFile ? pFile->zString : ":Memory:",` |
|      6707 | 2049 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 2050 | `		);` |
|         - | 2051 | `	/* All done,all super-global are installed now */` |
|      6712 | 2052 | `	return SXRET_OK;` |
|      3353 | 2053 | `}` |
|         - | 2054 | `/*` |
|         - | 2055 | ` * Release a hashmap.` |
|         - | 2056 | ` */` |
|   4916515 | 2057 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 2058 | `{` |
|         - | 2059 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   4916520 | 2060 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 2061 | `	sxu32 n;` |
|   4916520 | 2062 | `	if( pMap == pVm->pGlobal ){` |
|         - | 2063 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 2064 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 2065 | `		return SXRET_OK;` |
|         - | 2066 | `	}` |
|   4916520 | 2067 | `	if( pMap->pActiveSteps ){` |
|         - | 2068 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 2069 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 2070 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 2071 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 2072 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 2073 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 2074 | `		ph7_foreach_step *pStep;` |
|        17 | 2075 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 2076 | `			pStep->pCursor = 0;` |
|         5 | 2077 | `		}` |
|         4 | 2078 | `	}` |
|         - | 2079 | `	/* Start the release process */` |
|   4916520 | 2080 | `	n = 0;` |
|   4916520 | 2081 | `	pEntry = pMap->pFirst;` |
|   8454948 | 2082 | `	for(;;){` |
|  16916205 | 2083 | `		if( n >= pMap->nEntry ){` |
|   4916520 | 2084 | `			break;` |
|         - | 2085 | `		}` |
|  11999690 | 2086 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 2087 | `		/* Remove the reference from the foreign table */` |
|  11999690 | 2088 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|         - | 2089 | `		/* Restore the ph7_value to the free list if this node was its last holder` |
|         - | 2090 | `		 * (PH7_HashmapUnlinkNode explains both halves) */` |
|  11999690 | 2091 | `		PH7_VmReleaseUnheldSlot(pVm,pEntry->nValIdx);` |
|         - | 2092 | `		/* Release the node */` |
|  11999690 | 2093 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   3902918 | 2094 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   1949271 | 2095 | `		}` |
|  11999690 | 2096 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 2097 | `		/* Point to the next entry */` |
|  11999690 | 2098 | `		pEntry = pNext;` |
|  11999690 | 2099 | `		n++;` |
|         5 | 2100 | `	}` |
|   4916520 | 2101 | `	if( pMap->nEntry > 0 ){` |
|         - | 2102 | `		/* Release the hash bucket */` |
|   2677285 | 2103 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   1338391 | 2104 | `	}` |
|   4916520 | 2105 | `	if( FreeDS ){` |
|         - | 2106 | `		/* Free the whole instance -- and stop the collector's root buffer naming` |
|         - | 2107 | `		 * memory that is going back to the pool. */` |
|   4916510 | 2108 | `		PH7_GcForget(pVm,(void *)pMap,1);` |
|   4916510 | 2109 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   2457970 | 2110 | `	}else{` |
|         - | 2111 | `		/* Keep the instance but reset it's fields */` |
|        12 | 2112 | `		pMap->apBucket = 0;` |
|        12 | 2113 | `		pMap->iNextIdx = 0;` |
|        12 | 2114 | `	pMap->bIntKeySeen = 0;` |
|        12 | 2115 | `		pMap->nEntry = pMap->nSize = 0;` |
|        12 | 2116 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 2117 | `	}` |
|   4916520 | 2118 | `	return SXRET_OK;` |
|   2457975 | 2119 | `}` |
|         - | 2120 | `/*` |
|         - | 2121 | ` * Decrement the reference count of a given hashmap.` |
|         - | 2122 | ` * If the count reaches zero which mean no more variables` |
|         - | 2123 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 2124 | ` */` |
|  10594324 | 2125 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 2126 | `{` |
|  10594329 | 2127 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 2128 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  10594329 | 2129 | `	pMap->iRef--;` |
|  10594329 | 2130 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   4916412 | 2131 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   8135838 | 2132 | `	}else if( pMap->iRef >= 1 ){` |
|         - | 2133 | `		/* Still held -- but by whom? A drop that does NOT reach zero is the only` |
|         - | 2134 | `		 * event that can strand a cycle, so it is what the collector buffers. */` |
|   5677922 | 2135 | `		PH7_GcPossibleRoot(pVm,(void *)pMap,1);` |
|   2838178 | 2136 | `	}` |
|  10594329 | 2137 | `}` |
|         - | 2138 | `/*` |
|         - | 2139 | ` * Check if a given key exists in the given hashmap.` |
|         - | 2140 | ` * Write a pointer to the target node on success.` |
|         - | 2141 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 2142 | ` */` |
|   2332625 | 2143 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 2144 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 2145 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 2146 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 2147 | `	)` |
|         5 | 2148 | `{` |
|         - | 2149 | `	sxi32 rc;` |
|   2332630 | 2150 | `	if( pMap->nEntry < 1 ){` |
|         - | 2151 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 2152 | `		 */` |
|     86975 | 2153 | `		return SXERR_NOTFOUND;` |
|         - | 2154 | `	}` |
|   2245660 | 2155 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   2245660 | 2156 | `	return rc;` |
|   1166238 | 2157 | `}` |
|         - | 2158 | `/*` |
|         - | 2159 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 2160 | ` * hashmap.` |
|         - | 2161 | ` * If a node with the given key already exists in the database` |
|         - | 2162 | ` * then this function overwrite the old value.` |
|         - | 2163 | ` */` |
|  10808504 | 2164 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 2165 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2166 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2167 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 2168 | `	)` |
|         5 | 2169 | `{` |
|         - | 2170 | `	sxi32 rc;` |
|         - | 2171 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 2172 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 2173 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 2174 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|  10808509 | 2175 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|  10808509 | 2176 | `	return rc;` |
|         5 | 2177 | `}` |
|         - | 2178 | `/*` |
|         - | 2179 | ` * Insert (or overwrite) an entry under a RAW string key: php's zend_hash_update()` |
|         - | 2180 | ` * as opposed to the zend_symtable_update() every array subscript goes through, so` |
|         - | 2181 | ` * a numeric-looking NAME is kept as a string key instead of folding to the integer` |
|         - | 2182 | ` * one. php reaches for this where a name comes from OUTSIDE the language — the` |
|         - | 2183 | ``  * session store's `7\|i:1;` really does become a string key "7" that no `$_SESSION[7]` `` |
|         - | 2184 | `` * or `$_SESSION["7"]` can then reach. Only such a name-carrying reader should use it.`` |
|         - | 2185 | ` */` |
|      3330 | 2186 | `PH7_PRIVATE sxi32 PH7_HashmapInsertRawKey(` |
|         - | 2187 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 2188 | `	const char *zKey,    /* Raw key bytes */` |
|         - | 2189 | `	sxu32 nKey,          /* Key length */` |
|         - | 2190 | `	ph7_value *pVal      /* Node value */` |
|         - | 2191 | `	)` |
|         4 | 2192 | `{` |
|      3334 | 2193 | `	ph7_hashmap_node *pNode = 0;` |
|      3334 | 2194 | `	if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),zKey,nKey,&pNode) && pNode ){` |
|        27 | 2195 | `		ph7_value *pElem = (ph7_value *)PH7_MemObjAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        27 | 2196 | `		if( pElem ){` |
|        27 | 2197 | `			if( pVal ){` |
|        27 | 2198 | `				PH7_MemObjStore(pVal,pElem);` |
|        14 | 2199 | `			}else{` |
|       ! 0 | 2200 | `				PH7_MemObjToNull(pElem);` |
|         - | 2201 | `			}` |
|        13 | 2202 | `		}` |
|        27 | 2203 | `		return SXRET_OK;` |
|         - | 2204 | `	}` |
|      3308 | 2205 | `	return HashmapInsertBlobKey(&(*pMap),zKey,nKey,&(*pVal),0,FALSE);` |
|      1669 | 2206 | `}` |
|         - | 2207 | `/*` |
|         - | 2208 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 2209 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 2210 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 2211 | ` * This is the same routine that backs array_merge().` |
|         - | 2212 | ` */` |
|       668 | 2213 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         3 | 2214 | `{` |
|       671 | 2215 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         3 | 2216 | `}` |
|         - | 2217 | `/*` |
|         - | 2218 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 2219 | ` * hashmap.` |
|         - | 2220 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 2221 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 2222 | ` * The insertion by reference is triggered when the following` |
|         - | 2223 | ` * expression is encountered.` |
|         - | 2224 | ` * $var = 10;` |
|         - | 2225 | ` *  $a = array(&var);` |
|         - | 2226 | ` * OR` |
|         - | 2227 | ` *  $a[] =& $var;` |
|         - | 2228 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 2229 | ` * over it's contents.` |
|         - | 2230 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 2231 | ` * removed when the foreign ph7_value is unset.` |
|         - | 2232 | ` * Example:` |
|         - | 2233 | ` *  $var = 10;` |
|         - | 2234 | ` *  $a[] =& $var;` |
|         - | 2235 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 2236 | ` *  //Unset the foreign ph7_value now` |
|         - | 2237 | ` *  unset($var);` |
|         - | 2238 | ` *  echo count($a); //0` |
|         - | 2239 | ` * Note that this is a PH7 eXtension.` |
|         - | 2240 | ` * Refer to the official documentation for more information.` |
|         - | 2241 | ` * If a node with the given key already exists in the database` |
|         - | 2242 | ` * then this function overwrite the old value.` |
|         - | 2243 | ` */` |
|     93163 | 2244 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 2245 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2246 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2247 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 2248 | `	)` |
|         5 | 2249 | `{` |
|         - | 2250 | `	sxi32 rc;` |
|     93168 | 2251 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 2252 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 2253 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 2254 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 2255 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 2256 | `		return PH7_ABORT;` |
|         - | 2257 | `	}` |
|     93168 | 2258 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     93168 | 2259 | `	return rc;` |
|     46490 | 2260 | `}` |
|         - | 2261 | `/*` |
|         - | 2262 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 2263 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 2264 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 2265 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 2266 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 2267 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 2268 | ` */` |
|     42921 | 2269 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2270 | `{` |
|     42926 | 2271 | `	pStep->pCursor = pMap->pFirst;` |
|     42926 | 2272 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     42926 | 2273 | `	pMap->pActiveSteps = pStep;` |
|     42926 | 2274 | `}` |
|         - | 2275 | `/*` |
|         - | 2276 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 2277 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 2278 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 2279 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 2280 | ` */` |
|     42877 | 2281 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2282 | `{` |
|     42882 | 2283 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     42882 | 2284 | `	while( *ppLink ){` |
|     42882 | 2285 | `		if( *ppLink == pStep ){` |
|     42882 | 2286 | `			*ppLink = pStep->pNextActive;` |
|     42882 | 2287 | `			pStep->pNextActive = 0;` |
|     42882 | 2288 | `			return;` |
|         - | 2289 | `		}` |
|       ! 0 | 2290 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 2291 | `	}` |
|     21409 | 2292 | `}` |
|         - | 2293 | `/*` |
|         - | 2294 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 2295 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 2296 | ` * return NULL.` |
|         - | 2297 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 2298 | ` */` |
|    321725 | 2299 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         5 | 2300 | `{` |
|    321730 | 2301 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|    321730 | 2302 | `	if( pCur == 0 ){` |
|         - | 2303 | `		/* End of the list,return null */` |
|    160793 | 2304 | `		return 0;` |
|         - | 2305 | `	}` |
|         - | 2306 | `	/* Advance the node cursor */` |
|    160942 | 2307 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|    160942 | 2308 | `	return pCur;` |
|    160840 | 2309 | `}` |
|         - | 2310 | `/*` |
|         - | 2311 | ` * Extract a node value.` |
|         - | 2312 | ` */` |
|   2741405 | 2313 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2314 | `{` |
|   2741410 | 2315 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|   2741410 | 2316 | `	if( pEntry ){` |
|   2741410 | 2317 | `		if( bStore ){` |
|   1775757 | 2318 | `			PH7_MemObjStore(pEntry,pValue);` |
|    887493 | 2319 | `		}else{` |
|    965658 | 2320 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2321 | `		}` |
|   1368734 | 2322 | `	}else{` |
|       ! 0 | 2323 | `		PH7_MemObjRelease(pValue);` |
|         - | 2324 | `	}` |
|   2741410 | 2325 | `}` |
|         - | 2326 | `/*` |
|         - | 2327 | ` * Extract a node key.` |
|         - | 2328 | ` */` |
|   1831746 | 2329 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2330 | `{` |
|         - | 2331 | `	/* Fill with the current key */` |
|   1831751 | 2332 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   1637968 | 2333 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        63 | 2334 | `			SyBlobRelease(&pKey->sBlob);` |
|        31 | 2335 | `		}` |
|   1637968 | 2336 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   1637968 | 2337 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|    818847 | 2338 | `	}else{` |
|    193788 | 2339 | `		SyBlobReset(&pKey->sBlob);` |
|    193788 | 2340 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    193788 | 2341 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2342 | `	}` |
|   1831751 | 2343 | `}` |
|         - | 2344 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2345 | `/*` |
|         - | 2346 | ` * Store the address of nodes value in the given container.` |
|         - | 2347 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2348 | ` * defined in 'builtin.c' for more information.` |
|         - | 2349 | ` */` |
|        34 | 2350 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         3 | 2351 | `{` |
|        37 | 2352 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2353 | `	ph7_value *pValue;` |
|         - | 2354 | `	sxu32 n;` |
|         - | 2355 | `	/* Initialize the container */` |
|        37 | 2356 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        97 | 2357 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2358 | `		/* Extract node value */` |
|        63 | 2359 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        63 | 2360 | `		if( pValue ){` |
|        63 | 2361 | `			SySetPut(pOut,(const void *)&pValue);` |
|        30 | 2362 | `		}` |
|         - | 2363 | `		/* Point to the next entry */` |
|        63 | 2364 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 2365 | `	}` |
|         - | 2366 | `	/* Total inserted entries */` |
|        37 | 2367 | `	return (int)SySetUsed(pOut);` |
|         3 | 2368 | `}` |
|         - | 2369 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2370 | `/*` |
|         - | 2371 | ` * Table of hashmap functions.` |
|         - | 2372 | ` */` |
|         - | 2373 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 2374 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 2375 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 2376 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 2377 | `	{"count",             ph7_hashmap_count },` |
|         - | 2378 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 2379 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 2380 | `	{"key_exists",        ph7_hashmap_key_exists },` |
|         - | 2381 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 2382 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 2383 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 2384 | `	{"array_unshift",     ph7_hashmap_unshift },` |
|         - | 2385 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 2386 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 2387 | `	{"max",               ph7_hashmap_max     },` |
|         - | 2388 | `	{"min",               ph7_hashmap_min     },` |
|         - | 2389 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 2390 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 2391 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 2392 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 2393 | `	{"array_merge_recursive", ph7_hashmap_merge_recursive },` |
|         - | 2394 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 2395 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 2396 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 2397 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 2398 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 2399 | `	{"array_udiff_assoc", ph7_hashmap_udiff_assoc },` |
|         - | 2400 | `	{"array_udiff_uassoc",ph7_hashmap_udiff_uassoc },` |
|         - | 2401 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 2402 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 2403 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 2404 | `	{"array_diff_ukey",   ph7_hashmap_diff_ukey },` |
|         - | 2405 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 2406 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 2407 | `	{"array_intersect_uassoc", ph7_hashmap_intersect_uassoc},` |
|         - | 2408 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 2409 | `	{"array_uintersect_assoc", ph7_hashmap_uintersect_assoc},` |
|         - | 2410 | `	{"array_uintersect_uassoc", ph7_hashmap_uintersect_uassoc},` |
|         - | 2411 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 2412 | `	{"array_multisort",   ph7_hashmap_multisort },` |
|         - | 2413 | `	{"array_intersect_ukey",  ph7_hashmap_intersect_ukey},` |
|         - | 2414 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 2415 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 2416 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 2417 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 2418 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 2419 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 2420 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 2421 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 2422 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 2423 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 2424 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 2425 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 2426 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 2427 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 2428 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 2429 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 2430 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 2431 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 2432 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 2433 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 2434 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 2435 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 2436 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 2437 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 2438 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 2439 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 2440 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 2441 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 2442 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 2443 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 2444 | `	{"natsort",           ph7_hashmap_natsort },` |
|         - | 2445 | `	{"natcasesort",       ph7_hashmap_natsort },` |
|         - | 2446 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 2447 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 2448 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 2449 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 2450 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 2451 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 2452 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 2453 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 2454 | `	{"range",             ph7_hashmap_range   },` |
|         - | 2455 | `	{"current",           ph7_hashmap_current },` |
|         - | 2456 | `	{"each",              ph7_hashmap_each    },` |
|         - | 2457 | `	{"pos",               ph7_hashmap_current },` |
|         - | 2458 | `	{"next",              ph7_hashmap_next    },` |
|         - | 2459 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 2460 | `	{"end",               ph7_hashmap_end     },` |
|         - | 2461 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 2462 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 2463 | `};` |
|         - | 2464 | `/*` |
|         - | 2465 | ` * Register the built-in hashmap functions defined above.` |
|         - | 2466 | ` */` |
|      7925 | 2467 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 2468 | `{` |
|         - | 2469 | `	sxu32 n;` |
|    713255 | 2470 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    705330 | 2471 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    352178 | 2472 | `	}` |
|      7930 | 2473 | `}` |
|         - | 2474 | `/*` |
|         - | 2475 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 2476 | ` * the BLOB given as the first argument.` |
|         - | 2477 | ` * This function is typically invoked when the user issue a call to` |
|         - | 2478 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 2479 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 2480 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 2481 | ` */` |
|         - | 2482 | `/*` |
|         - | 2483 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 2484 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 2485 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 2486 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 2487 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 2488 | ` *` |
|         - | 2489 | ` * bProp says the entries are an object's PROPERTIES: php then reads each key` |
|         - | 2490 | ``  * through zend_unmangle_property_name, so "\0C\0p" prints as `["p":"C":private]` `` |
|         - | 2491 | `` * and "\0*\0p" as `["p":protected]`. That is how a get_debug_info handler (and a`` |
|         - | 2492 | ` * userland __debugInfo()) labels a non-public slot, and it is the ONLY place the` |
|         - | 2493 | `` * decode happens — `var_dump((array)$obj)` shows the mangled key raw.`` |
|         - | 2494 | ` */` |
|      2207 | 2495 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth,int bProp)` |
|         5 | 2496 | `{` |
|      2212 | 2497 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2498 | `	ph7_value *pObj;` |
|      2212 | 2499 | `	sxu32 n = 0;` |
|         - | 2500 | `	int isRef;` |
|      2212 | 2501 | `	sxi32 rc = SXRET_OK;` |
|         - | 2502 | `	int i;` |
|      3639 | 2503 | `	for(;;){` |
|      7284 | 2504 | `		if( n >= pMap->nEntry ){` |
|      2212 | 2505 | `			break;` |
|         - | 2506 | `		}` |
|      5077 | 2507 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 2508 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 2509 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|      5077 | 2510 | `		isRef = PH7_HashmapNodeIsRef(pEntry);` |
|      5077 | 2511 | `		if( ShowType ){` |
|         - | 2512 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 2513 | `			 * on the next line at the same indent (php). */` |
|     12361 | 2514 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|      8605 | 2515 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      4305 | 2516 | `			}` |
|      3761 | 2517 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|      2485 | 2518 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|      1245 | 2519 | `			}else{` |
|         - | 2520 | `				SyString sCls,sNm;` |
|      1281 | 2521 | `				if( bProp && PH7_UnmangleAttrName((const char *)SyBlobData(&pEntry->xKey.sKey),` |
|       155 | 2522 | `					SyBlobLength(&pEntry->xKey.sKey),&sCls,&sNm) ){` |
|       313 | 2523 | `					SyBlobFormat(&(*pOut),"[\"%z\"",&sNm);` |
|       313 | 2524 | `					if( sCls.nByte > 0 ){` |
|        85 | 2525 | `						if( sCls.zString[0] == '*' ){` |
|         7 | 2526 | `							SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|         4 | 2527 | `						}else{` |
|        79 | 2528 | `							SyBlobFormat(&(*pOut),":\"%z\":private",&sCls);` |
|         - | 2529 | `						}` |
|        42 | 2530 | `					}` |
|       313 | 2531 | `					SyBlobAppend(&(*pOut),"]=>",sizeof("]=>")-1);` |
|       158 | 2532 | `				}else{` |
|      1454 | 2533 | `					SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|       483 | 2534 | `						SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2535 | `				}` |
|         - | 2536 | `			}` |
|      3761 | 2537 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      3761 | 2538 | `			if( pObj ){` |
|      3761 | 2539 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|      3761 | 2540 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2541 | `					break;` |
|         - | 2542 | `				}` |
|      1878 | 2543 | `			}` |
|      1883 | 2544 | `		}else{` |
|         - | 2545 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 2546 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 2547 | `			 * php's extra blank line. References carry no marker. */` |
|      8873 | 2548 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      7557 | 2549 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      3781 | 2550 | `			}` |
|      1321 | 2551 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       579 | 2552 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|       292 | 2553 | `			}else{` |
|         - | 2554 | `				SyString sCls,sNm;` |
|       746 | 2555 | `				if( bProp && PH7_UnmangleAttrName((const char *)SyBlobData(&pEntry->xKey.sKey),` |
|       196 | 2556 | `					SyBlobLength(&pEntry->xKey.sKey),&sCls,&sNm) ){` |
|       396 | 2557 | `					SyBlobFormat(&(*pOut),"[%z",&sNm);` |
|       396 | 2558 | `					if( sCls.nByte > 0 ){` |
|        47 | 2559 | `						if( sCls.zString[0] == '*' ){` |
|         3 | 2560 | `							SyBlobAppend(&(*pOut),":protected",sizeof(":protected")-1);` |
|         2 | 2561 | `						}else{` |
|        45 | 2562 | `							SyBlobFormat(&(*pOut),":%z:private",&sCls);` |
|         - | 2563 | `						}` |
|        23 | 2564 | `					}` |
|       396 | 2565 | `					SyBlobAppend(&(*pOut),"] => ",sizeof("] => ")-1);` |
|       200 | 2566 | `				}else{` |
|       529 | 2567 | `					SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|       175 | 2568 | `						SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2569 | `				}` |
|         - | 2570 | `			}` |
|      1316 | 2571 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       737 | 2572 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|       153 | 2573 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|       153 | 2574 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       153 | 2575 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2576 | `					break;` |
|         - | 2577 | `				}` |
|        79 | 2578 | `			}else{` |
|      1173 | 2579 | `				if( pObj ){` |
|      1173 | 2580 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       584 | 2581 | `				}` |
|      1173 | 2582 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 2583 | `			}` |
|         - | 2584 | `		}` |
|         - | 2585 | `		/* Point to the next entry */` |
|      5077 | 2586 | `		n++;` |
|      5077 | 2587 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         5 | 2588 | `	}` |
|      2212 | 2589 | `	return rc;` |
|         5 | 2590 | `}` |
|      2037 | 2591 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         5 | 2592 | `{` |
|         - | 2593 | `	sxi32 rc;` |
|         - | 2594 | `	int i;` |
|      2042 | 2595 | `	if( nDepth > PH7_DUMP_MAX_DEPTH ){` |
|         - | 2596 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 2597 | `		/* Nesting limit reached */` |
|       ! 0 | 2598 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 2599 | `		return SXERR_LIMIT;` |
|         - | 2600 | `	}` |
|      2042 | 2601 | `	if( ShowType ){` |
|         - | 2602 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 2603 | `		 * newline (a nested array is itself an entry value line). A map that is` |
|         - | 2604 | `		 * its own descendant never reaches here: PH7_MemObjDump, the only caller,` |
|         - | 2605 | `		 * prints php's marker in place of the whole value. */` |
|      1708 | 2606 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|      1708 | 2607 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1708 | 2608 | `		pMap->iFlags \|= HASHMAP_DUMPING;` |
|      1708 | 2609 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth,0);` |
|      1708 | 2610 | `		pMap->iFlags &= ~HASHMAP_DUMPING;` |
|      2280 | 2611 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       577 | 2612 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       291 | 2613 | `		}` |
|      1708 | 2614 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      1708 | 2615 | `		return rc;` |
|         - | 2616 | `	}` |
|         - | 2617 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       339 | 2618 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       339 | 2619 | `	if( pMap->iFlags & HASHMAP_DUMPING ){` |
|         - | 2620 | `		/* php prints the header, then the marker in place of the body, and the` |
|         - | 2621 | `		 * entry line the caller is writing supplies the newline after it. */` |
|         3 | 2622 | `		SyBlobAppend(&(*pOut)," *RECURSION*",sizeof(" *RECURSION*")-1);` |
|         3 | 2623 | `		return SXRET_OK;` |
|         - | 2624 | `	}` |
|      1553 | 2625 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      1220 | 2626 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       612 | 2627 | `	}` |
|       337 | 2628 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       337 | 2629 | `	pMap->iFlags \|= HASHMAP_DUMPING;` |
|       337 | 2630 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth,0);` |
|       337 | 2631 | `	pMap->iFlags &= ~HASHMAP_DUMPING;` |
|      1553 | 2632 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      1220 | 2633 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       612 | 2634 | `	}` |
|       337 | 2635 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       337 | 2636 | `	return rc;` |
|      1023 | 2637 | `}` |
|         - | 2638 | `/*` |
|         - | 2639 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 2640 | ` * retrieved entry.` |
|         - | 2641 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 2642 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 2643 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 2644 | ` * a value different from PH7_OK.` |
|         - | 2645 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 2646 | ` */` |
|     69785 | 2647 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 2648 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2649 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 2650 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 2651 | `	)` |
|         5 | 2652 | `{` |
|         - | 2653 | `	ph7_hashmap_node *pEntry;` |
|         - | 2654 | `	ph7_value sKey,sValue;` |
|         - | 2655 | `	sxi32 rc;` |
|         - | 2656 | `	sxu32 n;` |
|         - | 2657 | `	/* Initialize walker parameter */` |
|     69790 | 2658 | `	rc = SXRET_OK;` |
|     69790 | 2659 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     69790 | 2660 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     69790 | 2661 | `	n = pMap->nEntry;` |
|     69790 | 2662 | `	pEntry = pMap->pFirst;` |
|         - | 2663 | `	/* Start the iteration process */` |
|    268012 | 2664 | `	for(;;){` |
|    536312 | 2665 | `		if( n < 1 ){` |
|     69704 | 2666 | `			break;` |
|         - | 2667 | `		}` |
|         - | 2668 | `		/* Extract a copy of the key and a copy the current value */` |
|    466613 | 2669 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    466613 | 2670 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 2671 | `		/* Invoke the user callback */` |
|    466613 | 2672 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 2673 | `		/* Release the copy of the key and the value */` |
|    466613 | 2674 | `		PH7_MemObjRelease(&sKey);` |
|    466613 | 2675 | `		PH7_MemObjRelease(&sValue);` |
|    466613 | 2676 | `		if( rc != PH7_OK ){` |
|         - | 2677 | `			/* Callback request an operation abort */` |
|        91 | 2678 | `			return SXERR_ABORT;` |
|         - | 2679 | `		}` |
|         - | 2680 | `		/* Point to the next entry */` |
|    466527 | 2681 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    466527 | 2682 | `		n--;` |
|         5 | 2683 | `	}` |
|         - | 2684 | `	/* All done */` |
|     69704 | 2685 | `	return SXRET_OK;` |
|     34873 | 2686 | `}` |
|         - | 2687 |  |
