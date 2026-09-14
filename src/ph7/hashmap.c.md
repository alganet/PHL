# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4029/4512 lines (89.30%)

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
|         - |   16 | `/* Node control flags */` |
|         - |   17 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value` |
|         - |   18 | `                                        * [i.e: array(&var)/$a[] =& $var ]` |
|         - |   19 | `										*/` |
|         - |   20 | `/*` |
|         - |   21 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|         - |   22 | ` */` |
|   7961842 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7961847 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7961847 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    521025 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    521030 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    521030 |   35 | `	sxu32 nH = 5381;` |
|    521030 |   36 | `	zEnd = &zIn[nLen];` |
|    597831 |   37 | `	for(;;){` |
|   1195668 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1021298 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    919661 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    795325 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    521030 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      2286 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      2291 |   54 | `	sxi64 iCount = 0;` |
|      2291 |   55 | `	if( !bRecursive ){` |
|      2117 |   56 | `		iCount = pMap->nEntry;` |
|      1061 |   57 | `	}else{` |
|         - |   58 | `		/* Recursive hashmap walk */` |
|       175 |   59 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|         - |   60 | `		ph7_value *pElem;` |
|       175 |   61 | `		sxu32 n = 0;` |
|         - |   62 | `		/* Mark this map as being counted */` |
|       175 |   63 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|       209 |   64 | `		for(;;){` |
|       419 |   65 | `			if( n >= pMap->nEntry ){` |
|       175 |   66 | `				break;` |
|         - |   67 | `			}` |
|         - |   68 | `			/* Point to the element value */` |
|       245 |   69 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|       245 |   70 | `			if( pElem ){` |
|       245 |   71 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|       151 |   72 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|       151 |   73 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|         - |   74 | `						/* Cycle detected — skip this entry */` |
|         3 |   75 | `						if( pCycleDetected ){` |
|         3 |   76 | `							*pCycleDetected = TRUE;` |
|         1 |   77 | `						}` |
|         2 |   78 | `					}else{` |
|       149 |   79 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|         - |   80 | `					}` |
|        75 |   81 | `				}` |
|       122 |   82 | `			}` |
|         - |   83 | `			/* Point to the next entry */` |
|       245 |   84 | `			pEntry = pEntry->pNext;` |
|       245 |   85 | `			++n;` |
|         1 |   86 | `		}` |
|         - |   87 | `		/* Clear the counting flag */` |
|       175 |   88 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|         - |   89 | `		/* Update count */` |
|       175 |   90 | `		iCount += pMap->nEntry;` |
|         - |   91 | `	}` |
|      2291 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3653192 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3653197 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3653197 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3653197 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3653197 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3653197 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3653197 |  112 | `	pNode->nHash = nHash;` |
|   3653197 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3653197 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3653197 |  115 | `	return pNode;` |
|   1826601 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    200185 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    200190 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    200190 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    200190 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    200190 |  133 | `	pNode->pMap  = &(*pMap);` |
|    200190 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    200190 |  135 | `	pNode->nHash = nHash;` |
|    200190 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    200190 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    200190 |  138 | `	pNode->nValIdx = nValIdx;` |
|    200190 |  139 | `	return pNode;` |
|    100097 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3853377 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3853382 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   3392811 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   3392811 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1696403 |  150 | `	}` |
|   3853382 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3853382 |  153 | `	if( pMap->pFirst == 0 ){` |
|     88908 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     88908 |  156 | `		pMap->pCur = pNode;` |
|     44456 |  157 | `	}else{` |
|   3764479 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3853382 |  160 | `	if( pMap->pActiveSteps ){` |
|         - |  161 | `		/* Re-arm any live foreach cursor parked past the end: php's by-ref` |
|         - |  162 | `		 * foreach iterates the LIVE array, so an element appended while the` |
|         - |  163 | `		 * loop stands on the last node (worklist idiom), or after the body` |
|         - |  164 | `		 * emptied the map, is still visited. A registered step with a NULL` |
|         - |  165 | `		 * cursor is always mid-loop — natural exhaustion unregisters before` |
|         - |  166 | `		 * the loop ends. */` |
|         - |  167 | `		ph7_foreach_step *pStep;` |
|        38 |  168 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        20 |  169 | `			if( pStep->pCursor == 0 ){` |
|        16 |  170 | `				pStep->pCursor = pNode;` |
|         7 |  171 | `			}` |
|        11 |  172 | `		}` |
|         9 |  173 | `	}` |
|   3853382 |  174 | `	++pMap->nEntry;` |
|   3853382 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7944 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7949 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7949 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7949 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7461 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3733 |  187 | `	}else{` |
|       490 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7949 |  190 | `	if( pNode->pNextCollide ){` |
|      5353 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2675 |  192 | `	}` |
|      7949 |  193 | `	if( pMap->pFirst == pNode ){` |
|       173 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        84 |  195 | `	}` |
|      7949 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       209 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       102 |  199 | `	}` |
|      7949 |  200 | `	if( pMap->pActiveSteps ){` |
|         - |  201 | `		/* Advance any live foreach cursor parked on this node (delete during` |
|         - |  202 | `		 * live-map iteration: by-ref foreach, $GLOBALS, snapshot fallbacks). */` |
|         - |  203 | `		ph7_foreach_step *pStep;` |
|        33 |  204 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        17 |  205 | `			if( pStep->pCursor == pNode ){` |
|         5 |  206 | `				pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|         2 |  207 | `			}` |
|         9 |  208 | `		}` |
|         8 |  209 | `	}` |
|         - |  210 | `	/* Unlink from the map list */` |
|      7949 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7949 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       215 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       215 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       215 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       105 |  218 | `		}` |
|       105 |  219 | `	}` |
|      7949 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7704 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3850 |  222 | `	}` |
|      7949 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7949 |  224 | `	pMap->nEntry--;` |
|      7949 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       101 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       101 |  228 | `		pMap->apBucket = 0;` |
|       101 |  229 | `		pMap->nSize = 0;` |
|       101 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        48 |  231 | `	}` |
|      7949 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3853377 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3853382 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     94408 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     94408 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     94408 |  245 | `		if( nNew < 1 ){` |
|     88908 |  246 | `			nNew = 16;` |
|     44451 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     94408 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     94408 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     94408 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     94408 |  260 | `		pMap->apBucket = apNew;` |
|     94408 |  261 | `		pMap->nSize = nNew;` |
|     94408 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     88908 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5505 |  267 | `		pEntry = pMap->pFirst;` |
|      5505 |  268 | `		n = 0;` |
|   2545406 |  269 | `		for( ;; ){` |
|   5090817 |  270 | `			if( n >= pMap->nEntry ){` |
|      5505 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   5085317 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   5085317 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   5085317 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   4412667 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   4412667 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2206331 |  280 | `			}` |
|   5085317 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   5085317 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   5085317 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5505 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2750 |  288 | `	}` |
|   3764479 |  289 | `	return SXRET_OK;` |
|   1926693 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3653192 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3653197 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3653155 |  310 | `		if( pValue ){` |
|   3653149 |  311 | `			sSafeVal = *pValue;` |
|   3653149 |  312 | `			pValue = &sSafeVal;` |
|   1826572 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3653155 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3653155 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3653155 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3653149 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1826572 |  322 | `		}` |
|   3653155 |  323 | `		nIdx = pObj->nIdx;` |
|   1826580 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3653197 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3653197 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3653197 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3653197 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3653197 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3653197 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3653197 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3653197 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3653197 |  349 | `	return SXRET_OK;` |
|   1826601 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    200185 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    200190 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    153212 |  370 | `		if( pValue ){` |
|    152902 |  371 | `			sSafeVal = *pValue;` |
|    152902 |  372 | `			pValue = &sSafeVal;` |
|     76448 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    153212 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    153212 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    153212 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    152902 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|     76448 |  382 | `		}` |
|    153212 |  383 | `		nIdx = pObj->nIdx;` |
|     76608 |  384 | `	}else{` |
|     46983 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    200190 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    200190 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    200190 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    200190 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     46983 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23489 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    200190 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    200190 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    200190 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    200190 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    200190 |  409 | `	return SXRET_OK;` |
|    100097 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4292316 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4292321 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       833 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4291493 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4291493 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110567720 |  433 | `	for(;;){` |
| 221135445 |  434 | `		if( pNode == 0 ){` |
|   4284687 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216850758 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216847742 |  438 | `			&& pNode->nHash == nHash` |
| 108425771 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      6811 |  441 | `				if( ppNode ){` |
|      6789 |  442 | `					*ppNode = pNode;` |
|      3392 |  443 | `				}` |
|      6811 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216843954 |  447 | `		pNode = pNode->pNextCollide;` |
|         2 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4284687 |  450 | `	return SXERR_NOTFOUND;` |
|   2146163 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    349339 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    349344 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     28504 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    320845 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    320845 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    259951 |  475 | `	for(;;){` |
|    519907 |  476 | `		if( pNode == 0 ){` |
|    252725 |  477 | `			break;` |
|         - |  478 | `		}` |
|    267182 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    265668 |  480 | `			&& pNode->nHash == nHash` |
|    166188 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     68227 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     68125 |  484 | `				if( ppNode ){` |
|     68097 |  485 | `					*ppNode = pNode;` |
|     34046 |  486 | `				}` |
|     68125 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    199067 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    252725 |  493 | `	return SXERR_NOTFOUND;` |
|    174674 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    349535 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    349540 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    349540 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    349540 |  504 | `	int isNeg = FALSE, nDigit;` |
|    349540 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    349518 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    349514 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    349514 |  516 | `	zDigit = zIn;` |
|    175221 |  517 | `	for(;;){` |
|    350448 |  518 | `		if( zIn >= zEnd ){` |
|       315 |  519 | `			break;` |
|         - |  520 | `		}` |
|    350134 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    349200 |  523 | `			return FALSE;` |
|         - |  524 | `		}` |
|       935 |  525 | `		zIn++;` |
|         1 |  526 | `	}` |
|         - |  527 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  528 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  529 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  530 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       315 |  531 | `	nDigit = (int)(zEnd - zDigit);` |
|       315 |  532 | `	if( nDigit < 1 ){` |
|         - |  533 | `		/* A lone sign ("-"/"+") */` |
|       ! 0 |  534 | `		return FALSE;` |
|         - |  535 | `	}` |
|       319 |  536 | `	if( nDigit > 19 \|\|` |
|       160 |  537 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|         7 |  538 | `		return FALSE;` |
|         - |  539 | `	}` |
|       309 |  540 | `	return TRUE;` |
|    174772 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    156058 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    156063 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    156063 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    149355 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    149355 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    149279 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    149279 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|        38 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      6789 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        89 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        44 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      6789 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     78029 |  575 | `result:` |
|    156063 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     74121 |  578 | `		if( ppNode ){` |
|     74069 |  579 | `			*ppNode = pNode;` |
|     37032 |  580 | `		}` |
|     74121 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     81947 |  584 | `	return SXERR_NOTFOUND;` |
|     78034 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  588 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  589 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  590 | ` * via HashmapAppendIndexBusy.` |
|         - |  591 | ` */` |
|   2142754 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2142759 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       877 |  596 | `		pMap->bIntKeySeen = 1;` |
|       877 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       877 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|       877 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2141887 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141635 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2141635 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1070815 |  609 | `	}` |
|   1071382 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   1506600 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1506605 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1506599 |  623 | `	return FALSE;` |
|    753305 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3802477 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3802482 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3802482 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3802482 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    153200 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    153200 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       231 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    229452 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     76482 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  654 | `				/* Overwrite the old value */` |
|         - |  655 | `				ph7_value *pElem;` |
|       493 |  656 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       493 |  657 | `				if( pElem ){` |
|       493 |  658 | `					if( pVal ){` |
|       493 |  659 | `						PH7_MemObjStore(pVal,pElem);` |
|       249 |  660 | `					}else{` |
|         - |  661 | `						/* Nullify the entry */` |
|       ! 0 |  662 | `						PH7_MemObjToNull(pElem);` |
|         - |  663 | `					}` |
|       244 |  664 | `				}` |
|       493 |  665 | `				return SXRET_OK;` |
|         - |  666 | `		}` |
|    152482 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  668 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  669 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       138 |  670 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  671 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  672 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  673 | `				return SXRET_OK;` |
|         - |  674 | `			}` |
|       206 |  675 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       136 |  676 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        68 |  677 | `				pVal,SXU32_HIGH);` |
|         - |  678 | `		}` |
|         - |  679 | `		/* Perform a blob-key insertion */` |
|    152346 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    152346 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1824641 |  683 | `IntKey:` |
|   3649517 |  684 | `	if( pKey ){` |
|   2142951 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       261 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       130 |  688 | `		}` |
|   2142951 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  690 | `			/* Overwrite the old value */` |
|         - |  691 | `			ph7_value *pElem;` |
|       198 |  692 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       198 |  693 | `			if( pElem ){` |
|       198 |  694 | `				if( pVal ){` |
|       198 |  695 | `					PH7_MemObjStore(pVal,pElem);` |
|       100 |  696 | `				}else{` |
|         - |  697 | `					/* Nullify the entry */` |
|       ! 0 |  698 | `					PH7_MemObjToNull(pElem);` |
|         - |  699 | `				}` |
|        98 |  700 | `			}` |
|       198 |  701 | `			return SXRET_OK;` |
|         - |  702 | `		}` |
|   2142755 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2142753 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2142753 |  712 | `		if( rc == SXRET_OK ){` |
|   2142753 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1071374 |  714 | `		}` |
|   1071379 |  715 | `	}else{` |
|   1506571 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1506569 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1506563 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1506563 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1506561 |  726 | `			++pMap->iNextIdx;` |
|    753278 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3649311 |  730 | `	return rc;` |
|   1901243 |  731 | `}` |
|         - |  732 | `/*` |
|         - |  733 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  734 | ` * hashmap.` |
|         - |  735 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  736 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  737 | ` * The insertion by reference is triggered when the following` |
|         - |  738 | ` * expression is encountered.` |
|         - |  739 | ` * $var = 10;` |
|         - |  740 | ` *  $a = array(&var);` |
|         - |  741 | ` * OR` |
|         - |  742 | ` *  $a[] =& $var;` |
|         - |  743 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  744 | ` * over it's contents.` |
|         - |  745 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  746 | ` * removed when the foreign ph7_value is unset.` |
|         - |  747 | ` * Example:` |
|         - |  748 | ` *  $var = 10;` |
|         - |  749 | ` *  $a[] =& $var;` |
|         - |  750 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  751 | ` *  //Unset the foreign ph7_value now` |
|         - |  752 | ` *  unset($var);` |
|         - |  753 | ` *  echo count($a); //0` |
|         - |  754 | ` * Note that this is a PH7 eXtension.` |
|         - |  755 | ` * Refer to the official documentation for more information.` |
|         - |  756 | ` * If a node with the given key already exists in the database` |
|         - |  757 | ` * then this function overwrite the old value.` |
|         - |  758 | ` */` |
|     47030 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     47035 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     47035 |  766 | `	sxi32 rc = SXRET_OK;` |
|     47035 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46995 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     46995 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     70487 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23494 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     46983 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46983 |  790 | `		return rc;` |
|         - |  791 | `	}` |
|        20 |  792 | `IntKey:` |
|        43 |  793 | `	if( pKey ){` |
|         7 |  794 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  795 | `			/* Force an integer cast */` |
|         3 |  796 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  797 | `		}` |
|         7 |  798 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  799 | `			/* Overwrite */` |
|       ! 0 |  800 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  801 | `			pNode->nValIdx = nRefIdx;` |
|         - |  802 | `			/* Install in the reference table */` |
|       ! 0 |  803 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  804 | `			return SXRET_OK;` |
|         - |  805 | `		}` |
|         - |  806 | `		/* Perform a 64-bit-int-key insertion */` |
|         7 |  807 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|         7 |  808 | `		if( rc == SXRET_OK ){` |
|         7 |  809 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         3 |  810 | `		}` |
|         4 |  811 | `	}else{` |
|        37 |  812 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  813 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  814 | `		}` |
|         - |  815 | `		/* Assign an automatic index */` |
|        37 |  816 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        37 |  817 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        37 |  818 | `			++pMap->iNextIdx;` |
|        18 |  819 | `		}` |
|         - |  820 | `	}` |
|         - |  821 | `	/* Insertion result */` |
|        43 |  822 | `	return rc;` |
|     23520 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1562663 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1562668 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1562668 |  832 | `	return pObj;` |
|         5 |  833 | `}` |
|         - |  834 | `/*` |
|         - |  835 | ` * Insert a node in the given hashmap.` |
|         - |  836 | ` * If a node with the given key already exists in the database` |
|         - |  837 | ` * then this function overwrite the old value.` |
|         - |  838 | ` */` |
|       568 |  839 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  840 | `{` |
|         - |  841 | `	ph7_value *pObj;` |
|         - |  842 | `	sxi32 rc;` |
|         - |  843 | `	/* Extract the node value */` |
|       573 |  844 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       573 |  845 | `	if( pObj == 0 ){` |
|       ! 0 |  846 | `		return SXERR_EMPTY;` |
|         - |  847 | `	}` |
|       568 |  848 | `	if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|       572 |  849 | `	 \|\| PH7_VmSlotIsReferenced(pMap->pVm,pNode->nValIdx) ){` |
|         - |  850 | `		/* A referenced element keeps its reference through the copy (php: array_slice()` |
|         - |  851 | ``		 * of an array holding `$r = &$a[1]` still var_dumps that element as &int(2)).`` |
|         - |  852 | `		 * Same rule HashmapDuplicateNode applies for array_merge()/spread. */` |
|         3 |  853 | `		sxu32 nRefIdx = pNode->nValIdx;` |
|         - |  854 | `		ph7_value sKey;` |
|         3 |  855 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         3 |  856 | `			if( !bPreserve ){` |
|         3 |  857 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  858 | `			}` |
|       ! 0 |  859 | `			PH7_MemObjInitFromInt(pMap->pVm,&sKey,pNode->xKey.iKey);` |
|       ! 0 |  860 | `		}else{` |
|       ! 0 |  861 | `			if( !bPreserve ){` |
|       ! 0 |  862 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  863 | `			}` |
|       ! 0 |  864 | `			PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       ! 0 |  865 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|       ! 0 |  866 | `				SyBlobLength(&pNode->xKey.sKey));` |
|         - |  867 | `		}` |
|       ! 0 |  868 | `		rc = HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|       ! 0 |  869 | `		PH7_MemObjRelease(&sKey);` |
|       ! 0 |  870 | `		return rc;` |
|         - |  871 | `	}` |
|         - |  872 | `	/* Preserve key */` |
|       571 |  873 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  874 | `		/* Int64 key */` |
|       439 |  875 | `		if( !bPreserve ){` |
|         - |  876 | `			/* Assign an automatic index */` |
|       267 |  877 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       136 |  878 | `		}else{` |
|       173 |  879 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  880 | `		}` |
|       222 |  881 | `	}else{` |
|         - |  882 | `		/* Blob key */` |
|       133 |  883 | `		if( !bPreserve ){` |
|         - |  884 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  885 | `			 * original string key entirely */` |
|        35 |  886 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  887 | `		}else{` |
|       148 |  888 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        49 |  889 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  890 | `		}` |
|         - |  891 | `	}` |
|       571 |  892 | `	return rc;` |
|       289 |  893 | `}` |
|         - |  894 | `/*` |
|         - |  895 | ` * Compare two node values.` |
|         - |  896 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  897 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  898 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  899 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  900 | ` * documenation.` |
|         - |  901 | ` */` |
|     79669 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     79674 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     79674 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     79674 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     79674 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     79674 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     79674 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     79674 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     79674 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     79674 |  921 | `	return rc;` |
|     39711 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     17162 |  927 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     17167 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     17167 |  932 | `	if( pEntry->pPrevCollide ){` |
|     12142 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5968 |  934 | `	}else{` |
|      5030 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     17167 |  937 | `	if( pEntry->pNextCollide ){` |
|      1086 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       564 |  939 | `	}` |
|     17167 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     17167 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     17167 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     17167 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     17167 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17167 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     12469 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      6138 |  949 | `	}` |
|     17167 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17167 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     17167 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     17167 |  956 | `		pMap->iNextIdx++;` |
|      8581 |  957 | `	}` |
|     17167 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     34066 |  966 | `static int HashmapFindValue(` |
|         - |  967 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  968 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  969 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  970 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  971 | `	)` |
|         5 |  972 | `{` |
|         - |  973 | `	ph7_hashmap_node *pEntry;` |
|         - |  974 | `	ph7_value sVal,*pVal;` |
|         - |  975 | `	ph7_value sNeedle;` |
|         - |  976 | `	sxi32 rc;` |
|         - |  977 | `	sxu32 n;` |
|         - |  978 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     34071 |  979 | `	pEntry = pMap->pFirst;` |
|     34071 |  980 | `	n = pMap->nEntry;` |
|     34071 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     34071 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     81297 |  983 | `	for(;;){` |
|    162600 |  984 | `		if( n < 1 ){` |
|        83 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    162518 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    162518 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    162518 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    162518 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    162518 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    162518 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    162518 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    162518 | 1001 | `			if( rc == 0 ){` |
|     33989 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     33989 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     64264 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    128534 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    128534 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|        83 | 1014 | `	return SXERR_NOTFOUND;` |
|     17038 | 1015 | `}` |
|         - | 1016 | `/*` |
|         - | 1017 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - | 1018 | ` * for values comparison.` |
|         - | 1019 | ` * Write a pointer to the target node on success.` |
|         - | 1020 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1021 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - | 1022 | ` * for more information.` |
|         - | 1023 | ` */` |
|        22 | 1024 | `static int HashmapFindValueByCallback(` |
|         - | 1025 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - | 1026 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - | 1027 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - | 1028 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - | 1029 | `	)` |
|         1 | 1030 | `{` |
|         - | 1031 | `	ph7_hashmap_node *pEntry;` |
|         - | 1032 | `	ph7_value sResult,*pVal;` |
|         - | 1033 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - | 1034 | `	sxi32 rc;` |
|         - | 1035 | `	sxu32 n;` |
|        23 | 1036 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - | 1037 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - | 1038 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 | 1039 | `		return SXERR_NOTFOUND;` |
|         - | 1040 | `	}` |
|         - | 1041 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        23 | 1042 | `	pEntry = pMap->pFirst;` |
|        23 | 1043 | `	n = pMap->nEntry;` |
|         - | 1044 | `	/* Store callback result here */` |
|        23 | 1045 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - | 1046 | `	/* First argument to the callback */` |
|        23 | 1047 | `	apArg[0] = pNeedle;` |
|        25 | 1048 | `	for(;;){` |
|        51 | 1049 | `		if( n < 1 ){` |
|         9 | 1050 | `			break;` |
|         - | 1051 | `		}` |
|         - | 1052 | `		/* Extract node value */` |
|        43 | 1053 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        43 | 1054 | `		if( pVal ){` |
|         - | 1055 | `			/* Invoke the user callback */` |
|        43 | 1056 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        43 | 1057 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        43 | 1058 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 1059 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1060 | `				 * and report no match for the rest of the run. */` |
|         5 | 1061 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1062 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1063 | `				return SXERR_NOTFOUND;` |
|         - | 1064 | `			}` |
|        39 | 1065 | `			if( rc == SXRET_OK ){` |
|         - | 1066 | `				/* Extract callback result */` |
|        39 | 1067 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1068 | `					/* Perform an int cast */` |
|       ! 0 | 1069 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1070 | `				}` |
|        39 | 1071 | `				rc = (sxi32)sResult.x.iVal;` |
|        39 | 1072 | `				PH7_MemObjRelease(&sResult);` |
|        39 | 1073 | `				if( rc == 0 ){` |
|         - | 1074 | `					/* Match found*/` |
|        11 | 1075 | `					if( ppNode ){` |
|       ! 0 | 1076 | `						*ppNode = pEntry;` |
|       ! 0 | 1077 | `					}` |
|        11 | 1078 | `					return SXRET_OK;` |
|         - | 1079 | `				}` |
|        14 | 1080 | `			}` |
|        14 | 1081 | `		}` |
|         - | 1082 | `		/* Point to the next entry */` |
|        29 | 1083 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 1084 | `		n--;` |
|         1 | 1085 | `	}` |
|         - | 1086 | `	/* No such entry */` |
|         9 | 1087 | `	return SXERR_NOTFOUND;` |
|        12 | 1088 | `}` |
|         - | 1089 | `/*` |
|         - | 1090 | ` * Compare two hashmaps.` |
|         - | 1091 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1092 | ` * Note on array comparison operators.` |
|         - | 1093 | ` *  According to the PHP language reference manual.` |
|         - | 1094 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1095 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1096 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1097 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1098 | ` *                          order and of the same types.` |
|         - | 1099 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1100 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1101 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1102 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1103 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1104 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1105 | ` * <?php` |
|         - | 1106 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1107 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1108 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1109 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1110 | ` * var_dump($c);` |
|         - | 1111 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1112 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1113 | ` * var_dump($c);` |
|         - | 1114 | ` * ?>` |
|         - | 1115 | ` * When executed, this script will print the following:` |
|         - | 1116 | ` * Union of $a and $b:` |
|         - | 1117 | ` * array(3) {` |
|         - | 1118 | ` *  ["a"]=>` |
|         - | 1119 | ` *  string(5) "apple"` |
|         - | 1120 | ` *  ["b"]=>` |
|         - | 1121 | ` * string(6) "banana"` |
|         - | 1122 | ` *  ["c"]=>` |
|         - | 1123 | ` * string(6) "cherry"` |
|         - | 1124 | ` * }` |
|         - | 1125 | ` * Union of $b and $a:` |
|         - | 1126 | ` * array(3) {` |
|         - | 1127 | ` * ["a"]=>` |
|         - | 1128 | ` * string(4) "pear"` |
|         - | 1129 | ` * ["b"]=>` |
|         - | 1130 | ` * string(10) "strawberry"` |
|         - | 1131 | ` * ["c"]=>` |
|         - | 1132 | ` * string(6) "cherry"` |
|         - | 1133 | ` * }` |
|         - | 1134 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1135 | ` */` |
|        54 | 1136 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1137 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1138 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1139 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1140 | `	)` |
|         1 | 1141 | `{` |
|         - | 1142 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1143 | `	sxi32 rc;` |
|         - | 1144 | `	sxu32 n;` |
|        55 | 1145 | `	if( pLeft == pRight ){` |
|         - | 1146 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1147 | `		 * Unlike the zend engine.` |
|         - | 1148 | `		 */` |
|         7 | 1149 | `		return 0;` |
|         - | 1150 | `	}` |
|        49 | 1151 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1152 | `		/* Must have the same number of entries */` |
|         5 | 1153 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1154 | `	}` |
|        45 | 1155 | `	if( bStrict ){` |
|         - | 1156 | `		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the` |
|         - | 1157 | `		 * same key/value pairs, with identical key types, in the same insertion` |
|         - | 1158 | `		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per` |
|         - | 1159 | `		 * this file's forward-iteration convention) in lockstep and compare each` |
|         - | 1160 | `		 * position's key then value. (Loose '==' below stays order-insensitive,` |
|         - | 1161 | `		 * matching each left key by lookup into the right map.) */` |
|        33 | 1162 | `		ph7_hashmap_node *pLs = pLeft->pFirst;` |
|        33 | 1163 | `		ph7_hashmap_node *pRs = pRight->pFirst;` |
|       121 | 1164 | `		for( n = pLeft->nEntry ; n > 0 ; n-- ){` |
|         - | 1165 | `			/* Keys must match in type and value at this position */` |
|       103 | 1166 | `			if( pLs->iType != pRs->iType ){` |
|       ! 0 | 1167 | `				return 1;` |
|         - | 1168 | `			}` |
|       103 | 1169 | `			if( pLs->iType == HASHMAP_INT_NODE ){` |
|        81 | 1170 | `				if( pLs->xKey.iKey != pRs->xKey.iKey ){` |
|         3 | 1171 | `					return 1;` |
|         - | 1172 | `				}` |
|        40 | 1173 | `			}else{` |
|        23 | 1174 | `				SyBlob *pLk = &pLs->xKey.sKey;` |
|        23 | 1175 | `				SyBlob *pRk = &pRs->xKey.sKey;` |
|        22 | 1176 | `				if( SyBlobLength(pLk) != SyBlobLength(pRk)` |
|        23 | 1177 | `				 \|\| (SyBlobLength(pLk) > 0` |
|        22 | 1178 | `				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){` |
|         7 | 1179 | `					return 1;` |
|         - | 1180 | `				}` |
|         - | 1181 | `			}` |
|         - | 1182 | `			/* Values must be strictly identical */` |
|        95 | 1183 | `			if( HashmapNodeCmp(pLs,pRs,TRUE) != 0 ){` |
|         7 | 1184 | `				return 1;` |
|         - | 1185 | `			}` |
|        89 | 1186 | `			pLs = pLs->pPrev; /* Reverse link = insertion order */` |
|        89 | 1187 | `			pRs = pRs->pPrev;` |
|        45 | 1188 | `		}` |
|        19 | 1189 | `		return 0; /* Same pairs, same order */` |
|         - | 1190 | `	}` |
|         - | 1191 | `	/* Point to the first inserted entry of the left hashmap */` |
|        13 | 1192 | `	pLe = pLeft->pFirst;` |
|        13 | 1193 | `	pRe = 0; /* cc warning */` |
|         - | 1194 | `	/* Perform the comparison */` |
|        13 | 1195 | `	n = pLeft->nEntry;` |
|        16 | 1196 | `	for(;;){` |
|        33 | 1197 | `		if( n < 1 ){` |
|        13 | 1198 | `			break;` |
|         - | 1199 | `		}` |
|        21 | 1200 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1201 | `			/* Int key */` |
|        13 | 1202 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|         7 | 1203 | `		}else{` |
|         9 | 1204 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1205 | `			/* Blob key */` |
|         9 | 1206 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1207 | `		}` |
|        21 | 1208 | `		if( rc != SXRET_OK ){` |
|         - | 1209 | `			/* No such entry in the right side */` |
|       ! 0 | 1210 | `			return 1;` |
|         - | 1211 | `		}` |
|        21 | 1212 | `		rc = 0;` |
|        21 | 1213 | `		if( bStrict ){` |
|         - | 1214 | `			/* Make sure,the keys are of the same type */` |
|       ! 0 | 1215 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1216 | `				rc = 1;` |
|       ! 0 | 1217 | `			}` |
|       ! 0 | 1218 | `		}` |
|        21 | 1219 | `		if( !rc ){` |
|         - | 1220 | `			/* Compare nodes */` |
|        21 | 1221 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        10 | 1222 | `		}` |
|        21 | 1223 | `		if( rc != 0 ){` |
|         - | 1224 | `			/* Nodes key/value differ */` |
|       ! 0 | 1225 | `			return rc;` |
|         - | 1226 | `		}` |
|         - | 1227 | `		/* Point to the next entry */` |
|        21 | 1228 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        21 | 1229 | `		n--;` |
|         1 | 1230 | `	}` |
|        13 | 1231 | `	return 0; /* Hashmaps are equals */` |
|        28 | 1232 | `}` |
|         - | 1233 | `/*` |
|         - | 1234 | ` * Duplicate a hashmap node.` |
|         - | 1235 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1236 | ` */` |
|    728882 | 1237 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1238 | `	ph7_hashmap *pDest,` |
|         - | 1239 | `	ph7_hashmap_node *pEntry,` |
|         - | 1240 | `	ph7_value *pVal,` |
|         - | 1241 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1242 | `	)` |
|         5 | 1243 | `{` |
|         - | 1244 | `	ph7_value sSafeVal;` |
|         - | 1245 | `	ph7_value sKey;` |
|         - | 1246 | `	sxi32 rc;` |
|         - | 1247 |  |
|    728882 | 1248 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    728884 | 1249 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
|         - | 1250 | ``		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points`` |
|         - | 1251 | `		 * at an outside slot) or, the case PH7 missed, an element somebody took a` |
|         - | 1252 | ``		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through`` |
|         - | 1253 | `		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep` |
|         - | 1254 | ``		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */`` |
|         9 | 1255 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         9 | 1256 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1257 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1258 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1259 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1260 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1261 | `		}else{` |
|         7 | 1262 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         7 | 1263 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         3 | 1264 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1265 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1266 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1267 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1268 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1269 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1270 | `			}` |
|         - | 1271 | `		}` |
|         9 | 1272 | `		return rc;` |
|         - | 1273 | `	}` |
|    728879 | 1274 | `	sSafeVal = *pVal;` |
|         - | 1275 |  |
|    728879 | 1276 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1277 | `		/* Blob key insertion */` |
|      4049 | 1278 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4049 | 1279 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4049 | 1280 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4049 | 1281 | `		PH7_MemObjRelease(&sKey);` |
|      2027 | 1282 | `	}else{` |
|         - | 1283 | `		/* Int key */` |
|    724835 | 1284 | `		if( iAction == 0 ){ /* Merge */` |
|    721149 | 1285 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    364263 | 1286 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1287 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1288 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1289 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1290 | `		}else{ /* Dup */` |
|      3661 | 1291 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1292 | `		}` |
|         - | 1293 | `	}` |
|    728879 | 1294 | `	return rc;` |
|    364446 | 1295 | `}` |
|         - | 1296 | `/*` |
|         - | 1297 | ` * Merge two hashmaps.` |
|         - | 1298 | ` * Note on the merge process` |
|         - | 1299 | ` * According to the PHP language reference manual.` |
|         - | 1300 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1301 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1302 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1303 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1304 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1305 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1306 | ` *  keys starting from zero in the result array.` |
|         - | 1307 | ` */` |
|      2914 | 1308 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1309 | `{` |
|         - | 1310 | `	ph7_hashmap_node *pEntry;` |
|         - | 1311 | `	ph7_value *pVal;` |
|         - | 1312 | `	sxi32 rc;` |
|         - | 1313 | `	sxu32 n;` |
|      2919 | 1314 | `	if( pSrc == pDest ){` |
|         - | 1315 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1316 | `		 * Unlike the zend engine.` |
|         - | 1317 | `		 */` |
|       ! 0 | 1318 | `		return SXRET_OK;` |
|         - | 1319 | `	}` |
|         - | 1320 | `	/* Point to the first inserted entry in the source */` |
|      2919 | 1321 | `	pEntry = pSrc->pFirst;` |
|         - | 1322 | `	/* Perform the merge */` |
|    724123 | 1323 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1324 | `		/* Extract the node value */` |
|    721209 | 1325 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    721209 | 1326 | `		if( pVal ){` |
|         - | 1327 | `			/* Make a local copy of the value.` |
|         - | 1328 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1329 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1330 | `			 * to the old pool.` |
|         - | 1331 | `			 */` |
|    721209 | 1332 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    360607 | 1333 | `		}else{` |
|       ! 0 | 1334 | `			rc = SXRET_OK;` |
|         - | 1335 | `		}` |
|    721209 | 1336 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1337 | `			return rc;` |
|         - | 1338 | `		}` |
|         - | 1339 | `		/* Point to the next entry */` |
|    721209 | 1340 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    360607 | 1341 | `	}` |
|      2919 | 1342 | `	return SXRET_OK;` |
|      1462 | 1343 | `}` |
|         - | 1344 | `/*` |
|         - | 1345 | ` * Overwrite entries with the same key.` |
|         - | 1346 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1347 | ` *  According to the PHP language reference manual.` |
|         - | 1348 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1349 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1350 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1351 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1352 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1353 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1354 | ` *  overwriting the previous values.` |
|         - | 1355 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1356 | ` *  by whatever type is in the second array.` |
|         - | 1357 | ` */` |
|        34 | 1358 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1359 | `{` |
|         - | 1360 | `	ph7_hashmap_node *pEntry;` |
|         - | 1361 | `	ph7_value *pVal;` |
|         - | 1362 | `	sxi32 rc;` |
|         - | 1363 | `	sxu32 n;` |
|        36 | 1364 | `	if( pSrc == pDest ){` |
|         - | 1365 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1366 | `		 * Unlike the zend engine.` |
|         - | 1367 | `		 */` |
|       ! 0 | 1368 | `		return SXRET_OK;` |
|         - | 1369 | `	}` |
|         - | 1370 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1371 | `	pEntry = pSrc->pFirst;` |
|         - | 1372 | `	/* Perform the merge */` |
|        80 | 1373 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1374 | `		/* Extract the node value */` |
|        46 | 1375 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1376 | `		if( pVal ){` |
|        46 | 1377 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1378 | `		}else{` |
|       ! 0 | 1379 | `			rc = SXRET_OK;` |
|         - | 1380 | `		}` |
|        46 | 1381 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1382 | `			return rc;` |
|         - | 1383 | `		}` |
|         - | 1384 | `		/* Point to the next entry */` |
|        46 | 1385 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1386 | `	}` |
|        36 | 1387 | `	return SXRET_OK;` |
|        19 | 1388 | `}` |
|         - | 1389 | `/*` |
|         - | 1390 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1391 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1392 | ` */` |
|      7350 | 1393 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1394 | `{` |
|         - | 1395 | `	ph7_hashmap_node *pEntry;` |
|         - | 1396 | `	ph7_value *pVal;` |
|         - | 1397 | `	sxi32 rc;` |
|         - | 1398 | `	sxu32 n;` |
|      7355 | 1399 | `	if( pSrc == pDest ){` |
|         - | 1400 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1401 | `		 * Unlike the zend engine.` |
|         - | 1402 | `		 */` |
|       ! 0 | 1403 | `		return SXRET_OK;` |
|         - | 1404 | `	}` |
|         - | 1405 | `	/* Point to the first inserted entry in the source */` |
|      7355 | 1406 | `	pEntry = pSrc->pFirst;` |
|         - | 1407 | `	/* Perform the duplication */` |
|     14989 | 1408 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1409 | `		/* Extract the node value */` |
|      7639 | 1410 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7639 | 1411 | `		if( pVal ){` |
|      7639 | 1412 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      3822 | 1413 | `		}else{` |
|       ! 0 | 1414 | `			rc = SXRET_OK;` |
|         - | 1415 | `		}` |
|      7639 | 1416 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1417 | `			return rc;` |
|         - | 1418 | `		}` |
|         - | 1419 | `		/* Point to the next entry */` |
|      7639 | 1420 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3822 | 1421 | `	}` |
|      7355 | 1422 | `	return SXRET_OK;` |
|      3680 | 1423 | `}` |
|         - | 1424 | `/*` |
|         - | 1425 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1426 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1427 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1428 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1429 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1430 | ` * this instead of PH7_HashmapDup.` |
|         - | 1431 | ` */` |
|        12 | 1432 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1433 | `{` |
|         - | 1434 | `	ph7_hashmap_node *pEntry;` |
|         - | 1435 | `	ph7_value *pVal;` |
|         - | 1436 | `	sxi32 rc;` |
|         - | 1437 | `	sxu32 n;` |
|        13 | 1438 | `	if( pSrc == pDest ){` |
|       ! 0 | 1439 | `		return SXRET_OK;` |
|         - | 1440 | `	}` |
|        13 | 1441 | `	pEntry = pSrc->pFirst;` |
|       773 | 1442 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1443 | `		/* Extract the node value (resolves foreign references) */` |
|       761 | 1444 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       760 | 1445 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       514 | 1446 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1447 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1448 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1449 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1450 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1451 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1452 | `			pVal = 0;` |
|         2 | 1453 | `		}` |
|       761 | 1454 | `		if( pVal ){` |
|       757 | 1455 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1129 | 1456 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       376 | 1457 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       377 | 1458 | `			}else{` |
|         5 | 1459 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1460 | `			}` |
|       757 | 1461 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1462 | `				return rc;` |
|         - | 1463 | `			}` |
|       378 | 1464 | `		}` |
|         - | 1465 | `		/* Point to the next entry */` |
|       761 | 1466 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       381 | 1467 | `	}` |
|        13 | 1468 | `	return SXRET_OK;` |
|         7 | 1469 | `}` |
|         - | 1470 | `/*` |
|         - | 1471 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1472 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1473 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1474 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1475 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1476 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1477 | ` * iterate-a-snapshot semantic.` |
|         - | 1478 | ` */` |
|        50 | 1479 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1480 | `{` |
|         - | 1481 | `	ph7_foreach_step *pStep;` |
|        53 | 1482 | `	sxi32 nRef = 0;` |
|       103 | 1483 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        53 | 1484 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1485 | `			nRef++;` |
|        21 | 1486 | `		}` |
|        28 | 1487 | `	}` |
|        53 | 1488 | `	return nRef;` |
|         3 | 1489 | `}` |
|         - | 1490 | `/*` |
|         - | 1491 | ` * Copy-on-write separation for arrays.` |
|         - | 1492 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1493 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1494 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1495 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1496 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1497 | ` * on the live map the loop is walking, like php.` |
|         - | 1498 | ` */` |
|    249656 | 1499 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1500 | `{` |
|    249661 | 1501 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1502 | `	ph7_hashmap *pNew;` |
|         - | 1503 | `	ph7_value *pBacking;` |
|         - | 1504 | `	sxu32 nValIdx;` |
|         - | 1505 | `	int bValueInPool;` |
|    249661 | 1506 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    249661 | 1507 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1508 | `		/* Sole owner, no separation needed */` |
|    246831 | 1509 | `		return pMap;` |
|         - | 1510 | `	}` |
|      2835 | 1511 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1512 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1513 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1514 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       131 | 1515 | `		return pMap;` |
|         - | 1516 | `	}` |
|         - | 1517 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1518 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1519 | `	 * frame is popped. */` |
|      2705 | 1520 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2705 | 1521 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2700 | 1522 | `		if( pBacking && pBacking != pValue` |
|      2675 | 1523 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2655 | 1524 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1525 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2655 | 1526 | `			pMap->iRef--;` |
|      2655 | 1527 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1528 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2605 | 1529 | `				pMap->iRef++;` |
|      2605 | 1530 | `				return pMap;` |
|         - | 1531 | `			}` |
|        52 | 1532 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        52 | 1533 | `			if( pNew == 0 ){` |
|       ! 0 | 1534 | `				pMap->iRef++;` |
|       ! 0 | 1535 | `				return pMap;` |
|         - | 1536 | `			}` |
|        52 | 1537 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1538 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1539 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1540 | `				pMap->iRef++;` |
|       ! 0 | 1541 | `				return pMap;` |
|         - | 1542 | `			}` |
|        52 | 1543 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        52 | 1544 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1545 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1546 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1547 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1548 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1549 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1550 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1551 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        52 | 1552 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        52 | 1553 | `			if( pBacking ){` |
|        52 | 1554 | `				pBacking->x.pOther = pNew;` |
|        25 | 1555 | `			}` |
|         - | 1556 | `			/* Update the stack value to match */` |
|        52 | 1557 | `			pValue->x.pOther = pNew;` |
|        52 | 1558 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        52 | 1559 | `			return pNew;` |
|         - | 1560 | `		}` |
|        25 | 1561 | `	}` |
|         - | 1562 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1563 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1564 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1565 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1566 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1567 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1568 | `	 * pBacking in the backing-variable branch above). */` |
|        53 | 1569 | `	nValIdx = pValue->nIdx;` |
|        78 | 1570 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        50 | 1571 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        53 | 1572 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        53 | 1573 | `	if( pNew == 0 ){` |
|         - | 1574 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1575 | `		return pMap;` |
|         - | 1576 | `	}` |
|        53 | 1577 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1578 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1579 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1580 | `		return pMap;` |
|         - | 1581 | `	}` |
|        53 | 1582 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        53 | 1583 | `	pMap->iRef--;` |
|        53 | 1584 | `	if( bValueInPool ){` |
|         - | 1585 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        53 | 1586 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        53 | 1587 | `		if( pValue == 0 ){` |
|       ! 0 | 1588 | `			return pNew;` |
|         - | 1589 | `		}` |
|        25 | 1590 | `	}` |
|        53 | 1591 | `	pValue->x.pOther = pNew;` |
|        53 | 1592 | `	return pNew;` |
|    124833 | 1593 | `}` |
|         - | 1594 | `/*` |
|         - | 1595 | ` * Perform the union of two hashmaps.` |
|         - | 1596 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1597 | ` * with a variable holding an array as follows:` |
|         - | 1598 | ` * <?php` |
|         - | 1599 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1600 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1601 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1602 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1603 | ` * var_dump($c);` |
|         - | 1604 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1605 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1606 | ` * var_dump($c);` |
|         - | 1607 | ` * ?>` |
|         - | 1608 | ` * When executed, this script will print the following:` |
|         - | 1609 | ` * Union of $a and $b:` |
|         - | 1610 | ` * array(3) {` |
|         - | 1611 | ` *  ["a"]=>` |
|         - | 1612 | ` *  string(5) "apple"` |
|         - | 1613 | ` *  ["b"]=>` |
|         - | 1614 | ` * string(6) "banana"` |
|         - | 1615 | ` *  ["c"]=>` |
|         - | 1616 | ` * string(6) "cherry"` |
|         - | 1617 | ` * }` |
|         - | 1618 | ` * Union of $b and $a:` |
|         - | 1619 | ` * array(3) {` |
|         - | 1620 | ` * ["a"]=>` |
|         - | 1621 | ` * string(4) "pear"` |
|         - | 1622 | ` * ["b"]=>` |
|         - | 1623 | ` * string(10) "strawberry"` |
|         - | 1624 | ` * ["c"]=>` |
|         - | 1625 | ` * string(6) "cherry"` |
|         - | 1626 | ` * }` |
|         - | 1627 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1628 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1629 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1630 | ` */` |
|      3822 | 1631 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1632 | `{` |
|         - | 1633 | `	ph7_hashmap_node *pEntry;` |
|      3827 | 1634 | `	sxi32 rc = SXRET_OK;` |
|         - | 1635 | `	ph7_value *pObj;` |
|         - | 1636 | `	sxu32 n;` |
|      3827 | 1637 | `	if( pLeft == pRight ){` |
|         - | 1638 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1639 | `		 * Unlike the zend engine.` |
|         - | 1640 | `		 */` |
|       ! 0 | 1641 | `		return SXRET_OK;` |
|         - | 1642 | `	}` |
|         - | 1643 | `	/* Perform the union */` |
|      3827 | 1644 | `	pEntry = pRight->pFirst;` |
|      3867 | 1645 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1646 | `		/* Make sure the given key does not exists in the left array */` |
|        44 | 1647 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1648 | `			/* BLOB key */` |
|        23 | 1649 | `			if( SXRET_OK !=` |
|        20 | 1650 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        19 | 1651 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        19 | 1652 | `					if( pObj ){` |
|        19 | 1653 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1654 | `						/* Perform the insertion */` |
|        19 | 1655 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1656 | `							&sSafeVal,0,FALSE);` |
|        19 | 1657 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1658 | `							return rc;` |
|         - | 1659 | `						}` |
|         8 | 1660 | `					}` |
|         8 | 1661 | `			}` |
|        13 | 1662 | `		}else{` |
|         - | 1663 | `			/* INT key */` |
|        22 | 1664 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        13 | 1665 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        13 | 1666 | `				if( pObj ){` |
|        13 | 1667 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1668 | `					/* Perform the insertion */` |
|        13 | 1669 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        13 | 1670 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1671 | `						return rc;` |
|         - | 1672 | `					}` |
|         6 | 1673 | `				}` |
|         6 | 1674 | `			}` |
|         - | 1675 | `		}` |
|         - | 1676 | `		/* Point to the next entry */` |
|        44 | 1677 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1678 | `	}` |
|      3827 | 1679 | `	return SXRET_OK;` |
|      1916 | 1680 | `}` |
|         - | 1681 | `/*` |
|         - | 1682 | ` * Allocate a new hashmap.` |
|         - | 1683 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1684 | ` */` |
|    143666 | 1685 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1686 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1687 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1688 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1689 | `	)` |
|         5 | 1690 | `{` |
|         - | 1691 | `	ph7_hashmap *pMap;` |
|         - | 1692 | `	/* Allocate a new instance */` |
|    143671 | 1693 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    143671 | 1694 | `	if( pMap == 0 ){` |
|       ! 0 | 1695 | `		return 0;` |
|         - | 1696 | `	}` |
|         - | 1697 | `	/* Zero the structure */` |
|    143671 | 1698 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1699 | `	/* Fill in the structure */` |
|    143671 | 1700 | `	pMap->pVm = &(*pVm);` |
|    143671 | 1701 | `	pMap->iRef = 1;` |
|         - | 1702 | `	/* Default hash functions */` |
|    143671 | 1703 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    143671 | 1704 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    143671 | 1705 | `	return pMap;` |
|     71838 | 1706 | `}` |
|         - | 1707 | `/*` |
|         - | 1708 | ` * Install superglobals in the given virtual machine.` |
|         - | 1709 | ` * Note on superglobals.` |
|         - | 1710 | ` *  According to the PHP language reference manual.` |
|         - | 1711 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1712 | `*   Description` |
|         - | 1713 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1714 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1715 | `*   global $variable; to access them within functions or methods.` |
|         - | 1716 | `*   These superglobal variables are:` |
|         - | 1717 | `*    $GLOBALS` |
|         - | 1718 | `*    $_SERVER` |
|         - | 1719 | `*    $_GET` |
|         - | 1720 | `*    $_POST` |
|         - | 1721 | `*    $_FILES` |
|         - | 1722 | `*    $_COOKIE` |
|         - | 1723 | `*    $_SESSION` |
|         - | 1724 | `*    $_REQUEST` |
|         - | 1725 | `*    $_ENV` |
|         - | 1726 | `*/` |
|      3436 | 1727 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1728 | `{` |
|         - | 1729 | `	static const char * azSuper[] = {` |
|         - | 1730 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1731 | `		"_GET",      /* $_GET */` |
|         - | 1732 | `		"_POST",     /* $_POST */` |
|         - | 1733 | `		"_FILES",    /* $_FILES */` |
|         - | 1734 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1735 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1736 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1737 | `		"_ENV",      /* $_ENV */` |
|         - | 1738 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1739 | `		"argv"       /* $argv */` |
|         - | 1740 | `	};` |
|         - | 1741 | `	ph7_hashmap *pMap;` |
|         - | 1742 | `	ph7_value *pObj;` |
|         - | 1743 | `	SyString *pFile;` |
|         - | 1744 | `	sxi32 rc;` |
|         - | 1745 | `	sxu32 n;` |
|         - | 1746 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3441 | 1747 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3441 | 1748 | `	if( pMap == 0 ){` |
|       ! 0 | 1749 | `		return SXERR_MEM;` |
|         - | 1750 | `	}` |
|      3441 | 1751 | `	pVm->pGlobal = pMap;` |
|         - | 1752 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3441 | 1753 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3441 | 1754 | `	if( pObj == 0 ){` |
|       ! 0 | 1755 | `		return SXERR_MEM;` |
|         - | 1756 | `	}` |
|      3441 | 1757 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1758 | `	/* Record object index */` |
|      3441 | 1759 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1760 | `	/* Install the special $GLOBALS array */` |
|      3441 | 1761 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3441 | 1762 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1763 | `		return rc;` |
|         - | 1764 | `	}` |
|         - | 1765 | `	/* Install superglobals now */` |
|     37801 | 1766 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1767 | `		ph7_value *pSuper;` |
|         - | 1768 | `		/* Request an empty array */` |
|     34365 | 1769 | `		pSuper = ph7_new_array(&(*pVm));` |
|     34365 | 1770 | `		if( pSuper == 0 ){` |
|       ! 0 | 1771 | `			return SXERR_MEM;` |
|         - | 1772 | `		}` |
|         - | 1773 | `		/* Install */` |
|     34365 | 1774 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     34365 | 1775 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1776 | `			return rc;` |
|         - | 1777 | `		}` |
|         - | 1778 | `		/* Release the value now it have been installed */` |
|     34365 | 1779 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17185 | 1780 | `	}` |
|         - | 1781 | `	/* Set some $_SERVER entries */` |
|      3441 | 1782 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1783 | `	/*` |
|         - | 1784 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1785 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1786 | `	 */` |
|      6877 | 1787 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1788 | `		"SCRIPT_FILENAME",` |
|      1718 | 1789 | `		pFile ? pFile->zString : ":Memory:",` |
|      3436 | 1790 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1791 | `		);` |
|         - | 1792 | `	/* All done,all super-global are installed now */` |
|      3441 | 1793 | `	return SXRET_OK;` |
|      1723 | 1794 | `}` |
|         - | 1795 | `/*` |
|         - | 1796 | ` * Release a hashmap.` |
|         - | 1797 | ` */` |
|     95650 | 1798 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1799 | `{` |
|         - | 1800 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     95655 | 1801 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1802 | `	sxu32 n;` |
|     95655 | 1803 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1804 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1805 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1806 | `		return SXRET_OK;` |
|         - | 1807 | `	}` |
|     95655 | 1808 | `	if( pMap->pActiveSteps ){` |
|         - | 1809 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1810 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1811 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1812 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1813 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1814 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1815 | `		ph7_foreach_step *pStep;` |
|        17 | 1816 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1817 | `			pStep->pCursor = 0;` |
|         5 | 1818 | `		}` |
|         4 | 1819 | `	}` |
|         - | 1820 | `	/* Start the release process */` |
|     95655 | 1821 | `	n = 0;` |
|     95655 | 1822 | `	pEntry = pMap->pFirst;` |
|   1919681 | 1823 | `	for(;;){` |
|   3839368 | 1824 | `		if( n >= pMap->nEntry ){` |
|     95655 | 1825 | `			break;` |
|         - | 1826 | `		}` |
|   3743718 | 1827 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1828 | `		/* Remove the reference from the foreign table */` |
|   3743718 | 1829 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3743718 | 1830 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1831 | `			/* Restore the ph7_value to the free list */` |
|   3743658 | 1832 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1871826 | 1833 | `		}` |
|         - | 1834 | `		/* Release the node */` |
|   3743718 | 1835 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    109982 | 1836 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     54988 | 1837 | `		}` |
|   3743718 | 1838 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1839 | `		/* Point to the next entry */` |
|   3743718 | 1840 | `		pEntry = pNext;` |
|   3743718 | 1841 | `		n++;` |
|         5 | 1842 | `	}` |
|     95655 | 1843 | `	if( pMap->nEntry > 0 ){` |
|         - | 1844 | `		/* Release the hash bucket */` |
|     69314 | 1845 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     34654 | 1846 | `	}` |
|     95655 | 1847 | `	if( FreeDS ){` |
|         - | 1848 | `		/* Free the whole instance */` |
|     95629 | 1849 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     47817 | 1850 | `	}else{` |
|         - | 1851 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1852 | `		pMap->apBucket = 0;` |
|        28 | 1853 | `		pMap->iNextIdx = 0;` |
|        28 | 1854 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1855 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1856 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1857 | `	}` |
|     95655 | 1858 | `	return SXRET_OK;` |
|     47830 | 1859 | `}` |
|         - | 1860 | `/*` |
|         - | 1861 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1862 | ` * If the count reaches zero which mean no more variables` |
|         - | 1863 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1864 | ` */` |
|    888934 | 1865 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1866 | `{` |
|    888939 | 1867 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1868 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    888939 | 1869 | `	pMap->iRef--;` |
|    888939 | 1870 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     95609 | 1871 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     47802 | 1872 | `	}` |
|    888939 | 1873 | `}` |
|         - | 1874 | `/*` |
|         - | 1875 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1876 | ` * Write a pointer to the target node on success.` |
|         - | 1877 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1878 | ` */` |
|    156278 | 1879 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1880 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1881 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1882 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1883 | `	)` |
|         5 | 1884 | `{` |
|         - | 1885 | `	sxi32 rc;` |
|    156283 | 1886 | `	if( pMap->nEntry < 1 ){` |
|         - | 1887 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1888 | `		 */` |
|       225 | 1889 | `		return SXERR_NOTFOUND;` |
|         - | 1890 | `	}` |
|    156063 | 1891 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    156063 | 1892 | `	return rc;` |
|     78144 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1896 | ` * hashmap.` |
|         - | 1897 | ` * If a node with the given key already exists in the database` |
|         - | 1898 | ` * then this function overwrite the old value.` |
|         - | 1899 | ` */` |
|   3080999 | 1900 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1901 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1902 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1903 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1904 | `	)` |
|         5 | 1905 | `{` |
|         - | 1906 | `	sxi32 rc;` |
|         - | 1907 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1908 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1909 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1910 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   3081004 | 1911 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   3081004 | 1912 | `	return rc;` |
|         5 | 1913 | `}` |
|         - | 1914 | `/*` |
|         - | 1915 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1916 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1917 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1918 | ` * This is the same routine that backs array_merge().` |
|         - | 1919 | ` */` |
|       658 | 1920 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1921 | `{` |
|       659 | 1922 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1923 | `}` |
|         - | 1924 | `/*` |
|         - | 1925 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1926 | ` * hashmap.` |
|         - | 1927 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1928 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1929 | ` * The insertion by reference is triggered when the following` |
|         - | 1930 | ` * expression is encountered.` |
|         - | 1931 | ` * $var = 10;` |
|         - | 1932 | ` *  $a = array(&var);` |
|         - | 1933 | ` * OR` |
|         - | 1934 | ` *  $a[] =& $var;` |
|         - | 1935 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1936 | ` * over it's contents.` |
|         - | 1937 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1938 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1939 | ` * Example:` |
|         - | 1940 | ` *  $var = 10;` |
|         - | 1941 | ` *  $a[] =& $var;` |
|         - | 1942 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1943 | ` *  //Unset the foreign ph7_value now` |
|         - | 1944 | ` *  unset($var);` |
|         - | 1945 | ` *  echo count($a); //0` |
|         - | 1946 | ` * Note that this is a PH7 eXtension.` |
|         - | 1947 | ` * Refer to the official documentation for more information.` |
|         - | 1948 | ` * If a node with the given key already exists in the database` |
|         - | 1949 | ` * then this function overwrite the old value.` |
|         - | 1950 | ` */` |
|     47020 | 1951 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1952 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1953 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1954 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1955 | `	)` |
|         5 | 1956 | `{` |
|         - | 1957 | `	sxi32 rc;` |
|     47025 | 1958 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1959 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1960 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1961 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1962 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1963 | `		return PH7_ABORT;` |
|         - | 1964 | `	}` |
|     47025 | 1965 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     47025 | 1966 | `	return rc;` |
|     23515 | 1967 | `}` |
|         - | 1968 | `/*` |
|         - | 1969 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1970 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1971 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1972 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1973 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1974 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1975 | ` */` |
|     23576 | 1976 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1977 | `{` |
|     23581 | 1978 | `	pStep->pCursor = pMap->pFirst;` |
|     23581 | 1979 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     23581 | 1980 | `	pMap->pActiveSteps = pStep;` |
|     23581 | 1981 | `}` |
|         - | 1982 | `/*` |
|         - | 1983 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1984 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1985 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1986 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1987 | ` */` |
|     23400 | 1988 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1989 | `{` |
|     23405 | 1990 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     23405 | 1991 | `	while( *ppLink ){` |
|     23405 | 1992 | `		if( *ppLink == pStep ){` |
|     23405 | 1993 | `			*ppLink = pStep->pNextActive;` |
|     23405 | 1994 | `			pStep->pNextActive = 0;` |
|     23405 | 1995 | `			return;` |
|         - | 1996 | `		}` |
|       ! 0 | 1997 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1998 | `	}` |
|     11705 | 1999 | `}` |
|         - | 2000 | `/*` |
|         - | 2001 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 2002 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 2003 | ` * return NULL.` |
|         - | 2004 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 2005 | ` */` |
|        64 | 2006 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 2007 | `{` |
|        65 | 2008 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 2009 | `	if( pCur == 0 ){` |
|         - | 2010 | `		/* End of the list,return null */` |
|        27 | 2011 | `		return 0;` |
|         - | 2012 | `	}` |
|         - | 2013 | `	/* Advance the node cursor */` |
|        39 | 2014 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 2015 | `	return pCur;` |
|        33 | 2016 | `}` |
|         - | 2017 | `/*` |
|         - | 2018 | ` * Extract a node value.` |
|         - | 2019 | ` */` |
|    632342 | 2020 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2021 | `{` |
|    632347 | 2022 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    632347 | 2023 | `	if( pEntry ){` |
|    632347 | 2024 | `		if( bStore ){` |
|    243749 | 2025 | `			PH7_MemObjStore(pEntry,pValue);` |
|    121877 | 2026 | `		}else{` |
|    388603 | 2027 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2028 | `		}` |
|    315919 | 2029 | `	}else{` |
|       ! 0 | 2030 | `		PH7_MemObjRelease(pValue);` |
|         - | 2031 | `	}` |
|    632347 | 2032 | `}` |
|         - | 2033 | `/*` |
|         - | 2034 | ` * Extract a node key.` |
|         - | 2035 | ` */` |
|    168836 | 2036 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2037 | `{` |
|         - | 2038 | `	/* Fill with the current key */` |
|    168841 | 2039 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    163009 | 2040 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2041 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2042 | `		}` |
|    163009 | 2043 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    163009 | 2044 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     81507 | 2045 | `	}else{` |
|      5837 | 2046 | `		SyBlobReset(&pKey->sBlob);` |
|      5837 | 2047 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5837 | 2048 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2049 | `	}` |
|    168841 | 2050 | `}` |
|         - | 2051 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2052 | `/*` |
|         - | 2053 | ` * Store the address of nodes value in the given container.` |
|         - | 2054 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2055 | ` * defined in 'builtin.c' for more information.` |
|         - | 2056 | ` */` |
|        14 | 2057 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 2058 | `{` |
|        15 | 2059 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2060 | `	ph7_value *pValue;` |
|         - | 2061 | `	sxu32 n;` |
|         - | 2062 | `	/* Initialize the container */` |
|        15 | 2063 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        41 | 2064 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2065 | `		/* Extract node value */` |
|        27 | 2066 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        27 | 2067 | `		if( pValue ){` |
|        27 | 2068 | `			SySetPut(pOut,(const void *)&pValue);` |
|        13 | 2069 | `		}` |
|         - | 2070 | `		/* Point to the next entry */` |
|        27 | 2071 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        14 | 2072 | `	}` |
|         - | 2073 | `	/* Total inserted entries */` |
|        15 | 2074 | `	return (int)SySetUsed(pOut);` |
|         1 | 2075 | `}` |
|         - | 2076 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2077 | `/* SPDX-SnippetBegin */` |
|         - | 2078 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 2079 | `/* SPDX-License-Identifier: blessing */` |
|         - | 2080 | `/*` |
|         - | 2081 | ` * Merge sort.` |
|         - | 2082 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 2083 | ` * Status: Public domain` |
|         - | 2084 | ` */` |
|         - | 2085 | `/* Node comparison callback signature */` |
|         - | 2086 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 2087 | `/*` |
|         - | 2088 | `** Inputs:` |
|         - | 2089 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2090 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2091 | `**   cmp:     A pointer to the comparison function.` |
|         - | 2092 | `**` |
|         - | 2093 | `** Return Value:` |
|         - | 2094 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 2095 | `**   of both a and b.` |
|         - | 2096 | `**` |
|         - | 2097 | `** Side effects:` |
|         - | 2098 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 2099 | `**   changed.` |
|         - | 2100 | `*/` |
|     52702 | 2101 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2102 | `{` |
|         - | 2103 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2104 | `    /* Prevent compiler warning */` |
|     52707 | 2105 | `	result.pNext = result.pPrev = 0;` |
|     52707 | 2106 | `	pTail = &result;` |
|    132667 | 2107 | `	while( pA && pB ){` |
|     79965 | 2108 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     62619 | 2109 | `			pTail->pPrev = pA;` |
|     62619 | 2110 | `			pA->pNext = pTail;` |
|     62619 | 2111 | `			pTail = pA;` |
|     62619 | 2112 | `			pA = pA->pPrev;` |
|     31515 | 2113 | `		}else{` |
|     17351 | 2114 | `			pTail->pPrev = pB;` |
|     17351 | 2115 | `			pB->pNext = pTail;` |
|     17351 | 2116 | `			pTail = pB;` |
|     17351 | 2117 | `			pB = pB->pPrev;` |
|         - | 2118 | `		}` |
|         5 | 2119 | `	}` |
|     52707 | 2120 | `	if( pA ){` |
|      4456 | 2121 | `		pTail->pPrev = pA;` |
|      4456 | 2122 | `		pA->pNext = pTail;` |
|     50305 | 2123 | `	}else if( pB ){` |
|     47954 | 2124 | `		pTail->pPrev = pB;` |
|     47954 | 2125 | `		pB->pNext = pTail;` |
|     24156 | 2126 | `	}else{` |
|       307 | 2127 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2128 | `	}` |
|     52707 | 2129 | `	return result.pPrev;` |
|         5 | 2130 | `}` |
|         - | 2131 | `/*` |
|         - | 2132 | `** Inputs:` |
|         - | 2133 | `**   Map:       Input hashmap` |
|         - | 2134 | `**   cmp:       A comparison function.` |
|         - | 2135 | `**` |
|         - | 2136 | `** Return Value:` |
|         - | 2137 | `**   Sorted hashmap.` |
|         - | 2138 | `**` |
|         - | 2139 | `** Side effects:` |
|         - | 2140 | `**   The "next" pointers for elements in list are changed.` |
|         - | 2141 | `*/` |
|         - | 2142 | `#define N_SORT_BUCKET  32` |
|      1222 | 2143 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2144 | `{` |
|         - | 2145 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2146 | `	sxu32 i;` |
|      1227 | 2147 | `	SyZero(a,sizeof(a));` |
|         - | 2148 | `	/* Point to the first inserted entry */` |
|      1227 | 2149 | `	pIn = pMap->pFirst;` |
|     18441 | 2150 | `	while( pIn ){` |
|     17219 | 2151 | `		p = pIn;` |
|     17219 | 2152 | `		pIn = p->pPrev;` |
|     17219 | 2153 | `		p->pPrev = 0;` |
|     32039 | 2154 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     32039 | 2155 | `			if( a[i]==0 ){` |
|     17219 | 2156 | `				a[i] = p;` |
|     17219 | 2157 | `				break;` |
|       ! 0 | 2158 | `			}else{` |
|     14825 | 2159 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     14825 | 2160 | `				a[i] = 0;` |
|         - | 2161 | `			}` |
|      7415 | 2162 | `		}` |
|     17219 | 2163 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2164 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2165 | `			 * But that is impossible.` |
|         - | 2166 | `			 */` |
|       ! 0 | 2167 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2168 | `		}` |
|         5 | 2169 | `	}` |
|      1227 | 2170 | `	p = a[0];` |
|     39109 | 2171 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|         - | 2172 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|         - | 2173 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|         - | 2174 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|         - | 2175 | `		 * keep their original order. Passing p (the later elements) on the left` |
|         - | 2176 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|         - | 2177 | `		 * the front). */` |
|     37887 | 2178 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     18946 | 2179 | `	}` |
|      1227 | 2180 | `	p->pNext = 0;` |
|         - | 2181 | `	/* Reflect the change */` |
|      1227 | 2182 | `	pMap->pFirst = p;` |
|         - | 2183 | `	/* Reset the loop cursor */` |
|      1227 | 2184 | `	pMap->pCur = pMap->pFirst;` |
|      1227 | 2185 | `	return SXRET_OK;` |
|         5 | 2186 | `}` |
|         - | 2187 | `/* SPDX-SnippetEnd */` |
|         - | 2188 | `/*` |
|         - | 2189 | ` * Node comparison callback.` |
|         - | 2190 | ` * used-by: [sort(),asort(),...]` |
|         - | 2191 | ` */` |
|         - | 2192 | `/*` |
|         - | 2193 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|         - | 2194 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|         - | 2195 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|         - | 2196 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|         - | 2197 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|         - | 2198 | ` * string cast); the caller owns and releases them.` |
|         - | 2199 | ` */` |
|       250 | 2200 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|         1 | 2201 | `{` |
|         - | 2202 | `	sxi32 rc;` |
|       251 | 2203 | `	if( base == 1 ){` |
|         - | 2204 | `		/* SORT_NUMERIC */` |
|        59 | 2205 | `		PH7_MemObjToNumeric(pA);` |
|        59 | 2206 | `		PH7_MemObjToNumeric(pB);` |
|        59 | 2207 | `		rc = PH7_MemObjCmp(pA,pB,FALSE,0);` |
|        30 | 2208 | `	}else{` |
|         - | 2209 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|         - | 2210 | `		const char *zA,*zB;` |
|         - | 2211 | `		sxu32 nA,nB,nMin,i;` |
|       193 | 2212 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pA); }` |
|       193 | 2213 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pB); }` |
|       193 | 2214 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
|       193 | 2215 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
|       193 | 2216 | `		nA = SyBlobLength(&pA->sBlob);` |
|       193 | 2217 | `		nB = SyBlobLength(&pB->sBlob);` |
|       193 | 2218 | `		if( base == 6 ){` |
|        29 | 2219 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|        15 | 2220 | `		}else{` |
|         - | 2221 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|       165 | 2222 | `			nMin = nA < nB ? nA : nB;` |
|       165 | 2223 | `			rc = 0;` |
|       241 | 2224 | `			for( i = 0 ; i < nMin ; ++i ){` |
|       187 | 2225 | `				int ca = (unsigned char)zA[i];` |
|       187 | 2226 | `				int cb = (unsigned char)zB[i];` |
|       187 | 2227 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|       187 | 2228 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|        39 | 2229 | `			}` |
|       165 | 2230 | `			if( rc == 0 ){` |
|        55 | 2231 | `				if( nA < nB ) rc = -1;` |
|        43 | 2232 | `				else if( nA > nB ) rc = 1;` |
|        27 | 2233 | `			}` |
|         - | 2234 | `		}` |
|         - | 2235 | `	}` |
|       251 | 2236 | `	return rc;` |
|         1 | 2237 | `}` |
|         - | 2238 | `/*` |
|         - | 2239 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|         - | 2240 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|         - | 2241 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|         - | 2242 | ` */` |
|       116 | 2243 | `static int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|         1 | 2244 | `{` |
|         - | 2245 | `	ph7_value sA,sB;` |
|         - | 2246 | `	sxi32 rc;` |
|       117 | 2247 | `	PH7_MemObjInit(pVm,&sA);` |
|       117 | 2248 | `	PH7_MemObjInit(pVm,&sB);` |
|       117 | 2249 | `	PH7_MemObjStore(pA,&sA);` |
|       117 | 2250 | `	PH7_MemObjStore(pB,&sB);` |
|       117 | 2251 | `	if( base == 0 ){` |
|        11 | 2252 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|         6 | 2253 | `	}else{` |
|       107 | 2254 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|         - | 2255 | `	}` |
|       117 | 2256 | `	PH7_MemObjRelease(&sA);` |
|       117 | 2257 | `	PH7_MemObjRelease(&sB);` |
|       117 | 2258 | `	return rc == 0;` |
|         1 | 2259 | `}` |
|         - | 2260 | `/*` |
|         - | 2261 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|         - | 2262 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|         - | 2263 | ` */` |
|       126 | 2264 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2265 | `{` |
|         - | 2266 | `	ph7_value sA,sB;` |
|       127 | 2267 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|       127 | 2268 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2269 | `	sxi32 rc;` |
|       127 | 2270 | `	if( base == 0 ){` |
|         - | 2271 | `		/* SORT_REGULAR */` |
|       ! 0 | 2272 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2273 | `	}` |
|       127 | 2274 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       127 | 2275 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       127 | 2276 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       127 | 2277 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       127 | 2278 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|       127 | 2279 | `	PH7_MemObjRelease(&sA);` |
|       127 | 2280 | `	PH7_MemObjRelease(&sB);` |
|       127 | 2281 | `	return rc;` |
|        64 | 2282 | `}` |
|     79585 | 2283 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2284 | `{` |
|     79590 | 2285 | `	if( pCmpData == 0 ){` |
|         - | 2286 | `		/* SORT_REGULAR fast path */` |
|     79502 | 2287 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2288 | `	}` |
|        89 | 2289 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     39669 | 2290 | `}` |
|         - | 2291 | `/*` |
|         - | 2292 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2293 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2294 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2295 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2296 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2297 | ` */` |
|         - | 2298 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2299 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2300 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        44 | 2301 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2302 | `{` |
|        46 | 2303 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        46 | 2304 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        46 | 2305 | `	if( rc == 0 ){` |
|       ! 0 | 2306 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2307 | `	}` |
|        46 | 2308 | `	return rc;` |
|         2 | 2309 | `}` |
|        66 | 2310 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2311 | `{` |
|         - | 2312 | `	sxi32 rc;` |
|        68 | 2313 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2314 | `		/* Perform a string comparison */` |
|        44 | 2315 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        28 | 2316 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        16 | 2317 | `	}else{` |
|         - | 2318 | `		SyString sStr;` |
|        39 | 2319 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2320 | `		int bNum = 1;` |
|        39 | 2321 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        13 | 2322 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        13 | 2323 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        13 | 2324 | `				bNum = 0;` |
|         7 | 2325 | `			}else{` |
|       ! 0 | 2326 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2327 | `			}` |
|         7 | 2328 | `		}else{` |
|        27 | 2329 | `			iA = pA->xKey.iKey;` |
|         - | 2330 | `		}` |
|        39 | 2331 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         5 | 2332 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         5 | 2333 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         5 | 2334 | `				bNum = 0;` |
|         3 | 2335 | `			}else{` |
|       ! 0 | 2336 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2337 | `			}` |
|         3 | 2338 | `		}else{` |
|        35 | 2339 | `			iB = pB->xKey.iKey;` |
|         - | 2340 | `		}` |
|        39 | 2341 | `		if( bNum ){` |
|        23 | 2342 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2343 | `		}else{` |
|         - | 2344 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2345 | `			char zNumA[24],zNumB[24];` |
|         - | 2346 | `			SyString sA,sB;` |
|        17 | 2347 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         5 | 2348 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         5 | 2349 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         3 | 2350 | `			}else{` |
|        13 | 2351 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2352 | `			}` |
|        17 | 2353 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        13 | 2354 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        13 | 2355 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         7 | 2356 | `			}else{` |
|         5 | 2357 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2358 | `			}` |
|        17 | 2359 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2360 | `		}` |
|         - | 2361 | `	}` |
|        68 | 2362 | `	return rc;` |
|         2 | 2363 | `}` |
|         - | 2364 | `/*` |
|         - | 2365 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|         - | 2366 | ` * -> string) for a flag-aware key comparison.` |
|         - | 2367 | ` */` |
|        36 | 2368 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|         1 | 2369 | `{` |
|        37 | 2370 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|        21 | 2371 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|        11 | 2372 | `	}else{` |
|        17 | 2373 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|        25 | 2374 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|         8 | 2375 | `			SyBlobLength(&pNode->xKey.sKey));` |
|         - | 2376 | `	}` |
|        37 | 2377 | `}` |
|         - | 2378 | `/*` |
|         - | 2379 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|         - | 2380 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|         - | 2381 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|         - | 2382 | ` */` |
|        18 | 2383 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2384 | `{` |
|         - | 2385 | `	ph7_value sA,sB;` |
|        19 | 2386 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|        19 | 2387 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2388 | `	sxi32 rc;` |
|        19 | 2389 | `	if( base == 0 ){` |
|       ! 0 | 2390 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2391 | `	}` |
|        19 | 2392 | `	HashmapNodeKeyToValue(pA,&sA);` |
|        19 | 2393 | `	HashmapNodeKeyToValue(pB,&sB);` |
|        19 | 2394 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|        19 | 2395 | `	PH7_MemObjRelease(&sA);` |
|        19 | 2396 | `	PH7_MemObjRelease(&sB);` |
|        19 | 2397 | `	return rc;` |
|        10 | 2398 | `}` |
|         - | 2399 | `/*` |
|         - | 2400 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2401 | ` * used-by: [ksort()]` |
|         - | 2402 | ` */` |
|        66 | 2403 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2404 | `{` |
|        68 | 2405 | `	if( pCmpData == 0 ){` |
|        54 | 2406 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2407 | `	}` |
|        15 | 2408 | `	return HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        35 | 2409 | `}` |
|         - | 2410 | `/*` |
|         - | 2411 | ` * Node comparison callback.` |
|         - | 2412 | ` * Used by: [rsort(),arsort()];` |
|         - | 2413 | ` */` |
|        96 | 2414 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2415 | `{` |
|        97 | 2416 | `	if( pCmpData == 0 ){` |
|         - | 2417 | `		/* SORT_REGULAR fast path, reversed */` |
|        59 | 2418 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2419 | `	}` |
|        39 | 2420 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        49 | 2421 | `}` |
|         - | 2422 | `/*` |
|         - | 2423 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2424 | ` * used-by: [usort(),uasort()]` |
|         - | 2425 | ` */` |
|       170 | 2426 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2427 | `{` |
|         - | 2428 | `	ph7_value sResult,*pCallback;` |
|         - | 2429 | `	ph7_value *pV1,*pV2;` |
|         - | 2430 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2431 | `	sxi32 rc;` |
|         - | 2432 | `	/* Point to the desired callback */` |
|       173 | 2433 | `	pCallback = (ph7_value *)pCmpData;` |
|       173 | 2434 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2435 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2436 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|        14 | 2437 | `		return 0;` |
|         - | 2438 | `	}` |
|         - | 2439 | `	/* initialize the result value */` |
|       161 | 2440 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2441 | `	/* Extract nodes values */` |
|       161 | 2442 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       161 | 2443 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       161 | 2444 | `	apArg[0] = pV1;` |
|       161 | 2445 | `	apArg[1] = pV2;` |
|         - | 2446 | `	/* Invoke the callback */` |
|       161 | 2447 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       161 | 2448 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2449 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2450 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2451 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2452 | `		rc = 0;` |
|       156 | 2453 | `	}else if( rc != SXRET_OK ){` |
|         - | 2454 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2455 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2456 | `	}else{` |
|         - | 2457 | `		/* Extract callback result */` |
|       152 | 2458 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2459 | `			/* Perform an int cast */` |
|       ! 0 | 2460 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2461 | `		}` |
|       152 | 2462 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2463 | `	}` |
|       161 | 2464 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2465 | `	/* Callback result */` |
|       161 | 2466 | `	return rc;` |
|        88 | 2467 | `}` |
|         - | 2468 | `/*` |
|         - | 2469 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2470 | ` * used-by: [krsort()]` |
|         - | 2471 | ` */` |
|        18 | 2472 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2473 | `{` |
|        19 | 2474 | `	if( pCmpData == 0 ){` |
|        15 | 2475 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         - | 2476 | `	}` |
|         5 | 2477 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        10 | 2478 | `}` |
|         - | 2479 | `/*` |
|         - | 2480 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2481 | ` * used-by: [uksort()]` |
|         - | 2482 | ` */` |
|         6 | 2483 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2484 | `{` |
|         - | 2485 | `	ph7_value sResult,*pCallback;` |
|         - | 2486 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2487 | `	ph7_value sK1,sK2;` |
|         - | 2488 | `	sxi32 rc;` |
|         - | 2489 | `	/* Point to the desired callback */` |
|         7 | 2490 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2491 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2492 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2493 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2494 | `		return 0;` |
|         - | 2495 | `	}` |
|         - | 2496 | `	/* initialize the result value */` |
|         7 | 2497 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2498 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2499 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2500 | `	/* Extract nodes keys */` |
|         7 | 2501 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2502 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2503 | `	apArg[0] = &sK1;` |
|         7 | 2504 | `	apArg[1] = &sK2;` |
|         - | 2505 | `	/* Mark keys as constants */` |
|         7 | 2506 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2507 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2508 | `	/* Invoke the callback */` |
|         7 | 2509 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2510 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2511 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2512 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2513 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2514 | `		rc = 0;` |
|         7 | 2515 | `	}else if( rc != SXRET_OK ){` |
|         - | 2516 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2517 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2518 | `	}else{` |
|         - | 2519 | `		/* Extract callback result */` |
|         7 | 2520 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2521 | `			/* Perform an int cast */` |
|       ! 0 | 2522 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2523 | `		}` |
|         7 | 2524 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2525 | `	}` |
|         7 | 2526 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2527 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2528 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2529 | `	/* Callback result */` |
|         7 | 2530 | `	return rc;` |
|         4 | 2531 | `}` |
|         - | 2532 | `/*` |
|         - | 2533 | ` * Node comparison callback: Random node comparison.` |
|         - | 2534 | ` * used-by: [shuffle()]` |
|         - | 2535 | ` */` |
|        19 | 2536 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2537 | `{` |
|         - | 2538 | `	sxu32 n;` |
|        11 | 2539 | `	SXUNUSED(pB); /* cc warning */` |
|        11 | 2540 | `	SXUNUSED(pCmpData);` |
|         - | 2541 | `	/* Grab a random number */` |
|        20 | 2542 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2543 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2544 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2545 | `	 */` |
|        20 | 2546 | `	return n&1 ? 1 : -1;` |
|         1 | 2547 | `}` |
|         - | 2548 | `/*` |
|         - | 2549 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2550 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2551 | ` */` |
|      1146 | 2552 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2553 | `{` |
|         - | 2554 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2555 | `	sxu32 i;` |
|         - | 2556 | `	/* Rehash all entries */` |
|      1151 | 2557 | `	pLast = p = pMap->pFirst;` |
|      1151 | 2558 | `	pMap->iNextIdx = 0;` |
|      1151 | 2559 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      1151 | 2560 | `	i = 0;` |
|      9042 | 2561 | `	for( ;; ){` |
|     18089 | 2562 | `		if( i >= pMap->nEntry ){` |
|      1151 | 2563 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|      1151 | 2564 | `			break;` |
|         - | 2565 | `		}` |
|     16943 | 2566 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2567 | `			/* Do not maintain index association as requested by the PHP specification */` |
|        11 | 2568 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2569 | `			/* Change key type */` |
|        11 | 2570 | `			p->iType = HASHMAP_INT_NODE;` |
|         5 | 2571 | `		}` |
|     16943 | 2572 | `		HashmapRehashIntNode(p);` |
|         - | 2573 | `		/* Point to the next entry */` |
|     16943 | 2574 | `		i++;` |
|     16943 | 2575 | `		pLast = p;` |
|     16943 | 2576 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2577 | `	}` |
|      1151 | 2578 | `}` |
|         - | 2579 | `/*` |
|         - | 2580 | ` * Array functions implementation.` |
|         - | 2581 | ` * Status:` |
|         - | 2582 | ` *  Stable.` |
|         - | 2583 | ` */` |
|         - | 2584 | `/*` |
|         - | 2585 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2586 | ` * Sort an array.` |
|         - | 2587 | ` * Parameters` |
|         - | 2588 | ` *  $array` |
|         - | 2589 | ` *   The input array.` |
|         - | 2590 | ` * $sort_flags` |
|         - | 2591 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2592 | ` *  Sorting type flags:` |
|         - | 2593 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2594 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2595 | ` *   SORT_STRING - compare items as strings` |
|         - | 2596 | ` * Return` |
|         - | 2597 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2598 | ` *` |
|         - | 2599 | ` */` |
|      1110 | 2600 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2601 | `{` |
|         - | 2602 | `	ph7_hashmap *pMap;` |
|         - | 2603 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1115 | 2604 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2605 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2606 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2607 | `		return PH7_OK;` |
|         - | 2608 | `	}` |
|         - | 2609 | `	/* Point to the internal representation of the input hashmap */` |
|      1115 | 2610 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1115 | 2611 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1115 | 2612 | `	if( pMap->nEntry > 1 ){` |
|      1109 | 2613 | `		sxi32 iCmpFlags = 0;` |
|      1109 | 2614 | `		if( nArg > 1 ){` |
|         - | 2615 | `			/* Extract comparison flags */` |
|        15 | 2616 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         7 | 2617 | `		}` |
|         - | 2618 | `		/* Do the merge sort */` |
|      1109 | 2619 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2620 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      1109 | 2621 | `		HashmapSortRehash(pMap);` |
|       559 | 2622 | `	}else if( pMap->nEntry == 1 ){` |
|         - | 2623 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|         5 | 2624 | `		HashmapSortRehash(pMap);` |
|         2 | 2625 | `	}` |
|         - | 2626 | `	/* All done,return TRUE */` |
|      1115 | 2627 | `	ph7_result_bool(pCtx,1);` |
|      1115 | 2628 | `	return PH7_OK;` |
|       560 | 2629 | `}` |
|         - | 2630 | `/*` |
|         - | 2631 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2632 | ` *  Sort an array and maintain index association.` |
|         - | 2633 | ` * Parameters` |
|         - | 2634 | ` *  $array` |
|         - | 2635 | ` *   The input array.` |
|         - | 2636 | ` * $sort_flags` |
|         - | 2637 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2638 | ` *  Sorting type flags:` |
|         - | 2639 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2640 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2641 | ` *   SORT_STRING - compare items as strings` |
|         - | 2642 | ` * Return` |
|         - | 2643 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2644 | ` */` |
|        36 | 2645 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2646 | `{` |
|         - | 2647 | `	ph7_hashmap *pMap;` |
|         - | 2648 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        41 | 2649 | `	if( nArg < 1 ){` |
|       ! 0 | 2650 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2651 | `			"ArgumentCountError",` |
|         - | 2652 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2653 | `			);` |
|         - | 2654 | `	}` |
|         - | 2655 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        41 | 2656 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2657 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2658 | `			"TypeError",` |
|         - | 2659 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2660 | `			ph7_type_name(apArg[0])` |
|         - | 2661 | `			);` |
|         - | 2662 | `	}` |
|         - | 2663 | `	/* Point to the internal representation of the input hashmap */` |
|        29 | 2664 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        29 | 2665 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 2666 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2667 | `		sxi32 iCmpFlags = 0;` |
|        23 | 2668 | `		if( nArg > 1 ){` |
|         - | 2669 | `			/* Extract comparison flags */` |
|         7 | 2670 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2671 | `		}` |
|         - | 2672 | `		/* Do the merge sort */` |
|        23 | 2673 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2674 | `		/* Fix the last link broken by the merge */` |
|        55 | 2675 | `		while(pMap->pLast->pPrev){` |
|        33 | 2676 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2677 | `		}` |
|        11 | 2678 | `	}` |
|         - | 2679 | `	/* All done,return TRUE */` |
|        29 | 2680 | `	ph7_result_bool(pCtx,1);` |
|        29 | 2681 | `	return PH7_OK;` |
|        23 | 2682 | `}` |
|         - | 2683 | `/*` |
|         - | 2684 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2685 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2686 | ` * Parameters` |
|         - | 2687 | ` *  $array` |
|         - | 2688 | ` *   The input array.` |
|         - | 2689 | ` * $sort_flags` |
|         - | 2690 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2691 | ` *  Sorting type flags:` |
|         - | 2692 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2693 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2694 | ` *   SORT_STRING - compare items as strings` |
|         - | 2695 | ` * Return` |
|         - | 2696 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2697 | ` */` |
|        32 | 2698 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2699 | `{` |
|         - | 2700 | `	ph7_hashmap *pMap;` |
|         - | 2701 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2702 | `	if( nArg < 1 ){` |
|       ! 0 | 2703 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2704 | `			"ArgumentCountError",` |
|         - | 2705 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2706 | `			);` |
|         - | 2707 | `	}` |
|         - | 2708 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2709 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2710 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2711 | `			"TypeError",` |
|         - | 2712 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2713 | `			ph7_type_name(apArg[0])` |
|         - | 2714 | `			);` |
|         - | 2715 | `	}` |
|         - | 2716 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2717 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2718 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2719 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2720 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2721 | `		if( nArg > 1 ){` |
|         - | 2722 | `			/* Extract comparison flags */` |
|         7 | 2723 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2724 | `		}` |
|         - | 2725 | `		/* Do the merge sort */` |
|        21 | 2726 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2727 | `		/* Fix the last link broken by the merge */` |
|        37 | 2728 | `		while(pMap->pLast->pPrev){` |
|        17 | 2729 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2730 | `		}` |
|        10 | 2731 | `	}` |
|         - | 2732 | `	/* All done,return TRUE */` |
|        25 | 2733 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2734 | `	return PH7_OK;` |
|        21 | 2735 | `}` |
|         - | 2736 | `/*` |
|         - | 2737 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2738 | ` *  Sort an array by key.` |
|         - | 2739 | ` * Parameters` |
|         - | 2740 | ` *  $array` |
|         - | 2741 | ` *   The input array.` |
|         - | 2742 | ` * $sort_flags` |
|         - | 2743 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2744 | ` *  Sorting type flags:` |
|         - | 2745 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2746 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2747 | ` *   SORT_STRING - compare items as strings` |
|         - | 2748 | ` * Return` |
|         - | 2749 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2750 | ` */` |
|        20 | 2751 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2752 | `{` |
|         - | 2753 | `	ph7_hashmap *pMap;` |
|         - | 2754 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 2755 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2756 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2757 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2758 | `		return PH7_OK;` |
|         - | 2759 | `	}` |
|         - | 2760 | `	/* Point to the internal representation of the input hashmap */` |
|        22 | 2761 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        22 | 2762 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        22 | 2763 | `	if( pMap->nEntry > 1 ){` |
|        22 | 2764 | `		sxi32 iCmpFlags = 0;` |
|        22 | 2765 | `		if( nArg > 1 ){` |
|         - | 2766 | `			/* Extract comparison flags */` |
|         5 | 2767 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2768 | `		}` |
|         - | 2769 | `		/* Do the merge sort */` |
|        22 | 2770 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2771 | `		/* Fix the last link broken by the merge */` |
|        56 | 2772 | `		while(pMap->pLast->pPrev){` |
|        35 | 2773 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2774 | `		}` |
|        10 | 2775 | `	}` |
|         - | 2776 | `	/* All done,return TRUE */` |
|        22 | 2777 | `	ph7_result_bool(pCtx,1);` |
|        22 | 2778 | `	return PH7_OK;` |
|        12 | 2779 | `}` |
|         - | 2780 | `/*` |
|         - | 2781 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2782 | ` *  Sort an array by key in reverse order.` |
|         - | 2783 | ` * Parameters` |
|         - | 2784 | ` *  $array` |
|         - | 2785 | ` *   The input array.` |
|         - | 2786 | ` * $sort_flags` |
|         - | 2787 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2788 | ` *  Sorting type flags:` |
|         - | 2789 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2790 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2791 | ` *   SORT_STRING - compare items as strings` |
|         - | 2792 | ` * Return` |
|         - | 2793 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2794 | ` */` |
|         6 | 2795 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2796 | `{` |
|         - | 2797 | `	ph7_hashmap *pMap;` |
|         - | 2798 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2799 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2800 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2801 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2802 | `		return PH7_OK;` |
|         - | 2803 | `	}` |
|         - | 2804 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2805 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2806 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2807 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2808 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2809 | `		if( nArg > 1 ){` |
|         - | 2810 | `			/* Extract comparison flags */` |
|         3 | 2811 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         1 | 2812 | `		}` |
|         - | 2813 | `		/* Do the merge sort */` |
|         7 | 2814 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2815 | `		/* Fix the last link broken by the merge */` |
|        23 | 2816 | `		while(pMap->pLast->pPrev){` |
|        17 | 2817 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2818 | `		}` |
|         3 | 2819 | `	}` |
|         - | 2820 | `	/* All done,return TRUE */` |
|         7 | 2821 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2822 | `	return PH7_OK;` |
|         4 | 2823 | `}` |
|         - | 2824 | `/*` |
|         - | 2825 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2826 | ` * Sort an array in reverse order.` |
|         - | 2827 | ` * Parameters` |
|         - | 2828 | ` *  $array` |
|         - | 2829 | ` *   The input array.` |
|         - | 2830 | ` * $sort_flags` |
|         - | 2831 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2832 | ` *  Sorting type flags:` |
|         - | 2833 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2834 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2835 | ` *   SORT_STRING - compare items as strings` |
|         - | 2836 | ` * Return` |
|         - | 2837 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2838 | ` */` |
|         8 | 2839 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2840 | `{` |
|         - | 2841 | `	ph7_hashmap *pMap;` |
|         - | 2842 | `	/* Make sure we are dealing with a valid hashmap */` |
|         9 | 2843 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2844 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2845 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2846 | `		return PH7_OK;` |
|         - | 2847 | `	}` |
|         - | 2848 | `	/* Point to the internal representation of the input hashmap */` |
|         9 | 2849 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         9 | 2850 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         9 | 2851 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2852 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2853 | `		if( nArg > 1 ){` |
|         - | 2854 | `			/* Extract comparison flags */` |
|         5 | 2855 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2856 | `		}` |
|         - | 2857 | `		/* Do the merge sort */` |
|         7 | 2858 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2859 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         7 | 2860 | `		HashmapSortRehash(pMap);` |
|         6 | 2861 | `	}else if( pMap->nEntry == 1 ){` |
|         - | 2862 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|         3 | 2863 | `		HashmapSortRehash(pMap);` |
|         1 | 2864 | `	}` |
|         - | 2865 | `	/* All done,return TRUE */` |
|         9 | 2866 | `	ph7_result_bool(pCtx,1);` |
|         9 | 2867 | `	return PH7_OK;` |
|         5 | 2868 | `}` |
|         - | 2869 | `/*` |
|         - | 2870 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2871 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2872 | ` * Parameters` |
|         - | 2873 | ` *  $array` |
|         - | 2874 | ` *   The input array.` |
|         - | 2875 | ` * $cmp_function` |
|         - | 2876 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2877 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2878 | ` *  to, or greater than the second.` |
|         - | 2879 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2880 | ` * Return` |
|         - | 2881 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2882 | ` */` |
|        28 | 2883 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2884 | `{` |
|         - | 2885 | `	ph7_hashmap *pMap;` |
|         - | 2886 | `	/* Make sure we are dealing with a valid hashmap */` |
|        31 | 2887 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2888 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2889 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2890 | `		return PH7_OK;` |
|         - | 2891 | `	}` |
|        31 | 2892 | `	if( nArg > 1 ){` |
|         - | 2893 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2894 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2895 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        31 | 2896 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        31 | 2897 | `		if( rcCb != PH7_OK ){` |
|         3 | 2898 | `			return rcCb;` |
|         - | 2899 | `		}` |
|        13 | 2900 | `	}` |
|         - | 2901 | `	/* Point to the internal representation of the input hashmap */` |
|        29 | 2902 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        29 | 2903 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 2904 | `	if( pMap->nEntry > 1 ){` |
|        27 | 2905 | `		ph7_value *pCallback = 0;` |
|         - | 2906 | `		ProcNodeCmp xCmp;` |
|        27 | 2907 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        27 | 2908 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2909 | `			/* Point to the desired callback */` |
|        27 | 2910 | `			pCallback = apArg[1];` |
|        15 | 2911 | `		}else{` |
|         - | 2912 | `			/* Use the default comparison function */` |
|       ! 0 | 2913 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2914 | `		}` |
|         - | 2915 | `		/* Do the merge sort */` |
|        27 | 2916 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        27 | 2917 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2918 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        27 | 2919 | `		HashmapSortRehash(pMap);` |
|        27 | 2920 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2921 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2922 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2923 | `			return PH7_EXCEPTION;` |
|         2 | 2924 | `		}` |
|        11 | 2925 | `	}else if( pMap->nEntry == 1 ){` |
|         - | 2926 | `		/* php reindexes even a single-element array: a string key becomes 0 */` |
|         3 | 2927 | `		HashmapSortRehash(pMap);` |
|         1 | 2928 | `	}` |
|         - | 2929 | `	/* All done,return TRUE */` |
|        20 | 2930 | `	ph7_result_bool(pCtx,1);` |
|        20 | 2931 | `	return PH7_OK;` |
|        17 | 2932 | `}` |
|         - | 2933 | `/*` |
|         - | 2934 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2935 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2936 | ` *  and maintain index association.` |
|         - | 2937 | ` * Parameters` |
|         - | 2938 | ` *  $array` |
|         - | 2939 | ` *   The input array.` |
|         - | 2940 | ` * $cmp_function` |
|         - | 2941 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2942 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2943 | ` *  to, or greater than the second.` |
|         - | 2944 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2945 | ` * Return` |
|         - | 2946 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2947 | ` */` |
|        14 | 2948 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2949 | `{` |
|         - | 2950 | `	ph7_hashmap *pMap;` |
|         - | 2951 | `	/* Make sure we are dealing with a valid hashmap */` |
|        15 | 2952 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2953 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2954 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2955 | `		return PH7_OK;` |
|         - | 2956 | `	}` |
|        15 | 2957 | `	if( nArg > 1 ){` |
|         - | 2958 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2959 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2960 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        15 | 2961 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        15 | 2962 | `		if( rcCb != PH7_OK ){` |
|         3 | 2963 | `			return rcCb;` |
|         - | 2964 | `		}` |
|         6 | 2965 | `	}` |
|         - | 2966 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 2967 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 2968 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 2969 | `	if( pMap->nEntry > 1 ){` |
|        13 | 2970 | `		ph7_value *pCallback = 0;` |
|         - | 2971 | `		ProcNodeCmp xCmp;` |
|        13 | 2972 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        13 | 2973 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2974 | `			/* Point to the desired callback */` |
|        13 | 2975 | `			pCallback = apArg[1];` |
|         7 | 2976 | `		}else{` |
|         - | 2977 | `			/* Use the default comparison function */` |
|       ! 0 | 2978 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2979 | `		}` |
|         - | 2980 | `		/* Do the merge sort */` |
|        13 | 2981 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        13 | 2982 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2983 | `		/* Fix the last link broken by the merge */` |
|        31 | 2984 | `		while(pMap->pLast->pPrev){` |
|        19 | 2985 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2986 | `		}` |
|        13 | 2987 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2988 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2989 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2990 | `			return PH7_EXCEPTION;` |
|         - | 2991 | `		}` |
|         6 | 2992 | `	}` |
|         - | 2993 | `	/* All done,return TRUE */` |
|        13 | 2994 | `	ph7_result_bool(pCtx,1);` |
|        13 | 2995 | `	return PH7_OK;` |
|         8 | 2996 | `}` |
|         - | 2997 | `/*` |
|         - | 2998 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2999 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 3000 | ` *  function and maintain index association.` |
|         - | 3001 | ` * Parameters` |
|         - | 3002 | ` *  $array` |
|         - | 3003 | ` *   The input array.` |
|         - | 3004 | ` * $cmp_function` |
|         - | 3005 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 3006 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 3007 | ` *  to, or greater than the second.` |
|         - | 3008 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 3009 | ` * Return` |
|         - | 3010 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3011 | ` */` |
|         4 | 3012 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3013 | `{` |
|         - | 3014 | `	ph7_hashmap *pMap;` |
|         - | 3015 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3016 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3017 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3018 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3019 | `		return PH7_OK;` |
|         - | 3020 | `	}` |
|         5 | 3021 | `	if( nArg > 1 ){` |
|         - | 3022 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 3023 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 3024 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|         5 | 3025 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|         5 | 3026 | `		if( rcCb != PH7_OK ){` |
|         3 | 3027 | `			return rcCb;` |
|         - | 3028 | `		}` |
|         1 | 3029 | `	}` |
|         - | 3030 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3031 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3032 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3033 | `	if( pMap->nEntry > 1 ){` |
|         3 | 3034 | `		ph7_value *pCallback = 0;` |
|         - | 3035 | `		ProcNodeCmp xCmp;` |
|         3 | 3036 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 3037 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 3038 | `			/* Point to the desired callback */` |
|         3 | 3039 | `			pCallback = apArg[1];` |
|         2 | 3040 | `		}else{` |
|         - | 3041 | `			/* Use the default comparison function */` |
|       ! 0 | 3042 | `			xCmp = HashmapCmpCallback2;` |
|         - | 3043 | `		}` |
|         - | 3044 | `		/* Do the merge sort */` |
|         3 | 3045 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 3046 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 3047 | `		/* Fix the last link broken by the merge */` |
|         3 | 3048 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 3049 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 3050 | `		}` |
|         3 | 3051 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 3052 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 3053 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 3054 | `			return PH7_EXCEPTION;` |
|         - | 3055 | `		}` |
|         1 | 3056 | `	}` |
|         - | 3057 | `	/* All done,return TRUE */` |
|         3 | 3058 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3059 | `	return PH7_OK;` |
|         3 | 3060 | `}` |
|         - | 3061 | `/*` |
|         - | 3062 | ` * bool shuffle(array &$array)` |
|         - | 3063 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 3064 | ` * Parameters` |
|         - | 3065 | ` *  $array` |
|         - | 3066 | ` *   The input array.` |
|         - | 3067 | ` * Return` |
|         - | 3068 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3069 | ` *` |
|         - | 3070 | ` */` |
|         2 | 3071 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3072 | `{` |
|         - | 3073 | `	ph7_hashmap *pMap;` |
|         - | 3074 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3075 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3076 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3077 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3078 | `		return PH7_OK;` |
|         - | 3079 | `	}` |
|         - | 3080 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3081 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3082 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3083 | `	if( pMap->nEntry > 1 ){` |
|         - | 3084 | `		/* Do the merge sort */` |
|         3 | 3085 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 3086 | `		/* Fix the last link broken by the merge */` |
|        10 | 3087 | `		while(pMap->pLast->pPrev){` |
|         8 | 3088 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 3089 | `		}` |
|         1 | 3090 | `	}` |
|         - | 3091 | `	/* All done,return TRUE */` |
|         3 | 3092 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3093 | `	return PH7_OK;` |
|         2 | 3094 | `}` |
|         - | 3095 | `/*` |
|         - | 3096 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 3097 | ` *   Count all elements in an array, or something in an object.` |
|         - | 3098 | ` * Parameters` |
|         - | 3099 | ` *  $var` |
|         - | 3100 | ` *   The array or the object.` |
|         - | 3101 | ` * $mode` |
|         - | 3102 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 3103 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 3104 | ` *  all the elements of a multidimensional array.` |
|         - | 3105 | ` * Return` |
|         - | 3106 | ` *  Returns the number of elements in the array.` |
|         - | 3107 | ` */` |
|      2218 | 3108 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3109 | `{` |
|      2223 | 3110 | `	int bRecursive = FALSE;` |
|      2223 | 3111 | `	int bCycleDetected = FALSE;` |
|         - | 3112 | `	sxi64 iCount;` |
|      2223 | 3113 | `	if( nArg < 1 ){` |
|       ! 0 | 3114 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3115 | `			"ArgumentCountError",` |
|         - | 3116 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 3117 | `			);` |
|         - | 3118 | `	}` |
|      2223 | 3119 | `	if( nArg > 2 ){` |
|         4 | 3120 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3121 | `			"ArgumentCountError",` |
|         - | 3122 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3123 | `			nArg` |
|         - | 3124 | `			);` |
|         - | 3125 | `	}` |
|         - | 3126 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3127 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3128 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      2221 | 3129 | `	if( nArg > 1 ){` |
|        44 | 3130 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 3131 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 3132 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3133 | `				"ValueError",` |
|         - | 3134 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3135 | `				);` |
|         - | 3136 | `		}` |
|        34 | 3137 | `		bRecursive = iMode == 1;` |
|        16 | 3138 | `	}` |
|      2213 | 3139 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3140 | `		/* Countable object: dispatch to ->count() */` |
|        75 | 3141 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        63 | 3142 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        63 | 3143 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        63 | 3144 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        61 | 3145 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3146 | `					"count",sizeof("count")-1);` |
|        61 | 3147 | `				if( pMeth ){` |
|         - | 3148 | `					ph7_value sResult;` |
|        61 | 3149 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        61 | 3150 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        61 | 3151 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        61 | 3152 | `					PH7_MemObjRelease(&sResult);` |
|        61 | 3153 | `					return PH7_OK;` |
|         - | 3154 | `				}` |
|       ! 0 | 3155 | `			}` |
|         1 | 3156 | `		}` |
|        22 | 3157 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3158 | `			"TypeError",` |
|         - | 3159 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3160 | `			ph7_type_name(apArg[0])` |
|         - | 3161 | `			);` |
|         - | 3162 | `	}` |
|         - | 3163 | `	/* Count */` |
|      2143 | 3164 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      2143 | 3165 | `	if( bCycleDetected ){` |
|         3 | 3166 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3167 | `	}` |
|      2143 | 3168 | `	ph7_result_int64(pCtx,iCount);` |
|      2143 | 3169 | `	return PH7_OK;` |
|      1114 | 3170 | `}` |
|         - | 3171 | `/*` |
|         - | 3172 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3173 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3174 | ` * Parameters` |
|         - | 3175 | ` * $key` |
|         - | 3176 | ` *   Value to check.` |
|         - | 3177 | ` * $search` |
|         - | 3178 | ` *  An array with keys to check.` |
|         - | 3179 | ` * Return` |
|         - | 3180 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3181 | ` */` |
|        94 | 3182 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3183 | `{` |
|         - | 3184 | `	sxi32 rc;` |
|        99 | 3185 | `	if( nArg != 2 ){` |
|         - | 3186 | `		/* PHP requires exactly two arguments */` |
|         4 | 3187 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3188 | `			"ArgumentCountError",` |
|         - | 3189 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3190 | `			nArg` |
|         - | 3191 | `			);` |
|         - | 3192 | `	}` |
|         - | 3193 | `	/* Make sure we are dealing with a valid hashmap */` |
|        97 | 3194 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3195 | `		/* Type mismatch -> TypeError */` |
|         8 | 3196 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3197 | `			"TypeError",` |
|         - | 3198 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3199 | `			ph7_type_name(apArg[1])` |
|         - | 3200 | `			);` |
|         - | 3201 | `	}` |
|         - | 3202 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        92 | 3203 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3204 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3205 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3206 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3207 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3208 | `			"use an empty string instead"` |
|         - | 3209 | `			);` |
|        91 | 3210 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3211 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3212 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3213 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3214 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3215 | `				,rVal` |
|         - | 3216 | `				);` |
|         1 | 3217 | `		}` |
|         1 | 3218 | `	}` |
|         - | 3219 | `	/* Perform the lookup */` |
|        92 | 3220 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3221 | `	/* lookup result */` |
|        92 | 3222 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        92 | 3223 | `	return PH7_OK;` |
|        52 | 3224 | `}` |
|         - | 3225 | `/*` |
|         - | 3226 | ` * value array_pop(array $array)` |
|         - | 3227 | ` *   POP the last inserted element from the array.` |
|         - | 3228 | ` * Parameter` |
|         - | 3229 | ` *  The array to get the value from.` |
|         - | 3230 | ` * Return` |
|         - | 3231 | ` *  Poped value or NULL on failure.` |
|         - | 3232 | ` */` |
|       108 | 3233 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3234 | `{` |
|         - | 3235 | `	ph7_hashmap *pMap;` |
|         - | 3236 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       112 | 3237 | `	if( nArg != 1 ){` |
|         4 | 3238 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3239 | `			"ArgumentCountError",` |
|         - | 3240 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3241 | `			nArg` |
|         - | 3242 | `			);` |
|         - | 3243 | `	}` |
|         - | 3244 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3245 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       110 | 3246 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3247 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3248 | `			"Error",` |
|         - | 3249 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3250 | `			);` |
|         - | 3251 | `	}` |
|         - | 3252 | `	/* Make sure we are dealing with a valid hashmap */` |
|       104 | 3253 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3254 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3255 | `			"TypeError",` |
|         - | 3256 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3257 | `			ph7_type_name(apArg[0])` |
|         - | 3258 | `			);` |
|         - | 3259 | `	}` |
|       101 | 3260 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       101 | 3261 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       101 | 3262 | `	if( pMap->nEntry < 1 ){` |
|         - | 3263 | `		/* Nothing to pop,return NULL */` |
|         3 | 3264 | `		ph7_result_null(pCtx);` |
|         2 | 3265 | `	}else{` |
|        99 | 3266 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3267 | `		ph7_value *pObj;` |
|        99 | 3268 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        99 | 3269 | `		if( pObj ){` |
|         - | 3270 | `			/* Node value */` |
|        99 | 3271 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3272 | `			/* Unlink the node */` |
|        99 | 3273 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        50 | 3274 | `		}else{` |
|       ! 0 | 3275 | `			ph7_result_null(pCtx);` |
|         - | 3276 | `		}` |
|         - | 3277 | `		/* Reset the cursor */` |
|        99 | 3278 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3279 | `	}` |
|       101 | 3280 | `	return PH7_OK;` |
|        58 | 3281 | `}` |
|         - | 3282 | `/*` |
|         - | 3283 | ` * int array_push($array,$var,...)` |
|         - | 3284 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3285 | ` * Parameters` |
|         - | 3286 | ` *  array` |
|         - | 3287 | ` *    The input array.` |
|         - | 3288 | ` *  var` |
|         - | 3289 | ` *   On or more value to push.` |
|         - | 3290 | ` * Return` |
|         - | 3291 | ` *  New array count (including old items).` |
|         - | 3292 | ` */` |
|        22 | 3293 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3294 | `{` |
|         - | 3295 | `	ph7_hashmap *pMap;` |
|         - | 3296 | `	sxi32 rc;` |
|         - | 3297 | `	int i;` |
|        26 | 3298 | `	if( nArg < 1 ){` |
|       ! 0 | 3299 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3300 | `			"ArgumentCountError",` |
|         - | 3301 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3302 | `			nArg` |
|         - | 3303 | `			);` |
|         - | 3304 | `	}` |
|         - | 3305 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3306 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3307 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3308 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3309 | `			"Error",` |
|         - | 3310 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3311 | `			);` |
|         - | 3312 | `	}` |
|         - | 3313 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3314 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3315 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3316 | `			"TypeError",` |
|         - | 3317 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3318 | `			ph7_type_name(apArg[0])` |
|         - | 3319 | `			);` |
|         - | 3320 | `	}` |
|         - | 3321 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3322 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3323 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3324 | `	/* Start pushing given values */` |
|        34 | 3325 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3326 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3327 | `		if( rc != SXRET_OK ){` |
|         3 | 3328 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3329 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3330 | `				return rc;` |
|         - | 3331 | `			}` |
|       ! 0 | 3332 | `			break;` |
|         - | 3333 | `		}` |
|         9 | 3334 | `	}` |
|         - | 3335 | `	/* Return the new count */` |
|        15 | 3336 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3337 | `	return PH7_OK;` |
|        15 | 3338 | `}` |
|         - | 3339 | `/*` |
|         - | 3340 | ` * value array_shift(array $array)` |
|         - | 3341 | ` *   Shift an element off the beginning of array.` |
|         - | 3342 | ` * Parameter` |
|         - | 3343 | ` *  The array to get the value from.` |
|         - | 3344 | ` * Return` |
|         - | 3345 | ` *  Shifted value or NULL on failure.` |
|         - | 3346 | ` */` |
|        44 | 3347 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3348 | `{` |
|         - | 3349 | `	ph7_hashmap *pMap;` |
|         - | 3350 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3351 | `	if( nArg != 1 ){` |
|         4 | 3352 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3353 | `			"ArgumentCountError",` |
|         - | 3354 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3355 | `			nArg` |
|         - | 3356 | `			);` |
|         - | 3357 | `	}` |
|         - | 3358 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3359 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3360 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3361 | `			"Error",` |
|         - | 3362 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3363 | `			);` |
|         - | 3364 | `	}` |
|         - | 3365 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3366 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3367 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3368 | `			"TypeError",` |
|         - | 3369 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3370 | `			ph7_type_name(apArg[0])` |
|         - | 3371 | `			);` |
|         - | 3372 | `	}` |
|         - | 3373 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3374 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3375 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3376 | `	if( pMap->nEntry < 1 ){` |
|         - | 3377 | `		/* Empty hashmap,return NULL */` |
|         3 | 3378 | `		ph7_result_null(pCtx);` |
|         2 | 3379 | `	}else{` |
|        39 | 3380 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3381 | `		ph7_value *pObj;` |
|         - | 3382 | `		sxu32 n;` |
|        39 | 3383 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3384 | `		if( pObj ){` |
|         - | 3385 | `			/* Node value */` |
|        39 | 3386 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3387 | `			/* Unlink the first node */` |
|        39 | 3388 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3389 | `		}else{` |
|       ! 0 | 3390 | `			ph7_result_null(pCtx);` |
|         - | 3391 | `		}` |
|         - | 3392 | `		/* Rehash all int keys */` |
|        39 | 3393 | `		n = pMap->nEntry;` |
|        39 | 3394 | `		pEntry = pMap->pFirst;` |
|        39 | 3395 | `		pMap->iNextIdx = 0;` |
|        39 | 3396 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3397 | `		for(;;){` |
|        99 | 3398 | `			if( n < 1 ){` |
|        39 | 3399 | `				break;` |
|         - | 3400 | `			}` |
|        65 | 3401 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3402 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3403 | `			}` |
|         - | 3404 | `			/* Point to the next entry */` |
|        65 | 3405 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3406 | `			n--;` |
|         5 | 3407 | `		}` |
|         - | 3408 | `		/* Reset the cursor */` |
|        39 | 3409 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3410 | `	}` |
|        41 | 3411 | `	return PH7_OK;` |
|        27 | 3412 | `}` |
|         - | 3413 | `/*` |
|         - | 3414 | ` * Extract the node cursor value.` |
|         - | 3415 | ` */` |
|      1232 | 3416 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3417 | `{` |
|      1233 | 3418 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3419 | `	ph7_value *pVal;` |
|      1233 | 3420 | `	if( pCur == 0 ){` |
|         - | 3421 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3422 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3423 | `		return PH7_OK;` |
|         - | 3424 | `	}` |
|      1195 | 3425 | `	if( iDirection != 0 ){` |
|       227 | 3426 | `		if( iDirection > 0 ){` |
|         - | 3427 | `			/* Point to the next entry */` |
|       225 | 3428 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       225 | 3429 | `			pCur = pMap->pCur;` |
|       113 | 3430 | `		}else{` |
|         - | 3431 | `			/* Point to the previous entry */` |
|         3 | 3432 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3433 | `			pCur = pMap->pCur;` |
|         - | 3434 | `		}` |
|       227 | 3435 | `		if( pCur == 0 ){` |
|         - | 3436 | `			/* End of input reached,return FALSE */` |
|        91 | 3437 | `			ph7_result_bool(pCtx,0);` |
|        91 | 3438 | `			return PH7_OK;` |
|         - | 3439 | `		}` |
|        68 | 3440 | `	}` |
|         - | 3441 | `	/* Point to the desired element */` |
|      1105 | 3442 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      1105 | 3443 | `	if( pVal ){` |
|      1105 | 3444 | `		ph7_result_value(pCtx,pVal);` |
|       553 | 3445 | `	}else{` |
|       ! 0 | 3446 | `		ph7_result_bool(pCtx,0);` |
|         - | 3447 | `	}` |
|      1105 | 3448 | `	return PH7_OK;` |
|       617 | 3449 | `}` |
|         - | 3450 | `/*` |
|         - | 3451 | ` * value current(array $array)` |
|         - | 3452 | ` *  Return the current element in an array.` |
|         - | 3453 | ` * Parameter` |
|         - | 3454 | ` *  $input: The input array.` |
|         - | 3455 | ` * Return` |
|         - | 3456 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3457 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3458 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3459 | ` *  is empty, current() returns FALSE.` |
|         - | 3460 | ` */` |
|       356 | 3461 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3462 | `{` |
|       357 | 3463 | `	if( nArg < 1 ){` |
|         - | 3464 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3465 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3466 | `		return PH7_OK;` |
|         - | 3467 | `	}` |
|         - | 3468 | `	/* Make sure we are dealing with a valid hashmap */` |
|       357 | 3469 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3470 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3471 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3472 | `		return PH7_OK;` |
|         - | 3473 | `	}` |
|       357 | 3474 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       357 | 3475 | `	return PH7_OK;` |
|       179 | 3476 | `}` |
|         - | 3477 | `/*` |
|         - | 3478 | ` * value next(array $input)` |
|         - | 3479 | ` *  Advance the internal array pointer of an array.` |
|         - | 3480 | ` * Parameter` |
|         - | 3481 | ` *  $input: The input array.` |
|         - | 3482 | ` * Return` |
|         - | 3483 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3484 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3485 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3486 | ` */` |
|       224 | 3487 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3488 | `{` |
|       225 | 3489 | `	if( nArg < 1 ){` |
|         - | 3490 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3491 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3492 | `		return PH7_OK;` |
|         - | 3493 | `	}` |
|         - | 3494 | `	/* Make sure we are dealing with a valid hashmap */` |
|       225 | 3495 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3496 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3497 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3498 | `		return PH7_OK;` |
|         - | 3499 | `	}` |
|       225 | 3500 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       225 | 3501 | `	return PH7_OK;` |
|       113 | 3502 | `}` |
|         - | 3503 | `/*` |
|         - | 3504 | ` * value prev(array $input)` |
|         - | 3505 | ` *  Rewind the internal array pointer.` |
|         - | 3506 | ` * Parameter` |
|         - | 3507 | ` *  $input: The input array.` |
|         - | 3508 | ` * Return` |
|         - | 3509 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3510 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3511 | ` *  elements.` |
|         - | 3512 | ` */` |
|         2 | 3513 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3514 | `{` |
|         3 | 3515 | `	if( nArg < 1 ){` |
|         - | 3516 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3517 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3518 | `		return PH7_OK;` |
|         - | 3519 | `	}` |
|         - | 3520 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3521 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3522 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3523 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3524 | `		return PH7_OK;` |
|         - | 3525 | `	}` |
|         3 | 3526 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3527 | `	return PH7_OK;` |
|         2 | 3528 | `}` |
|         - | 3529 | `/*` |
|         - | 3530 | ` * value end(array $input)` |
|         - | 3531 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3532 | ` * Parameter` |
|         - | 3533 | ` *  $input: The input array.` |
|         - | 3534 | ` * Return` |
|         - | 3535 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3536 | ` */` |
|       390 | 3537 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3538 | `{` |
|         - | 3539 | `	ph7_hashmap *pMap;` |
|       391 | 3540 | `	if( nArg < 1 ){` |
|         - | 3541 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3542 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3543 | `		return PH7_OK;` |
|         - | 3544 | `	}` |
|         - | 3545 | `	/* Make sure we are dealing with a valid hashmap */` |
|       391 | 3546 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3547 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3548 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3549 | `		return PH7_OK;` |
|         - | 3550 | `	}` |
|         - | 3551 | `	/* Point to the internal representation of the input hashmap */` |
|       391 | 3552 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3553 | `	/* Point to the last node */` |
|       391 | 3554 | `	pMap->pCur = pMap->pLast;` |
|         - | 3555 | `	/* Return the last node value */` |
|       391 | 3556 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       391 | 3557 | `	return PH7_OK;` |
|       196 | 3558 | `}` |
|         - | 3559 | `/*` |
|         - | 3560 | ` * value reset(array $array )` |
|         - | 3561 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3562 | ` * Parameter` |
|         - | 3563 | ` *  $input: The input array.` |
|         - | 3564 | ` * Return` |
|         - | 3565 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3566 | ` */` |
|       260 | 3567 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3568 | `{` |
|         - | 3569 | `	ph7_hashmap *pMap;` |
|       261 | 3570 | `	if( nArg < 1 ){` |
|         - | 3571 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3572 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3573 | `		return PH7_OK;` |
|         - | 3574 | `	}` |
|         - | 3575 | `	/* Make sure we are dealing with a valid hashmap */` |
|       261 | 3576 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3577 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3578 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3579 | `		return PH7_OK;` |
|         - | 3580 | `	}` |
|         - | 3581 | `	/* Point to the internal representation of the input hashmap */` |
|       261 | 3582 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3583 | `	/* Point to the first node */` |
|       261 | 3584 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3585 | `	/* Return the last node value if available */` |
|       261 | 3586 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       261 | 3587 | `	return PH7_OK;` |
|       131 | 3588 | `}` |
|         - | 3589 | `/*` |
|         - | 3590 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3591 | ` * array_key_first() and array_key_last().` |
|         - | 3592 | ` */` |
|       772 | 3593 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3594 | `{` |
|       773 | 3595 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3596 | `		/* Key is integer */` |
|       383 | 3597 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       192 | 3598 | `	}else{` |
|         - | 3599 | `		/* Key is blob */` |
|       586 | 3600 | `		ph7_result_string(pCtx,` |
|       390 | 3601 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3602 | `	}` |
|       773 | 3603 | `}` |
|         - | 3604 | `/*` |
|         - | 3605 | ` * value key(array $array)` |
|         - | 3606 | ` *   Fetch a key from an array` |
|         - | 3607 | ` * Parameter` |
|         - | 3608 | ` *  $input` |
|         - | 3609 | ` *   The input array.` |
|         - | 3610 | ` * Return` |
|         - | 3611 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3612 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3613 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3614 | ` *  is empty, key() returns NULL.` |
|         - | 3615 | ` */` |
|       892 | 3616 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3617 | `{` |
|         - | 3618 | `	ph7_hashmap_node *pCur;` |
|         - | 3619 | `	ph7_hashmap *pMap;` |
|       893 | 3620 | `	if( nArg < 1 ){` |
|         - | 3621 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3622 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3623 | `		return PH7_OK;` |
|         - | 3624 | `	}` |
|         - | 3625 | `	/* Make sure we are dealing with a valid hashmap */` |
|       893 | 3626 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3627 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3628 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3629 | `		return PH7_OK;` |
|         - | 3630 | `	}` |
|       893 | 3631 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       893 | 3632 | `	pCur = pMap->pCur;` |
|       893 | 3633 | `	if( pCur == 0 ){` |
|         - | 3634 | `		/* Cursor does not point to anything,return NULL */` |
|       137 | 3635 | `		ph7_result_null(pCtx);` |
|       137 | 3636 | `		return PH7_OK;` |
|         - | 3637 | `	}` |
|       757 | 3638 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       757 | 3639 | `	return PH7_OK;` |
|       447 | 3640 | `}` |
|         - | 3641 | `/*` |
|         - | 3642 | ` * array each(array $input)` |
|         - | 3643 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3644 | ` * Parameter` |
|         - | 3645 | ` *  $input` |
|         - | 3646 | ` *    The input array.` |
|         - | 3647 | ` * Return` |
|         - | 3648 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3649 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3650 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3651 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3652 | ` *  each() returns FALSE.` |
|         - | 3653 | ` */` |
|        22 | 3654 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3655 | `{` |
|         - | 3656 | `	ph7_hashmap_node *pCur;` |
|         - | 3657 | `	ph7_hashmap *pMap;` |
|         - | 3658 | `	ph7_value *pArray;` |
|         - | 3659 | `	ph7_value *pVal;` |
|         - | 3660 | `	ph7_value sKey;` |
|        23 | 3661 | `	if( nArg < 1 ){` |
|         - | 3662 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3663 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3664 | `		return PH7_OK;` |
|         - | 3665 | `	}` |
|         - | 3666 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3667 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3668 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3669 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3670 | `		return PH7_OK;` |
|         - | 3671 | `	}` |
|         - | 3672 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3673 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3674 | `	if( pMap->pCur == 0 ){` |
|         - | 3675 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3676 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3677 | `		return PH7_OK;` |
|         - | 3678 | `	}` |
|        15 | 3679 | `	pCur = pMap->pCur;` |
|         - | 3680 | `	/* Create a new array */` |
|        15 | 3681 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3682 | `	if( pArray == 0 ){` |
|       ! 0 | 3683 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3684 | `		return PH7_OK;` |
|         - | 3685 | `	}` |
|        15 | 3686 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3687 | `	/* Insert the current value */` |
|        15 | 3688 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3689 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3690 | `	/* Make the key */` |
|        15 | 3691 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3692 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3693 | `	}else{` |
|         9 | 3694 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3695 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3696 | `	}` |
|         - | 3697 | `	/* Insert the current key */` |
|        15 | 3698 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3699 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3700 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3701 | `	/* Advance the cursor */` |
|        15 | 3702 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3703 | `	/* Return the current entry */` |
|        15 | 3704 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3705 | `	return PH7_OK;` |
|        12 | 3706 | `}` |
|         - | 3707 | `/*` |
|         - | 3708 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3709 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3710 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3711 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3712 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3713 | ` */` |
|         - | 3714 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3715 | `/*` |
|         - | 3716 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3717 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3718 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3719 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3720 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3721 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3722 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3723 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3724 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3725 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3726 | ` */` |
|         - | 3727 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3728 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3729 | `/*` |
|         - | 3730 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3731 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3732 | ` */` |
|       ! 0 | 3733 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3734 | `{` |
|       ! 0 | 3735 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3736 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3737 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3738 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3739 | `		zBuf[n] = 0;` |
|       ! 0 | 3740 | `		return zBuf;` |
|         - | 3741 | `	}` |
|       ! 0 | 3742 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3743 | `}` |
|         - | 3744 | `/*` |
|         - | 3745 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3746 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3747 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3748 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3749 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3750 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3751 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3752 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3753 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3754 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3755 | ` */` |
|       156 | 3756 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3757 | `{` |
|       157 | 3758 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3759 | `	sxu64 uVal = 0;` |
|       157 | 3760 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3761 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3762 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3763 | `		bNeg = (z[0] == '-');` |
|         3 | 3764 | `		z++;` |
|         1 | 3765 | `	}` |
|       237 | 3766 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3767 | `		int d = z[0] - '0';` |
|         - | 3768 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3769 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3770 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3771 | `			bOverflow = 1;` |
|       ! 0 | 3772 | `		}else{` |
|        81 | 3773 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3774 | `		}` |
|        81 | 3775 | `		bDigit = 1;` |
|        81 | 3776 | `		z++;` |
|         1 | 3777 | `	}` |
|       157 | 3778 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3779 | `		bReal = 1;` |
|         3 | 3780 | `		z++;` |
|         5 | 3781 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3782 | `			bDigit = 1;` |
|         3 | 3783 | `			z++;` |
|         1 | 3784 | `		}` |
|         1 | 3785 | `	}` |
|         - | 3786 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3787 | `	if( !bDigit ){` |
|        61 | 3788 | `		return RANGE_IN_ERROR;` |
|         - | 3789 | `	}` |
|         - | 3790 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3791 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3792 | `		z++;` |
|         9 | 3793 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3794 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3795 | `			return RANGE_IN_ERROR;` |
|         - | 3796 | `		}` |
|         9 | 3797 | `		bReal = 1;` |
|        17 | 3798 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3799 | `	}` |
|         - | 3800 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3801 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3802 | `	if( z != zEnd ){` |
|        13 | 3803 | `		return RANGE_IN_ERROR;` |
|         - | 3804 | `	}` |
|        84 | 3805 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3806 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3807 | `		bReal = 1;` |
|        84 | 3808 | `	}` |
|        43 | 3809 | `	if( bReal ){` |
|        11 | 3810 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3811 | `		return RANGE_IN_DOUBLE;` |
|         - | 3812 | `	}` |
|         - | 3813 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3814 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3815 | `	return RANGE_IN_LONG;` |
|        58 | 3816 | `}` |
|         - | 3817 | `/*` |
|         - | 3818 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3819 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3820 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3821 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3822 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3823 | ` */` |
|       332 | 3824 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3825 | `{` |
|         - | 3826 | `	char zMsg[160];` |
|       333 | 3827 | `	*pRc = PH7_OK;` |
|       333 | 3828 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3829 | `		char zType[80];` |
|       ! 0 | 3830 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3831 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3832 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3833 | `		return FALSE;` |
|         - | 3834 | `	}` |
|       333 | 3835 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3836 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3837 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3838 | `			iArg,zName);` |
|         5 | 3839 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3840 | `		*pbNullCoerced = TRUE;` |
|         2 | 3841 | `	}` |
|       333 | 3842 | `	return TRUE;` |
|       167 | 3843 | `}` |
|         - | 3844 | `/*` |
|         - | 3845 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3846 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3847 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3848 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3849 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3850 | ` */` |
|        60 | 3851 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3852 | `{` |
|        61 | 3853 | `	*pRc = PH7_OK;` |
|        61 | 3854 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3855 | `		char zType[80];` |
|       ! 0 | 3856 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3857 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3858 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3859 | `		return RANGE_IN_ERROR;` |
|         - | 3860 | `	}` |
|        61 | 3861 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3862 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3863 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3864 | `		*pLong = 0;` |
|         3 | 3865 | `		return RANGE_IN_LONG;` |
|         - | 3866 | `	}` |
|        59 | 3867 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3868 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3869 | `		return RANGE_IN_DOUBLE;` |
|         - | 3870 | `	}` |
|        35 | 3871 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3872 | `		const char *zStr;` |
|         - | 3873 | `		int nLen;` |
|         - | 3874 | `		sxu8 iKind;` |
|         3 | 3875 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3876 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3877 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3878 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3879 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3880 | `		}` |
|         3 | 3881 | `		return iKind;` |
|         - | 3882 | `	}` |
|         - | 3883 | `	/* int / bool */` |
|        33 | 3884 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3885 | `	return RANGE_IN_LONG;` |
|        31 | 3886 | `}` |
|         - | 3887 | `/*` |
|         - | 3888 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3889 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3890 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3891 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3892 | ` */` |
|       300 | 3893 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3894 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3895 | `{` |
|         - | 3896 | `	char zMsg[160];` |
|         - | 3897 | `	double r;` |
|       301 | 3898 | `	*pRc = PH7_OK;` |
|       301 | 3899 | `	if( bNullCoerced ){` |
|         - | 3900 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3901 | `		*pLong = 0;` |
|         5 | 3902 | `		*pDouble = 0.0;` |
|         5 | 3903 | `		return RANGE_IN_LONG;` |
|         - | 3904 | `	}` |
|       297 | 3905 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3906 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3907 | `check_dval:` |
|        25 | 3908 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3909 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3910 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3911 | `			return RANGE_IN_ERROR;` |
|         - | 3912 | `		}` |
|        21 | 3913 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3914 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3915 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3916 | `			return RANGE_IN_ERROR;` |
|         - | 3917 | `		}` |
|        17 | 3918 | `		*pDouble = r;` |
|        17 | 3919 | `		return RANGE_IN_DOUBLE;` |
|         - | 3920 | `	}` |
|       277 | 3921 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3922 | `		const char *zStr;` |
|         - | 3923 | `		int nLen;` |
|         - | 3924 | `		sxu8 iKind;` |
|        81 | 3925 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3926 | `		if( nLen == 0 ){` |
|         7 | 3927 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3928 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3929 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3930 | `			*pLong = 0;` |
|         5 | 3931 | `			*pDouble = 0.0;` |
|        41 | 3932 | `			return RANGE_IN_LONG;` |
|         - | 3933 | `		}` |
|        77 | 3934 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3935 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3936 | `			r = *pDouble;` |
|         5 | 3937 | `			goto check_dval;` |
|         - | 3938 | `		}` |
|        73 | 3939 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3940 | `			*pDouble = (double)*pLong;` |
|        23 | 3941 | `			if( nLen == 1 ){` |
|         - | 3942 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3943 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3944 | `				return RANGE_IN_DIGIT;` |
|         - | 3945 | `			}` |
|        15 | 3946 | `			return RANGE_IN_LONG;` |
|         - | 3947 | `		}` |
|        51 | 3948 | `		if( nLen != 1 ){` |
|        10 | 3949 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3950 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3951 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3952 | `		}` |
|        51 | 3953 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3954 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3955 | `		*pLong = 0;` |
|        51 | 3956 | `		*pDouble = 0.0;` |
|        51 | 3957 | `		return RANGE_IN_STRING;` |
|         - | 3958 | `	}` |
|         - | 3959 | `	/* int / bool */` |
|       197 | 3960 | `	*pLong = ph7_value_to_int64(pIn);` |
|       197 | 3961 | `	*pDouble = (double)*pLong;` |
|       197 | 3962 | `	return RANGE_IN_LONG;` |
|       151 | 3963 | `}` |
|         - | 3964 | `/*` |
|         - | 3965 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3966 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3967 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3968 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3969 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3970 | ` * exactly like php's two macros.` |
|         - | 3971 | ` */` |
|         6 | 3972 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3973 | `{` |
|        10 | 3974 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3975 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3976 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3977 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3978 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3979 | `}` |
|         6 | 3980 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3981 | `{` |
|         - | 3982 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3983 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3984 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3985 | `	const unsigned int nBuf = 1500;` |
|         7 | 3986 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3987 | `	if( zMsg == 0 ){` |
|       ! 0 | 3988 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3989 | `	}` |
|         7 | 3990 | `	snprintf(zMsg,nBuf,` |
|         - | 3991 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3992 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3993 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3994 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3995 | `}` |
|         - | 3996 | `/*` |
|         - | 3997 | ` * Set the element container to the next range element and append it to the` |
|         - | 3998 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3999 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 4000 | ` * below stay one line per iteration.` |
|         - | 4001 | ` */` |
|    401680 | 4002 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 4003 | `{` |
|    401681 | 4004 | `	ph7_value_int64(pValue,iVal);` |
|    401681 | 4005 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 4006 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4007 | `	}` |
|    401681 | 4008 | `	return PH7_OK;` |
|    200841 | 4009 | `}` |
|        70 | 4010 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 4011 | `{` |
|        71 | 4012 | `	ph7_value_double(pValue,rVal);` |
|        71 | 4013 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 4014 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4015 | `	}` |
|        71 | 4016 | `	return PH7_OK;` |
|        36 | 4017 | `}` |
|       168 | 4018 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 4019 | `{` |
|       169 | 4020 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 4021 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 4022 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4023 | `	}` |
|       169 | 4024 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 4025 | `	return PH7_OK;` |
|        85 | 4026 | `}` |
|         - | 4027 | `/*` |
|         - | 4028 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 4029 | ` *  Create an array containing a range of elements.` |
|         - | 4030 | ` * Return` |
|         - | 4031 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 4032 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 4033 | ` */` |
|       168 | 4034 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4035 | `{` |
|         - | 4036 | `	ph7_value *pValue,*pArray;` |
|       169 | 4037 | `	sxi32 rc = PH7_OK;` |
|       169 | 4038 | `	int is_step_double = 0,is_step_negative = 0;` |
|       169 | 4039 | `	double step_double = 1.0;` |
|       169 | 4040 | `	sxi64 step = 1;` |
|         - | 4041 | `	sxu8 start_type,end_type;` |
|       169 | 4042 | `	sxi64 start_long = 0,end_long = 0;` |
|       169 | 4043 | `	double start_double = 0.0,end_double = 0.0;` |
|       169 | 4044 | `	unsigned char cStart = 0,cEnd = 0;` |
|       169 | 4045 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 4046 | `	sxu32 i,size;` |
|         - | 4047 |  |
|         - | 4048 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       169 | 4049 | `	if( nArg > 3 ){` |
|         4 | 4050 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 4051 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 4052 | `	}` |
|       167 | 4053 | `	if( nArg < 2 ){` |
|         - | 4054 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 4055 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 4056 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 4057 | `	}` |
|         - | 4058 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 4059 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       167 | 4060 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 4061 | `		return rc;` |
|         - | 4062 | `	}` |
|       167 | 4063 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 4064 | `		return rc;` |
|         - | 4065 | `	}` |
|       167 | 4066 | `	if( nArg > 2 ){` |
|        61 | 4067 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 4068 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 4069 | `			return rc;` |
|         - | 4070 | `		}` |
|        59 | 4071 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 4072 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 4073 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4074 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 4075 | `			}` |
|        23 | 4076 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 4077 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4078 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 4079 | `			}` |
|         - | 4080 | `			/* We only want positive step values. */` |
|        21 | 4081 | `			if( step_double < 0.0 ){` |
|       ! 0 | 4082 | `				is_step_negative = 1;` |
|       ! 0 | 4083 | `				step_double *= -1;` |
|       ! 0 | 4084 | `			}` |
|         - | 4085 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 4086 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 4087 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 4088 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 4089 | `				step = (sxi64)step_double;` |
|        19 | 4090 | `				if( (double)step != step_double ){` |
|        17 | 4091 | `					is_step_double = 1;` |
|         8 | 4092 | `				}` |
|        10 | 4093 | `			}else{` |
|         - | 4094 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 4095 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 4096 | `				is_step_double = 1;` |
|         - | 4097 | `			}` |
|        11 | 4098 | `		}else{` |
|         - | 4099 | `			/* We only want positive step values. */` |
|        35 | 4100 | `			if( step < 0 ){` |
|        11 | 4101 | `				if( step == SMALLEST_INT64 ){` |
|         - | 4102 | `					/* -step would overflow */` |
|         4 | 4103 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 4104 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 4105 | `				}` |
|         9 | 4106 | `				is_step_negative = 1;` |
|         9 | 4107 | `				step = -step;` |
|         4 | 4108 | `			}` |
|        33 | 4109 | `			step_double = (double)step;` |
|         - | 4110 | `		}` |
|        53 | 4111 | `		if( step_double == 0.0 ){` |
|         7 | 4112 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4113 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 4114 | `		}` |
|        23 | 4115 | `	}` |
|       153 | 4116 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       153 | 4117 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 4118 | `		return rc;` |
|         - | 4119 | `	}` |
|       149 | 4120 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       149 | 4121 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 4122 | `		return rc;` |
|         - | 4123 | `	}` |
|         - | 4124 | `	/* Element container + result array */` |
|       145 | 4125 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       145 | 4126 | `	pArray = ph7_context_new_array(pCtx);` |
|       145 | 4127 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4128 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4129 | `	}` |
|         - | 4130 | `	/* If the range is given as strings, generate an array of characters. */` |
|       145 | 4131 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4132 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4133 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4134 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4135 | `			 * and the range is numeric. */` |
|        15 | 4136 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4137 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4138 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4139 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4140 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4141 | `				}` |
|         7 | 4142 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4143 | `			}else{` |
|         9 | 4144 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4145 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4146 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4147 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4148 | `				}` |
|         9 | 4149 | `				start_type = RANGE_IN_LONG;` |
|         - | 4150 | `			}` |
|        15 | 4151 | `			goto handle_numeric_inputs;` |
|         - | 4152 | `		}` |
|        23 | 4153 | `		if( is_step_double ){` |
|         - | 4154 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4155 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4156 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4157 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4158 | `					" of characters, inputs converted to 0");` |
|         1 | 4159 | `			}` |
|         5 | 4160 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4161 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4162 | `			goto handle_numeric_inputs;` |
|         - | 4163 | `		}` |
|         - | 4164 | `		/* Generate an array of characters */` |
|        19 | 4165 | `		if( cStart > cEnd ){` |
|         - | 4166 | `			/* Decreasing char range */` |
|         - | 4167 | `			int iCur;` |
|         3 | 4168 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4169 | `				goto boundary_error;` |
|         - | 4170 | `			}` |
|        17 | 4171 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4172 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4173 | `					return rc;` |
|         - | 4174 | `				}` |
|         8 | 4175 | `			}` |
|        18 | 4176 | `		}else if( cEnd > cStart ){` |
|         - | 4177 | `			/* Increasing char range */` |
|         - | 4178 | `			int iCur;` |
|        15 | 4179 | `			if( is_step_negative ){` |
|         3 | 4180 | `				goto negative_step_error;` |
|         - | 4181 | `			}` |
|        13 | 4182 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4183 | `				goto boundary_error;` |
|         - | 4184 | `			}` |
|       163 | 4185 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4186 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4187 | `					return rc;` |
|         - | 4188 | `				}` |
|        77 | 4189 | `			}` |
|         6 | 4190 | `		}else{` |
|         3 | 4191 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4192 | `				return rc;` |
|         - | 4193 | `			}` |
|         - | 4194 | `		}` |
|        15 | 4195 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4196 | `		return PH7_OK;` |
|         - | 4197 | `	}` |
|        54 | 4198 | `handle_numeric_inputs:` |
|       135 | 4199 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4200 | `		/* Float range */` |
|         - | 4201 | `		double elem,calc;` |
|        25 | 4202 | `		if( start_double > end_double ){` |
|         - | 4203 | `			/* Decreasing float range */` |
|         7 | 4204 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4205 | `				goto boundary_error;` |
|         - | 4206 | `			}` |
|         7 | 4207 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4208 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4209 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4210 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4211 | `			}` |
|         5 | 4212 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4213 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4214 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4215 | `					return rc;` |
|         - | 4216 | `				}` |
|         8 | 4217 | `			}` |
|        21 | 4218 | `		}else if( end_double > start_double ){` |
|         - | 4219 | `			/* Increasing float range */` |
|        17 | 4220 | `			if( is_step_negative ){` |
|       ! 0 | 4221 | `				goto negative_step_error;` |
|         - | 4222 | `			}` |
|        17 | 4223 | `			if( end_double - start_double < step_double ){` |
|         3 | 4224 | `				goto boundary_error;` |
|         - | 4225 | `			}` |
|        15 | 4226 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4227 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4228 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4229 | `			}` |
|        11 | 4230 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4231 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4232 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4233 | `					return rc;` |
|         - | 4234 | `				}` |
|        28 | 4235 | `			}` |
|         6 | 4236 | `		}else{` |
|         3 | 4237 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4238 | `				return rc;` |
|         - | 4239 | `			}` |
|         - | 4240 | `		}` |
|         9 | 4241 | `	}else{` |
|         - | 4242 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4243 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4244 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       103 | 4245 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4246 | `		sxu64 calc;` |
|       103 | 4247 | `		if( start_long > end_long ){` |
|         - | 4248 | `			/* Decreasing int range */` |
|        19 | 4249 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4250 | `				goto boundary_error;` |
|         - | 4251 | `			}` |
|        17 | 4252 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4253 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4254 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4255 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4256 | `			}` |
|        15 | 4257 | `			size = (sxu32)(calc + 1);` |
|       101 | 4258 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4259 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4260 | `					return rc;` |
|         - | 4261 | `				}` |
|        44 | 4262 | `			}` |
|        92 | 4263 | `		}else if( end_long > start_long ){` |
|         - | 4264 | `			/* Increasing int range */` |
|        79 | 4265 | `			if( is_step_negative ){` |
|         3 | 4266 | `				goto negative_step_error;` |
|         - | 4267 | `			}` |
|        77 | 4268 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4269 | `				goto boundary_error;` |
|         - | 4270 | `			}` |
|        75 | 4271 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        75 | 4272 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4273 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4274 | `			}` |
|        71 | 4275 | `			size = (sxu32)(calc + 1);` |
|    401659 | 4276 | `			for( i = 0 ; i < size ; ++i ){` |
|    401589 | 4277 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4278 | `					return rc;` |
|         - | 4279 | `				}` |
|    200795 | 4280 | `			}` |
|        36 | 4281 | `		}else{` |
|         7 | 4282 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4283 | `				return rc;` |
|         - | 4284 | `			}` |
|         - | 4285 | `		}` |
|         - | 4286 | `	}` |
|         - | 4287 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4288 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       107 | 4289 | `	ph7_result_value(pCtx,pArray);` |
|       107 | 4290 | `	return PH7_OK;` |
|         2 | 4291 | `negative_step_error:` |
|         5 | 4292 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4293 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4294 | `boundary_error:` |
|         9 | 4295 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4296 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        85 | 4297 | `}` |
|         - | 4298 | `/*` |
|         - | 4299 | ` * array array_values(array $array)` |
|         - | 4300 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4301 | ` * Parameters` |
|         - | 4302 | ` *  $array` |
|         - | 4303 | ` *   The input array.` |
|         - | 4304 | ` * Return` |
|         - | 4305 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4306 | ` */` |
|        48 | 4307 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4308 | `{` |
|         - | 4309 | `	ph7_hashmap_node *pNode;` |
|         - | 4310 | `	ph7_hashmap *pMap;` |
|         - | 4311 | `	ph7_value *pArray;` |
|         - | 4312 | `	ph7_value *pObj;` |
|         - | 4313 | `	sxu32 n;` |
|        51 | 4314 | `	if( nArg != 1 ){` |
|         - | 4315 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4316 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4317 | `			"ArgumentCountError",` |
|         - | 4318 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4319 | `			nArg` |
|         - | 4320 | `			);` |
|         - | 4321 | `	}` |
|         - | 4322 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4323 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4324 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4325 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4326 | `			"TypeError",` |
|         - | 4327 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4328 | `			ph7_type_name(apArg[0])` |
|         - | 4329 | `			);` |
|         - | 4330 | `	}` |
|         - | 4331 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4332 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4333 | `	/* Create a new array */` |
|        46 | 4334 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4335 | `	if( pArray == 0 ){` |
|       ! 0 | 4336 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4337 | `		return PH7_OK;` |
|         - | 4338 | `	}` |
|         - | 4339 | `	/* Perform the requested operation */` |
|        46 | 4340 | `	pNode = pMap->pFirst;` |
|       144 | 4341 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4342 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4343 | `		if( pObj ){` |
|         - | 4344 | `			/* perform the insertion */` |
|       100 | 4345 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4346 | `		}` |
|         - | 4347 | `		/* Point to the next entry */` |
|       100 | 4348 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4349 | `	}` |
|         - | 4350 | `	/* return the new array */` |
|        46 | 4351 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4352 | `	return PH7_OK;` |
|        27 | 4353 | `}` |
|         - | 4354 | `/*` |
|         - | 4355 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4356 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4357 | ` * Parameters` |
|         - | 4358 | ` *  $input` |
|         - | 4359 | ` *   An array containing keys to return.` |
|         - | 4360 | ` * $search_value` |
|         - | 4361 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4362 | ` * $strict` |
|         - | 4363 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4364 | ` * Return` |
|         - | 4365 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4366 | ` */` |
|       180 | 4367 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4368 | `{` |
|         - | 4369 | `	ph7_hashmap_node *pNode;` |
|         - | 4370 | `	ph7_hashmap *pMap;` |
|         - | 4371 | `	ph7_value *pArray;` |
|         - | 4372 | `	ph7_value sObj;` |
|         - | 4373 | `	ph7_value sVal;` |
|         - | 4374 | `	SyString sKey;` |
|         - | 4375 | `	int bStrict;` |
|         - | 4376 | `	sxi32 rc;` |
|         - | 4377 | `	sxu32 n;` |
|       184 | 4378 | `	if( nArg < 1 ){` |
|         - | 4379 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4380 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4381 | `			"ArgumentCountError",` |
|         - | 4382 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4383 | `			);` |
|         - | 4384 | `	}` |
|         - | 4385 | `	/* Make sure we are dealing with a valid hashmap */` |
|       184 | 4386 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4387 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4388 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4389 | `			"TypeError",` |
|         - | 4390 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4391 | `			ph7_type_name(apArg[0])` |
|         - | 4392 | `			);` |
|         - | 4393 | `	}` |
|         - | 4394 | `	/* Point to the internal representation of the input hashmap */` |
|       181 | 4395 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4396 | `	/* Create a new array */` |
|       181 | 4397 | `	pArray = ph7_context_new_array(pCtx);` |
|       181 | 4398 | `	if( pArray == 0 ){` |
|       ! 0 | 4399 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4400 | `		return PH7_OK;` |
|         - | 4401 | `	}` |
|       181 | 4402 | `	bStrict = FALSE;` |
|       181 | 4403 | `	if( nArg > 2 ){` |
|         - | 4404 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4405 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4406 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4407 | `				"TypeError",` |
|         - | 4408 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4409 | `				ph7_type_name(apArg[2])` |
|         - | 4410 | `				);` |
|         - | 4411 | `		}` |
|         9 | 4412 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4413 | `	}` |
|         - | 4414 | `	/* Perform the requested operation */` |
|       181 | 4415 | `	pNode = pMap->pFirst;` |
|       181 | 4416 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1569 | 4417 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1391 | 4418 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       199 | 4419 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|       101 | 4420 | `		}else{` |
|      1194 | 4421 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1194 | 4422 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4423 | `		}` |
|      1391 | 4424 | `		rc = 0;` |
|      1391 | 4425 | `		if( nArg > 1 ){` |
|        65 | 4426 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4427 | `			if( pValue ){` |
|         - | 4428 | `				ph7_value sNeedle;` |
|        65 | 4429 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4430 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4431 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4432 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4433 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4434 | `				 * corrupt every later comparison. */` |
|        65 | 4435 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4436 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4437 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4438 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4439 | `			}` |
|        32 | 4440 | `		}` |
|      1391 | 4441 | `		if( rc == 0 ){` |
|         - | 4442 | `			/* Perform the insertion */` |
|      1359 | 4443 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       678 | 4444 | `		}` |
|      1391 | 4445 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4446 | `		/* Point to the next entry */` |
|      1391 | 4447 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       697 | 4448 | `	}` |
|         - | 4449 | `	/* return the new array */` |
|       181 | 4450 | `	ph7_result_value(pCtx,pArray);` |
|       181 | 4451 | `	return PH7_OK;` |
|        94 | 4452 | `}` |
|         - | 4453 | `/*` |
|         - | 4454 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4455 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4456 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4457 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4458 | ` * Parameters` |
|         - | 4459 | ` *  $arr1` |
|         - | 4460 | ` *   First array` |
|         - | 4461 | ` *  $arr2` |
|         - | 4462 | ` *   Second array` |
|         - | 4463 | ` * Return` |
|         - | 4464 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4465 | ` * Note` |
|         - | 4466 | ` *  This function is a symisc eXtension.` |
|         - | 4467 | ` */` |
|         4 | 4468 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4469 | `{` |
|         - | 4470 | `	ph7_hashmap *p1,*p2;` |
|         - | 4471 | `	int rc;` |
|         5 | 4472 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4473 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4474 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4475 | `		return PH7_OK;` |
|         - | 4476 | `	}` |
|         - | 4477 | `	/* Point to the hashmaps */` |
|         5 | 4478 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4479 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4480 | `	rc = (p1 == p2);` |
|         - | 4481 | `	/* Same instance? */` |
|         5 | 4482 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4483 | `	return PH7_OK;` |
|         3 | 4484 | `}` |
|         - | 4485 | `/*` |
|         - | 4486 | ` * array array_merge(array ...$arrays)` |
|         - | 4487 | ` *  Merge one or more arrays.` |
|         - | 4488 | ` * Parameters` |
|         - | 4489 | ` *  ...$arrays` |
|         - | 4490 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4491 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4492 | ` * Return` |
|         - | 4493 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4494 | ` *  with no arguments.` |
|         - | 4495 | ` */` |
|      1128 | 4496 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4497 | `{` |
|         - | 4498 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4499 | `	ph7_value *pArray;` |
|         - | 4500 | `	int i;` |
|         - | 4501 | `	/* Create a new array */` |
|      1133 | 4502 | `	pArray = ph7_context_new_array(pCtx);` |
|      1133 | 4503 | `	if( pArray == 0 ){` |
|       ! 0 | 4504 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4505 | `		return PH7_OK;` |
|         - | 4506 | `	}` |
|         - | 4507 | `	/* Point to the internal representation of the hashmap */` |
|      1133 | 4508 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4509 | `	/* Start merging */` |
|      3365 | 4510 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4511 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2241 | 4512 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4513 | `			/* Type mismatch -> TypeError */` |
|         8 | 4514 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4515 | `				"TypeError",` |
|         - | 4516 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4517 | `				i + 1,` |
|         4 | 4518 | `				ph7_type_name(apArg[i])` |
|         - | 4519 | `				);` |
|       ! 0 | 4520 | `		}else{` |
|      2237 | 4521 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4522 | `			/* Merge the two hashmaps */` |
|      2237 | 4523 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4524 | `		}` |
|      1121 | 4525 | `	}` |
|         - | 4526 | `	/* Return the freshly created array */` |
|      1129 | 4527 | `	ph7_result_value(pCtx,pArray);` |
|      1129 | 4528 | `	return PH7_OK;` |
|       569 | 4529 | `}` |
|         - | 4530 | `/*` |
|         - | 4531 | ` * array array_copy(array $source)` |
|         - | 4532 | ` *  Make a blind copy of the target array.` |
|         - | 4533 | ` * Parameters` |
|         - | 4534 | ` *  $source` |
|         - | 4535 | ` *   Target array` |
|         - | 4536 | ` * Return` |
|         - | 4537 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4538 | ` * Note` |
|         - | 4539 | ` *  This function is a symisc eXtension.` |
|         - | 4540 | ` */` |
|        18 | 4541 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4542 | `{` |
|         - | 4543 | `	ph7_hashmap *pMap;` |
|         - | 4544 | `	ph7_value *pArray;` |
|        19 | 4545 | `	if( nArg < 1 ){` |
|         - | 4546 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4547 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4548 | `		return PH7_OK;` |
|         - | 4549 | `	}` |
|         - | 4550 | `	/* Create a new array */` |
|        19 | 4551 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4552 | `	if( pArray == 0 ){` |
|       ! 0 | 4553 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4554 | `		return PH7_OK;` |
|         - | 4555 | `	}` |
|         - | 4556 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4557 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4558 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4559 | `		/* Point to the internal representation of the source */` |
|        19 | 4560 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4561 | `		/* Perform the copy */` |
|        19 | 4562 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4563 | `	}else{` |
|         - | 4564 | `		/* Simple insertion */` |
|       ! 0 | 4565 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4566 | `	}` |
|         - | 4567 | `	/* Return the duplicated array */` |
|        19 | 4568 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4569 | `	return PH7_OK;` |
|        10 | 4570 | `}` |
|         - | 4571 | `/*` |
|         - | 4572 | ` * bool array_erase(array $source)` |
|         - | 4573 | ` *  Remove all elements from a given array.` |
|         - | 4574 | ` * Parameters` |
|         - | 4575 | ` *  $source` |
|         - | 4576 | ` *   Target array` |
|         - | 4577 | ` * Return` |
|         - | 4578 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4579 | ` * Note` |
|         - | 4580 | ` *  This function is a symisc eXtension.` |
|         - | 4581 | ` */` |
|        26 | 4582 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4583 | `{` |
|         - | 4584 | `	ph7_hashmap *pMap;` |
|        28 | 4585 | `	if( nArg < 1 ){` |
|         - | 4586 | `		/* Missing arguments */` |
|       ! 0 | 4587 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4588 | `		return PH7_OK;` |
|         - | 4589 | `	}` |
|         - | 4590 | `	/* Point to the target hashmap */` |
|        28 | 4591 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4592 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4593 | `	/* Erase */` |
|        28 | 4594 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4595 | `	return PH7_OK;` |
|        15 | 4596 | `}` |
|         - | 4597 | `/*` |
|         - | 4598 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4599 | ` *  Extract a slice of the array.` |
|         - | 4600 | ` * Parameters` |
|         - | 4601 | ` *  $array` |
|         - | 4602 | ` *    The input array.` |
|         - | 4603 | ` * $offset` |
|         - | 4604 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4605 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4606 | ` * $length (optional, nullable)` |
|         - | 4607 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4608 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4609 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4610 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4611 | ` * $preserve_keys (optional)` |
|         - | 4612 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4613 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4614 | ` * Return` |
|         - | 4615 | ` *   The new slice.` |
|         - | 4616 | ` */` |
|        66 | 4617 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4618 | `{` |
|         - | 4619 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4620 | `	ph7_hashmap_node *pCur;` |
|         - | 4621 | `	ph7_value *pArray;` |
|         - | 4622 | `	int iLength,iOfft;` |
|         - | 4623 | `	int bPreserve;` |
|         - | 4624 | `	sxi32 rc;` |
|        71 | 4625 | `	if( nArg < 2 ){` |
|       ! 0 | 4626 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4627 | `			"ArgumentCountError",` |
|         - | 4628 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4629 | `			nArg` |
|         - | 4630 | `			);` |
|         - | 4631 | `	}` |
|        71 | 4632 | `	if( nArg > 4 ){` |
|         4 | 4633 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4634 | `			"ArgumentCountError",` |
|         - | 4635 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4636 | `			nArg` |
|         - | 4637 | `			);` |
|         - | 4638 | `	}` |
|        69 | 4639 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4640 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4641 | `			"TypeError",` |
|         - | 4642 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4643 | `			ph7_type_name(apArg[0])` |
|         - | 4644 | `			);` |
|         - | 4645 | `	}` |
|         - | 4646 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        92 | 4647 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        95 | 4648 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4649 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4650 | `			"TypeError",` |
|         - | 4651 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4652 | `			ph7_type_name(apArg[1])` |
|         - | 4653 | `			);` |
|         - | 4654 | `	}` |
|         - | 4655 | `	/* Validate $length type if provided: nullable int */` |
|        65 | 4656 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        56 | 4657 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        56 | 4658 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4659 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4660 | `				"TypeError",` |
|         - | 4661 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4662 | `				ph7_type_name(apArg[2])` |
|         - | 4663 | `				);` |
|         - | 4664 | `		}` |
|        18 | 4665 | `	}` |
|         - | 4666 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        63 | 4667 | `	if( nArg > 3 ){` |
|         7 | 4668 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4669 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4670 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4671 | `				"TypeError",` |
|         - | 4672 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4673 | `				ph7_type_name(apArg[3])` |
|         - | 4674 | `				);` |
|         - | 4675 | `		}` |
|         2 | 4676 | `	}` |
|         - | 4677 | `	/* Point the internal representation of the target array */` |
|        63 | 4678 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        63 | 4679 | `	bPreserve = FALSE;` |
|         - | 4680 | `	/* Get the offset */` |
|         - | 4681 | `	{` |
|        63 | 4682 | `		sxi64 iTmp = 0;` |
|        63 | 4683 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        63 | 4684 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4685 | `			return rcArg;` |
|         - | 4686 | `		}` |
|        63 | 4687 | `		iOfft = (int)iTmp;` |
|         - | 4688 | `	}` |
|        63 | 4689 | `	if( iOfft < 0 ){` |
|         5 | 4690 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4691 | `		if( iOfft < 0 ){` |
|         3 | 4692 | `			iOfft = 0;` |
|         1 | 4693 | `		}` |
|         2 | 4694 | `	}` |
|        63 | 4695 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4696 | `		/* Offset past end of array, return empty array */` |
|         5 | 4697 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4698 | `		if( pArray == 0 ){` |
|       ! 0 | 4699 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4700 | `			return PH7_OK;` |
|         - | 4701 | `		}` |
|         5 | 4702 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4703 | `		return PH7_OK;` |
|         - | 4704 | `	}` |
|         - | 4705 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        59 | 4706 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        59 | 4707 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        37 | 4708 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        37 | 4709 | `		if( iLength < 0 ){` |
|         5 | 4710 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4711 | `		}` |
|        37 | 4712 | `		if( iLength < 0 ){` |
|         3 | 4713 | `			iLength = 0;` |
|         1 | 4714 | `		}` |
|        37 | 4715 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4716 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4717 | `		}` |
|        18 | 4718 | `	}` |
|        59 | 4719 | `	if( nArg > 3 ){` |
|         5 | 4720 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4721 | `	}` |
|         - | 4722 | `	/* Create a new array */` |
|        59 | 4723 | `	pArray = ph7_context_new_array(pCtx);` |
|        59 | 4724 | `	if( pArray == 0 ){` |
|       ! 0 | 4725 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4726 | `		return PH7_OK;` |
|         - | 4727 | `	}` |
|        59 | 4728 | `	if( iLength < 1 ){` |
|         - | 4729 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4730 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4731 | `		return PH7_OK;` |
|         - | 4732 | `	}` |
|         - | 4733 | `	/* Point to the desired entry */` |
|        55 | 4734 | `	pCur = pSrc->pFirst;` |
|        54 | 4735 | `	for(;;){` |
|       113 | 4736 | `		if( iOfft < 1 ){` |
|        55 | 4737 | `			break;` |
|         - | 4738 | `		}` |
|         - | 4739 | `		/* Point to the next entry */` |
|        63 | 4740 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        63 | 4741 | `		iOfft--;` |
|         5 | 4742 | `	}` |
|         - | 4743 | `	/* Point to the internal representation of the hashmap */` |
|        55 | 4744 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       106 | 4745 | `	for(;;){` |
|       217 | 4746 | `		if( iLength < 1 ){` |
|        55 | 4747 | `			break;` |
|         - | 4748 | `		}` |
|         - | 4749 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4750 | `		{` |
|       167 | 4751 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|       167 | 4752 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4753 | `		}` |
|       167 | 4754 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4755 | `			break;` |
|         - | 4756 | `		}` |
|         - | 4757 | `		/* Point to the next entry */` |
|       167 | 4758 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       167 | 4759 | `		iLength--;` |
|         5 | 4760 | `	}` |
|         - | 4761 | `	/* Return the freshly created array */` |
|        55 | 4762 | `	ph7_result_value(pCtx,pArray);` |
|        55 | 4763 | `	return PH7_OK;` |
|        38 | 4764 | `}` |
|         - | 4765 | `/*` |
|         - | 4766 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4767 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4768 | ` * beginning (becomes the new pFirst).` |
|         - | 4769 | ` */` |
|        38 | 4770 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4771 | `{` |
|         - | 4772 | `	ph7_hashmap_node *pNode;` |
|         - | 4773 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4774 | `	pNode = pMap->pLast;` |
|        39 | 4775 | `	if( pNode == 0 ){` |
|       ! 0 | 4776 | `		return;` |
|         - | 4777 | `	}` |
|        39 | 4778 | `	if( pNode->pNext == 0 ){` |
|         - | 4779 | `		/* Only node in the list, nothing to move */` |
|         5 | 4780 | `		return;` |
|         - | 4781 | `	}` |
|        35 | 4782 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4783 | `		/* Already in the correct position */` |
|         9 | 4784 | `		return;` |
|         - | 4785 | `	}` |
|         - | 4786 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4787 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4788 | `	pMap->pLast->pPrev = 0;` |
|         - | 4789 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4790 | `	if( pAfter == 0 ){` |
|         - | 4791 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4792 | `		pNode->pNext = 0;` |
|         3 | 4793 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4794 | `		if( pMap->pFirst ){` |
|         3 | 4795 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4796 | `		}` |
|         3 | 4797 | `		pMap->pFirst = pNode;` |
|         2 | 4798 | `	}else{` |
|        25 | 4799 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4800 | `		pNode->pPrev = pOldNext;` |
|        25 | 4801 | `		pNode->pNext = pAfter;` |
|        25 | 4802 | `		pAfter->pPrev = pNode;` |
|        25 | 4803 | `		if( pOldNext ){` |
|        25 | 4804 | `			pOldNext->pNext = pNode;` |
|        13 | 4805 | `		}else{` |
|       ! 0 | 4806 | `			pMap->pLast = pNode;` |
|         - | 4807 | `		}` |
|         - | 4808 | `	}` |
|        20 | 4809 | `}` |
|         - | 4810 | `/*` |
|         - | 4811 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4812 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4813 | ` * Parameters` |
|         - | 4814 | ` *  $array` |
|         - | 4815 | ` *    The input array.` |
|         - | 4816 | ` *  $offset` |
|         - | 4817 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4818 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4819 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4820 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4821 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4822 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4823 | ` *  $length (optional)` |
|         - | 4824 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4825 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4826 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4827 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4828 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4829 | ` *  $replacement (optional)` |
|         - | 4830 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4831 | ` *    with elements from this array.` |
|         - | 4832 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4833 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4834 | ` *    offset.` |
|         - | 4835 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4836 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4837 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4838 | ` * Return` |
|         - | 4839 | ` *   A new array consisting of the extracted elements.` |
|         - | 4840 | ` */` |
|        64 | 4841 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4842 | `{` |
|         - | 4843 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4844 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4845 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4846 | `	int iLength,iOfft,i;` |
|         - | 4847 | `	sxi32 rc;` |
|        66 | 4848 | `	if( nArg < 2 ){` |
|       ! 0 | 4849 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4850 | `			"ArgumentCountError",` |
|         - | 4851 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4852 | `			nArg` |
|         - | 4853 | `			);` |
|         - | 4854 | `	}` |
|        66 | 4855 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4856 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4857 | `			"TypeError",` |
|         - | 4858 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4859 | `			ph7_type_name(apArg[0])` |
|         - | 4860 | `			);` |
|         - | 4861 | `	}` |
|         - | 4862 | `	/* Point to the internal representation of the target array */` |
|        63 | 4863 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4864 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4865 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4866 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4867 | `	if( iOfft < 0 ){` |
|         9 | 4868 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4869 | `		if( iOfft < 0 ){` |
|         3 | 4870 | `			iOfft = 0;` |
|         2 | 4871 | `		}` |
|        59 | 4872 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4873 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4874 | `	}` |
|         - | 4875 | `	/* Get the length and clamp to valid range.` |
|         - | 4876 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4877 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4878 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4879 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4880 | `		if( iLength < 0 ){` |
|         7 | 4881 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4882 | `			if( iLength < 0 ){` |
|         3 | 4883 | `				iLength = 0;` |
|         1 | 4884 | `			}` |
|         3 | 4885 | `		}` |
|        45 | 4886 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4887 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4888 | `		}` |
|        22 | 4889 | `	}` |
|         - | 4890 | `	/* Create the result array for removed elements */` |
|        63 | 4891 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4892 | `	if( pArray == 0 ){` |
|       ! 0 | 4893 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4894 | `		return PH7_OK;` |
|         - | 4895 | `	}` |
|         - | 4896 | `	/* Get replacement array if provided */` |
|        63 | 4897 | `	pRep = 0;` |
|        63 | 4898 | `	if( nArg > 3 ){` |
|        27 | 4899 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4900 | `			/* Perform an array cast */` |
|         3 | 4901 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4902 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4903 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4904 | `			}` |
|         2 | 4905 | `		}else{` |
|        25 | 4906 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4907 | `		}` |
|        27 | 4908 | `		if( pRep ){` |
|         - | 4909 | `			/* Reset the loop cursor */` |
|        27 | 4910 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4911 | `		}` |
|        13 | 4912 | `	}` |
|         - | 4913 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4914 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4915 | `	/* Navigate to the offset position */` |
|        63 | 4916 | `	pCur = pSrc->pFirst;` |
|       131 | 4917 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4918 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4919 | `	}` |
|         - | 4920 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4921 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4922 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4923 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4924 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4925 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4926 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4927 | `		pPrev = pCur->pPrev;` |
|        79 | 4928 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4929 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4930 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4931 | `			break;` |
|         - | 4932 | `		}` |
|        79 | 4933 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4934 | `	}` |
|         - | 4935 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4936 | `	if( pRep ){` |
|         - | 4937 | `		ph7_value sSafeVal;` |
|        78 | 4938 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4939 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4940 | `			if( pRvalue ){` |
|         - | 4941 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4942 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4943 | `				 * since it points into that same pool. */` |
|        39 | 4944 | `				sSafeVal = *pRvalue;` |
|        39 | 4945 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4946 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4947 | `					pNewNode = pSrc->pLast;` |
|        39 | 4948 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4949 | `					pInsertAfter = pNewNode;` |
|        19 | 4950 | `				}` |
|        19 | 4951 | `			}` |
|         1 | 4952 | `		}` |
|        13 | 4953 | `	}` |
|         - | 4954 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4955 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4956 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4957 | `	 * and removals left gaps. */` |
|         - | 4958 | `	{` |
|        63 | 4959 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4960 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4961 | `		pSrc->iNextIdx = 0;` |
|       233 | 4962 | `		while( n > 0 ){` |
|       171 | 4963 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4964 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4965 | `			}` |
|       171 | 4966 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4967 | `			n--;` |
|         1 | 4968 | `		}` |
|        63 | 4969 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4970 | `	}` |
|         - | 4971 | `	/* Return the freshly created array */` |
|        63 | 4972 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4973 | `	return PH7_OK;` |
|        34 | 4974 | `}` |
|         - | 4975 | `/*` |
|         - | 4976 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4977 | ` *  Checks if a value exists in an array.` |
|         - | 4978 | ` * Parameters` |
|         - | 4979 | ` *  $needle` |
|         - | 4980 | ` *   The searched value.` |
|         - | 4981 | ` *   Note:` |
|         - | 4982 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4983 | ` * $haystack` |
|         - | 4984 | ` *  The target array.` |
|         - | 4985 | ` * $strict` |
|         - | 4986 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4987 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4988 | ` */` |
|     33938 | 4989 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4990 | `{` |
|         - | 4991 | `	ph7_value *pNeedle;` |
|         - | 4992 | `	int bStrict;` |
|         - | 4993 | `	int rc;` |
|     33943 | 4994 | `	if( nArg < 2 ){` |
|         - | 4995 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4996 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4997 | `		return PH7_OK;` |
|         - | 4998 | `	}` |
|     33943 | 4999 | `	pNeedle = apArg[0];` |
|     33943 | 5000 | `	bStrict = 0;` |
|     33943 | 5001 | `	if( nArg > 2 ){` |
|        62 | 5002 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        30 | 5003 | `	}` |
|     33943 | 5004 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 5005 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 5006 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 5007 | `		/* Set the comparison result */` |
|       ! 0 | 5008 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 5009 | `		return PH7_OK;` |
|         - | 5010 | `	}` |
|         - | 5011 | `	/* Perform the lookup */` |
|     33943 | 5012 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 5013 | `	/* Lookup result */` |
|     33943 | 5014 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33943 | 5015 | `	return PH7_OK;` |
|     16974 | 5016 | `}` |
|         - | 5017 | `/*` |
|         - | 5018 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 5019 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 5020 | ` * Parameters` |
|         - | 5021 | ` * $needle` |
|         - | 5022 | ` *   The searched value.` |
|         - | 5023 | ` * $haystack` |
|         - | 5024 | ` *   The array.` |
|         - | 5025 | ` * $strict` |
|         - | 5026 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 5027 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 5028 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 5029 | ` * Return` |
|         - | 5030 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 5031 | ` */` |
|        28 | 5032 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 5033 | `{` |
|         - | 5034 | `	ph7_hashmap_node *pEntry;` |
|         - | 5035 | `	ph7_value *pVal,sNeedle;` |
|         - | 5036 | `	ph7_hashmap *pMap;` |
|         - | 5037 | `	ph7_value sVal;` |
|         - | 5038 | `	int bStrict;` |
|         - | 5039 | `	sxu32 n;` |
|         - | 5040 | `	int rc;` |
|        30 | 5041 | `	if( nArg < 2 ){` |
|         - | 5042 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 5043 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5044 | `			"ArgumentCountError",` |
|         - | 5045 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 5046 | `			nArg` |
|         - | 5047 | `			);` |
|         - | 5048 | `	}` |
|        30 | 5049 | `	bStrict = FALSE;` |
|        30 | 5050 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 5051 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 5052 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5053 | `			"TypeError",` |
|         - | 5054 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 5055 | `			ph7_type_name(apArg[1])` |
|         - | 5056 | `			);` |
|         - | 5057 | `	}` |
|        27 | 5058 | `	if( nArg > 2 ){` |
|         - | 5059 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        13 | 5060 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 5061 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5062 | `				"TypeError",` |
|         - | 5063 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 5064 | `				ph7_type_name(apArg[2])` |
|         - | 5065 | `				);` |
|         - | 5066 | `		}` |
|        13 | 5067 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         6 | 5068 | `	}` |
|         - | 5069 | `	/* Point to the internal representation of the internal hashmap */` |
|        27 | 5070 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 5071 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        27 | 5072 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        27 | 5073 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        27 | 5074 | `	pEntry = pMap->pFirst;` |
|        27 | 5075 | `	n = pMap->nEntry;` |
|        29 | 5076 | `	for(;;){` |
|        59 | 5077 | `		if( !n ){` |
|         9 | 5078 | `			break;` |
|         - | 5079 | `		}` |
|         - | 5080 | `		/* Extract node value */` |
|        51 | 5081 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        51 | 5082 | `		if( pVal ){` |
|         - | 5083 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 5084 | `			 * can change their type.` |
|         - | 5085 | `			 */` |
|        51 | 5086 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        51 | 5087 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        51 | 5088 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        51 | 5089 | `			PH7_MemObjRelease(&sVal);` |
|        51 | 5090 | `			PH7_MemObjRelease(&sNeedle);` |
|        51 | 5091 | `			if( rc == 0 ){` |
|         - | 5092 | `				/* Match found,return key */` |
|        19 | 5093 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 5094 | `					/* INT key */` |
|        13 | 5095 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         7 | 5096 | `				}else{` |
|         7 | 5097 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5098 | `					/* Blob key */` |
|         7 | 5099 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 5100 | `				}` |
|        19 | 5101 | `				return PH7_OK;` |
|         - | 5102 | `			}` |
|        16 | 5103 | `		}` |
|         - | 5104 | `		/* Point to the next entry */` |
|        33 | 5105 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5106 | `		n--;` |
|         1 | 5107 | `	}` |
|         - | 5108 | `	/* No such value,return FALSE */` |
|         9 | 5109 | `	ph7_result_bool(pCtx,0);` |
|         9 | 5110 | `	return PH7_OK;` |
|        16 | 5111 | `}` |
|         - | 5112 | `/*` |
|         - | 5113 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 5114 | ` *  Computes the difference of arrays.` |
|         - | 5115 | ` * Parameters` |
|         - | 5116 | ` *  $array1` |
|         - | 5117 | ` *    The array to compare from` |
|         - | 5118 | ` *  $array2` |
|         - | 5119 | ` *    An array to compare against` |
|         - | 5120 | ` *  $...` |
|         - | 5121 | ` *   More arrays to compare against` |
|         - | 5122 | ` * Return` |
|         - | 5123 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5124 | ` *  are not present in any of the other arrays.` |
|         - | 5125 | ` */` |
|        20 | 5126 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5127 | `{` |
|         - | 5128 | `	ph7_hashmap_node *pEntry;` |
|         - | 5129 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5130 | `	ph7_value *pArray;` |
|         - | 5131 | `	ph7_value *pVal;` |
|         - | 5132 | `	sxi32 rc;` |
|         - | 5133 | `	sxu32 n;` |
|         - | 5134 | `	int i;` |
|         - | 5135 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5136 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5137 | `	 * debugging difficult. */` |
|        23 | 5138 | `	if( nArg < 1 ){` |
|       ! 0 | 5139 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5140 | `			"ArgumentCountError",` |
|         - | 5141 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5142 | `			nArg` |
|         - | 5143 | `			);` |
|         - | 5144 | `	}` |
|        23 | 5145 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5146 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5147 | `			"TypeError",` |
|         - | 5148 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5149 | `			ph7_type_name(apArg[0])` |
|         - | 5150 | `			);` |
|         - | 5151 | `	}` |
|        36 | 5152 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5153 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5154 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5155 | `				"TypeError",` |
|         - | 5156 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5157 | `				i + 1,` |
|         2 | 5158 | `				ph7_type_name(apArg[i])` |
|         - | 5159 | `				);` |
|         - | 5160 | `		}` |
|         9 | 5161 | `	}` |
|        17 | 5162 | `	if( nArg == 1 ){` |
|         - | 5163 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5164 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5165 | `		return PH7_OK;` |
|         - | 5166 | `	}` |
|         - | 5167 | `	/* Create a new array */` |
|        15 | 5168 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5169 | `	if( pArray == 0 ){` |
|       ! 0 | 5170 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5171 | `		return PH7_OK;` |
|         - | 5172 | `	}` |
|         - | 5173 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5174 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5175 | `	/* Perform the diff */` |
|        15 | 5176 | `	pEntry = pSrc->pFirst;` |
|        15 | 5177 | `	n = pSrc->nEntry;` |
|        27 | 5178 | `	for(;;){` |
|        55 | 5179 | `		if( n < 1 ){` |
|        15 | 5180 | `			break;` |
|         - | 5181 | `		}` |
|         - | 5182 | `		/* Extract the node value */` |
|        41 | 5183 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5184 | `		if( pVal ){` |
|        69 | 5185 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5186 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5187 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5188 | `				/* Perform the lookup */` |
|        45 | 5189 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5190 | `				if( rc == SXRET_OK ){` |
|         - | 5191 | `					/* Value exist */` |
|        17 | 5192 | `					break;` |
|         - | 5193 | `				}` |
|        15 | 5194 | `			}` |
|        41 | 5195 | `			if( i >= nArg ){` |
|         - | 5196 | `				/* Perform the insertion */` |
|        25 | 5197 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5198 | `			}` |
|        20 | 5199 | `		}` |
|         - | 5200 | `		/* Point to the next entry */` |
|        41 | 5201 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5202 | `		n--;` |
|         1 | 5203 | `	}` |
|         - | 5204 | `	/* Return the freshly created array */` |
|        15 | 5205 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5206 | `	return PH7_OK;` |
|        13 | 5207 | `}` |
|         - | 5208 | `/*` |
|         - | 5209 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5210 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5211 | ` * Parameters` |
|         - | 5212 | ` *  $array1` |
|         - | 5213 | ` *    The array to compare from` |
|         - | 5214 | ` *  $array2` |
|         - | 5215 | ` *    An array to compare against` |
|         - | 5216 | ` *  $...` |
|         - | 5217 | ` *   More arrays to compare against.` |
|         - | 5218 | ` * $callback` |
|         - | 5219 | ` *  The callback comparison function.` |
|         - | 5220 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5221 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5222 | ` *  than the second.` |
|         - | 5223 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5224 | ` * Return` |
|         - | 5225 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5226 | ` *  are not present in any of the other arrays.` |
|         - | 5227 | ` */` |
|        20 | 5228 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5229 | `{` |
|         - | 5230 | `	ph7_hashmap_node *pEntry;` |
|         - | 5231 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5232 | `	ph7_value *pCallback;` |
|         - | 5233 | `	ph7_value *pArray;` |
|         - | 5234 | `	ph7_value *pVal;` |
|         - | 5235 | `	sxi32 rc;` |
|         - | 5236 | `	sxu32 n;` |
|         - | 5237 | `	int i;` |
|         - | 5238 |  |
|         - | 5239 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5240 | `	if( nArg < 2 ){` |
|       ! 0 | 5241 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5242 | `			"ArgumentCountError",` |
|         - | 5243 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5244 | `			nArg` |
|         - | 5245 | `			);` |
|         - | 5246 | `	}` |
|        25 | 5247 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5248 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5249 | `			"TypeError",` |
|         - | 5250 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5251 | `			ph7_type_name(apArg[0])` |
|         - | 5252 | `			);` |
|         - | 5253 | `	}` |
|         - | 5254 |  |
|        23 | 5255 | `	if( nArg == 2 ){` |
|         - | 5256 | `		/* Only the original array and the callback were provided. */` |
|         - | 5257 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5258 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5259 | `		 * validation order.` |
|         - | 5260 | `		 */` |
|         4 | 5261 | `	} else {` |
|         - | 5262 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5263 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5264 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5265 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5266 | `					"TypeError",` |
|         - | 5267 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5268 | `					i + 1,` |
|         6 | 5269 | `					ph7_type_name(apArg[i])` |
|         - | 5270 | `					);` |
|         - | 5271 | `			}` |
|         7 | 5272 | `		}` |
|         - | 5273 | `	}` |
|         - | 5274 |  |
|         - | 5275 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5276 | `	pCallback = apArg[nArg - 1];` |
|         - | 5277 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5278 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5279 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5280 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5281 | `				"TypeError",` |
|         - | 5282 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5283 | `				nArg` |
|         - | 5284 | `				);` |
|         - | 5285 | `		}` |
|         6 | 5286 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5287 | `			int len;` |
|         3 | 5288 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5289 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5290 | `				"TypeError",` |
|         - | 5291 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5292 | `				nArg,` |
|         1 | 5293 | `				zName` |
|         - | 5294 | `				);` |
|         - | 5295 | `		}` |
|         4 | 5296 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5297 | `			"TypeError",` |
|         - | 5298 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5299 | `			nArg` |
|         - | 5300 | `			);` |
|         - | 5301 | `	}` |
|         - | 5302 |  |
|         7 | 5303 | `	if( nArg == 2 ){` |
|         - | 5304 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5305 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5306 | `		return PH7_OK;` |
|         - | 5307 | `	}` |
|         - | 5308 |  |
|         - | 5309 | `	/* Create a new array */` |
|         5 | 5310 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5311 | `	if( pArray == 0 ){` |
|       ! 0 | 5312 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5313 | `		return PH7_OK;` |
|         - | 5314 | `	}` |
|         - | 5315 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5316 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5317 | `	/* Perform the diff */` |
|         5 | 5318 | `	pEntry = pSrc->pFirst;` |
|         5 | 5319 | `	n = pSrc->nEntry;` |
|         5 | 5320 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5321 | `	for(;;){` |
|        11 | 5322 | `		if( n < 1 ){` |
|         3 | 5323 | `			break;` |
|         - | 5324 | `		}` |
|         - | 5325 | `		/* Extract the node value */` |
|         9 | 5326 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5327 | `		if( pVal ){` |
|        15 | 5328 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5329 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5330 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5331 | `				/* Perform the lookup */` |
|         9 | 5332 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5333 | `				if( rc == SXRET_OK ){` |
|         - | 5334 | `					/* Value exist */` |
|         3 | 5335 | `					break;` |
|         - | 5336 | `				}` |
|         4 | 5337 | `			}` |
|         9 | 5338 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5339 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5340 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5341 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5342 | `				return PH7_EXCEPTION;` |
|         - | 5343 | `			}` |
|         7 | 5344 | `			if( i >= (nArg - 1)){` |
|         - | 5345 | `				/* Perform the insertion */` |
|         5 | 5346 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5347 | `			}` |
|         3 | 5348 | `		}` |
|         - | 5349 | `		/* Point to the next entry */` |
|         7 | 5350 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5351 | `		n--;` |
|         1 | 5352 | `	}` |
|         - | 5353 | `	/* Return the freshly created array */` |
|         3 | 5354 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5355 | `	return PH7_OK;` |
|        15 | 5356 | `}` |
|         - | 5357 | `/*` |
|         - | 5358 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5359 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5360 | ` * Parameters` |
|         - | 5361 | ` *  $array1` |
|         - | 5362 | ` *    The array to compare from` |
|         - | 5363 | ` *  $array2` |
|         - | 5364 | ` *    An array to compare against` |
|         - | 5365 | ` *  $...` |
|         - | 5366 | ` *   More arrays to compare against` |
|         - | 5367 | ` * Return` |
|         - | 5368 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5369 | ` *  are not present in any of the other arrays.` |
|         - | 5370 | ` */` |
|        20 | 5371 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5372 | `{` |
|         - | 5373 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5374 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5375 | `	ph7_value *pArray;` |
|         - | 5376 | `	ph7_value *pVal;` |
|         - | 5377 | `	sxi32 rc;` |
|         - | 5378 | `	sxu32 n;` |
|         - | 5379 | `	int i;` |
|         - | 5380 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5381 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5382 | `	 * accompanying integration tests to pass. */` |
|        24 | 5383 | `	if( nArg < 1 ){` |
|       ! 0 | 5384 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5385 | `			"ArgumentCountError",` |
|         - | 5386 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5387 | `			nArg` |
|         - | 5388 | `			);` |
|         - | 5389 | `	}` |
|        24 | 5390 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5391 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5392 | `			"TypeError",` |
|         - | 5393 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5394 | `			ph7_type_name(apArg[0])` |
|         - | 5395 | `			);` |
|         - | 5396 | `	}` |
|        37 | 5397 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5398 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5399 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5400 | `				"TypeError",` |
|         - | 5401 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5402 | `				i + 1,` |
|         4 | 5403 | `				ph7_type_name(apArg[i])` |
|         - | 5404 | `				);` |
|         - | 5405 | `		}` |
|        10 | 5406 | `	}` |
|        15 | 5407 | `	if( nArg == 1 ){` |
|         - | 5408 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5409 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5410 | `		return PH7_OK;` |
|         - | 5411 | `	}` |
|         - | 5412 | `	/* Create a new array */` |
|        13 | 5413 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5414 | `	if( pArray == 0 ){` |
|       ! 0 | 5415 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5416 | `		return PH7_OK;` |
|         - | 5417 | `	}` |
|         - | 5418 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5419 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5420 | `	/* Perform the diff */` |
|        13 | 5421 | `	pEntry = pSrc->pFirst;` |
|        13 | 5422 | `	n = pSrc->nEntry;` |
|        13 | 5423 | `	pN1 = pN2 = 0;` |
|        34 | 5424 | `	for(;;){` |
|         - | 5425 | `		int keep;` |
|        41 | 5426 | `		if( n < 1 ){` |
|        13 | 5427 | `			break;` |
|         - | 5428 | `		}` |
|         - | 5429 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5430 | `		keep = 1;` |
|        47 | 5431 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5432 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5433 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5434 | `			/* Perform a key lookup first */` |
|        33 | 5435 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5436 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5437 | `			}else{` |
|        21 | 5438 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5439 | `			}` |
|        33 | 5440 | `			if( rc != SXRET_OK ){` |
|         - | 5441 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5442 | `				continue;` |
|         - | 5443 | `			}` |
|         - | 5444 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5445 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5446 | `			if( pVal ){` |
|         - | 5447 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5448 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5449 | `				if( pVal2 ){` |
|         - | 5450 | `					ph7_value sV1,sV2;` |
|         - | 5451 | `					sxi32 cmp;` |
|         - | 5452 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5453 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5454 | `					 * null element used to come back bool(false) in the` |
|         - | 5455 | `					 * caller's array). */` |
|        17 | 5456 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5457 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5458 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5459 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5460 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5461 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5462 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5463 | `					if( cmp == 0 ){` |
|         - | 5464 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5465 | `						keep = 0;` |
|        15 | 5466 | `						break;` |
|         - | 5467 | `					}` |
|         1 | 5468 | `				}` |
|         1 | 5469 | `			}` |
|         2 | 5470 | `		}` |
|        29 | 5471 | `		if( keep ){` |
|         - | 5472 | `			/* Perform the insertion */` |
|        15 | 5473 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5474 | `		}` |
|         - | 5475 | `		/* Point to the next entry */` |
|        29 | 5476 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5477 | `		n--;` |
|         1 | 5478 | `	}` |
|         - | 5479 | `	/* Return the freshly created array */` |
|        13 | 5480 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5481 | `	return PH7_OK;` |
|        14 | 5482 | `}` |
|         - | 5483 | `/*` |
|         - | 5484 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5485 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5486 | ` *  by a user supplied callback function.` |
|         - | 5487 | ` * Parameters` |
|         - | 5488 | ` *  $array1` |
|         - | 5489 | ` *    The array to compare from` |
|         - | 5490 | ` *  $array2` |
|         - | 5491 | ` *    An array to compare against` |
|         - | 5492 | ` *  $...` |
|         - | 5493 | ` *   More arrays to compare against.` |
|         - | 5494 | ` *  $key_compare_func` |
|         - | 5495 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5496 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5497 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5498 | ` * Return` |
|         - | 5499 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5500 | ` *  are not present in any of the other arrays.` |
|         - | 5501 | ` */` |
|        22 | 5502 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5503 | `{` |
|         - | 5504 | `	ph7_hashmap_node *pEntry;` |
|         - | 5505 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5506 | `	ph7_value *pCallback;` |
|         - | 5507 | `	ph7_value *pArray;` |
|         - | 5508 | `	sxi32 rc;` |
|         - | 5509 | `	sxu32 n;` |
|         - | 5510 | `	int i;` |
|         - | 5511 |  |
|         - | 5512 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5513 | `	if( nArg < 2 ){` |
|       ! 0 | 5514 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5515 | `			"ArgumentCountError",` |
|         - | 5516 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5517 | `			nArg` |
|         - | 5518 | `			);` |
|         - | 5519 | `	}` |
|        26 | 5520 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5521 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5522 | `			"TypeError",` |
|         - | 5523 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5524 | `			ph7_type_name(apArg[0])` |
|         - | 5525 | `			);` |
|         - | 5526 | `	}` |
|         - | 5527 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5528 | `	 * expected to be a callback. */` |
|        38 | 5529 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5530 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5531 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5532 | `				"TypeError",` |
|         - | 5533 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5534 | `				i + 1,` |
|         2 | 5535 | `				ph7_type_name(apArg[i])` |
|         - | 5536 | `				);` |
|         - | 5537 | `		}` |
|         9 | 5538 | `	}` |
|         - | 5539 | `	/* Point to the callback value */` |
|        22 | 5540 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5541 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5542 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5543 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5544 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5545 | `		 * string given" which we also reproduce. */` |
|         9 | 5546 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5547 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5548 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5549 | `				"TypeError",` |
|         - | 5550 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5551 | `				nArg` |
|         - | 5552 | `				);` |
|         - | 5553 | `		}` |
|         6 | 5554 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5555 | `			/* neither array nor string */` |
|         8 | 5556 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5557 | `				"TypeError",` |
|         - | 5558 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5559 | `				nArg` |
|         - | 5560 | `				);` |
|         - | 5561 | `		}` |
|         - | 5562 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5563 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5564 | `			"TypeError",` |
|         - | 5565 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5566 | `			nArg,` |
|       ! 0 | 5567 | `			ph7_type_name(pCallback)` |
|         - | 5568 | `			);` |
|         - | 5569 | `	}` |
|        13 | 5570 | `	if( nArg == 2 ){` |
|         - | 5571 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5572 | `		 * input array. */` |
|         3 | 5573 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5574 | `		return PH7_OK;` |
|         - | 5575 | `	}` |
|         - | 5576 | `	/* Create a new array */` |
|        11 | 5577 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5578 | `	if( pArray == 0 ){` |
|       ! 0 | 5579 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5580 | `		return PH7_OK;` |
|         - | 5581 | `	}` |
|         - | 5582 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5583 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5584 | `	/* Perform the diff */` |
|        11 | 5585 | `	pEntry = pSrc->pFirst;` |
|        11 | 5586 | `	n = pSrc->nEntry;` |
|        21 | 5587 | `	for(;;){` |
|         - | 5588 | `		int keep;` |
|        27 | 5589 | `		if( n < 1 ){` |
|         9 | 5590 | `			break;` |
|         - | 5591 | `		}` |
|        19 | 5592 | `		keep = 1;` |
|        31 | 5593 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5594 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5595 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5596 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5597 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5598 | `			while( pIt ){` |
|         - | 5599 | `				/* build temporary key values for callback */` |
|         - | 5600 | `				ph7_value key1, key2, result;` |
|         - | 5601 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5602 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5603 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5604 | `				}else{` |
|         - | 5605 | `					SyString sStr;` |
|        33 | 5606 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5607 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5608 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5609 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5610 | `				}` |
|        33 | 5611 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5612 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5613 | `				}else{` |
|         - | 5614 | `					SyString sStr;` |
|        33 | 5615 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5616 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5617 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5618 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5619 | `				}` |
|        33 | 5620 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5621 | `				/* call user callback with (key1, key2) */` |
|         - | 5622 | `				{` |
|         - | 5623 | `					ph7_value *apK[2];` |
|        33 | 5624 | `					apK[0] = &key1;` |
|        33 | 5625 | `					apK[1] = &key2;` |
|        33 | 5626 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5627 | `				}` |
|        33 | 5628 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5629 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5630 | `					 * array_uintersect (which signal back from` |
|         - | 5631 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5632 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5633 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5634 | `					PH7_MemObjRelease(&result);` |
|         3 | 5635 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5636 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5637 | `					return PH7_EXCEPTION;` |
|         - | 5638 | `				}` |
|        31 | 5639 | `				if( rc == SXRET_OK ){` |
|        31 | 5640 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5641 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5642 | `					}` |
|        31 | 5643 | `					if( result.x.iVal == 0 ){` |
|         - | 5644 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5645 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5646 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5647 | `						if( pVal1 && pVal2 ){` |
|         - | 5648 | `							ph7_value sV1,sV2;` |
|         - | 5649 | `							sxi32 cmp;` |
|         - | 5650 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5651 | `							 * place and these are LIVE array elements. */` |
|        13 | 5652 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5653 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5654 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5655 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5656 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5657 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5658 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5659 | `							if( cmp == 0 ){` |
|         9 | 5660 | `								keep = 0;` |
|         9 | 5661 | `								PH7_MemObjRelease(&result);` |
|         - | 5662 | `								/* release keys too before breaking */` |
|         9 | 5663 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5664 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5665 | `								break;` |
|         - | 5666 | `							}` |
|         2 | 5667 | `						}` |
|         2 | 5668 | `					}` |
|        11 | 5669 | `				}` |
|        23 | 5670 | `				PH7_MemObjRelease(&result);` |
|        23 | 5671 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5672 | `				PH7_MemObjRelease(&key2);` |
|         - | 5673 | `				/* move to next node */` |
|        23 | 5674 | `				pIt = pIt->pPrev;` |
|        23 | 5675 | `				if( keep == 0 ) break;` |
|         1 | 5676 | `			}` |
|        21 | 5677 | `			if( keep == 0 ) break;` |
|         7 | 5678 | `		}` |
|        17 | 5679 | `		if( keep ){` |
|         - | 5680 | `			/* Perform the insertion */` |
|         9 | 5681 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5682 | `		}` |
|         - | 5683 | `		/* Point to the next entry */` |
|        17 | 5684 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5685 | `		n--;` |
|         1 | 5686 | `	}` |
|         - | 5687 | `	/* Return the freshly created array */` |
|         9 | 5688 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5689 | `	return PH7_OK;` |
|        15 | 5690 | `}` |
|         - | 5691 | `/*` |
|         - | 5692 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5693 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5694 | ` * Parameters` |
|         - | 5695 | ` *  $array1` |
|         - | 5696 | ` *    The array to compare from` |
|         - | 5697 | ` *  $array2` |
|         - | 5698 | ` *    An array to compare against` |
|         - | 5699 | ` *  $...` |
|         - | 5700 | ` *   More arrays to compare against` |
|         - | 5701 | ` * Return` |
|         - | 5702 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5703 | ` *  in any of the other arrays.` |
|         - | 5704 | ` * Note that NULL is returned on failure.` |
|         - | 5705 | ` */` |
|        12 | 5706 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5707 | `{` |
|         - | 5708 | `	ph7_hashmap_node *pEntry;` |
|         - | 5709 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5710 | `	ph7_value *pArray;` |
|         - | 5711 | `	sxi32 rc;` |
|         - | 5712 | `	sxu32 n;` |
|         - | 5713 | `	int i;` |
|         - | 5714 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5715 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5716 | `	 * helpers. */` |
|        15 | 5717 | `	if( nArg < 1 ){` |
|       ! 0 | 5718 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5719 | `			"ArgumentCountError",` |
|         - | 5720 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5721 | `			nArg` |
|         - | 5722 | `			);` |
|         - | 5723 | `	}` |
|        15 | 5724 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5725 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5726 | `			"TypeError",` |
|         - | 5727 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5728 | `			ph7_type_name(apArg[0])` |
|         - | 5729 | `			);` |
|         - | 5730 | `	}` |
|        20 | 5731 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5732 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5733 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5734 | `				"TypeError",` |
|         - | 5735 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5736 | `				i + 1,` |
|         2 | 5737 | `				ph7_type_name(apArg[i])` |
|         - | 5738 | `				);` |
|         - | 5739 | `		}` |
|         5 | 5740 | `	}` |
|         9 | 5741 | `	if( nArg == 1 ){` |
|         - | 5742 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5743 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5744 | `		return PH7_OK;` |
|         - | 5745 | `	}` |
|         - | 5746 | `	/* Create a new array */` |
|         7 | 5747 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5748 | `	if( pArray == 0 ){` |
|       ! 0 | 5749 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5750 | `		return PH7_OK;` |
|         - | 5751 | `	}` |
|         - | 5752 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5753 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5754 | `	/* Perfrom the diff */` |
|         7 | 5755 | `	pEntry = pSrc->pFirst;` |
|         7 | 5756 | `	n = pSrc->nEntry;` |
|        12 | 5757 | `	for(;;){` |
|        25 | 5758 | `		if( n < 1 ){` |
|         7 | 5759 | `			break;` |
|         - | 5760 | `		}` |
|        31 | 5761 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5762 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5763 | `				/* ignore */` |
|       ! 0 | 5764 | `				continue;` |
|         - | 5765 | `			}` |
|        23 | 5766 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5767 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5768 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5769 | `				/* Blob lookup */` |
|        17 | 5770 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5771 | `			}else{` |
|         - | 5772 | `				/* Int lookup */` |
|         7 | 5773 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5774 | `			}` |
|        23 | 5775 | `			if( rc == SXRET_OK ){` |
|         - | 5776 | `				/* Key exists,break immediately */` |
|        11 | 5777 | `				break;` |
|         - | 5778 | `			}` |
|         7 | 5779 | `		}` |
|        19 | 5780 | `		if( i >= nArg ){` |
|         - | 5781 | `			/* Perform the insertion */` |
|         9 | 5782 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5783 | `		}` |
|         - | 5784 | `		/* Point to the next entry */` |
|        19 | 5785 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5786 | `		n--;` |
|         1 | 5787 | `	}` |
|         - | 5788 | `	/* Return the freshly created array */` |
|         7 | 5789 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5790 | `	return PH7_OK;` |
|         9 | 5791 | `}` |
|         - | 5792 | `/*` |
|         - | 5793 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5794 | ` *  Computes the intersection of arrays.` |
|         - | 5795 | ` * Parameters` |
|         - | 5796 | ` *  $array1` |
|         - | 5797 | ` *    The array to compare from` |
|         - | 5798 | ` *  $array2` |
|         - | 5799 | ` *    An array to compare against` |
|         - | 5800 | ` *  $...` |
|         - | 5801 | ` *   More arrays to compare against` |
|         - | 5802 | ` * Return` |
|         - | 5803 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5804 | ` *  in all of the parameters.` |
|         - | 5805 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5806 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5807 | ` */` |
|        20 | 5808 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5809 | `{` |
|         - | 5810 | `	ph7_hashmap_node *pEntry;` |
|         - | 5811 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5812 | `	ph7_value *pArray;` |
|         - | 5813 | `	ph7_value *pVal;` |
|         - | 5814 | `	sxi32 rc;` |
|         - | 5815 | `	sxu32 n;` |
|         - | 5816 | `	int i;` |
|        23 | 5817 | `	if( nArg < 1 ){` |
|       ! 0 | 5818 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5819 | `			"ArgumentCountError",` |
|         - | 5820 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5821 | `			nArg` |
|         - | 5822 | `			);` |
|         - | 5823 | `	}` |
|        23 | 5824 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5825 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5826 | `			"TypeError",` |
|         - | 5827 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5828 | `			ph7_type_name(apArg[0])` |
|         - | 5829 | `			);` |
|         - | 5830 | `	}` |
|        36 | 5831 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5832 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5833 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5834 | `				"TypeError",` |
|         - | 5835 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5836 | `				i + 1,` |
|         2 | 5837 | `				ph7_type_name(apArg[i])` |
|         - | 5838 | `				);` |
|         - | 5839 | `		}` |
|         9 | 5840 | `	}` |
|        17 | 5841 | `	if( nArg == 1 ){` |
|         - | 5842 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5843 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5844 | `		return PH7_OK;` |
|         - | 5845 | `	}` |
|         - | 5846 | `	/* Create a new array */` |
|        15 | 5847 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5848 | `	if( pArray == 0 ){` |
|       ! 0 | 5849 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5850 | `		return PH7_OK;` |
|         - | 5851 | `	}` |
|         - | 5852 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5853 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5854 | `	/* Perform the intersection */` |
|        15 | 5855 | `	pEntry = pSrc->pFirst;` |
|        15 | 5856 | `	n = pSrc->nEntry;` |
|        31 | 5857 | `	for(;;){` |
|        63 | 5858 | `		if( n < 1 ){` |
|        15 | 5859 | `			break;` |
|         - | 5860 | `		}` |
|         - | 5861 | `		/* Extract the node value */` |
|        49 | 5862 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5863 | `		if( pVal ){` |
|        79 | 5864 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5865 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5866 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5867 | `				/* Perform the lookup */` |
|        55 | 5868 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5869 | `				if( rc != SXRET_OK ){` |
|         - | 5870 | `					/* Value does not exist */` |
|        25 | 5871 | `					break;` |
|         - | 5872 | `				}` |
|        16 | 5873 | `			}` |
|        49 | 5874 | `			if( i >= nArg ){` |
|         - | 5875 | `				/* Perform the insertion */` |
|        25 | 5876 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5877 | `			}` |
|        24 | 5878 | `		}` |
|         - | 5879 | `		/* Point to the next entry */` |
|        49 | 5880 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5881 | `		n--;` |
|         1 | 5882 | `	}` |
|         - | 5883 | `	/* Return the freshly created array */` |
|        15 | 5884 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5885 | `	return PH7_OK;` |
|        13 | 5886 | `}` |
|         - | 5887 | `/*` |
|         - | 5888 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5889 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5890 | ` * Parameters` |
|         - | 5891 | ` *  $array1` |
|         - | 5892 | ` *    The array to compare from` |
|         - | 5893 | ` *  $array2` |
|         - | 5894 | ` *    An array to compare against` |
|         - | 5895 | ` *  $...` |
|         - | 5896 | ` *   More arrays to compare against` |
|         - | 5897 | ` * Return` |
|         - | 5898 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5899 | ` *  in all the arguments, with matching keys.` |
|         - | 5900 | ` */` |
|        20 | 5901 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5902 | `{` |
|         - | 5903 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5904 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5905 | `	ph7_value *pArray;` |
|         - | 5906 | `	ph7_value *pVal;` |
|         - | 5907 | `	sxi32 rc;` |
|         - | 5908 | `	sxu32 n;` |
|         - | 5909 | `	int i;` |
|        23 | 5910 | `	if( nArg < 1 ){` |
|       ! 0 | 5911 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5912 | `			"ArgumentCountError",` |
|         - | 5913 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5914 | `			nArg` |
|         - | 5915 | `			);` |
|         - | 5916 | `	}` |
|        23 | 5917 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5918 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5919 | `			"TypeError",` |
|         - | 5920 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5921 | `			ph7_type_name(apArg[0])` |
|         - | 5922 | `			);` |
|         - | 5923 | `	}` |
|        36 | 5924 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5925 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5926 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5927 | `				"TypeError",` |
|         - | 5928 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5929 | `				i + 1,` |
|         2 | 5930 | `				ph7_type_name(apArg[i])` |
|         - | 5931 | `				);` |
|         - | 5932 | `		}` |
|         9 | 5933 | `	}` |
|        17 | 5934 | `	if( nArg == 1 ){` |
|         - | 5935 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5936 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5937 | `		return PH7_OK;` |
|         - | 5938 | `	}` |
|         - | 5939 | `	/* Create a new array */` |
|        15 | 5940 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5941 | `	if( pArray == 0 ){` |
|       ! 0 | 5942 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5943 | `		return PH7_OK;` |
|         - | 5944 | `	}` |
|         - | 5945 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5946 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5947 | `	/* Perform the intersection */` |
|        15 | 5948 | `	pEntry = pSrc->pFirst;` |
|        15 | 5949 | `	n = pSrc->nEntry;` |
|        15 | 5950 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5951 | `	for(;;){` |
|        47 | 5952 | `		if( n < 1 ){` |
|        15 | 5953 | `			break;` |
|         - | 5954 | `		}` |
|         - | 5955 | `		/* Extract the node value */` |
|        33 | 5956 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5957 | `		if( pVal ){` |
|        53 | 5958 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5959 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5960 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5961 | `				/* Perform a key lookup first */` |
|        37 | 5962 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5963 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5964 | `				}else{` |
|        23 | 5965 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5966 | `				}` |
|        37 | 5967 | `				if( rc != SXRET_OK ){` |
|         - | 5968 | `					/* No such key,break immediately */` |
|         7 | 5969 | `					break;` |
|         - | 5970 | `				}` |
|         - | 5971 | `				/* Perform the lookup */` |
|        31 | 5972 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5973 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5974 | `					/* Value does not exist */` |
|         6 | 5975 | `					break;` |
|         - | 5976 | `				}` |
|        11 | 5977 | `			}` |
|        33 | 5978 | `			if( i >= nArg ){` |
|         - | 5979 | `				/* Perform the insertion */` |
|        17 | 5980 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5981 | `			}` |
|        16 | 5982 | `		}` |
|         - | 5983 | `		/* Point to the next entry */` |
|        33 | 5984 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5985 | `		n--;` |
|         1 | 5986 | `	}` |
|         - | 5987 | `	/* Return the freshly created array */` |
|        15 | 5988 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5989 | `	return PH7_OK;` |
|        13 | 5990 | `}` |
|         - | 5991 | `/*` |
|         - | 5992 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5993 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5994 | ` * Parameters` |
|         - | 5995 | ` *  $array1` |
|         - | 5996 | ` *    The array to compare from` |
|         - | 5997 | ` *  $...` |
|         - | 5998 | ` *   More arrays to compare against` |
|         - | 5999 | ` * Return` |
|         - | 6000 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 6001 | ` *  have keys that are present in all arguments.` |
|         - | 6002 | ` * Note that NULL is returned on failure.` |
|         - | 6003 | ` */` |
|        20 | 6004 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6005 | `{` |
|         - | 6006 | `	ph7_hashmap_node *pEntry;` |
|         - | 6007 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 6008 | `	ph7_value *pArray;` |
|         - | 6009 | `	sxi32 rc;` |
|         - | 6010 | `	sxu32 n;` |
|         - | 6011 | `	int i;` |
|        23 | 6012 | `	if( nArg < 1 ){` |
|       ! 0 | 6013 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6014 | `			"ArgumentCountError",` |
|         - | 6015 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 6016 | `			nArg` |
|         - | 6017 | `			);` |
|         - | 6018 | `	}` |
|        23 | 6019 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6020 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6021 | `			"TypeError",` |
|         - | 6022 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6023 | `			ph7_type_name(apArg[0])` |
|         - | 6024 | `			);` |
|         - | 6025 | `	}` |
|        36 | 6026 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 6027 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6028 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6029 | `				"TypeError",` |
|         - | 6030 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 6031 | `				i + 1,` |
|         2 | 6032 | `				ph7_type_name(apArg[i])` |
|         - | 6033 | `				);` |
|         - | 6034 | `		}` |
|         9 | 6035 | `	}` |
|        17 | 6036 | `	if( nArg == 1 ){` |
|         - | 6037 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 6038 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 6039 | `		return PH7_OK;` |
|         - | 6040 | `	}` |
|         - | 6041 | `	/* Create a new array */` |
|        15 | 6042 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 6043 | `	if( pArray == 0 ){` |
|       ! 0 | 6044 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6045 | `		return PH7_OK;` |
|         - | 6046 | `	}` |
|         - | 6047 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 6048 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6049 | `	/* Perform the intersection */` |
|        15 | 6050 | `	pEntry = pSrc->pFirst;` |
|        15 | 6051 | `	n = pSrc->nEntry;` |
|        24 | 6052 | `	for(;;){` |
|        49 | 6053 | `		if( n < 1 ){` |
|        15 | 6054 | `			break;` |
|         - | 6055 | `		}` |
|        57 | 6056 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 6057 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 6058 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 6059 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 6060 | `				/* Blob lookup */` |
|        27 | 6061 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 6062 | `			}else{` |
|         - | 6063 | `				/* Int key */` |
|        13 | 6064 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 6065 | `			}` |
|        39 | 6066 | `			if( rc != SXRET_OK ){` |
|         - | 6067 | `				/* Key does not exist, break immediately */` |
|        17 | 6068 | `				break;` |
|         - | 6069 | `			}` |
|        12 | 6070 | `		}` |
|        35 | 6071 | `		if( i >= nArg ){` |
|         - | 6072 | `			/* Perform the insertion */` |
|        19 | 6073 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 6074 | `		}` |
|         - | 6075 | `		/* Point to the next entry */` |
|        35 | 6076 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6077 | `		n--;` |
|         1 | 6078 | `	}` |
|         - | 6079 | `	/* Return the freshly created array */` |
|        15 | 6080 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 6081 | `	return PH7_OK;` |
|        13 | 6082 | `}` |
|         - | 6083 | `/*` |
|         - | 6084 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 6085 | ` *  Computes the intersection of arrays.` |
|         - | 6086 | ` * Parameters` |
|         - | 6087 | ` *  $array1` |
|         - | 6088 | ` *    The array to compare from` |
|         - | 6089 | ` *  $array2` |
|         - | 6090 | ` *    An array to compare against` |
|         - | 6091 | ` *  $...` |
|         - | 6092 | ` *   More arrays to compare against` |
|         - | 6093 | ` * $callback` |
|         - | 6094 | ` *  The callback comparison function.` |
|         - | 6095 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 6096 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 6097 | ` *  than the second.` |
|         - | 6098 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 6099 | ` * Return` |
|         - | 6100 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 6101 | ` *  in all of the parameters. .` |
|         - | 6102 | ` * Note that NULL is returned on failure.` |
|         - | 6103 | ` */` |
|        24 | 6104 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6105 | `{` |
|         - | 6106 | `	ph7_hashmap_node *pEntry;` |
|         - | 6107 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 6108 | `	ph7_value *pCallback;` |
|         - | 6109 | `	ph7_value *pArray;` |
|         - | 6110 | `	ph7_value *pVal;` |
|         - | 6111 | `	sxi32 rc;` |
|         - | 6112 | `	sxu32 n;` |
|         - | 6113 | `	int i;` |
|         - | 6114 |  |
|         - | 6115 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 6116 | `	if( nArg < 2 ){` |
|       ! 0 | 6117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6118 | `			"ArgumentCountError",` |
|         - | 6119 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 6120 | `			nArg` |
|         - | 6121 | `			);` |
|         - | 6122 | `	}` |
|        29 | 6123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6124 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6125 | `			"TypeError",` |
|         - | 6126 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6127 | `			ph7_type_name(apArg[0])` |
|         - | 6128 | `			);` |
|         - | 6129 | `	}` |
|         - | 6130 |  |
|        27 | 6131 | `	if( nArg == 2 ){` |
|         - | 6132 | `		/* Only the original array and the callback were provided. */` |
|         - | 6133 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6134 | `		 * validation ordering. */` |
|         3 | 6135 | `	} else {` |
|         - | 6136 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6137 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6138 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6139 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6140 | `					"TypeError",` |
|         - | 6141 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6142 | `					i + 1,` |
|         2 | 6143 | `					ph7_type_name(apArg[i])` |
|         - | 6144 | `					);` |
|         - | 6145 | `			}` |
|        13 | 6146 | `		}` |
|         - | 6147 | `	}` |
|         - | 6148 |  |
|         - | 6149 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6150 | `	pCallback = apArg[nArg - 1];` |
|         - | 6151 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6152 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6153 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6154 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6155 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6156 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6157 | `			 */` |
|         9 | 6158 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6159 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6160 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6161 | `					"TypeError",` |
|         - | 6162 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6163 | `					nArg` |
|         - | 6164 | `					);` |
|         - | 6165 | `			}` |
|         - | 6166 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6167 | `			{` |
|         6 | 6168 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6169 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6170 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6171 | `					int nMethodLen;` |
|         6 | 6172 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6173 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6174 | `					if( pClass ){` |
|         - | 6175 | `						/* Class exists but method is missing. */` |
|         4 | 6176 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6177 | `							"TypeError",` |
|         - | 6178 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6179 | `							nArg,` |
|         1 | 6180 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6181 | `							zMethod` |
|         - | 6182 | `							);` |
|         - | 6183 | `					}` |
|         - | 6184 | `					/* Class not found */` |
|         - | 6185 | `					{` |
|         - | 6186 | `						int nName;` |
|         3 | 6187 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6188 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6189 | `							"TypeError",` |
|         - | 6190 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6191 | `							nArg,` |
|         1 | 6192 | `							zName` |
|         - | 6193 | `							);` |
|         - | 6194 | `					}` |
|         - | 6195 | `				}` |
|         - | 6196 | `			}` |
|         - | 6197 | `			/* Fallback message */` |
|       ! 0 | 6198 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6199 | `				"TypeError",` |
|         - | 6200 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6201 | `				nArg` |
|         - | 6202 | `				);` |
|         - | 6203 | `		}` |
|         6 | 6204 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6205 | `			int len;` |
|         3 | 6206 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6207 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6208 | `				"TypeError",` |
|         - | 6209 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6210 | `				nArg,` |
|         1 | 6211 | `				zName` |
|         - | 6212 | `				);` |
|         - | 6213 | `		}` |
|         4 | 6214 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6215 | `			"TypeError",` |
|         - | 6216 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6217 | `			nArg` |
|         - | 6218 | `			);` |
|         - | 6219 | `	}` |
|         - | 6220 |  |
|        11 | 6221 | `	if( nArg == 2 ){` |
|         - | 6222 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6223 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6224 | `		return PH7_OK;` |
|         - | 6225 | `	}` |
|         - | 6226 |  |
|         - | 6227 | `	/* Create a new array */` |
|         7 | 6228 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6229 | `	if( pArray == 0 ){` |
|       ! 0 | 6230 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6231 | `		return PH7_OK;` |
|         - | 6232 | `	}` |
|         - | 6233 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6234 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6235 | `	/* Perform the intersection */` |
|         7 | 6236 | `	pEntry = pSrc->pFirst;` |
|         7 | 6237 | `	n = pSrc->nEntry;` |
|         7 | 6238 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6239 | `	for(;;){` |
|        19 | 6240 | `		if( n < 1 ){` |
|         5 | 6241 | `			break;` |
|         - | 6242 | `		}` |
|         - | 6243 | `		/* Extract the node value */` |
|        15 | 6244 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6245 | `		if( pVal ){` |
|        23 | 6246 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6247 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6248 | `					/* ignore */` |
|       ! 0 | 6249 | `					continue;` |
|         - | 6250 | `				}` |
|         - | 6251 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6252 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6253 | `				/* Perform the lookup */` |
|        15 | 6254 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6255 | `				if( rc != SXRET_OK ){` |
|         - | 6256 | `					/* Value does not exist */` |
|         7 | 6257 | `					break;` |
|         - | 6258 | `				}` |
|         5 | 6259 | `			}` |
|        15 | 6260 | `			if( i >= (nArg-1) ){` |
|         - | 6261 | `				/* Perform the insertion */` |
|         9 | 6262 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6263 | `			}` |
|         7 | 6264 | `		}` |
|        15 | 6265 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6266 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6267 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6268 | `			return PH7_EXCEPTION;` |
|         - | 6269 | `		}` |
|         - | 6270 | `		/* Point to the next entry */` |
|        13 | 6271 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6272 | `		n--;` |
|         1 | 6273 | `	}` |
|         - | 6274 | `	/* Return the freshly created array */` |
|         5 | 6275 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6276 | `	return PH7_OK;` |
|        17 | 6277 | `}` |
|         - | 6278 | `/*` |
|         - | 6279 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6280 | ` *  Fill an array with values.` |
|         - | 6281 | ` * Parameters` |
|         - | 6282 | ` *  $start_index` |
|         - | 6283 | ` *    The first index of the returned array.` |
|         - | 6284 | ` *  $num` |
|         - | 6285 | ` *   Number of elements to insert.` |
|         - | 6286 | ` *  $value` |
|         - | 6287 | ` *    Value to use for filling.` |
|         - | 6288 | ` * Return` |
|         - | 6289 | ` *  The filled array or null on failure.` |
|         - | 6290 | ` */` |
|       240 | 6291 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6292 | `{` |
|         - | 6293 | `	ph7_value *pArray;` |
|         - | 6294 | `	int i,nEntry;` |
|         - | 6295 |  |
|         - | 6296 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6297 | `	if( nArg != 3 ){` |
|         - | 6298 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6299 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6300 | `			"ArgumentCountError",` |
|         - | 6301 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6302 | `			nArg` |
|         - | 6303 | `			);` |
|         - | 6304 | `	}` |
|         - | 6305 |  |
|         - | 6306 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6307 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6308 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6309 | `	 * and NULLs are rejected outright. */` |
|       357 | 6310 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6311 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6312 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6313 | `			"TypeError",` |
|         - | 6314 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6315 | `			ph7_type_name(apArg[0])` |
|         - | 6316 | `			);` |
|         - | 6317 | `	}` |
|       242 | 6318 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6319 | `		int len;` |
|         8 | 6320 | `		sxu8 bReal = FALSE;` |
|         8 | 6321 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6322 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6323 | `			/* Non‑numeric string is an error. */` |
|         3 | 6324 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6325 | `				"TypeError",` |
|         - | 6326 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6327 | `				);` |
|         - | 6328 | `		}` |
|         5 | 6329 | `		if( bReal ){` |
|         - | 6330 | `			/* float-string -> deprecation warning */` |
|         4 | 6331 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6332 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6333 | `				zStr` |
|         - | 6334 | `				);` |
|         1 | 6335 | `		}` |
|         2 | 6336 | `	}` |
|         - | 6337 |  |
|         - | 6338 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6339 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6340 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6341 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6342 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6343 | `			"TypeError",` |
|         - | 6344 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6345 | `			ph7_type_name(apArg[1])` |
|         - | 6346 | `			);` |
|         - | 6347 | `	}` |
|       239 | 6348 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6349 | `		int len;` |
|         3 | 6350 | `		sxu8 bReal = FALSE;` |
|         3 | 6351 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6352 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6353 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6354 | `				"TypeError",` |
|         - | 6355 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6356 | `				);` |
|         - | 6357 | `		}` |
|       ! 0 | 6358 | `	}` |
|         - | 6359 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6360 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6361 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6362 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6363 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6364 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6365 | `		if( d != (double)i64 ){` |
|         7 | 6366 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6367 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6368 | `				d` |
|         - | 6369 | `				);` |
|         2 | 6370 | `		}` |
|         2 | 6371 | `	}` |
|         - | 6372 |  |
|         - | 6373 | `	/* Total number of entries to insert */` |
|       236 | 6374 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6375 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6376 | `	if( nEntry < 0 ){` |
|         3 | 6377 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6378 | `			"ValueError",` |
|         - | 6379 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6380 | `			);` |
|         - | 6381 | `	}` |
|         - | 6382 |  |
|         - | 6383 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6384 | `	if( nEntry == 0 ){` |
|         7 | 6385 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6386 | `		return PH7_OK;` |
|         - | 6387 | `	}` |
|         - | 6388 |  |
|         - | 6389 | `	/* Create a new array */` |
|       227 | 6390 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6391 | `	if( pArray == 0 ){` |
|       ! 0 | 6392 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6393 | `	}` |
|         - | 6394 |  |
|         - | 6395 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6396 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6397 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6398 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6399 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6400 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6401 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6402 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6403 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6404 | `		}` |
|   1058803 | 6405 | `	}` |
|         - | 6406 | `	/* Return the filled array */` |
|       227 | 6407 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6408 | `	return PH7_OK;` |
|       124 | 6409 | `}` |
|         - | 6410 | `/*` |
|         - | 6411 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6412 | ` *  Fill an array with values, specifying keys.` |
|         - | 6413 | ` * Parameters` |
|         - | 6414 | ` *  $input` |
|         - | 6415 | ` *   Array of values that will be used as key.` |
|         - | 6416 | ` *  $value` |
|         - | 6417 | ` *    Value to use for filling.` |
|         - | 6418 | ` * Return` |
|         - | 6419 | ` *  The filled array.` |
|         - | 6420 | ` * Throws` |
|         - | 6421 | ` *  ValueError if $input is not an array.` |
|         - | 6422 | ` */` |
|        22 | 6423 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6424 | `{` |
|         - | 6425 | `	ph7_hashmap_node *pEntry;` |
|         - | 6426 | `	ph7_hashmap *pSrc;` |
|         - | 6427 | `	ph7_value *pArray;` |
|         - | 6428 | `	sxu32 n;` |
|         - | 6429 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6430 | `	if( nArg != 2 ){` |
|         4 | 6431 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6432 | `			"ArgumentCountError",` |
|         - | 6433 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6434 | `			nArg` |
|         - | 6435 | `			);` |
|         - | 6436 | `	}` |
|         - | 6437 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6438 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6439 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6440 | `			"TypeError",` |
|         - | 6441 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6442 | `			ph7_type_name(apArg[0])` |
|         - | 6443 | `			);` |
|         - | 6444 | `	}` |
|         - | 6445 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6446 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6447 | `	/* Create a new array */` |
|        17 | 6448 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6449 | `	if( pArray == 0 ){` |
|       ! 0 | 6450 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6451 | `		return PH7_OK;` |
|         - | 6452 | `	}` |
|         - | 6453 | `	/* Perform the requested operation */` |
|        17 | 6454 | `	pEntry = pSrc->pFirst;` |
|        45 | 6455 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6456 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6457 | `		/* Point to the next entry */` |
|        29 | 6458 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6459 | `	}` |
|         - | 6460 | `	/* Return the filled array */` |
|        17 | 6461 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6462 | `	return PH7_OK;` |
|        14 | 6463 | `}` |
|         - | 6464 | `/*` |
|         - | 6465 | ` * array array_combine(array $keys,array $values)` |
|         - | 6466 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6467 | ` * Parameters` |
|         - | 6468 | ` *  $keys` |
|         - | 6469 | ` *    Array of keys to be used.` |
|         - | 6470 | ` * $values` |
|         - | 6471 | ` *   Array of values to be used.` |
|         - | 6472 | ` * Return` |
|         - | 6473 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6474 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6475 | ` *  not an array.` |
|         - | 6476 | ` */` |
|        16 | 6477 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6478 | `{` |
|         - | 6479 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6480 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6481 | `	ph7_value *pArray;` |
|         - | 6482 | `	sxu32 n;` |
|         - | 6483 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6484 | `	if( nArg != 2 ){` |
|         - | 6485 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6486 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6487 | `			"ArgumentCountError",` |
|         - | 6488 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6489 | `			nArg` |
|         - | 6490 | `			);` |
|         - | 6491 | `	}` |
|         - | 6492 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6493 | `	 * argument index in the error message. */` |
|        20 | 6494 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6495 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6496 | `			"TypeError",` |
|         - | 6497 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6498 | `			ph7_type_name(apArg[0])` |
|         - | 6499 | `			);` |
|         - | 6500 | `	}` |
|        17 | 6501 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6502 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6503 | `			"TypeError",` |
|         - | 6504 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6505 | `			ph7_type_name(apArg[1])` |
|         - | 6506 | `			);` |
|         - | 6507 | `	}` |
|         - | 6508 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6509 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6510 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6511 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6512 | `		/* Length mismatch -> ValueError */` |
|         3 | 6513 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6514 | `			"ValueError",` |
|         - | 6515 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6516 | `			);` |
|         - | 6517 | `	}` |
|         - | 6518 | `	/* Create a new array */` |
|        11 | 6519 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6520 | `	if( pArray == 0 ){` |
|       ! 0 | 6521 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6522 | `		return PH7_OK;` |
|         - | 6523 | `	}` |
|         - | 6524 | `	/* Perform the requested operation */` |
|        11 | 6525 | `	pKe = pKey->pFirst;` |
|        11 | 6526 | `	pVe = pValue->pFirst;` |
|        33 | 6527 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6528 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6529 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6530 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6531 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6532 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6533 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6534 | `		 * original array must not be mutated. */` |
|        23 | 6535 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6536 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6537 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6538 | `			if( pTmpKey ){` |
|         5 | 6539 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6540 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6541 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6542 | `				pKeyCopy = pTmpKey;` |
|         2 | 6543 | `			}` |
|         2 | 6544 | `		}` |
|        23 | 6545 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6546 | `		/* Point to the next entry */` |
|        23 | 6547 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6548 | `		pVe = pVe->pPrev;` |
|        12 | 6549 | `	}` |
|         - | 6550 | `	/* Return the filled array */` |
|        11 | 6551 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6552 | `	return PH7_OK;` |
|        12 | 6553 | `}` |
|         - | 6554 | `/*` |
|         - | 6555 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6556 | ` *  Return an array with elements in reverse order.` |
|         - | 6557 | ` * Parameters` |
|         - | 6558 | ` *  $array` |
|         - | 6559 | ` *   The input array.` |
|         - | 6560 | ` *  $preserve_keys (optional)` |
|         - | 6561 | ` *   If set to TRUE keys are preserved.` |
|         - | 6562 | ` * Return` |
|         - | 6563 | ` *  The reversed array.` |
|         - | 6564 | ` */` |
|        18 | 6565 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6566 | `{` |
|         - | 6567 | `	ph7_hashmap_node *pEntry;` |
|         - | 6568 | `	ph7_hashmap *pSrc;` |
|         - | 6569 | `	ph7_value *pArray;` |
|         - | 6570 | `	int bPreserve;` |
|         - | 6571 | `	sxu32 n;` |
|        20 | 6572 | `	if( nArg < 1 ){` |
|       ! 0 | 6573 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6574 | `			"ArgumentCountError",` |
|         - | 6575 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6576 | `			nArg` |
|         - | 6577 | `			);` |
|         - | 6578 | `	}` |
|         - | 6579 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6580 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6581 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6582 | `			"TypeError",` |
|         - | 6583 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6584 | `			ph7_type_name(apArg[0])` |
|         - | 6585 | `			);` |
|         - | 6586 | `	}` |
|        17 | 6587 | `	bPreserve = FALSE;` |
|        17 | 6588 | `	if( nArg > 1 ){` |
|         7 | 6589 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6590 | `	}` |
|         - | 6591 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6592 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6593 | `	/* Create a new array */` |
|        17 | 6594 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6595 | `	if( pArray == 0 ){` |
|       ! 0 | 6596 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6597 | `		return PH7_OK;` |
|         - | 6598 | `	}` |
|         - | 6599 | `	/* Perform the requested operation */` |
|        17 | 6600 | `	pEntry = pSrc->pLast;` |
|        55 | 6601 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6602 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6603 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6604 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6605 | `		/* Point to the previous entry */` |
|        39 | 6606 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6607 | `	}` |
|        17 | 6608 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6609 | `	return PH7_OK;` |
|        11 | 6610 | `}` |
|         - | 6611 | `/*` |
|         - | 6612 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6613 | ` *  Removes duplicate values from an array.` |
|         - | 6614 | ` * Parameters` |
|         - | 6615 | ` *  $array` |
|         - | 6616 | ` *   The input array.` |
|         - | 6617 | ` *  $flags` |
|         - | 6618 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6619 | ` *   behavior using these values:` |
|         - | 6620 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6621 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6622 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6623 | ` * Return` |
|         - | 6624 | ` *  The filtered array.` |
|         - | 6625 | ` */` |
|        36 | 6626 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6627 | `{` |
|         - | 6628 | `	ph7_hashmap_node *pEntry;` |
|         - | 6629 | `	ph7_value *pNeedle;` |
|         - | 6630 | `	ph7_hashmap *pSrc;` |
|         - | 6631 | `	ph7_value *pArray;` |
|         - | 6632 | `	int iFlags,base,bFold;` |
|         - | 6633 | `	sxu32 n;` |
|        39 | 6634 | `	if( nArg < 1 ){` |
|         - | 6635 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6636 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6637 | `			"ArgumentCountError",` |
|         - | 6638 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6639 | `			);` |
|         - | 6640 | `	}` |
|        39 | 6641 | `	if( nArg > 2 ){` |
|         - | 6642 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6643 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6644 | `			"ArgumentCountError",` |
|         - | 6645 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6646 | `			nArg` |
|         - | 6647 | `			);` |
|         - | 6648 | `	}` |
|         - | 6649 | `	/* Make sure we are dealing with a valid hashmap */` |
|        36 | 6650 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6651 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6652 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6653 | `			"TypeError",` |
|         - | 6654 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6655 | `			ph7_type_name(apArg[0])` |
|         - | 6656 | `			);` |
|         - | 6657 | `	}` |
|         - | 6658 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|         - | 6659 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|        33 | 6660 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|        33 | 6661 | `	base = iFlags & ~8;` |
|        33 | 6662 | `	bFold = (iFlags & 8) != 0;` |
|         - | 6663 | `	/* Point to the internal representation of the input hashmap */` |
|        33 | 6664 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6665 | `	/* Create a new array */` |
|        33 | 6666 | `	pArray = ph7_context_new_array(pCtx);` |
|        33 | 6667 | `	if( pArray == 0 ){` |
|       ! 0 | 6668 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6669 | `		return PH7_OK;` |
|         - | 6670 | `	}` |
|         - | 6671 | `	/* Perform the requested operation */` |
|        33 | 6672 | `	pEntry = pSrc->pFirst;` |
|       145 | 6673 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       113 | 6674 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|       113 | 6675 | `		if( pNeedle ){` |
|         - | 6676 | `			/* Keep this element unless a flag-equal one is already present. */` |
|       113 | 6677 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|       113 | 6678 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|       113 | 6679 | `			int bDup = 0;` |
|         - | 6680 | `			sxu32 i;` |
|         - | 6681 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|         - | 6682 | `			 * loop over pSrc). */` |
|       177 | 6683 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|       117 | 6684 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|       117 | 6685 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|        53 | 6686 | `					bDup = 1;` |
|        53 | 6687 | `					break;` |
|         - | 6688 | `				}` |
|        65 | 6689 | `				pK = pK->pPrev;` |
|        33 | 6690 | `			}` |
|       113 | 6691 | `			if( !bDup ){` |
|        61 | 6692 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|        30 | 6693 | `			}` |
|        56 | 6694 | `		}` |
|         - | 6695 | `		/* Point to the next entry */` |
|       113 | 6696 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        57 | 6697 | `	}` |
|         - | 6698 | `	/* Return the freshly created array */` |
|        33 | 6699 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 6700 | `	return PH7_OK;` |
|        21 | 6701 | `}` |
|         - | 6702 | `/*` |
|         - | 6703 | ` * array array_flip(array $input)` |
|         - | 6704 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6705 | ` * Parameter` |
|         - | 6706 | ` *  $input` |
|         - | 6707 | ` *   Input array.` |
|         - | 6708 | ` * Return` |
|         - | 6709 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6710 | ` */` |
|        30 | 6711 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6712 | `{` |
|         - | 6713 | `	ph7_hashmap_node *pEntry;` |
|         - | 6714 | `	ph7_hashmap *pSrc;` |
|         - | 6715 | `	ph7_value *pArray;` |
|         - | 6716 | `	ph7_value *pKey;` |
|         - | 6717 | `	ph7_value sVal;` |
|         - | 6718 | `	sxu32 n;` |
|         - | 6719 |  |
|         - | 6720 | `	/* PHP requires exactly one argument */` |
|        33 | 6721 | `	if( nArg != 1 ){` |
|         - | 6722 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6723 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6724 | `			"ArgumentCountError",` |
|         - | 6725 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6726 | `			nArg` |
|         - | 6727 | `			);` |
|         - | 6728 | `	}` |
|         - | 6729 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6730 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6731 | `		/* Type mismatch -> TypeError */` |
|         4 | 6732 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6733 | `			"TypeError",` |
|         - | 6734 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6735 | `			ph7_type_name(apArg[0])` |
|         - | 6736 | `			);` |
|         - | 6737 | `	}` |
|         - | 6738 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6739 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6740 | `	/* Create a new array */` |
|        27 | 6741 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6742 | `	if( pArray == 0 ){` |
|       ! 0 | 6743 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6744 | `		return PH7_OK;` |
|         - | 6745 | `	}` |
|         - | 6746 | `	/* Start processing */` |
|        27 | 6747 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6748 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6749 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6750 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6751 | `		if( pKey ){` |
|         - | 6752 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6753 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6754 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6755 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6756 | `					);` |
|     22236 | 6757 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6758 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6759 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6760 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6761 | `				}else{` |
|         - | 6762 | `					SyString sStr;` |
|      2227 | 6763 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6764 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6765 | `				}` |
|         - | 6766 | `				/* Perform the insertion */` |
|     22227 | 6767 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6768 | `				/* Safely release the value because each inserted entry` |
|         - | 6769 | `				 * has its own private copy of the value.` |
|         - | 6770 | `				 */` |
|     22227 | 6771 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6772 | `			}else{` |
|         - | 6773 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6774 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6775 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6776 | `					);` |
|         - | 6777 | `			}` |
|     11118 | 6778 | `		}` |
|         - | 6779 | `		/* Point to the next entry */` |
|     22237 | 6780 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6781 | `	}` |
|         - | 6782 | `	/* Return the freshly created array */` |
|        27 | 6783 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6784 | `	return PH7_OK;` |
|        18 | 6785 | `}` |
|         - | 6786 | `/*` |
|         - | 6787 | ` * number array_sum(array $array )` |
|         - | 6788 | ` *  Calculate the sum of values in an array.` |
|         - | 6789 | ` * Parameters` |
|         - | 6790 | ` *  $array: The input array.` |
|         - | 6791 | ` * Return` |
|         - | 6792 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6793 | ` */` |
|        24 | 6794 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6795 | `{` |
|         - | 6796 | `	ph7_hashmap_node *pEntry;` |
|         - | 6797 | `	ph7_value *pObj;` |
|        26 | 6798 | `	double dSum = 0;` |
|         - | 6799 | `	sxu32 n;` |
|        26 | 6800 | `	pEntry = pMap->pFirst;` |
|        92 | 6801 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6802 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6803 | `		if( pObj ){` |
|        68 | 6804 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6805 | `				dSum += pObj->rVal;` |
|        54 | 6806 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6807 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6808 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6809 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6810 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6811 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6812 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6813 | `						"Addition is not supported on type string");` |
|        14 | 6814 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6815 | `					double dv = 0;` |
|        13 | 6816 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6817 | `					dSum += dv;` |
|         8 | 6818 | `				}` |
|        12 | 6819 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6820 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6821 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6822 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6823 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6824 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6825 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6826 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6827 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6828 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6829 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6830 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6831 | `			}` |
|         - | 6832 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6833 | `		}` |
|         - | 6834 | `		/* Point to the next entry */` |
|        68 | 6835 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6836 | `	}` |
|         - | 6837 | `	/* Return sum */` |
|        26 | 6838 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6839 | `}` |
|       690 | 6840 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         3 | 6841 | `{` |
|         - | 6842 | `	ph7_hashmap_node *pEntry;` |
|         - | 6843 | `	ph7_value *pObj;` |
|       693 | 6844 | `	sxi64 nSum = 0;` |
|         - | 6845 | `	sxu32 n;` |
|       693 | 6846 | `	pEntry = pMap->pFirst;` |
|      6705 | 6847 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      6015 | 6848 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      6015 | 6849 | `		if( pObj ){` |
|      6015 | 6850 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      5995 | 6851 | `				nSum += pObj->x.iVal;` |
|      3018 | 6852 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6853 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6854 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6855 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6856 | `						"Addition is not supported on type string");` |
|        10 | 6857 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6858 | `					sxi64 nv = 0;` |
|         8 | 6859 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6860 | `					nSum += nv;` |
|         5 | 6861 | `				}` |
|        17 | 6862 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6863 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6864 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6865 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6866 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6867 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6868 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6869 | `					"Addition is not supported on type %s",` |
|         2 | 6870 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6871 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6872 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6873 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6874 | `			}` |
|         - | 6875 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      3006 | 6876 | `		}` |
|         - | 6877 | `		/* Point to the next entry */` |
|      6015 | 6878 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3009 | 6879 | `	}` |
|         - | 6880 | `	/* Return sum */` |
|       693 | 6881 | `	ph7_result_int64(pCtx,nSum);` |
|       693 | 6882 | `}` |
|         - | 6883 | `/* number array_sum(array $array )` |
|         - | 6884 | ` * (See block-coment above)` |
|         - | 6885 | ` */` |
|       726 | 6886 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6887 | `{` |
|         - | 6888 | `	ph7_hashmap_node *pEntry;` |
|         - | 6889 | `	ph7_hashmap *pMap;` |
|         - | 6890 | `	ph7_value *pObj;` |
|       730 | 6891 | `	int useDouble = 0;` |
|         - | 6892 | `	sxu32 n;` |
|         - | 6893 | `	/* PHP requires exactly one argument */` |
|       730 | 6894 | `	if( nArg != 1 ){` |
|         4 | 6895 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6896 | `			"ArgumentCountError",` |
|         - | 6897 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6898 | `			nArg` |
|         - | 6899 | `			);` |
|         - | 6900 | `	}` |
|         - | 6901 | `	/* Make sure we are dealing with a valid hashmap */` |
|       728 | 6902 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6903 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6904 | `		char zBuf[64];` |
|         8 | 6905 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6906 | `			"TypeError",` |
|         - | 6907 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6908 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6909 | `			);` |
|         - | 6910 | `	}` |
|       723 | 6911 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       723 | 6912 | `	if( pMap->nEntry < 1 ){` |
|         - | 6913 | `		/* Nothing to compute,return 0 */` |
|         7 | 6914 | `		ph7_result_int(pCtx,0);` |
|         7 | 6915 | `		return PH7_OK;` |
|         - | 6916 | `	}` |
|         - | 6917 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6918 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6919 | `	 */` |
|       717 | 6920 | `	pEntry = pMap->pFirst;` |
|      6737 | 6921 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      6047 | 6922 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      6047 | 6923 | `		if( pObj ){` |
|      6047 | 6924 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6925 | `				useDouble = 1;` |
|        20 | 6926 | `				break;` |
|         - | 6927 | `			}` |
|      6029 | 6928 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6929 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6930 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6931 | `				sxu32 i;` |
|        32 | 6932 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6933 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6934 | `						useDouble = 1;` |
|         7 | 6935 | `						break;` |
|         - | 6936 | `					}` |
|         9 | 6937 | `				}` |
|        18 | 6938 | `				if( useDouble ){` |
|         7 | 6939 | `					break;` |
|         - | 6940 | `				}` |
|         5 | 6941 | `			}` |
|      3010 | 6942 | `		}` |
|      6023 | 6943 | `		pEntry = pEntry->pPrev;` |
|      3013 | 6944 | `	}` |
|       717 | 6945 | `	if( useDouble ){` |
|        26 | 6946 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6947 | `	}else{` |
|       693 | 6948 | `		Int64Sum(pCtx,pMap);` |
|         - | 6949 | `	}` |
|       717 | 6950 | `	return PH7_OK;` |
|       367 | 6951 | `}` |
|         - | 6952 | `/*` |
|         - | 6953 | ` * number array_product(array $array )` |
|         - | 6954 | ` *  Calculate the product of values in an array.` |
|         - | 6955 | ` * Parameters` |
|         - | 6956 | ` *  $array: The input array.` |
|         - | 6957 | ` * Return` |
|         - | 6958 | ` *  Returns the product of values as an integer or float.` |
|         - | 6959 | ` */` |
|         2 | 6960 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6961 | `{` |
|         - | 6962 | `	ph7_hashmap_node *pEntry;` |
|         - | 6963 | `	ph7_value *pObj;` |
|         - | 6964 | `	double dProd;` |
|         - | 6965 | `	sxu32 n;` |
|         3 | 6966 | `	pEntry = pMap->pFirst;` |
|         3 | 6967 | `	dProd = 1;` |
|         7 | 6968 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6969 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6970 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6971 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6972 | `				dProd *= pObj->rVal;` |
|         4 | 6973 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6974 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6975 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6976 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6977 | `					double dv = 0;` |
|       ! 0 | 6978 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6979 | `					dProd *= dv;` |
|       ! 0 | 6980 | `				}` |
|       ! 0 | 6981 | `			}` |
|         2 | 6982 | `		}` |
|         - | 6983 | `		/* Point to the next entry */` |
|         5 | 6984 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6985 | `	}` |
|         - | 6986 | `	/* Return product */` |
|         3 | 6987 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6988 | `}` |
|         2 | 6989 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6990 | `{` |
|         - | 6991 | `	ph7_hashmap_node *pEntry;` |
|         - | 6992 | `	ph7_value *pObj;` |
|         - | 6993 | `	sxi64 nProd;` |
|         - | 6994 | `	sxu32 n;` |
|         3 | 6995 | `	pEntry = pMap->pFirst;` |
|         3 | 6996 | `	nProd = 1;` |
|         9 | 6997 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6998 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6999 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 7000 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 7001 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 7002 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 7003 | `				nProd *= pObj->x.iVal;` |
|         3 | 7004 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 7005 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 7006 | `					sxi64 nv = 0;` |
|       ! 0 | 7007 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 7008 | `					nProd *= nv;` |
|       ! 0 | 7009 | `				}` |
|       ! 0 | 7010 | `			}` |
|         3 | 7011 | `		}` |
|         - | 7012 | `		/* Point to the next entry */` |
|         7 | 7013 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 7014 | `	}` |
|         - | 7015 | `	/* Return product */` |
|         3 | 7016 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 7017 | `}` |
|         - | 7018 | `/* number array_product(array $array )` |
|         - | 7019 | ` * (See block-block comment above)` |
|         - | 7020 | ` */` |
|        16 | 7021 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7022 | `{` |
|         - | 7023 | `	ph7_hashmap *pMap;` |
|         - | 7024 | `	ph7_value *pObj;` |
|        17 | 7025 | `	if( nArg < 1 ){` |
|         - | 7026 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 7027 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 7028 | `		return PH7_OK;` |
|         - | 7029 | `	}` |
|         - | 7030 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 7031 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7032 | `		char zBuf[64];` |
|        16 | 7033 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7034 | `			"TypeError",` |
|         - | 7035 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 7036 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7037 | `			);` |
|         - | 7038 | `	}` |
|         7 | 7039 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 7040 | `	if( pMap->nEntry < 1 ){` |
|         - | 7041 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 7042 | `		ph7_result_int(pCtx,1);` |
|         3 | 7043 | `		return PH7_OK;` |
|         - | 7044 | `	}` |
|         - | 7045 | `	/* If the first element is of type float,then perform floating` |
|         - | 7046 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 7047 | `	 */` |
|         5 | 7048 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 7049 | `	if( pObj == 0 ){` |
|       ! 0 | 7050 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 7051 | `		return PH7_OK;` |
|         - | 7052 | `	}` |
|         5 | 7053 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 7054 | `		DoubleProd(pCtx,pMap);` |
|         2 | 7055 | `	}else{` |
|         3 | 7056 | `		Int64Prod(pCtx,pMap);` |
|         - | 7057 | `	}` |
|         5 | 7058 | `	return PH7_OK;` |
|         9 | 7059 | `}` |
|         - | 7060 | `/*` |
|         - | 7061 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 7062 | ` *  Pick one or more random entries out of an array.` |
|         - | 7063 | ` * Parameters` |
|         - | 7064 | ` * $input` |
|         - | 7065 | ` *  The input array.` |
|         - | 7066 | ` * $num_req` |
|         - | 7067 | ` *  Specifies how many entries you want to pick.` |
|         - | 7068 | ` * Return` |
|         - | 7069 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 7070 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 7071 | ` *  NULL is returned on failure.` |
|         - | 7072 | ` */` |
|        36 | 7073 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7074 | `{` |
|         - | 7075 | `	ph7_hashmap_node *pNode;` |
|         - | 7076 | `	ph7_hashmap *pMap;` |
|        37 | 7077 | `	int nItem = 1;` |
|        37 | 7078 | `	if( nArg < 1 ){` |
|         - | 7079 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7080 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7081 | `		return PH7_OK;` |
|         - | 7082 | `	}` |
|         - | 7083 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 7084 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7085 | `		char zBuf[64];` |
|        10 | 7086 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7087 | `			"TypeError",` |
|         - | 7088 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7089 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7090 | `			);` |
|         - | 7091 | `	}` |
|         - | 7092 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 7093 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 7094 | `	if( nArg > 1 ){` |
|        23 | 7095 | `		ph7_value *pNum = apArg[1];` |
|        22 | 7096 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 7097 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 7098 | `			char zBuf[64];` |
|       ! 0 | 7099 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7100 | `				"TypeError",` |
|         - | 7101 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 7102 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 7103 | `				);` |
|         - | 7104 | `		}` |
|        23 | 7105 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 7106 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 7107 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 7108 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 7109 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 7110 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 7111 | `			int len;` |
|         9 | 7112 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 7113 | `			sxi64 iLong; double dReal;` |
|         9 | 7114 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 7115 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 7116 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7117 | `					"TypeError",` |
|         - | 7118 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 7119 | `					);` |
|         - | 7120 | `			}` |
|         - | 7121 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 7122 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 7123 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 7124 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 7125 | `			}` |
|         3 | 7126 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 7127 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 7128 | `			nItem = (int)iLong;` |
|         2 | 7129 | `		}else{` |
|        15 | 7130 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 7131 | `		}` |
|         8 | 7132 | `	}` |
|         - | 7133 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 7134 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7135 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 7136 | `	if( pMap->nEntry < 1 ){` |
|         5 | 7137 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7138 | `			"ValueError",` |
|         - | 7139 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7140 | `			);` |
|         - | 7141 | `	}` |
|         - | 7142 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7143 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7144 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7145 | `			"ValueError",` |
|         - | 7146 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7147 | `			);` |
|         - | 7148 | `	}` |
|        13 | 7149 | `	if( nItem < 2 ){` |
|         - | 7150 | `		sxu32 nEntry;` |
|         - | 7151 | `		/* Select a random number */` |
|         9 | 7152 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7153 | `		/* Extract the desired entry.` |
|         - | 7154 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7155 | `		 */` |
|         9 | 7156 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         2 | 7157 | `			pNode = pMap->pLast;` |
|         2 | 7158 | `			nEntry = pMap->nEntry - nEntry;` |
|         2 | 7159 | `			if( nEntry > 1 ){` |
|       ! 0 | 7160 | `				for(;;){` |
|       ! 0 | 7161 | `					if( nEntry == 0 ){` |
|       ! 0 | 7162 | `						break;` |
|         - | 7163 | `					}` |
|         - | 7164 | `					/* Point to the previous entry */` |
|       ! 0 | 7165 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7166 | `					nEntry--;` |
|       ! 0 | 7167 | `				}` |
|       ! 0 | 7168 | `			}` |
|         1 | 7169 | `		}else{` |
|         7 | 7170 | `			pNode = pMap->pFirst;` |
|         5 | 7171 | `			for(;;){` |
|        10 | 7172 | `				if( nEntry == 0 ){` |
|         7 | 7173 | `					break;` |
|         - | 7174 | `				}` |
|         - | 7175 | `				/* Point to the next entry */` |
|         4 | 7176 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         4 | 7177 | `				nEntry--;` |
|         1 | 7178 | `			}` |
|         - | 7179 | `		}` |
|         9 | 7180 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7181 | `			/* Int key */` |
|         7 | 7182 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7183 | `		}else{` |
|         - | 7184 | `			/* Blob key */` |
|         3 | 7185 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7186 | `		}` |
|         5 | 7187 | `	}else{` |
|         - | 7188 | `		ph7_value sKey,*pArray;` |
|         - | 7189 | `		ph7_hashmap *pDest;` |
|         - | 7190 | `		/* Create a new array */` |
|         5 | 7191 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7192 | `		if( pArray == 0 ){` |
|       ! 0 | 7193 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7194 | `			return PH7_OK;` |
|         - | 7195 | `		}` |
|         - | 7196 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7197 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7198 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7199 | `		/* Copy the first n items */` |
|         5 | 7200 | `		pNode = pMap->pFirst;` |
|         5 | 7201 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7202 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7203 | `		}` |
|        15 | 7204 | `		while( nItem > 0){` |
|        11 | 7205 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7206 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7207 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7208 | `			/* Point to the next entry */` |
|        11 | 7209 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7210 | `			nItem--;` |
|         1 | 7211 | `		}` |
|         - | 7212 | `		/* Shuffle the array */` |
|         5 | 7213 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7214 | `		/* Rehash node */` |
|         5 | 7215 | `		HashmapSortRehash(pDest);` |
|         - | 7216 | `		/* Return the random array */` |
|         5 | 7217 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7218 | `	}` |
|        13 | 7219 | `	return PH7_OK;` |
|        19 | 7220 | `}` |
|         - | 7221 | `/*` |
|         - | 7222 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7223 | ` *  Split an array into chunks.` |
|         - | 7224 | ` * Parameters` |
|         - | 7225 | ` * $input` |
|         - | 7226 | ` *   The array to work on` |
|         - | 7227 | ` * $size` |
|         - | 7228 | ` *   The size of each chunk` |
|         - | 7229 | ` * $preserve_keys` |
|         - | 7230 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7231 | ` *   the chunk numerically.` |
|         - | 7232 | ` * Return` |
|         - | 7233 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7234 | ` *  zero, with each dimension containing size elements.` |
|         - | 7235 | ` */` |
|        36 | 7236 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7237 | `{` |
|         - | 7238 | `	ph7_value *pArray,*pChunk;` |
|         - | 7239 | `	ph7_hashmap_node *pEntry;` |
|         - | 7240 | `	ph7_hashmap *pMap;` |
|         - | 7241 | `	int bPreserve;` |
|         - | 7242 | `	sxu32 nChunk;` |
|         - | 7243 | `	sxu32 nSize;` |
|         - | 7244 | `	sxu32 n;` |
|         - | 7245 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7246 | `	if( nArg < 2 ){` |
|         - | 7247 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7248 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7249 | `			"ArgumentCountError",` |
|         - | 7250 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7251 | `			nArg` |
|         - | 7252 | `			);` |
|         - | 7253 | `	}` |
|        41 | 7254 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7255 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7256 | `			"TypeError",` |
|         - | 7257 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7258 | `			ph7_type_name(apArg[0])` |
|         - | 7259 | `			);` |
|         - | 7260 | `	}` |
|         - | 7261 | `	/* Create a new array */` |
|        38 | 7262 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7263 | `	if( pArray == 0 ){` |
|       ! 0 | 7264 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7265 | `		return PH7_OK;` |
|         - | 7266 | `	}` |
|         - | 7267 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7268 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7269 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7270 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7271 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7272 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7273 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7274 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7275 | `			"TypeError",` |
|         - | 7276 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7277 | `			ph7_type_name(apArg[1])` |
|         - | 7278 | `			);` |
|         - | 7279 | `	}` |
|         - | 7280 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7281 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7282 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7283 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7284 | `		int len;` |
|         3 | 7285 | `		sxu8 bReal = FALSE;` |
|         3 | 7286 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7287 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7288 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7289 | `				"TypeError",` |
|         - | 7290 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7291 | `				);` |
|         - | 7292 | `		}` |
|       ! 0 | 7293 | `		if( bReal ){` |
|         - | 7294 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7295 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7296 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7297 | `				zStr` |
|         - | 7298 | `				);` |
|       ! 0 | 7299 | `		}` |
|       ! 0 | 7300 | `	}` |
|         - | 7301 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7302 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7303 | `	 * later via ph7_value_to_int. */` |
|        35 | 7304 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7305 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7306 | `		sxi64 i = (sxi64)d;` |
|         3 | 7307 | `		if( d != (double)i ){` |
|         4 | 7308 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7309 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7310 | `				d` |
|         - | 7311 | `				);` |
|         1 | 7312 | `		}` |
|         1 | 7313 | `	}` |
|         - | 7314 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7315 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7316 | `	{` |
|        35 | 7317 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7318 | `		if( nSizeSigned < 1 ){` |
|         - | 7319 | `			/* size <= 0 -> ValueError */` |
|         6 | 7320 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7321 | `				"ValueError",` |
|         - | 7322 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7323 | `				);` |
|         - | 7324 | `		}` |
|        29 | 7325 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7326 | `	}` |
|        29 | 7327 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7328 | `		/* Return the whole array */` |
|         3 | 7329 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7330 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7331 | `		return PH7_OK;` |
|         - | 7332 | `	}` |
|        27 | 7333 | `	bPreserve = 0;` |
|        27 | 7334 | `	if( nArg > 2 ){` |
|         - | 7335 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7336 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7337 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7338 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7339 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7340 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7341 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7342 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7343 | `				"TypeError",` |
|         - | 7344 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7345 | `				ph7_type_name(apArg[2])` |
|         - | 7346 | `				);` |
|         - | 7347 | `		}` |
|        21 | 7348 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7349 | `	}` |
|         - | 7350 | `	/* Start processing */` |
|        27 | 7351 | `	pEntry = pMap->pFirst;` |
|        27 | 7352 | `	nChunk = 0;` |
|        27 | 7353 | `	pChunk = 0;` |
|        27 | 7354 | `	n = pMap->nEntry;` |
|        56 | 7355 | `	for( ;; ){` |
|       113 | 7356 | `		if( n < 1 ){` |
|         - | 7357 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7358 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7359 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7360 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7361 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7362 | `			 * exists. */` |
|        27 | 7363 | `			if( pChunk ){` |
|        27 | 7364 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7365 | `			}` |
|        27 | 7366 | `			break;` |
|         - | 7367 | `		}` |
|        87 | 7368 | `		if( nChunk < 1 ){` |
|        71 | 7369 | `			if( pChunk ){` |
|         - | 7370 | `				/* Put the first chunk */` |
|        45 | 7371 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7372 | `			}` |
|         - | 7373 | `			/* Create a new dimension */` |
|        71 | 7374 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7375 | `												   * will be automatically released as soon we return` |
|         - | 7376 | `												   * from this function */` |
|        71 | 7377 | `			if( pChunk == 0 ){` |
|       ! 0 | 7378 | `				break;` |
|         - | 7379 | `			}` |
|        71 | 7380 | `			nChunk = nSize;` |
|        35 | 7381 | `		}` |
|         - | 7382 | `		/* Insert the entry */` |
|        87 | 7383 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7384 | `		/* Point to the next entry */` |
|        87 | 7385 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7386 | `		nChunk--;` |
|        87 | 7387 | `		n--;` |
|         1 | 7388 | `	}` |
|         - | 7389 | `	/* Return the multidimensional array */` |
|        27 | 7390 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7391 | `	return PH7_OK;` |
|        23 | 7392 | `}` |
|         - | 7393 | `/*` |
|         - | 7394 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7395 | ` *  Pad array to the specified length with a value.` |
|         - | 7396 | ` * $input` |
|         - | 7397 | ` *   Initial array of values to pad.` |
|         - | 7398 | ` * $pad_size` |
|         - | 7399 | ` *   New size of the array.` |
|         - | 7400 | ` * $pad_value` |
|         - | 7401 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7402 | ` */` |
|         - | 7403 | `/*` |
|         - | 7404 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7405 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7406 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7407 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7408 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7409 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7410 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7411 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7412 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7413 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7414 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7415 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7416 | ` */` |
|        50 | 7417 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7418 | `	ph7_context *pCtx,` |
|         - | 7419 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7420 | `	int iArg,              /* 1-based argument position */` |
|         - | 7421 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7422 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7423 | `	)` |
|         1 | 7424 | `{` |
|        51 | 7425 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7426 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7427 | `			"ValueError",` |
|         - | 7428 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7429 | `			zFunc,iArg,zParam` |
|         - | 7430 | `			);` |
|         - | 7431 | `	}` |
|        37 | 7432 | `	return SXRET_OK;` |
|        26 | 7433 | `}` |
|        62 | 7434 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7435 | `{` |
|         - | 7436 | `	ph7_hashmap *pMap;` |
|         - | 7437 | `	ph7_value *pArray;` |
|         - | 7438 | `	sxi64 iLen,iAbs;` |
|         - | 7439 | `	int nEntry;` |
|         - | 7440 | `	sxi32 rc;` |
|        65 | 7441 | `	if( nArg != 3 ){` |
|         4 | 7442 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7443 | `			"ArgumentCountError",` |
|         - | 7444 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7445 | `			nArg` |
|         - | 7446 | `			);` |
|         - | 7447 | `	}` |
|        62 | 7448 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7449 | `		char zBuf[64];` |
|        11 | 7450 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7451 | `			"TypeError",` |
|         - | 7452 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7453 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7454 | `			);` |
|         - | 7455 | `	}` |
|         - | 7456 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7457 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7458 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7459 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7460 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7461 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7462 | `		char zBuf[64];` |
|       ! 0 | 7463 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7464 | `			"TypeError",` |
|         - | 7465 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7466 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7467 | `			);` |
|         - | 7468 | `	}` |
|        55 | 7469 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7470 | `		int nStr;` |
|        11 | 7471 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7472 | `		sxi64 iLong; double dReal;` |
|        11 | 7473 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7474 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7475 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7476 | `				"TypeError",` |
|         - | 7477 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7478 | `				);` |
|         - | 7479 | `		}` |
|         7 | 7480 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7481 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7482 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7483 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7484 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7485 | `					"TypeError",` |
|         - | 7486 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7487 | `					);` |
|         - | 7488 | `			}` |
|         3 | 7489 | `			iLen = (sxi64)dReal;` |
|         3 | 7490 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7491 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7492 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7493 | `					zStr` |
|         - | 7494 | `					);` |
|       ! 0 | 7495 | `			}` |
|         2 | 7496 | `		}else{` |
|         5 | 7497 | `			iLen = iLong;` |
|         - | 7498 | `		}` |
|         4 | 7499 | `	}else{` |
|        45 | 7500 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7501 | `	}` |
|         - | 7502 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7503 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7504 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7505 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7506 | `	 * overflow). */` |
|        51 | 7507 | `	iAbs = iLen;` |
|        51 | 7508 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7509 | `		iAbs = -iAbs;` |
|         7 | 7510 | `	}` |
|        51 | 7511 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7512 | `	if( rc != SXRET_OK ){` |
|        15 | 7513 | `		return rc;` |
|         - | 7514 | `	}` |
|        37 | 7515 | `	nEntry = (int)iLen;` |
|         - | 7516 | `	/* Create a new array */` |
|        37 | 7517 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7518 | `	if( pArray == 0 ){` |
|       ! 0 | 7519 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7520 | `	}` |
|        37 | 7521 | `	if( nEntry < 0 ){` |
|        11 | 7522 | `		nEntry = -nEntry;` |
|        11 | 7523 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7524 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7525 | `			/* Insert given items first */` |
|        25 | 7526 | `			while( nEntry > 0 ){` |
|        19 | 7527 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7528 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7529 | `				}` |
|        19 | 7530 | `				nEntry--;` |
|         1 | 7531 | `			}` |
|         - | 7532 | `			/* Merge the two arrays */` |
|         7 | 7533 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7534 | `		}else{` |
|         5 | 7535 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7536 | `		}` |
|        32 | 7537 | `	}else if( nEntry > 0 ){` |
|        25 | 7538 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7539 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7540 | `			/* Merge the two arrays first */` |
|        19 | 7541 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7542 | `			/* Insert given items */` |
|       275 | 7543 | `			while( nEntry > 0 ){` |
|       257 | 7544 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7545 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7546 | `				}` |
|       257 | 7547 | `				nEntry--;` |
|         1 | 7548 | `			}` |
|        10 | 7549 | `		}else{` |
|         7 | 7550 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7551 | `		}` |
|        13 | 7552 | `	}else{` |
|         - | 7553 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7554 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7555 | `	}` |
|         - | 7556 | `	/* Return the new array */` |
|        37 | 7557 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7558 | `	return PH7_OK;` |
|        34 | 7559 | `}` |
|         - | 7560 | `/*` |
|         - | 7561 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7562 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7563 | ` * Parameters` |
|         - | 7564 | ` * $array` |
|         - | 7565 | ` *   The array in which elements are replaced.` |
|         - | 7566 | ` * $array1` |
|         - | 7567 | ` *   The array from which elements will be extracted.` |
|         - | 7568 | ` * ....` |
|         - | 7569 | ` *  More arrays from which elements will be extracted.` |
|         - | 7570 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7571 | ` * Return` |
|         - | 7572 | ` *  Returns an array.` |
|         - | 7573 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7574 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7575 | ` */` |
|        20 | 7576 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7577 | `{` |
|         - | 7578 | `	ph7_hashmap *pMap;` |
|         - | 7579 | `	ph7_value *pArray;` |
|         - | 7580 | `	int i;` |
|        23 | 7581 | `	if( nArg < 1 ){` |
|       ! 0 | 7582 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7583 | `			"ArgumentCountError",` |
|         - | 7584 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7585 | `			);` |
|         - | 7586 | `	}` |
|        23 | 7587 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7588 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7589 | `			"TypeError",` |
|         - | 7590 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7591 | `			ph7_type_name(apArg[0])` |
|         - | 7592 | `			);` |
|         - | 7593 | `	}` |
|         - | 7594 | `	/* Create a new array */` |
|        20 | 7595 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7596 | `	if( pArray == 0 ){` |
|       ! 0 | 7597 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7598 | `		return PH7_OK;` |
|         - | 7599 | `	}` |
|         - | 7600 | `	/* Overwrite from the first array */` |
|        20 | 7601 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7602 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7603 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7604 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7605 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7606 | `			/* Type mismatch -> TypeError */` |
|         4 | 7607 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7608 | `				"TypeError",` |
|         - | 7609 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7610 | `				i + 1,` |
|         2 | 7611 | `				ph7_type_name(apArg[i])` |
|         - | 7612 | `				);` |
|         - | 7613 | `		}` |
|         - | 7614 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7615 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7616 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7617 | `	}` |
|         - | 7618 | `	/* Return the new array */` |
|        17 | 7619 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7620 | `	return PH7_OK;` |
|        13 | 7621 | `}` |
|         - | 7622 | `/*` |
|         - | 7623 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7624 | ` *  Filters elements of an array using a callback function.` |
|         - | 7625 | ` * Parameters` |
|         - | 7626 | ` *  $input` |
|         - | 7627 | ` *    The array to iterate over` |
|         - | 7628 | ` * $callback` |
|         - | 7629 | ` *    The callback function to use` |
|         - | 7630 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7631 | ` *    will be removed.` |
|         - | 7632 | ` * Return` |
|         - | 7633 | ` *  The filtered array.` |
|         - | 7634 | ` */` |
|        30 | 7635 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7636 | `{` |
|         - | 7637 | `	ph7_hashmap_node *pEntry;` |
|         - | 7638 | `	ph7_hashmap *pMap;` |
|         - | 7639 | `	ph7_value *pArray;` |
|         - | 7640 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7641 | `	ph7_value *pValue;` |
|         - | 7642 | `	sxi32 rc;` |
|         - | 7643 | `	int keep;` |
|         - | 7644 | `	sxu32 n;` |
|        32 | 7645 | `	if( nArg < 1 ){` |
|         - | 7646 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7647 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7648 | `		return PH7_OK;` |
|         - | 7649 | `	}` |
|         - | 7650 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7651 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7652 | `		char zBuf[64];` |
|        19 | 7653 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7654 | `			"TypeError",` |
|         - | 7655 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7656 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7657 | `			);` |
|         - | 7658 | `	}` |
|         - | 7659 | `	/* Create a new array */` |
|        20 | 7660 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7661 | `	if( pArray == 0 ){` |
|       ! 0 | 7662 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7663 | `		return PH7_OK;` |
|         - | 7664 | `	}` |
|         - | 7665 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7666 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7667 | `	pEntry = pMap->pFirst;` |
|        20 | 7668 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7669 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7670 | `	/* Perform the requested operation */` |
|        78 | 7671 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7672 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7673 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7674 | `		if( pValue == 0 ){` |
|         - | 7675 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7676 | `			keep = FALSE;` |
|        64 | 7677 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7678 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7679 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7680 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7681 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7682 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7683 | `					int len;` |
|         3 | 7684 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7685 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7686 | `						"TypeError",` |
|         - | 7687 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7688 | `						zName` |
|         - | 7689 | `						);` |
|       ! 0 | 7690 | `				}else{` |
|       ! 0 | 7691 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7692 | `						"TypeError",` |
|         - | 7693 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7694 | `						ph7_type_name(apArg[1])` |
|         - | 7695 | `						);` |
|         - | 7696 | `				}` |
|         - | 7697 | `			}` |
|        33 | 7698 | `			keep = FALSE;` |
|        33 | 7699 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7700 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7701 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7702 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7703 | `				return PH7_EXCEPTION;` |
|         - | 7704 | `			}` |
|        31 | 7705 | `			if( rc == SXRET_OK ){` |
|         - | 7706 | `				/* Perform a boolean cast */` |
|        31 | 7707 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7708 | `			}` |
|        31 | 7709 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7710 | `		}else{` |
|         - | 7711 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7712 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7713 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7714 | `			 */` |
|        29 | 7715 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7716 | `		}` |
|        59 | 7717 | `		if( keep ){` |
|         - | 7718 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7719 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7720 | `		}` |
|         - | 7721 | `		/* Point to the next entry */` |
|        59 | 7722 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7723 | `	}` |
|        15 | 7724 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7725 | `	return PH7_OK;` |
|        17 | 7726 | `}` |
|         - | 7727 | `/*` |
|         - | 7728 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7729 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7730 | ` * Parameters` |
|         - | 7731 | ` *  $callback` |
|         - | 7732 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7733 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7734 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7735 | ` *   are zipped together.` |
|         - | 7736 | ` *  $array` |
|         - | 7737 | ` *   The first array to run through the callback function.` |
|         - | 7738 | ` *  $arrays` |
|         - | 7739 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7740 | ` * Return` |
|         - | 7741 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7742 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7743 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7744 | ` *  padding shorter arrays with NULL.` |
|         - | 7745 | ` */` |
|        84 | 7746 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7747 | `{` |
|         - | 7748 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7749 | `	ph7_hashmap_node *pEntry;` |
|         - | 7750 | `	ph7_hashmap *pMap;` |
|         - | 7751 | `	ph7_vm *pVm;` |
|         - | 7752 | `	int bNullCallback;` |
|         - | 7753 | `	sxi32 rc;` |
|         - | 7754 | `	int i;` |
|         - | 7755 | `	sxu32 n;` |
|        89 | 7756 | `	if( nArg < 2 ){` |
|       ! 0 | 7757 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7758 | `			"ArgumentCountError",` |
|         - | 7759 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7760 | `			nArg` |
|         - | 7761 | `			);` |
|         - | 7762 | `	}` |
|        89 | 7763 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        89 | 7764 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         8 | 7765 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         6 | 7766 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         8 | 7767 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7768 | `				"TypeError",` |
|         - | 7769 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7770 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7771 | `				zFunc` |
|         - | 7772 | `				);` |
|         - | 7773 | `		}` |
|         3 | 7774 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7775 | `			"TypeError",` |
|         - | 7776 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7777 | `			"no array or string given"` |
|         - | 7778 | `			);` |
|         - | 7779 | `	}` |
|         - | 7780 | `	/* Every remaining argument must be an array */` |
|       170 | 7781 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        94 | 7782 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7783 | `			if( i == 1 ){` |
|         4 | 7784 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7785 | `					"TypeError",` |
|         - | 7786 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7787 | `					ph7_type_name(apArg[1])` |
|         - | 7788 | `					);` |
|         - | 7789 | `			}` |
|       ! 0 | 7790 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7791 | `				"TypeError",` |
|         - | 7792 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7793 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7794 | `				);` |
|         - | 7795 | `		}` |
|        48 | 7796 | `	}` |
|        80 | 7797 | `	pVm = pCtx->pVm;` |
|         - | 7798 | `	/* Create a new array */` |
|        80 | 7799 | `	pArray = ph7_context_new_array(pCtx);` |
|        80 | 7800 | `	if( pArray == 0 ){` |
|       ! 0 | 7801 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7802 | `		return PH7_OK;` |
|         - | 7803 | `	}` |
|        80 | 7804 | `	PH7_MemObjInit(pVm,&sResult);` |
|        80 | 7805 | `	PH7_MemObjInit(pVm,&sKey);` |
|        80 | 7806 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        80 | 7807 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        80 | 7808 | `	if( nArg == 2 ){` |
|         - | 7809 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        70 | 7810 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        70 | 7811 | `		pEntry = pMap->pFirst;` |
|       240 | 7812 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7813 | `			/* Extract the node value */` |
|       178 | 7814 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|       178 | 7815 | `			if( pValue ){` |
|         - | 7816 | `				/* Extract the node key */` |
|       178 | 7817 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       178 | 7818 | `				if( bNullCallback ){` |
|         - | 7819 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7820 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7821 | `				}else{` |
|         - | 7822 | `					/* Invoke the supplied callback */` |
|       168 | 7823 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|       168 | 7824 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7825 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7826 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7827 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7828 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7829 | `						return PH7_EXCEPTION;` |
|         - | 7830 | `					}` |
|         - | 7831 | `					/* Insert the callback return value */` |
|       164 | 7832 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7833 | `				}` |
|       174 | 7834 | `				PH7_MemObjRelease(&sKey);` |
|       174 | 7835 | `				PH7_MemObjRelease(&sResult);` |
|        85 | 7836 | `			}` |
|         - | 7837 | `			/* Point to the next entry */` |
|       174 | 7838 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        89 | 7839 | `		}` |
|        35 | 7840 | `	}else{` |
|         - | 7841 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7842 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7843 | `		int nArrays = nArg - 1;` |
|         - | 7844 | `		ph7_hashmap_node **apCur;` |
|         - | 7845 | `		ph7_value **apCallArg;` |
|         - | 7846 | `		ph7_value sNull;` |
|        11 | 7847 | `		sxu32 nMax = 0;` |
|        11 | 7848 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7849 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7850 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7851 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7852 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7853 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7854 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7855 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7856 | `			return PH7_OK;` |
|         - | 7857 | `		}` |
|        11 | 7858 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7859 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7860 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7861 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7862 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7863 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7864 | `				nMax = pMap->nEntry;` |
|         6 | 7865 | `			}` |
|        12 | 7866 | `		}` |
|        35 | 7867 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7868 | `			ph7_value *pZip = 0;` |
|        25 | 7869 | `			if( bNullCallback ){` |
|         - | 7870 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7871 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7872 | `			}` |
|        79 | 7873 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7874 | `				ph7_value *pv = &sNull;` |
|        55 | 7875 | `				if( apCur[i] ){` |
|        53 | 7876 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7877 | `					if( pNodeVal ){` |
|        53 | 7878 | `						pv = pNodeVal;` |
|        26 | 7879 | `					}` |
|        53 | 7880 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7881 | `				}` |
|        55 | 7882 | `				if( bNullCallback ){` |
|         9 | 7883 | `					if( pZip ){` |
|         9 | 7884 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7885 | `					}` |
|         5 | 7886 | `				}else{` |
|        47 | 7887 | `					apCallArg[i] = pv;` |
|         - | 7888 | `				}` |
|        28 | 7889 | `			}` |
|        25 | 7890 | `			if( bNullCallback ){` |
|         5 | 7891 | `				if( pZip ){` |
|         5 | 7892 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7893 | `				}` |
|         3 | 7894 | `			}else{` |
|        21 | 7895 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7896 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7897 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7898 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7899 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7900 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7901 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7902 | `					return PH7_EXCEPTION;` |
|         - | 7903 | `				}` |
|        21 | 7904 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7905 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7906 | `			}` |
|        13 | 7907 | `		}` |
|        11 | 7908 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7909 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7910 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7911 | `	}` |
|        76 | 7912 | `	PH7_MemObjRelease(&sKey);` |
|        76 | 7913 | `	PH7_MemObjRelease(&sResult);` |
|        76 | 7914 | `	ph7_result_value(pCtx,pArray);` |
|        76 | 7915 | `	return PH7_OK;` |
|        47 | 7916 | `}` |
|         - | 7917 | `/*` |
|         - | 7918 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7919 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7920 | ` * Parameters` |
|         - | 7921 | ` *  $array` |
|         - | 7922 | ` *   The input array.` |
|         - | 7923 | ` *  $callback` |
|         - | 7924 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7925 | ` *  $initial` |
|         - | 7926 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7927 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7928 | ` * Return` |
|         - | 7929 | ` *  Returns the resulting value.` |
|         - | 7930 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7931 | ` */` |
|        30 | 7932 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7933 | `{` |
|         - | 7934 | `	ph7_hashmap_node *pEntry;` |
|         - | 7935 | `	ph7_hashmap *pMap;` |
|         - | 7936 | `	ph7_value *pValue;` |
|         - | 7937 | `	ph7_value sResult;` |
|         - | 7938 | `	sxi32 rc;` |
|         - | 7939 | `	sxu32 n;` |
|        35 | 7940 | `	if( nArg < 2 ){` |
|       ! 0 | 7941 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7942 | `			"ArgumentCountError",` |
|         - | 7943 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7944 | `			nArg` |
|         - | 7945 | `			);` |
|         - | 7946 | `	}` |
|        35 | 7947 | `	if( nArg > 3 ){` |
|         4 | 7948 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7949 | `			"ArgumentCountError",` |
|         - | 7950 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7951 | `			nArg` |
|         - | 7952 | `			);` |
|         - | 7953 | `	}` |
|        33 | 7954 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7955 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7956 | `			"TypeError",` |
|         - | 7957 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7958 | `			ph7_type_name(apArg[0])` |
|         - | 7959 | `			);` |
|         - | 7960 | `	}` |
|        31 | 7961 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7962 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7963 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7964 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7965 | `				"TypeError",` |
|         - | 7966 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7967 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7968 | `				zFunc` |
|         - | 7969 | `				);` |
|         - | 7970 | `		}` |
|         9 | 7971 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7972 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7973 | `				"TypeError",` |
|         - | 7974 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7975 | `				"array callback must have exactly two members"` |
|         - | 7976 | `				);` |
|         - | 7977 | `		}` |
|         6 | 7978 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7979 | `			"TypeError",` |
|         - | 7980 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7981 | `			"no array or string given"` |
|         - | 7982 | `			);` |
|         - | 7983 | `	}` |
|         - | 7984 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7985 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7986 | `	/* Assume a NULL initial value */` |
|        19 | 7987 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7988 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7989 | `	if( nArg > 2 ){` |
|         - | 7990 | `		/* Set the initial value */` |
|        13 | 7991 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7992 | `	}` |
|         - | 7993 | `	/* Perform the requested operation */` |
|        19 | 7994 | `	pEntry = pMap->pFirst;` |
|        55 | 7995 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7996 | `		/* Extract the node value */` |
|        39 | 7997 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7998 | `		/* Invoke the supplied callback */` |
|        39 | 7999 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 8000 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 8001 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 8002 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 8003 | `			return PH7_EXCEPTION;` |
|         - | 8004 | `		}` |
|         - | 8005 | `		/* Point to the next entry */` |
|        37 | 8006 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8007 | `	}` |
|        17 | 8008 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 8009 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 8010 | `	return PH7_OK;` |
|        20 | 8011 | `}` |
|         - | 8012 | `/*` |
|         - | 8013 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8014 | ` *  Apply a user function to every member of an array.` |
|         - | 8015 | ` * Parameters` |
|         - | 8016 | ` *  $array` |
|         - | 8017 | ` *   The input array.` |
|         - | 8018 | ` *  $funcname` |
|         - | 8019 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8020 | ` *   the first, and the key/index second.` |
|         - | 8021 | ` * Note:` |
|         - | 8022 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8023 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8024 | ` *  be made in the original array itself.` |
|         - | 8025 | ` *  $userdata` |
|         - | 8026 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8027 | ` *   to the callback funcname.` |
|         - | 8028 | ` * Return` |
|         - | 8029 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8030 | ` */` |
|        36 | 8031 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8032 | `{` |
|         - | 8033 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 8034 | `	ph7_hashmap_node *pEntry;` |
|         - | 8035 | `	ph7_hashmap *pMap;` |
|         - | 8036 | `	sxu32 n;` |
|        41 | 8037 | `	if( nArg < 2 ){` |
|       ! 0 | 8038 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8039 | `			"ArgumentCountError",` |
|         - | 8040 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 8041 | `			nArg` |
|         - | 8042 | `			);` |
|         - | 8043 | `	}` |
|        41 | 8044 | `	if( nArg > 3 ){` |
|         4 | 8045 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8046 | `			"ArgumentCountError",` |
|         - | 8047 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 8048 | `			nArg` |
|         - | 8049 | `			);` |
|         - | 8050 | `	}` |
|        39 | 8051 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8052 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8053 | `			"TypeError",` |
|         - | 8054 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8055 | `			ph7_type_name(apArg[0])` |
|         - | 8056 | `			);` |
|         - | 8057 | `	}` |
|        37 | 8058 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        17 | 8059 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         6 | 8060 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         8 | 8061 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8062 | `				"TypeError",` |
|         - | 8063 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8064 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 8065 | `				zFunc` |
|         - | 8066 | `				);` |
|         - | 8067 | `		}` |
|        12 | 8068 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8069 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8070 | `				"TypeError",` |
|         - | 8071 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8072 | `				"array callback must have exactly two members"` |
|         - | 8073 | `				);` |
|         - | 8074 | `		}` |
|         6 | 8075 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8076 | `			"TypeError",` |
|         - | 8077 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8078 | `			"no array or string given"` |
|         - | 8079 | `			);` |
|         - | 8080 | `	}` |
|        21 | 8081 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 8082 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 8083 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 8084 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8085 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 8086 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 8087 | `	/* Perform the desired operation */` |
|        21 | 8088 | `	pEntry = pMap->pFirst;` |
|        61 | 8089 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8090 | `		/* Extract the node value */` |
|        43 | 8091 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 8092 | `		if( pValue ){` |
|         - | 8093 | `			sxi32 rcW;` |
|         - | 8094 | `			/* Extract the entry key */` |
|        43 | 8095 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8096 | `			/* Invoke the supplied callback */` |
|        43 | 8097 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 8098 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 8099 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 8100 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 8101 | `				return PH7_EXCEPTION;` |
|         - | 8102 | `			}` |
|        20 | 8103 | `		}` |
|         - | 8104 | `		/* Point to the next entry */` |
|        41 | 8105 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 8106 | `	}` |
|         - | 8107 | `	/* All done, return TRUE */` |
|        19 | 8108 | `	ph7_result_bool(pCtx,1);` |
|        19 | 8109 | `	return PH7_OK;` |
|        23 | 8110 | `}` |
|         - | 8111 | `/*` |
|         - | 8112 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 8113 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 8114 | ` */` |
|        22 | 8115 | `static sxi32 HashmapWalkRecursive(` |
|         - | 8116 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 8117 | `	ph7_value *pCallback, /* User callback */` |
|         - | 8118 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 8119 | `	int iNest             /* Nesting level */` |
|         - | 8120 | `	)` |
|         1 | 8121 | `{` |
|         - | 8122 | `	ph7_hashmap_node *pEntry;` |
|         - | 8123 | `	ph7_value *pValue,sKey;` |
|         - | 8124 | `	sxi32 rc;` |
|         - | 8125 | `	sxu32 n;` |
|         - | 8126 | `	/* Iterate through hashmap entries */` |
|        23 | 8127 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 8128 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 8129 | `	pEntry = pMap->pFirst;` |
|        59 | 8130 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8131 | `		/* Extract the node value */` |
|        37 | 8132 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 8133 | `		if( pValue ){` |
|        37 | 8134 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 8135 | `				if( iNest < 32 ){` |
|         - | 8136 | `					/* Recurse */` |
|        11 | 8137 | `					iNest++;` |
|        11 | 8138 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8139 | `					iNest--;` |
|        11 | 8140 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8141 | `						return PH7_EXCEPTION;` |
|         - | 8142 | `					}` |
|         5 | 8143 | `				}` |
|         6 | 8144 | `			}else{` |
|         - | 8145 | `				/* Extract the node key */` |
|        27 | 8146 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8147 | `				/* Invoke the supplied callback */` |
|        27 | 8148 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8149 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8150 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8151 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8152 | `					return PH7_EXCEPTION;` |
|         - | 8153 | `				}` |
|         - | 8154 | `			}` |
|        18 | 8155 | `		}` |
|         - | 8156 | `		/* Point to the next entry */` |
|        37 | 8157 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8158 | `	}` |
|        23 | 8159 | `	return PH7_OK;` |
|        12 | 8160 | `}` |
|         - | 8161 | `/*` |
|         - | 8162 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8163 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8164 | ` * Parameters` |
|         - | 8165 | ` *  $array` |
|         - | 8166 | ` *   The input array.` |
|         - | 8167 | ` *  $funcname` |
|         - | 8168 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8169 | ` *   the first, and the key/index second.` |
|         - | 8170 | ` * Note:` |
|         - | 8171 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8172 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8173 | ` *  be made in the original array itself.` |
|         - | 8174 | ` *  $userdata` |
|         - | 8175 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8176 | ` *   to the callback funcname.` |
|         - | 8177 | ` * Return` |
|         - | 8178 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8179 | ` */` |
|        26 | 8180 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8181 | `{` |
|         - | 8182 | `	ph7_hashmap *pMap;` |
|        31 | 8183 | `	if( nArg < 2 ){` |
|       ! 0 | 8184 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8185 | `			"ArgumentCountError",` |
|         - | 8186 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8187 | `			nArg` |
|         - | 8188 | `			);` |
|         - | 8189 | `	}` |
|        31 | 8190 | `	if( nArg > 3 ){` |
|         4 | 8191 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8192 | `			"ArgumentCountError",` |
|         - | 8193 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8194 | `			nArg` |
|         - | 8195 | `			);` |
|         - | 8196 | `	}` |
|        29 | 8197 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8198 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8199 | `			"TypeError",` |
|         - | 8200 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8201 | `			ph7_type_name(apArg[0])` |
|         - | 8202 | `			);` |
|         - | 8203 | `	}` |
|        27 | 8204 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8205 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8206 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8207 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8208 | `				"TypeError",` |
|         - | 8209 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8210 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8211 | `				zFunc` |
|         - | 8212 | `				);` |
|         - | 8213 | `		}` |
|        12 | 8214 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8215 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8216 | `				"TypeError",` |
|         - | 8217 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8218 | `				"array callback must have exactly two members"` |
|         - | 8219 | `				);` |
|         - | 8220 | `		}` |
|         6 | 8221 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8222 | `			"TypeError",` |
|         - | 8223 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8224 | `			"no array or string given"` |
|         - | 8225 | `			);` |
|         - | 8226 | `	}` |
|         - | 8227 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8228 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8229 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8230 | `	/* Perform the desired operation */` |
|        13 | 8231 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8232 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8233 | `		return PH7_EXCEPTION;` |
|         - | 8234 | `	}` |
|         - | 8235 | `	/* All done, return TRUE */` |
|        13 | 8236 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8237 | `	return PH7_OK;` |
|        18 | 8238 | `}` |
|         - | 8239 | `/*` |
|         - | 8240 | ` * bool array_is_list(array $array)` |
|         - | 8241 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8242 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8243 | ` * Return` |
|         - | 8244 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8245 | ` */` |
|         - | 8246 | `/*` |
|         - | 8247 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8248 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8249 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8250 | ` */` |
|       336 | 8251 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         3 | 8252 | `{` |
|       339 | 8253 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       339 | 8254 | `	sxi64 iExpect = 0;` |
|         - | 8255 | `	sxu32 n;` |
|       777 | 8256 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       583 | 8257 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8258 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       145 | 8259 | `			return 0;` |
|         - | 8260 | `		}` |
|       441 | 8261 | `		++iExpect;` |
|       441 | 8262 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       222 | 8263 | `	}` |
|       197 | 8264 | `	return 1;` |
|       171 | 8265 | `}` |
|        12 | 8266 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8267 | `{` |
|        13 | 8268 | `	if( nArg < 1 ){` |
|       ! 0 | 8269 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8270 | `			"ArgumentCountError",` |
|         - | 8271 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8272 | `			);` |
|         - | 8273 | `	}` |
|        13 | 8274 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8275 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8276 | `			"TypeError",` |
|         - | 8277 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8278 | `			ph7_type_name(apArg[0])` |
|         - | 8279 | `			);` |
|         - | 8280 | `	}` |
|        13 | 8281 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8282 | `	return PH7_OK;` |
|         7 | 8283 | `}` |
|         - | 8284 | `/*` |
|         - | 8285 | ` * mixed array_first(array $array)` |
|         - | 8286 | ` * mixed array_last(array $array)` |
|         - | 8287 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8288 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8289 | ` *  untouched (unlike reset()/end()).` |
|         - | 8290 | ` */` |
|        18 | 8291 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8292 | `{` |
|         - | 8293 | `	ph7_hashmap *pMap;` |
|         - | 8294 | `	ph7_hashmap_node *pNode;` |
|         - | 8295 | `	ph7_value *pVal;` |
|        19 | 8296 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8297 | `	if( nArg < 1 ){` |
|       ! 0 | 8298 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8299 | `			"ArgumentCountError",` |
|         - | 8300 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8301 | `			zName` |
|         - | 8302 | `			);` |
|         - | 8303 | `	}` |
|        19 | 8304 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8305 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8306 | `			"TypeError",` |
|         - | 8307 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8308 | `			zName,` |
|         1 | 8309 | `			ph7_type_name(apArg[0])` |
|         - | 8310 | `			);` |
|         - | 8311 | `	}` |
|        17 | 8312 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8313 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8314 | `	if( pNode == 0 ){` |
|         - | 8315 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8316 | `		ph7_result_null(pCtx);` |
|         5 | 8317 | `		return PH7_OK;` |
|         - | 8318 | `	}` |
|        13 | 8319 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8320 | `	if( pVal ){` |
|        13 | 8321 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8322 | `	}else{` |
|       ! 0 | 8323 | `		ph7_result_null(pCtx);` |
|         - | 8324 | `	}` |
|        13 | 8325 | `	return PH7_OK;` |
|        10 | 8326 | `}` |
|         8 | 8327 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8328 | `{` |
|         9 | 8329 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8330 | `}` |
|        10 | 8331 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8332 | `{` |
|        11 | 8333 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8334 | `}` |
|         - | 8335 | `/*` |
|         - | 8336 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8337 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8338 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8339 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8340 | ` *  untouched.` |
|         - | 8341 | ` */` |
|        22 | 8342 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8343 | `{` |
|         - | 8344 | `	ph7_hashmap *pMap;` |
|         - | 8345 | `	ph7_hashmap_node *pNode;` |
|        23 | 8346 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8347 | `	if( nArg < 1 ){` |
|       ! 0 | 8348 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8349 | `			"ArgumentCountError",` |
|         - | 8350 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8351 | `			zName` |
|         - | 8352 | `			);` |
|         - | 8353 | `	}` |
|        23 | 8354 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8355 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8356 | `			"TypeError",` |
|         - | 8357 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8358 | `			zName,` |
|         1 | 8359 | `			ph7_type_name(apArg[0])` |
|         - | 8360 | `			);` |
|         - | 8361 | `	}` |
|        21 | 8362 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8363 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8364 | `	if( pNode == 0 ){` |
|         - | 8365 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8366 | `		ph7_result_null(pCtx);` |
|         5 | 8367 | `		return PH7_OK;` |
|         - | 8368 | `	}` |
|        17 | 8369 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8370 | `	return PH7_OK;` |
|        12 | 8371 | `}` |
|        10 | 8372 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8373 | `{` |
|        11 | 8374 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8375 | `}` |
|        12 | 8376 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8377 | `{` |
|        13 | 8378 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8379 | `}` |
|         - | 8380 | `/*` |
|         - | 8381 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8382 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8383 | ` * array_column() for both the column value and the index key.` |
|         - | 8384 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8385 | ` * container or the key is absent.` |
|         - | 8386 | ` */` |
|        32 | 8387 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8388 | `{` |
|        33 | 8389 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8390 | `		ph7_hashmap_node *pNode;` |
|        25 | 8391 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8392 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8393 | `		}` |
|        11 | 8394 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8395 | `		ph7_value sName;` |
|         - | 8396 | `		const char *zName;` |
|         - | 8397 | `		ph7_value *pAttr;` |
|         - | 8398 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8399 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8400 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8401 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8402 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8403 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8404 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8405 | `		return pAttr;` |
|         - | 8406 | `	}` |
|         5 | 8407 | `	return 0;` |
|        17 | 8408 | `}` |
|         - | 8409 | `/*` |
|         - | 8410 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8411 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8412 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8413 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8414 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8415 | ` *  Each row may be an array or an object.` |
|         - | 8416 | ` */` |
|        12 | 8417 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8418 | `{` |
|         - | 8419 | `	ph7_hashmap_node *pNode;` |
|         - | 8420 | `	ph7_hashmap *pMap;` |
|         - | 8421 | `	ph7_value *pArray;` |
|         - | 8422 | `	ph7_value *pRow;` |
|         - | 8423 | `	ph7_value *pCol;` |
|         - | 8424 | `	ph7_value *pIdx;` |
|         - | 8425 | `	int bWantCol;` |
|         - | 8426 | `	int bWantIdx;` |
|         - | 8427 | `	sxu32 n;` |
|        13 | 8428 | `	if( nArg < 2 ){` |
|       ! 0 | 8429 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8430 | `			"ArgumentCountError",` |
|         - | 8431 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8432 | `			nArg` |
|         - | 8433 | `			);` |
|         - | 8434 | `	}` |
|        13 | 8435 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8436 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8437 | `			"TypeError",` |
|         - | 8438 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8439 | `			ph7_type_name(apArg[0])` |
|         - | 8440 | `			);` |
|         - | 8441 | `	}` |
|        13 | 8442 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8443 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8444 | `	if( pArray == 0 ){` |
|       ! 0 | 8445 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8446 | `		return PH7_OK;` |
|         - | 8447 | `	}` |
|         - | 8448 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8449 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8450 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8451 | `	pNode = pMap->pFirst;` |
|        33 | 8452 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8453 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8454 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8455 | `		if( pRow == 0 ){` |
|       ! 0 | 8456 | `			continue;` |
|         - | 8457 | `		}` |
|        21 | 8458 | `		if( bWantCol ){` |
|        19 | 8459 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8460 | `			if( pCol == 0 ){` |
|         - | 8461 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8462 | `				continue;` |
|         - | 8463 | `			}` |
|         9 | 8464 | `		}else{` |
|         3 | 8465 | `			pCol = pRow;` |
|         - | 8466 | `		}` |
|        19 | 8467 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8468 | `		if( pIdx ){` |
|        13 | 8469 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8470 | `		}else{` |
|         7 | 8471 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8472 | `		}` |
|        10 | 8473 | `	}` |
|        13 | 8474 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8475 | `	return PH7_OK;` |
|         7 | 8476 | `}` |
|         - | 8477 | `/*` |
|         - | 8478 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8479 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8480 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8481 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8482 | ` */` |
|        28 | 8483 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8484 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8485 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8486 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8487 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8488 | `	)` |
|         1 | 8489 | `{` |
|         - | 8490 | `	ph7_hashmap_node *pEntry;` |
|         - | 8491 | `	ph7_hashmap *pMap;` |
|         - | 8492 | `	ph7_value *pValue;` |
|         - | 8493 | `	ph7_value *apCbArg[2];` |
|         - | 8494 | `	ph7_value sKey;` |
|         - | 8495 | `	ph7_value sResult;` |
|         - | 8496 | `	sxi32 rc;` |
|         - | 8497 | `	sxu32 n;` |
|        29 | 8498 | `	*ppMatch = 0;` |
|        29 | 8499 | `	if( nArg < 2 ){` |
|       ! 0 | 8500 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8501 | `			"ArgumentCountError",` |
|         - | 8502 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8503 | `			zName,nArg` |
|         - | 8504 | `			);` |
|         - | 8505 | `	}` |
|        29 | 8506 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8507 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8508 | `			"TypeError",` |
|         - | 8509 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8510 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8511 | `			);` |
|         - | 8512 | `	}` |
|        29 | 8513 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8514 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8515 | `			"TypeError",` |
|         - | 8516 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8517 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8518 | `			);` |
|         - | 8519 | `	}` |
|        29 | 8520 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8521 | `	pEntry = pMap->pFirst;` |
|        29 | 8522 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8523 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8524 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8525 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8526 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8527 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8528 | `		if( pValue ){` |
|         - | 8529 | `			/* The callback receives ($value, $key). */` |
|        59 | 8530 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8531 | `			apCbArg[0] = pValue;` |
|        59 | 8532 | `			apCbArg[1] = &sKey;` |
|        59 | 8533 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8534 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8535 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8536 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8537 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8538 | `				return PH7_EXCEPTION;` |
|         - | 8539 | `			}` |
|        59 | 8540 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8541 | `				*ppMatch = pEntry;` |
|        15 | 8542 | `				break;` |
|         - | 8543 | `			}` |
|        22 | 8544 | `		}` |
|        45 | 8545 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8546 | `	}` |
|        29 | 8547 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8548 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8549 | `	return PH7_OK;` |
|        15 | 8550 | `}` |
|         - | 8551 | `/*` |
|         - | 8552 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8553 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8554 | ` *  is truthy, or NULL if none match.` |
|         - | 8555 | ` */` |
|         6 | 8556 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8557 | `{` |
|         - | 8558 | `	ph7_hashmap_node *pMatch;` |
|         - | 8559 | `	ph7_value *pVal;` |
|         - | 8560 | `	sxi32 rc;` |
|         7 | 8561 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8562 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8563 | `		return rc;` |
|         - | 8564 | `	}` |
|         7 | 8565 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8566 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8567 | `	}else{` |
|         3 | 8568 | `		ph7_result_null(pCtx);` |
|         - | 8569 | `	}` |
|         7 | 8570 | `	return PH7_OK;` |
|         4 | 8571 | `}` |
|         - | 8572 | `/*` |
|         - | 8573 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8574 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8575 | ` *  is truthy, or NULL if none match.` |
|         - | 8576 | ` */` |
|         6 | 8577 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8578 | `{` |
|         - | 8579 | `	ph7_hashmap_node *pMatch;` |
|         - | 8580 | `	sxi32 rc;` |
|         7 | 8581 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8582 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8583 | `		return rc;` |
|         - | 8584 | `	}` |
|         7 | 8585 | `	if( pMatch == 0 ){` |
|         3 | 8586 | `		ph7_result_null(pCtx);` |
|         6 | 8587 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8588 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8589 | `	}else{` |
|         4 | 8590 | `		ph7_result_string(pCtx,` |
|         2 | 8591 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8592 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8593 | `	}` |
|         7 | 8594 | `	return PH7_OK;` |
|         4 | 8595 | `}` |
|         - | 8596 | `/*` |
|         - | 8597 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8598 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8599 | ` *  FALSE for an empty array.` |
|         - | 8600 | ` */` |
|         8 | 8601 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8602 | `{` |
|         - | 8603 | `	ph7_hashmap_node *pMatch;` |
|         - | 8604 | `	sxi32 rc;` |
|         9 | 8605 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8606 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8607 | `		return rc;` |
|         - | 8608 | `	}` |
|         9 | 8609 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8610 | `	return PH7_OK;` |
|         5 | 8611 | `}` |
|         - | 8612 | `/*` |
|         - | 8613 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8614 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8615 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8616 | ` */` |
|         8 | 8617 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8618 | `{` |
|         - | 8619 | `	ph7_hashmap_node *pMatch;` |
|         - | 8620 | `	sxi32 rc;` |
|         9 | 8621 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8622 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8623 | `		return rc;` |
|         - | 8624 | `	}` |
|         9 | 8625 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8626 | `	return PH7_OK;` |
|         5 | 8627 | `}` |
|         - | 8628 | `/*` |
|         - | 8629 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8630 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8631 | ` */` |
|         - | 8632 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8633 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8634 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8635 | `{` |
|        84 | 8636 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8637 | `	(void)pVm;` |
|        84 | 8638 | `	p->nCount++;` |
|        84 | 8639 | `	if( p->pArray ){` |
|         - | 8640 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8641 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8642 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8643 | `	}` |
|        84 | 8644 | `	return SXRET_OK;` |
|         4 | 8645 | `}` |
|         - | 8646 | `/*` |
|         - | 8647 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8648 | ` */` |
|        30 | 8649 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8650 | `{` |
|         - | 8651 | `	struct IterCollect sCol;` |
|         - | 8652 | `	ph7_value *pArray;` |
|         - | 8653 | `	sxi32 rc;` |
|        34 | 8654 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8655 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8656 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8657 | `	sCol.pArray = pArray;` |
|        34 | 8658 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8659 | `	sCol.nCount = 0;` |
|        34 | 8660 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8661 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8662 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8663 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8664 | `		sxu32 n;` |
|         9 | 8665 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8666 | `			ph7_value sKey, *pVal;` |
|         7 | 8667 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8668 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8669 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8670 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8671 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8672 | `			pEntry = pEntry->pPrev;` |
|         4 | 8673 | `		}` |
|         3 | 8674 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8675 | `		return PH7_OK;` |
|         - | 8676 | `	}` |
|        32 | 8677 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8678 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8679 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8680 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8681 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8682 | `			ph7_type_name(apArg[0]));` |
|         - | 8683 | `	}` |
|        30 | 8684 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8685 | `	return PH7_OK;` |
|        19 | 8686 | `}` |
|         - | 8687 | `/*` |
|         - | 8688 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8689 | ` */` |
|         8 | 8690 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8691 | `{` |
|         - | 8692 | `	struct IterCollect sCol;` |
|         - | 8693 | `	sxi32 rc;` |
|         9 | 8694 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8695 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8696 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8697 | `		return PH7_OK;` |
|         - | 8698 | `	}` |
|         7 | 8699 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8700 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8701 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8702 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8703 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8704 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8705 | `			ph7_type_name(apArg[0]));` |
|         - | 8706 | `	}` |
|         7 | 8707 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8708 | `	return PH7_OK;` |
|         5 | 8709 | `}` |
|         - | 8710 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8711 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8712 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8713 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8714 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8715 | `{` |
|        33 | 8716 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8717 | `	ph7_value sResult;` |
|         - | 8718 | `	SySet aArg;` |
|         - | 8719 | `	sxi32 rc;` |
|         - | 8720 | `	int bContinue;` |
|        16 | 8721 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8722 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8723 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8724 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8725 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8726 | `		sxu32 n;` |
|        17 | 8727 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8728 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8729 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8730 | `			pEntry = pEntry->pPrev;` |
|         5 | 8731 | `		}` |
|         4 | 8732 | `	}` |
|        33 | 8733 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8734 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8735 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8736 | `	SySetRelease(&aArg);` |
|        33 | 8737 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8738 | `	p->nCount++;` |
|        31 | 8739 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8740 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8741 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8742 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8743 | `}` |
|         - | 8744 | `/*` |
|         - | 8745 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8746 | ` */` |
|        12 | 8747 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8748 | `{` |
|         - | 8749 | `	struct IterApply sApp;` |
|         - | 8750 | `	sxi32 rc;` |
|        13 | 8751 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8752 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8753 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8754 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8755 | `	}` |
|        13 | 8756 | `	sApp.pCallback = apArg[1];` |
|        13 | 8757 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8758 | `	sApp.nCount = 0;` |
|        13 | 8759 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8760 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8761 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8762 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8763 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8764 | `			ph7_type_name(apArg[0]));` |
|         - | 8765 | `	}` |
|        11 | 8766 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8767 | `	return PH7_OK;` |
|         7 | 8768 | `}` |
|         - | 8769 | `/*` |
|         - | 8770 | ` * Table of hashmap functions.` |
|         - | 8771 | ` */` |
|         - | 8772 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8773 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8774 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8775 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8776 | `	{"count",             ph7_hashmap_count },` |
|         - | 8777 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8778 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8779 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8780 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8781 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8782 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8783 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8784 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8785 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8786 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8787 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8788 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8789 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8790 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8791 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8792 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8793 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8794 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8795 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8796 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8797 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8798 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8799 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8800 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8801 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8802 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8803 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8804 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8805 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8806 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8807 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8808 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8809 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8810 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8811 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8812 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8813 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8814 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8815 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8816 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8817 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8818 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8819 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8820 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8821 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8822 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8823 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8824 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8825 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8826 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8827 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8828 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8829 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8830 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8831 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8832 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8833 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8834 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8835 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8836 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8837 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8838 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8839 | `	{"current",           ph7_hashmap_current },` |
|         - | 8840 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8841 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8842 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8843 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8844 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8845 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8846 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8847 | `};` |
|         - | 8848 | `/*` |
|         - | 8849 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8850 | ` */` |
|      3428 | 8851 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8852 | `{` |
|         - | 8853 | `	sxu32 n;` |
|    257105 | 8854 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    253677 | 8855 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    126841 | 8856 | `	}` |
|      3433 | 8857 | `}` |
|         - | 8858 | `/*` |
|         - | 8859 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8860 | ` * the BLOB given as the first argument.` |
|         - | 8861 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8862 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8863 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8864 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8865 | ` */` |
|         - | 8866 | `/*` |
|         - | 8867 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8868 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8869 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8870 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8871 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8872 | ` */` |
|       134 | 8873 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8874 | `{` |
|       136 | 8875 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8876 | `	ph7_value *pObj;` |
|       136 | 8877 | `	sxu32 n = 0;` |
|         - | 8878 | `	int isRef;` |
|       136 | 8879 | `	sxi32 rc = SXRET_OK;` |
|         - | 8880 | `	int i;` |
|       216 | 8881 | `	for(;;){` |
|       434 | 8882 | `		if( n >= pMap->nEntry ){` |
|       136 | 8883 | `			break;` |
|         - | 8884 | `		}` |
|       300 | 8885 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8886 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8887 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       596 | 8888 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       298 | 8889 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       300 | 8890 | `		if( ShowType ){` |
|         - | 8891 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8892 | `			 * on the next line at the same indent (php). */` |
|       176 | 8893 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       118 | 8894 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        60 | 8895 | `			}` |
|        60 | 8896 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        47 | 8897 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        24 | 8898 | `			}else{` |
|        20 | 8899 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8900 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8901 | `			}` |
|        60 | 8902 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        60 | 8903 | `			if( pObj ){` |
|        60 | 8904 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        60 | 8905 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8906 | `					break;` |
|         - | 8907 | `				}` |
|        29 | 8908 | `			}` |
|        31 | 8909 | `		}else{` |
|         - | 8910 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8911 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8912 | `			 * php's extra blank line. References carry no marker. */` |
|      1314 | 8913 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1074 | 8914 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       538 | 8915 | `			}` |
|       242 | 8916 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       129 | 8917 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        65 | 8918 | `			}else{` |
|       170 | 8919 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8920 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8921 | `			}` |
|       240 | 8922 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       134 | 8923 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8924 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8925 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8926 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8927 | `					break;` |
|         - | 8928 | `				}` |
|        13 | 8929 | `			}else{` |
|       218 | 8930 | `				if( pObj ){` |
|       218 | 8931 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       108 | 8932 | `				}` |
|       218 | 8933 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8934 | `			}` |
|         - | 8935 | `		}` |
|         - | 8936 | `		/* Point to the next entry */` |
|       300 | 8937 | `		n++;` |
|       300 | 8938 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8939 | `	}` |
|       136 | 8940 | `	return rc;` |
|         2 | 8941 | `}` |
|       130 | 8942 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8943 | `{` |
|         - | 8944 | `	sxi32 rc;` |
|         - | 8945 | `	int i;` |
|       132 | 8946 | `	if( nDepth > 31 ){` |
|         - | 8947 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8948 | `		/* Nesting limit reached */` |
|       ! 0 | 8949 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8950 | `		return SXERR_LIMIT;` |
|         - | 8951 | `	}` |
|       132 | 8952 | `	if( ShowType ){` |
|         - | 8953 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8954 | `		 * newline (a nested array is itself an entry value line). */` |
|        26 | 8955 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        26 | 8956 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        26 | 8957 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        26 | 8958 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8959 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8960 | `		}` |
|        26 | 8961 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        26 | 8962 | `		return rc;` |
|         - | 8963 | `	}` |
|         - | 8964 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       107 | 8965 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       299 | 8966 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8967 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8968 | `	}` |
|       107 | 8969 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       107 | 8970 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       299 | 8971 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8972 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8973 | `	}` |
|       107 | 8974 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       107 | 8975 | `	return rc;` |
|        67 | 8976 | `}` |
|         - | 8977 | `/*` |
|         - | 8978 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8979 | ` * retrieved entry.` |
|         - | 8980 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8981 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8982 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8983 | ` * a value different from PH7_OK.` |
|         - | 8984 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8985 | ` */` |
|     35216 | 8986 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8987 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8988 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8989 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8990 | `	)` |
|         5 | 8991 | `{` |
|         - | 8992 | `	ph7_hashmap_node *pEntry;` |
|         - | 8993 | `	ph7_value sKey,sValue;` |
|         - | 8994 | `	sxi32 rc;` |
|         - | 8995 | `	sxu32 n;` |
|         - | 8996 | `	/* Initialize walker parameter */` |
|     35221 | 8997 | `	rc = SXRET_OK;` |
|     35221 | 8998 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     35221 | 8999 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     35221 | 9000 | `	n = pMap->nEntry;` |
|     35221 | 9001 | `	pEntry = pMap->pFirst;` |
|         - | 9002 | `	/* Start the iteration process */` |
|     98448 | 9003 | `	for(;;){` |
|    196901 | 9004 | `		if( n < 1 ){` |
|     35219 | 9005 | `			break;` |
|         - | 9006 | `		}` |
|         - | 9007 | `		/* Extract a copy of the key and a copy the current value */` |
|    161687 | 9008 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    161687 | 9009 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 9010 | `		/* Invoke the user callback */` |
|    161687 | 9011 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 9012 | `		/* Release the copy of the key and the value */` |
|    161687 | 9013 | `		PH7_MemObjRelease(&sKey);` |
|    161687 | 9014 | `		PH7_MemObjRelease(&sValue);` |
|    161687 | 9015 | `		if( rc != PH7_OK ){` |
|         - | 9016 | `			/* Callback request an operation abort */` |
|         3 | 9017 | `			return SXERR_ABORT;` |
|         - | 9018 | `		}` |
|         - | 9019 | `		/* Point to the next entry */` |
|    161685 | 9020 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    161685 | 9021 | `		n--;` |
|         5 | 9022 | `	}` |
|         - | 9023 | `	/* All done */` |
|     35219 | 9024 | `	return SXRET_OK;` |
|     17613 | 9025 | `}` |
|         - | 9026 |  |
