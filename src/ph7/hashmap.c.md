# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4002/4483 lines (89.27%)

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
|   7513762 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7513767 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7513767 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    634628 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    634633 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    634633 |   35 | `	sxu32 nH = 5381;` |
|    634633 |   36 | `	zEnd = &zIn[nLen];` |
|    717585 |   37 | `	for(;;){` |
|   1435175 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1228417 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1100387 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    958309 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    634633 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      2048 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      2053 |   54 | `	sxi64 iCount = 0;` |
|      2053 |   55 | `	if( !bRecursive ){` |
|      1879 |   56 | `		iCount = pMap->nEntry;` |
|       942 |   57 | `	}else{` |
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
|      2053 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3209662 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3209667 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3209667 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3209667 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3209667 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3209667 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3209667 |  112 | `	pNode->nHash = nHash;` |
|   3209667 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3209667 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3209667 |  115 | `	return pNode;` |
|   1604836 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    262932 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    262937 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    262937 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    262937 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    262937 |  133 | `	pNode->pMap  = &(*pMap);` |
|    262937 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    262937 |  135 | `	pNode->nHash = nHash;` |
|    262937 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    262937 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    262937 |  138 | `	pNode->nValIdx = nValIdx;` |
|    262937 |  139 | `	return pNode;` |
|    131471 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3472594 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3472599 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2995207 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2995207 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1497601 |  150 | `	}` |
|   3472599 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3472599 |  153 | `	if( pMap->pFirst == 0 ){` |
|     90613 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     90613 |  156 | `		pMap->pCur = pNode;` |
|     45309 |  157 | `	}else{` |
|   3381991 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3472599 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3472599 |  174 | `	++pMap->nEntry;` |
|   3472599 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7890 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7895 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7895 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7895 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7427 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3716 |  187 | `	}else{` |
|       470 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7895 |  190 | `	if( pNode->pNextCollide ){` |
|      5033 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2515 |  192 | `	}` |
|      7895 |  193 | `	if( pMap->pFirst == pNode ){` |
|       171 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        83 |  195 | `	}` |
|      7895 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       203 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        99 |  199 | `	}` |
|      7895 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7895 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7895 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       209 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       209 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       209 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       102 |  218 | `		}` |
|       102 |  219 | `	}` |
|      7895 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7657 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3826 |  222 | `	}` |
|      7895 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7895 |  224 | `	pMap->nEntry--;` |
|      7895 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|        99 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        99 |  228 | `		pMap->apBucket = 0;` |
|        99 |  229 | `		pMap->nSize = 0;` |
|        99 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        47 |  231 | `	}` |
|      7895 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3472594 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3472599 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     95963 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     95963 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     95963 |  245 | `		if( nNew < 1 ){` |
|     90613 |  246 | `			nNew = 16;` |
|     45304 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     95963 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     95963 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     95963 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     95963 |  260 | `		pMap->apBucket = apNew;` |
|     95963 |  261 | `		pMap->nSize = nNew;` |
|     95963 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     90613 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5355 |  267 | `		pEntry = pMap->pFirst;` |
|      5355 |  268 | `		n = 0;` |
|   2140307 |  269 | `		for( ;; ){` |
|   4280619 |  270 | `			if( n >= pMap->nEntry ){` |
|      5355 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4275269 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4275269 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4275269 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3624481 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3624481 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1812238 |  280 | `			}` |
|   4275269 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4275269 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4275269 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5355 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2675 |  288 | `	}` |
|   3381991 |  289 | `	return SXRET_OK;` |
|   1736302 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3209662 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3209667 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3209625 |  310 | `		if( pValue ){` |
|   3209619 |  311 | `			sSafeVal = *pValue;` |
|   3209619 |  312 | `			pValue = &sSafeVal;` |
|   1604807 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3209625 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3209625 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3209625 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3209619 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1604807 |  322 | `		}` |
|   3209625 |  323 | `		nIdx = pObj->nIdx;` |
|   1604815 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3209667 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3209667 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3209667 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3209667 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3209667 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3209667 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3209667 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3209667 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3209667 |  349 | `	return SXRET_OK;` |
|   1604836 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    262932 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    262937 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    216993 |  370 | `		if( pValue ){` |
|    216683 |  371 | `			sSafeVal = *pValue;` |
|    216683 |  372 | `			pValue = &sSafeVal;` |
|    108339 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    216993 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    216993 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    216993 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    216683 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    108339 |  382 | `		}` |
|    216993 |  383 | `		nIdx = pObj->nIdx;` |
|    108499 |  384 | `	}else{` |
|     45949 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    262937 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    262937 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    262937 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    262937 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     45949 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     22972 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    262937 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    262937 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    262937 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    262937 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    262937 |  409 | `	return SXRET_OK;` |
|    131471 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4290696 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4290701 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       771 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4289935 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4289935 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110566949 |  433 | `	for(;;){` |
| 221133903 |  434 | `		if( pNode == 0 ){` |
|   4284389 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216849514 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216846502 |  438 | `			&& pNode->nHash == nHash` |
| 108424523 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      5551 |  441 | `				if( ppNode ){` |
|      5529 |  442 | `					*ppNode = pNode;` |
|      2762 |  443 | `				}` |
|      5551 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216843970 |  447 | `		pNode = pNode->pNextCollide;` |
|         2 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4284389 |  450 | `	return SXERR_NOTFOUND;` |
|   2145353 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    406318 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    406323 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     34627 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    371701 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    371701 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    305879 |  475 | `	for(;;){` |
|    611763 |  476 | `		if( pNode == 0 ){` |
|    306923 |  477 | `			break;` |
|         - |  478 | `		}` |
|    304840 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    303329 |  480 | `			&& pNode->nHash == nHash` |
|    183348 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     64883 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     64783 |  484 | `				if( ppNode ){` |
|     64755 |  485 | `					*ppNode = pNode;` |
|     32375 |  486 | `				}` |
|     64783 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    240067 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    306923 |  493 | `	return SXERR_NOTFOUND;` |
|    203164 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    406450 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    406455 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    406455 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    406455 |  504 | `	int isNeg = FALSE, nDigit;` |
|    406455 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    406433 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    406429 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    406429 |  516 | `	zDigit = zIn;` |
|    203646 |  517 | `	for(;;){` |
|    407297 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    407047 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    406179 |  523 | `			return FALSE;` |
|         - |  524 | `		}` |
|       869 |  525 | `		zIn++;` |
|         1 |  526 | `	}` |
|         - |  527 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  528 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  529 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  530 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       251 |  531 | `	nDigit = (int)(zEnd - zDigit);` |
|       251 |  532 | `	if( nDigit < 1 ){` |
|         - |  533 | `		/* A lone sign ("-"/"+") */` |
|       ! 0 |  534 | `		return FALSE;` |
|         - |  535 | `	}` |
|       255 |  536 | `	if( nDigit > 19 \|\|` |
|       128 |  537 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|         7 |  538 | `		return FALSE;` |
|         - |  539 | `	}` |
|       245 |  540 | `	return TRUE;` |
|    203230 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    148934 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    148939 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    148939 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    143509 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    143509 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    143495 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    143495 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|         7 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      5449 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        27 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        13 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      5449 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     74467 |  575 | `result:` |
|    148939 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     69447 |  578 | `		if( ppNode ){` |
|     69395 |  579 | `			*ppNode = pNode;` |
|     34695 |  580 | `		}` |
|     69447 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     79497 |  584 | `	return SXERR_NOTFOUND;` |
|     74472 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  588 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  589 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  590 | ` * via HashmapAppendIndexBusy.` |
|         - |  591 | ` */` |
|   2142576 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2142581 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       815 |  596 | `		pMap->bIntKeySeen = 1;` |
|       815 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       815 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|       815 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2141771 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141521 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2141521 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1070758 |  609 | `	}` |
|   1071293 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   1066688 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1066693 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1066687 |  623 | `	return FALSE;` |
|    533349 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3426182 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3426187 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3426187 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3426187 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    216995 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    216995 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       229 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    325148 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    108381 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  654 | `				/* Overwrite the old value */` |
|         - |  655 | `				ph7_value *pElem;` |
|       490 |  656 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       490 |  657 | `				if( pElem ){` |
|       490 |  658 | `					if( pVal ){` |
|       490 |  659 | `						PH7_MemObjStore(pVal,pElem);` |
|       247 |  660 | `					}else{` |
|         - |  661 | `						/* Nullify the entry */` |
|       ! 0 |  662 | `						PH7_MemObjToNull(pElem);` |
|         - |  663 | `					}` |
|       243 |  664 | `				}` |
|       490 |  665 | `				return SXRET_OK;` |
|         - |  666 | `		}` |
|    216281 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  668 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  669 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       131 |  670 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  671 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  672 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  673 | `				return SXRET_OK;` |
|         - |  674 | `			}` |
|       196 |  675 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       130 |  676 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        65 |  677 | `				pVal,SXU32_HIGH);` |
|         - |  678 | `		}` |
|         - |  679 | `		/* Perform a blob-key insertion */` |
|    216151 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    216151 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1604596 |  683 | `IntKey:` |
|   3209425 |  684 | `	if( pKey ){` |
|   2142771 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       259 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       129 |  688 | `		}` |
|   2142771 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  690 | `			/* Overwrite the old value */` |
|         - |  691 | `			ph7_value *pElem;` |
|       196 |  692 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       196 |  693 | `			if( pElem ){` |
|       196 |  694 | `				if( pVal ){` |
|       196 |  695 | `					PH7_MemObjStore(pVal,pElem);` |
|        99 |  696 | `				}else{` |
|         - |  697 | `					/* Nullify the entry */` |
|       ! 0 |  698 | `					PH7_MemObjToNull(pElem);` |
|         - |  699 | `				}` |
|        97 |  700 | `			}` |
|       196 |  701 | `			return SXRET_OK;` |
|         - |  702 | `		}` |
|   2142577 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2142575 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2142575 |  712 | `		if( rc == SXRET_OK ){` |
|   2142575 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1071285 |  714 | `		}` |
|   1071290 |  715 | `	}else{` |
|   1066659 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1066657 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1066651 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1066651 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1066649 |  726 | `			++pMap->iNextIdx;` |
|    533322 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3209221 |  730 | `	return rc;` |
|   1713096 |  731 | `}` |
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
|     45996 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     46001 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     46001 |  766 | `	sxi32 rc = SXRET_OK;` |
|     46001 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     45961 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     45961 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     68936 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     22977 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     45949 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     45949 |  790 | `		return rc;` |
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
|     23003 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1500451 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1500456 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1500456 |  832 | `	return pObj;` |
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
|     72524 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     72529 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     72529 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     72529 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     72529 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     72529 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     72529 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     72529 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     72529 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     72529 |  921 | `	return rc;` |
|     36241 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     14170 |  927 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     14175 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     14175 |  932 | `	if( pEntry->pPrevCollide ){` |
|     11402 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5708 |  934 | `	}else{` |
|      2778 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     14175 |  937 | `	if( pEntry->pNextCollide ){` |
|       982 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       509 |  939 | `	}` |
|     14175 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     14175 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     14175 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     14175 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     14175 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14175 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     11721 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5867 |  949 | `	}` |
|     14175 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14175 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     14175 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     14175 |  956 | `		pMap->iNextIdx++;` |
|      7085 |  957 | `	}` |
|     14175 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     33196 |  966 | `static int HashmapFindValue(` |
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
|     33201 |  979 | `	pEntry = pMap->pFirst;` |
|     33201 |  980 | `	n = pMap->nEntry;` |
|     33201 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33201 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     79239 |  983 | `	for(;;){` |
|    158484 |  984 | `		if( n < 1 ){` |
|        79 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    158406 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    158406 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    158406 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    158406 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    158406 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    158406 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    158406 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    158406 | 1001 | `			if( rc == 0 ){` |
|     33123 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     33123 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     62641 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    125288 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    125288 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|        79 | 1014 | `	return SXERR_NOTFOUND;` |
|     16603 | 1015 | `}` |
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
|        30 | 1136 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1137 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1138 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1139 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1140 | `	)` |
|         1 | 1141 | `{` |
|         - | 1142 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1143 | `	sxi32 rc;` |
|         - | 1144 | `	sxu32 n;` |
|        31 | 1145 | `	if( pLeft == pRight ){` |
|         - | 1146 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1147 | `		 * Unlike the zend engine.` |
|         - | 1148 | `		 */` |
|         3 | 1149 | `		return 0;` |
|         - | 1150 | `	}` |
|        29 | 1151 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1152 | `		/* Must have the same number of entries */` |
|         5 | 1153 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1154 | `	}` |
|         - | 1155 | `	/* Point to the first inserted entry of the left hashmap */` |
|        25 | 1156 | `	pLe = pLeft->pFirst;` |
|        25 | 1157 | `	pRe = 0; /* cc warning */` |
|         - | 1158 | `	/* Perform the comparison */` |
|        25 | 1159 | `	n = pLeft->nEntry;` |
|        59 | 1160 | `	for(;;){` |
|       119 | 1161 | `		if( n < 1 ){` |
|        23 | 1162 | `			break;` |
|         - | 1163 | `		}` |
|        97 | 1164 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1165 | `			/* Int key */` |
|        89 | 1166 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        45 | 1167 | `		}else{` |
|         9 | 1168 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1169 | `			/* Blob key */` |
|         9 | 1170 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1171 | `		}` |
|        97 | 1172 | `		if( rc != SXRET_OK ){` |
|         - | 1173 | `			/* No such entry in the right side */` |
|       ! 0 | 1174 | `			return 1;` |
|         - | 1175 | `		}` |
|        97 | 1176 | `		rc = 0;` |
|        97 | 1177 | `		if( bStrict ){` |
|         - | 1178 | `			/* Make sure,the keys are of the same type */` |
|        81 | 1179 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1180 | `				rc = 1;` |
|       ! 0 | 1181 | `			}` |
|        40 | 1182 | `		}` |
|        97 | 1183 | `		if( !rc ){` |
|         - | 1184 | `			/* Compare nodes */` |
|        97 | 1185 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        48 | 1186 | `		}` |
|        97 | 1187 | `		if( rc != 0 ){` |
|         - | 1188 | `			/* Nodes key/value differ */` |
|         3 | 1189 | `			return rc;` |
|         - | 1190 | `		}` |
|         - | 1191 | `		/* Point to the next entry */` |
|        95 | 1192 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        95 | 1193 | `		n--;` |
|         1 | 1194 | `	}` |
|        23 | 1195 | `	return 0; /* Hashmaps are equals */` |
|        16 | 1196 | `}` |
|         - | 1197 | `/*` |
|         - | 1198 | ` * Duplicate a hashmap node.` |
|         - | 1199 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1200 | ` */` |
|    707344 | 1201 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1202 | `	ph7_hashmap *pDest,` |
|         - | 1203 | `	ph7_hashmap_node *pEntry,` |
|         - | 1204 | `	ph7_value *pVal,` |
|         - | 1205 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1206 | `	)` |
|         5 | 1207 | `{` |
|         - | 1208 | `	ph7_value sSafeVal;` |
|         - | 1209 | `	ph7_value sKey;` |
|         - | 1210 | `	sxi32 rc;` |
|         - | 1211 |  |
|    707344 | 1212 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    707346 | 1213 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
|         - | 1214 | ``		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points`` |
|         - | 1215 | `		 * at an outside slot) or, the case PH7 missed, an element somebody took a` |
|         - | 1216 | ``		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through`` |
|         - | 1217 | `		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep` |
|         - | 1218 | ``		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */`` |
|         9 | 1219 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         9 | 1220 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1221 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1222 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1223 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1224 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1225 | `		}else{` |
|         7 | 1226 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         7 | 1227 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         3 | 1228 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1229 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1230 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1231 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1232 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1233 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1234 | `			}` |
|         - | 1235 | `		}` |
|         9 | 1236 | `		return rc;` |
|         - | 1237 | `	}` |
|    707341 | 1238 | `	sSafeVal = *pVal;` |
|         - | 1239 |  |
|    707341 | 1240 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1241 | `		/* Blob key insertion */` |
|      3987 | 1242 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      3987 | 1243 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      3987 | 1244 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      3987 | 1245 | `		PH7_MemObjRelease(&sKey);` |
|      1996 | 1246 | `	}else{` |
|         - | 1247 | `		/* Int key */` |
|    703359 | 1248 | `		if( iAction == 0 ){ /* Merge */` |
|    703113 | 1249 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    351804 | 1250 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1251 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1252 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1253 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1254 | `		}else{ /* Dup */` |
|       220 | 1255 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1256 | `		}` |
|         - | 1257 | `	}` |
|    707341 | 1258 | `	return rc;` |
|    353677 | 1259 | `}` |
|         - | 1260 | `/*` |
|         - | 1261 | ` * Merge two hashmaps.` |
|         - | 1262 | ` * Note on the merge process` |
|         - | 1263 | ` * According to the PHP language reference manual.` |
|         - | 1264 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1265 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1266 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1267 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1268 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1269 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1270 | ` *  keys starting from zero in the result array.` |
|         - | 1271 | ` */` |
|      2860 | 1272 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1273 | `{` |
|         - | 1274 | `	ph7_hashmap_node *pEntry;` |
|         - | 1275 | `	ph7_value *pVal;` |
|         - | 1276 | `	sxi32 rc;` |
|         - | 1277 | `	sxu32 n;` |
|      2865 | 1278 | `	if( pSrc == pDest ){` |
|         - | 1279 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1280 | `		 * Unlike the zend engine.` |
|         - | 1281 | `		 */` |
|       ! 0 | 1282 | `		return SXRET_OK;` |
|         - | 1283 | `	}` |
|         - | 1284 | `	/* Point to the first inserted entry in the source */` |
|      2865 | 1285 | `	pEntry = pSrc->pFirst;` |
|         - | 1286 | `	/* Perform the merge */` |
|    706033 | 1287 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1288 | `		/* Extract the node value */` |
|    703173 | 1289 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    703173 | 1290 | `		if( pVal ){` |
|         - | 1291 | `			/* Make a local copy of the value.` |
|         - | 1292 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1293 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1294 | `			 * to the old pool.` |
|         - | 1295 | `			 */` |
|    703173 | 1296 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    351589 | 1297 | `		}else{` |
|       ! 0 | 1298 | `			rc = SXRET_OK;` |
|         - | 1299 | `		}` |
|    703173 | 1300 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1301 | `			return rc;` |
|         - | 1302 | `		}` |
|         - | 1303 | `		/* Point to the next entry */` |
|    703173 | 1304 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    351589 | 1305 | `	}` |
|      2865 | 1306 | `	return SXRET_OK;` |
|      1435 | 1307 | `}` |
|         - | 1308 | `/*` |
|         - | 1309 | ` * Overwrite entries with the same key.` |
|         - | 1310 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1311 | ` *  According to the PHP language reference manual.` |
|         - | 1312 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1313 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1314 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1315 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1316 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1317 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1318 | ` *  overwriting the previous values.` |
|         - | 1319 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1320 | ` *  by whatever type is in the second array.` |
|         - | 1321 | ` */` |
|        34 | 1322 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1323 | `{` |
|         - | 1324 | `	ph7_hashmap_node *pEntry;` |
|         - | 1325 | `	ph7_value *pVal;` |
|         - | 1326 | `	sxi32 rc;` |
|         - | 1327 | `	sxu32 n;` |
|        36 | 1328 | `	if( pSrc == pDest ){` |
|         - | 1329 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1330 | `		 * Unlike the zend engine.` |
|         - | 1331 | `		 */` |
|       ! 0 | 1332 | `		return SXRET_OK;` |
|         - | 1333 | `	}` |
|         - | 1334 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1335 | `	pEntry = pSrc->pFirst;` |
|         - | 1336 | `	/* Perform the merge */` |
|        80 | 1337 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1338 | `		/* Extract the node value */` |
|        46 | 1339 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1340 | `		if( pVal ){` |
|        46 | 1341 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1342 | `		}else{` |
|       ! 0 | 1343 | `			rc = SXRET_OK;` |
|         - | 1344 | `		}` |
|        46 | 1345 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1346 | `			return rc;` |
|         - | 1347 | `		}` |
|         - | 1348 | `		/* Point to the next entry */` |
|        46 | 1349 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1350 | `	}` |
|        36 | 1351 | `	return SXRET_OK;` |
|        19 | 1352 | `}` |
|         - | 1353 | `/*` |
|         - | 1354 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1355 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1356 | ` */` |
|      3888 | 1357 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1358 | `{` |
|         - | 1359 | `	ph7_hashmap_node *pEntry;` |
|         - | 1360 | `	ph7_value *pVal;` |
|         - | 1361 | `	sxi32 rc;` |
|         - | 1362 | `	sxu32 n;` |
|      3893 | 1363 | `	if( pSrc == pDest ){` |
|         - | 1364 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1365 | `		 * Unlike the zend engine.` |
|         - | 1366 | `		 */` |
|       ! 0 | 1367 | `		return SXRET_OK;` |
|         - | 1368 | `	}` |
|         - | 1369 | `	/* Point to the first inserted entry in the source */` |
|      3893 | 1370 | `	pEntry = pSrc->pFirst;` |
|         - | 1371 | `	/* Perform the duplication */` |
|      8025 | 1372 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1373 | `		/* Extract the node value */` |
|      4137 | 1374 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4137 | 1375 | `		if( pVal ){` |
|      4137 | 1376 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2071 | 1377 | `		}else{` |
|       ! 0 | 1378 | `			rc = SXRET_OK;` |
|         - | 1379 | `		}` |
|      4137 | 1380 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1381 | `			return rc;` |
|         - | 1382 | `		}` |
|         - | 1383 | `		/* Point to the next entry */` |
|      4137 | 1384 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2071 | 1385 | `	}` |
|      3893 | 1386 | `	return SXRET_OK;` |
|      1949 | 1387 | `}` |
|         - | 1388 | `/*` |
|         - | 1389 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1390 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1391 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1392 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1393 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1394 | ` * this instead of PH7_HashmapDup.` |
|         - | 1395 | ` */` |
|        12 | 1396 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1397 | `{` |
|         - | 1398 | `	ph7_hashmap_node *pEntry;` |
|         - | 1399 | `	ph7_value *pVal;` |
|         - | 1400 | `	sxi32 rc;` |
|         - | 1401 | `	sxu32 n;` |
|        13 | 1402 | `	if( pSrc == pDest ){` |
|       ! 0 | 1403 | `		return SXRET_OK;` |
|         - | 1404 | `	}` |
|        13 | 1405 | `	pEntry = pSrc->pFirst;` |
|       749 | 1406 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1407 | `		/* Extract the node value (resolves foreign references) */` |
|       737 | 1408 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       736 | 1409 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       496 | 1410 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1411 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1412 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1413 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1414 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1415 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1416 | `			pVal = 0;` |
|         2 | 1417 | `		}` |
|       737 | 1418 | `		if( pVal ){` |
|       733 | 1419 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1093 | 1420 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       364 | 1421 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       365 | 1422 | `			}else{` |
|         5 | 1423 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1424 | `			}` |
|       733 | 1425 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1426 | `				return rc;` |
|         - | 1427 | `			}` |
|       366 | 1428 | `		}` |
|         - | 1429 | `		/* Point to the next entry */` |
|       737 | 1430 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       369 | 1431 | `	}` |
|        13 | 1432 | `	return SXRET_OK;` |
|         7 | 1433 | `}` |
|         - | 1434 | `/*` |
|         - | 1435 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1436 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1437 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1438 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1439 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1440 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1441 | ` * iterate-a-snapshot semantic.` |
|         - | 1442 | ` */` |
|        50 | 1443 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1444 | `{` |
|         - | 1445 | `	ph7_foreach_step *pStep;` |
|        53 | 1446 | `	sxi32 nRef = 0;` |
|       103 | 1447 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        53 | 1448 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1449 | `			nRef++;` |
|        21 | 1450 | `		}` |
|        28 | 1451 | `	}` |
|        53 | 1452 | `	return nRef;` |
|         3 | 1453 | `}` |
|         - | 1454 | `/*` |
|         - | 1455 | ` * Copy-on-write separation for arrays.` |
|         - | 1456 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1457 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1458 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1459 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1460 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1461 | ` * on the live map the loop is walking, like php.` |
|         - | 1462 | ` */` |
|    237684 | 1463 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1464 | `{` |
|    237689 | 1465 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1466 | `	ph7_hashmap *pNew;` |
|         - | 1467 | `	ph7_value *pBacking;` |
|         - | 1468 | `	sxu32 nValIdx;` |
|         - | 1469 | `	int bValueInPool;` |
|    237689 | 1470 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    237689 | 1471 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1472 | `		/* Sole owner, no separation needed */` |
|    234935 | 1473 | `		return pMap;` |
|         - | 1474 | `	}` |
|      2759 | 1475 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1476 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1477 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1478 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1479 | `		return pMap;` |
|         - | 1480 | `	}` |
|         - | 1481 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1482 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1483 | `	 * frame is popped. */` |
|      2633 | 1484 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2633 | 1485 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2628 | 1486 | `		if( pBacking && pBacking != pValue` |
|      2603 | 1487 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2583 | 1488 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1489 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2583 | 1490 | `			pMap->iRef--;` |
|      2583 | 1491 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1492 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2537 | 1493 | `				pMap->iRef++;` |
|      2537 | 1494 | `				return pMap;` |
|         - | 1495 | `			}` |
|        48 | 1496 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        48 | 1497 | `			if( pNew == 0 ){` |
|       ! 0 | 1498 | `				pMap->iRef++;` |
|       ! 0 | 1499 | `				return pMap;` |
|         - | 1500 | `			}` |
|        48 | 1501 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1502 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1503 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1504 | `				pMap->iRef++;` |
|       ! 0 | 1505 | `				return pMap;` |
|         - | 1506 | `			}` |
|        48 | 1507 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        48 | 1508 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1509 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1510 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1511 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1512 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1513 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1514 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1515 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        48 | 1516 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        48 | 1517 | `			if( pBacking ){` |
|        48 | 1518 | `				pBacking->x.pOther = pNew;` |
|        23 | 1519 | `			}` |
|         - | 1520 | `			/* Update the stack value to match */` |
|        48 | 1521 | `			pValue->x.pOther = pNew;` |
|        48 | 1522 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        48 | 1523 | `			return pNew;` |
|         - | 1524 | `		}` |
|        25 | 1525 | `	}` |
|         - | 1526 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1527 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1528 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1529 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1530 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1531 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1532 | `	 * pBacking in the backing-variable branch above). */` |
|        52 | 1533 | `	nValIdx = pValue->nIdx;` |
|        77 | 1534 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        50 | 1535 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        52 | 1536 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        52 | 1537 | `	if( pNew == 0 ){` |
|         - | 1538 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1539 | `		return pMap;` |
|         - | 1540 | `	}` |
|        52 | 1541 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1542 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1543 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1544 | `		return pMap;` |
|         - | 1545 | `	}` |
|        52 | 1546 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        52 | 1547 | `	pMap->iRef--;` |
|        52 | 1548 | `	if( bValueInPool ){` |
|         - | 1549 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        52 | 1550 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        52 | 1551 | `		if( pValue == 0 ){` |
|       ! 0 | 1552 | `			return pNew;` |
|         - | 1553 | `		}` |
|        25 | 1554 | `	}` |
|        52 | 1555 | `	pValue->x.pOther = pNew;` |
|        52 | 1556 | `	return pNew;` |
|    118847 | 1557 | `}` |
|         - | 1558 | `/*` |
|         - | 1559 | ` * Perform the union of two hashmaps.` |
|         - | 1560 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1561 | ` * with a variable holding an array as follows:` |
|         - | 1562 | ` * <?php` |
|         - | 1563 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1564 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1565 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1566 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1567 | ` * var_dump($c);` |
|         - | 1568 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1569 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1570 | ` * var_dump($c);` |
|         - | 1571 | ` * ?>` |
|         - | 1572 | ` * When executed, this script will print the following:` |
|         - | 1573 | ` * Union of $a and $b:` |
|         - | 1574 | ` * array(3) {` |
|         - | 1575 | ` *  ["a"]=>` |
|         - | 1576 | ` *  string(5) "apple"` |
|         - | 1577 | ` *  ["b"]=>` |
|         - | 1578 | ` * string(6) "banana"` |
|         - | 1579 | ` *  ["c"]=>` |
|         - | 1580 | ` * string(6) "cherry"` |
|         - | 1581 | ` * }` |
|         - | 1582 | ` * Union of $b and $a:` |
|         - | 1583 | ` * array(3) {` |
|         - | 1584 | ` * ["a"]=>` |
|         - | 1585 | ` * string(4) "pear"` |
|         - | 1586 | ` * ["b"]=>` |
|         - | 1587 | ` * string(10) "strawberry"` |
|         - | 1588 | ` * ["c"]=>` |
|         - | 1589 | ` * string(6) "cherry"` |
|         - | 1590 | ` * }` |
|         - | 1591 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1592 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1593 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1594 | ` */` |
|      3764 | 1595 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1596 | `{` |
|         - | 1597 | `	ph7_hashmap_node *pEntry;` |
|      3769 | 1598 | `	sxi32 rc = SXRET_OK;` |
|         - | 1599 | `	ph7_value *pObj;` |
|         - | 1600 | `	sxu32 n;` |
|      3769 | 1601 | `	if( pLeft == pRight ){` |
|         - | 1602 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1603 | `		 * Unlike the zend engine.` |
|         - | 1604 | `		 */` |
|       ! 0 | 1605 | `		return SXRET_OK;` |
|         - | 1606 | `	}` |
|         - | 1607 | `	/* Perform the union */` |
|      3769 | 1608 | `	pEntry = pRight->pFirst;` |
|      3809 | 1609 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1610 | `		/* Make sure the given key does not exists in the left array */` |
|        44 | 1611 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1612 | `			/* BLOB key */` |
|        24 | 1613 | `			if( SXRET_OK !=` |
|        20 | 1614 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1615 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1616 | `					if( pObj ){` |
|        20 | 1617 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1618 | `						/* Perform the insertion */` |
|        20 | 1619 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1620 | `							&sSafeVal,0,FALSE);` |
|        20 | 1621 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1622 | `							return rc;` |
|         - | 1623 | `						}` |
|         8 | 1624 | `					}` |
|         8 | 1625 | `			}` |
|        14 | 1626 | `		}else{` |
|         - | 1627 | `			/* INT key */` |
|        22 | 1628 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        13 | 1629 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        13 | 1630 | `				if( pObj ){` |
|        13 | 1631 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1632 | `					/* Perform the insertion */` |
|        13 | 1633 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        13 | 1634 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1635 | `						return rc;` |
|         - | 1636 | `					}` |
|         6 | 1637 | `				}` |
|         6 | 1638 | `			}` |
|         - | 1639 | `		}` |
|         - | 1640 | `		/* Point to the next entry */` |
|        44 | 1641 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1642 | `	}` |
|      3769 | 1643 | `	return SXRET_OK;` |
|      1887 | 1644 | `}` |
|         - | 1645 | `/*` |
|         - | 1646 | ` * Allocate a new hashmap.` |
|         - | 1647 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1648 | ` */` |
|    144910 | 1649 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1650 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1651 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1652 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1653 | `	)` |
|         5 | 1654 | `{` |
|         - | 1655 | `	ph7_hashmap *pMap;` |
|         - | 1656 | `	/* Allocate a new instance */` |
|    144915 | 1657 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    144915 | 1658 | `	if( pMap == 0 ){` |
|       ! 0 | 1659 | `		return 0;` |
|         - | 1660 | `	}` |
|         - | 1661 | `	/* Zero the structure */` |
|    144915 | 1662 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1663 | `	/* Fill in the structure */` |
|    144915 | 1664 | `	pMap->pVm = &(*pVm);` |
|    144915 | 1665 | `	pMap->iRef = 1;` |
|         - | 1666 | `	/* Default hash functions */` |
|    144915 | 1667 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    144915 | 1668 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    144915 | 1669 | `	return pMap;` |
|     72460 | 1670 | `}` |
|         - | 1671 | `/*` |
|         - | 1672 | ` * Install superglobals in the given virtual machine.` |
|         - | 1673 | ` * Note on superglobals.` |
|         - | 1674 | ` *  According to the PHP language reference manual.` |
|         - | 1675 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1676 | `*   Description` |
|         - | 1677 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1678 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1679 | `*   global $variable; to access them within functions or methods.` |
|         - | 1680 | `*   These superglobal variables are:` |
|         - | 1681 | `*    $GLOBALS` |
|         - | 1682 | `*    $_SERVER` |
|         - | 1683 | `*    $_GET` |
|         - | 1684 | `*    $_POST` |
|         - | 1685 | `*    $_FILES` |
|         - | 1686 | `*    $_COOKIE` |
|         - | 1687 | `*    $_SESSION` |
|         - | 1688 | `*    $_REQUEST` |
|         - | 1689 | `*    $_ENV` |
|         - | 1690 | `*/` |
|      3364 | 1691 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1692 | `{` |
|         - | 1693 | `	static const char * azSuper[] = {` |
|         - | 1694 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1695 | `		"_GET",      /* $_GET */` |
|         - | 1696 | `		"_POST",     /* $_POST */` |
|         - | 1697 | `		"_FILES",    /* $_FILES */` |
|         - | 1698 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1699 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1700 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1701 | `		"_ENV",      /* $_ENV */` |
|         - | 1702 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1703 | `		"argv"       /* $argv */` |
|         - | 1704 | `	};` |
|         - | 1705 | `	ph7_hashmap *pMap;` |
|         - | 1706 | `	ph7_value *pObj;` |
|         - | 1707 | `	SyString *pFile;` |
|         - | 1708 | `	sxi32 rc;` |
|         - | 1709 | `	sxu32 n;` |
|         - | 1710 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3369 | 1711 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3369 | 1712 | `	if( pMap == 0 ){` |
|       ! 0 | 1713 | `		return SXERR_MEM;` |
|         - | 1714 | `	}` |
|      3369 | 1715 | `	pVm->pGlobal = pMap;` |
|         - | 1716 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3369 | 1717 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3369 | 1718 | `	if( pObj == 0 ){` |
|       ! 0 | 1719 | `		return SXERR_MEM;` |
|         - | 1720 | `	}` |
|      3369 | 1721 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1722 | `	/* Record object index */` |
|      3369 | 1723 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1724 | `	/* Install the special $GLOBALS array */` |
|      3369 | 1725 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3369 | 1726 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1727 | `		return rc;` |
|         - | 1728 | `	}` |
|         - | 1729 | `	/* Install superglobals now */` |
|     37009 | 1730 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1731 | `		ph7_value *pSuper;` |
|         - | 1732 | `		/* Request an empty array */` |
|     33645 | 1733 | `		pSuper = ph7_new_array(&(*pVm));` |
|     33645 | 1734 | `		if( pSuper == 0 ){` |
|       ! 0 | 1735 | `			return SXERR_MEM;` |
|         - | 1736 | `		}` |
|         - | 1737 | `		/* Install */` |
|     33645 | 1738 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     33645 | 1739 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1740 | `			return rc;` |
|         - | 1741 | `		}` |
|         - | 1742 | `		/* Release the value now it have been installed */` |
|     33645 | 1743 | `		ph7_release_value(&(*pVm),pSuper);` |
|     16825 | 1744 | `	}` |
|         - | 1745 | `	/* Set some $_SERVER entries */` |
|      3369 | 1746 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1747 | `	/*` |
|         - | 1748 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1749 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1750 | `	 */` |
|      6733 | 1751 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1752 | `		"SCRIPT_FILENAME",` |
|      1682 | 1753 | `		pFile ? pFile->zString : ":Memory:",` |
|      3364 | 1754 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1755 | `		);` |
|         - | 1756 | `	/* All done,all super-global are installed now */` |
|      3369 | 1757 | `	return SXRET_OK;` |
|      1687 | 1758 | `}` |
|         - | 1759 | `/*` |
|         - | 1760 | ` * Release a hashmap.` |
|         - | 1761 | ` */` |
|    102982 | 1762 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1763 | `{` |
|         - | 1764 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    102987 | 1765 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1766 | `	sxu32 n;` |
|    102987 | 1767 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1768 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1769 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1770 | `		return SXRET_OK;` |
|         - | 1771 | `	}` |
|    102987 | 1772 | `	if( pMap->pActiveSteps ){` |
|         - | 1773 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1774 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1775 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1776 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1777 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1778 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1779 | `		ph7_foreach_step *pStep;` |
|        17 | 1780 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1781 | `			pStep->pCursor = 0;` |
|         5 | 1782 | `		}` |
|         4 | 1783 | `	}` |
|         - | 1784 | `	/* Start the release process */` |
|    102987 | 1785 | `	n = 0;` |
|    102987 | 1786 | `	pEntry = pMap->pFirst;` |
|   1745393 | 1787 | `	for(;;){` |
|   3490791 | 1788 | `		if( n >= pMap->nEntry ){` |
|    102987 | 1789 | `			break;` |
|         - | 1790 | `		}` |
|   3387809 | 1791 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1792 | `		/* Remove the reference from the foreign table */` |
|   3387809 | 1793 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3387809 | 1794 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1795 | `			/* Restore the ph7_value to the free list */` |
|   3387749 | 1796 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1693872 | 1797 | `		}` |
|         - | 1798 | `		/* Release the node */` |
|   3387809 | 1799 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    189473 | 1800 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     94734 | 1801 | `		}` |
|   3387809 | 1802 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1803 | `		/* Point to the next entry */` |
|   3387809 | 1804 | `		pEntry = pNext;` |
|   3387809 | 1805 | `		n++;` |
|         5 | 1806 | `	}` |
|    102987 | 1807 | `	if( pMap->nEntry > 0 ){` |
|         - | 1808 | `		/* Release the hash bucket */` |
|     75677 | 1809 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     37836 | 1810 | `	}` |
|    102987 | 1811 | `	if( FreeDS ){` |
|         - | 1812 | `		/* Free the whole instance */` |
|    102961 | 1813 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     51483 | 1814 | `	}else{` |
|         - | 1815 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1816 | `		pMap->apBucket = 0;` |
|        28 | 1817 | `		pMap->iNextIdx = 0;` |
|        28 | 1818 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1819 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1820 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1821 | `	}` |
|    102987 | 1822 | `	return SXRET_OK;` |
|     51496 | 1823 | `}` |
|         - | 1824 | `/*` |
|         - | 1825 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1826 | ` * If the count reaches zero which mean no more variables` |
|         - | 1827 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1828 | ` */` |
|    846708 | 1829 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1830 | `{` |
|    846713 | 1831 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1832 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    846713 | 1833 | `	pMap->iRef--;` |
|    846713 | 1834 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    102941 | 1835 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     51468 | 1836 | `	}` |
|    846713 | 1837 | `}` |
|         - | 1838 | `/*` |
|         - | 1839 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1840 | ` * Write a pointer to the target node on success.` |
|         - | 1841 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1842 | ` */` |
|    149122 | 1843 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1844 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1845 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1846 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1847 | `	)` |
|         5 | 1848 | `{` |
|         - | 1849 | `	sxi32 rc;` |
|    149127 | 1850 | `	if( pMap->nEntry < 1 ){` |
|         - | 1851 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1852 | `		 */` |
|       193 | 1853 | `		return SXERR_NOTFOUND;` |
|         - | 1854 | `	}` |
|    148939 | 1855 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    148939 | 1856 | `	return rc;` |
|     74566 | 1857 | `}` |
|         - | 1858 | `/*` |
|         - | 1859 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1860 | ` * hashmap.` |
|         - | 1861 | ` * If a node with the given key already exists in the database` |
|         - | 1862 | ` * then this function overwrite the old value.` |
|         - | 1863 | ` */` |
|   2722740 | 1864 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1865 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1866 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1867 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1868 | `	)` |
|         5 | 1869 | `{` |
|         - | 1870 | `	sxi32 rc;` |
|         - | 1871 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1872 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1873 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1874 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   2722745 | 1875 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2722745 | 1876 | `	return rc;` |
|         5 | 1877 | `}` |
|         - | 1878 | `/*` |
|         - | 1879 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1880 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1881 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1882 | ` * This is the same routine that backs array_merge().` |
|         - | 1883 | ` */` |
|       654 | 1884 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1885 | `{` |
|       655 | 1886 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1887 | `}` |
|         - | 1888 | `/*` |
|         - | 1889 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1890 | ` * hashmap.` |
|         - | 1891 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1892 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1893 | ` * The insertion by reference is triggered when the following` |
|         - | 1894 | ` * expression is encountered.` |
|         - | 1895 | ` * $var = 10;` |
|         - | 1896 | ` *  $a = array(&var);` |
|         - | 1897 | ` * OR` |
|         - | 1898 | ` *  $a[] =& $var;` |
|         - | 1899 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1900 | ` * over it's contents.` |
|         - | 1901 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1902 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1903 | ` * Example:` |
|         - | 1904 | ` *  $var = 10;` |
|         - | 1905 | ` *  $a[] =& $var;` |
|         - | 1906 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1907 | ` *  //Unset the foreign ph7_value now` |
|         - | 1908 | ` *  unset($var);` |
|         - | 1909 | ` *  echo count($a); //0` |
|         - | 1910 | ` * Note that this is a PH7 eXtension.` |
|         - | 1911 | ` * Refer to the official documentation for more information.` |
|         - | 1912 | ` * If a node with the given key already exists in the database` |
|         - | 1913 | ` * then this function overwrite the old value.` |
|         - | 1914 | ` */` |
|     45986 | 1915 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1916 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1917 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1918 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1919 | `	)` |
|         5 | 1920 | `{` |
|         - | 1921 | `	sxi32 rc;` |
|     45991 | 1922 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1923 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1924 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1925 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1926 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1927 | `		return PH7_ABORT;` |
|         - | 1928 | `	}` |
|     45991 | 1929 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     45991 | 1930 | `	return rc;` |
|     22998 | 1931 | `}` |
|         - | 1932 | `/*` |
|         - | 1933 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1934 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1935 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1936 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1937 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1938 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1939 | ` */` |
|     19192 | 1940 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1941 | `{` |
|     19197 | 1942 | `	pStep->pCursor = pMap->pFirst;` |
|     19197 | 1943 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     19197 | 1944 | `	pMap->pActiveSteps = pStep;` |
|     19197 | 1945 | `}` |
|         - | 1946 | `/*` |
|         - | 1947 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1948 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1949 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1950 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1951 | ` */` |
|     19090 | 1952 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1953 | `{` |
|     19095 | 1954 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     19095 | 1955 | `	while( *ppLink ){` |
|     19095 | 1956 | `		if( *ppLink == pStep ){` |
|     19095 | 1957 | `			*ppLink = pStep->pNextActive;` |
|     19095 | 1958 | `			pStep->pNextActive = 0;` |
|     19095 | 1959 | `			return;` |
|         - | 1960 | `		}` |
|       ! 0 | 1961 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1962 | `	}` |
|      9550 | 1963 | `}` |
|         - | 1964 | `/*` |
|         - | 1965 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1966 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1967 | ` * return NULL.` |
|         - | 1968 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1969 | ` */` |
|        64 | 1970 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 1971 | `{` |
|        65 | 1972 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 1973 | `	if( pCur == 0 ){` |
|         - | 1974 | `		/* End of the list,return null */` |
|        27 | 1975 | `		return 0;` |
|         - | 1976 | `	}` |
|         - | 1977 | `	/* Advance the node cursor */` |
|        39 | 1978 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 1979 | `	return pCur;` |
|        33 | 1980 | `}` |
|         - | 1981 | `/*` |
|         - | 1982 | ` * Extract a node value.` |
|         - | 1983 | ` */` |
|    600140 | 1984 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1985 | `{` |
|    600145 | 1986 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    600145 | 1987 | `	if( pEntry ){` |
|    600145 | 1988 | `		if( bStore ){` |
|    238763 | 1989 | `			PH7_MemObjStore(pEntry,pValue);` |
|    119384 | 1990 | `		}else{` |
|    361387 | 1991 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1992 | `		}` |
|    300023 | 1993 | `	}else{` |
|       ! 0 | 1994 | `		PH7_MemObjRelease(pValue);` |
|         - | 1995 | `	}` |
|    600145 | 1996 | `}` |
|         - | 1997 | `/*` |
|         - | 1998 | ` * Extract a node key.` |
|         - | 1999 | ` */` |
|    159776 | 2000 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2001 | `{` |
|         - | 2002 | `	/* Fill with the current key */` |
|    159781 | 2003 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    154481 | 2004 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2005 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2006 | `		}` |
|    154481 | 2007 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    154481 | 2008 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     77243 | 2009 | `	}else{` |
|      5305 | 2010 | `		SyBlobReset(&pKey->sBlob);` |
|      5305 | 2011 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5305 | 2012 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2013 | `	}` |
|    159781 | 2014 | `}` |
|         - | 2015 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2016 | `/*` |
|         - | 2017 | ` * Store the address of nodes value in the given container.` |
|         - | 2018 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2019 | ` * defined in 'builtin.c' for more information.` |
|         - | 2020 | ` */` |
|        14 | 2021 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 2022 | `{` |
|        15 | 2023 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2024 | `	ph7_value *pValue;` |
|         - | 2025 | `	sxu32 n;` |
|         - | 2026 | `	/* Initialize the container */` |
|        15 | 2027 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        41 | 2028 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2029 | `		/* Extract node value */` |
|        27 | 2030 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        27 | 2031 | `		if( pValue ){` |
|        27 | 2032 | `			SySetPut(pOut,(const void *)&pValue);` |
|        13 | 2033 | `		}` |
|         - | 2034 | `		/* Point to the next entry */` |
|        27 | 2035 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        14 | 2036 | `	}` |
|         - | 2037 | `	/* Total inserted entries */` |
|        15 | 2038 | `	return (int)SySetUsed(pOut);` |
|         1 | 2039 | `}` |
|         - | 2040 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2041 | `/* SPDX-SnippetBegin */` |
|         - | 2042 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 2043 | `/* SPDX-License-Identifier: blessing */` |
|         - | 2044 | `/*` |
|         - | 2045 | ` * Merge sort.` |
|         - | 2046 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 2047 | ` * Status: Public domain` |
|         - | 2048 | ` */` |
|         - | 2049 | `/* Node comparison callback signature */` |
|         - | 2050 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 2051 | `/*` |
|         - | 2052 | `** Inputs:` |
|         - | 2053 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2054 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2055 | `**   cmp:     A pointer to the comparison function.` |
|         - | 2056 | `**` |
|         - | 2057 | `** Return Value:` |
|         - | 2058 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 2059 | `**   of both a and b.` |
|         - | 2060 | `**` |
|         - | 2061 | `** Side effects:` |
|         - | 2062 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 2063 | `**   changed.` |
|         - | 2064 | `*/` |
|     36962 | 2065 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2066 | `{` |
|         - | 2067 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2068 | `    /* Prevent compiler warning */` |
|     36967 | 2069 | `	result.pNext = result.pPrev = 0;` |
|     36967 | 2070 | `	pTail = &result;` |
|    109739 | 2071 | `	while( pA && pB ){` |
|     72777 | 2072 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47966 | 2073 | `			pTail->pPrev = pA;` |
|     47966 | 2074 | `			pA->pNext = pTail;` |
|     47966 | 2075 | `			pTail = pA;` |
|     47966 | 2076 | `			pA = pA->pPrev;` |
|     23939 | 2077 | `		}else{` |
|     24816 | 2078 | `			pTail->pPrev = pB;` |
|     24816 | 2079 | `			pB->pNext = pTail;` |
|     24816 | 2080 | `			pTail = pB;` |
|     24816 | 2081 | `			pB = pB->pPrev;` |
|         - | 2082 | `		}` |
|         5 | 2083 | `	}` |
|     36967 | 2084 | `	if( pA ){` |
|     26322 | 2085 | `		pTail->pPrev = pA;` |
|     26322 | 2086 | `		pA->pNext = pTail;` |
|     23844 | 2087 | `	}else if( pB ){` |
|     10422 | 2088 | `		pTail->pPrev = pB;` |
|     10422 | 2089 | `		pB->pNext = pTail;` |
|      5178 | 2090 | `	}else{` |
|       233 | 2091 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2092 | `	}` |
|     36967 | 2093 | `	return result.pPrev;` |
|         5 | 2094 | `}` |
|         - | 2095 | `/*` |
|         - | 2096 | `** Inputs:` |
|         - | 2097 | `**   Map:       Input hashmap` |
|         - | 2098 | `**   cmp:       A comparison function.` |
|         - | 2099 | `**` |
|         - | 2100 | `** Return Value:` |
|         - | 2101 | `**   Sorted hashmap.` |
|         - | 2102 | `**` |
|         - | 2103 | `** Side effects:` |
|         - | 2104 | `**   The "next" pointers for elements in list are changed.` |
|         - | 2105 | `*/` |
|         - | 2106 | `#define N_SORT_BUCKET  32` |
|       778 | 2107 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2108 | `{` |
|         - | 2109 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2110 | `	sxu32 i;` |
|       783 | 2111 | `	SyZero(a,sizeof(a));` |
|         - | 2112 | `	/* Point to the first inserted entry */` |
|       783 | 2113 | `	pIn = pMap->pFirst;` |
|     14997 | 2114 | `	while( pIn ){` |
|     14219 | 2115 | `		p = pIn;` |
|     14219 | 2116 | `		pIn = p->pPrev;` |
|     14219 | 2117 | `		p->pPrev = 0;` |
|     27063 | 2118 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     27063 | 2119 | `			if( a[i]==0 ){` |
|     14219 | 2120 | `				a[i] = p;` |
|     14219 | 2121 | `				break;` |
|       ! 0 | 2122 | `			}else{` |
|     12849 | 2123 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12849 | 2124 | `				a[i] = 0;` |
|         - | 2125 | `			}` |
|      6427 | 2126 | `		}` |
|     14219 | 2127 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2128 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2129 | `			 * But that is impossible.` |
|         - | 2130 | `			 */` |
|       ! 0 | 2131 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2132 | `		}` |
|         5 | 2133 | `	}` |
|       783 | 2134 | `	p = a[0];` |
|     24901 | 2135 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     24123 | 2136 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     12064 | 2137 | `	}` |
|       783 | 2138 | `	p->pNext = 0;` |
|         - | 2139 | `	/* Reflect the change */` |
|       783 | 2140 | `	pMap->pFirst = p;` |
|         - | 2141 | `	/* Reset the loop cursor */` |
|       783 | 2142 | `	pMap->pCur = pMap->pFirst;` |
|       783 | 2143 | `	return SXRET_OK;` |
|         5 | 2144 | `}` |
|         - | 2145 | `/* SPDX-SnippetEnd */` |
|         - | 2146 | `/*` |
|         - | 2147 | ` * Node comparison callback.` |
|         - | 2148 | ` * used-by: [sort(),asort(),...]` |
|         - | 2149 | ` */` |
|         - | 2150 | `/*` |
|         - | 2151 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|         - | 2152 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|         - | 2153 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|         - | 2154 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|         - | 2155 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|         - | 2156 | ` * string cast); the caller owns and releases them.` |
|         - | 2157 | ` */` |
|       250 | 2158 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|         1 | 2159 | `{` |
|         - | 2160 | `	sxi32 rc;` |
|       251 | 2161 | `	if( base == 1 ){` |
|         - | 2162 | `		/* SORT_NUMERIC */` |
|        59 | 2163 | `		PH7_MemObjToNumeric(pA);` |
|        59 | 2164 | `		PH7_MemObjToNumeric(pB);` |
|        59 | 2165 | `		rc = PH7_MemObjCmp(pA,pB,FALSE,0);` |
|        30 | 2166 | `	}else{` |
|         - | 2167 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|         - | 2168 | `		const char *zA,*zB;` |
|         - | 2169 | `		sxu32 nA,nB,nMin,i;` |
|       193 | 2170 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pA); }` |
|       193 | 2171 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pB); }` |
|       193 | 2172 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
|       193 | 2173 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
|       193 | 2174 | `		nA = SyBlobLength(&pA->sBlob);` |
|       193 | 2175 | `		nB = SyBlobLength(&pB->sBlob);` |
|       193 | 2176 | `		if( base == 6 ){` |
|        29 | 2177 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|        15 | 2178 | `		}else{` |
|         - | 2179 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|       165 | 2180 | `			nMin = nA < nB ? nA : nB;` |
|       165 | 2181 | `			rc = 0;` |
|       241 | 2182 | `			for( i = 0 ; i < nMin ; ++i ){` |
|       187 | 2183 | `				int ca = (unsigned char)zA[i];` |
|       187 | 2184 | `				int cb = (unsigned char)zB[i];` |
|       187 | 2185 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|       187 | 2186 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|        39 | 2187 | `			}` |
|       165 | 2188 | `			if( rc == 0 ){` |
|        55 | 2189 | `				if( nA < nB ) rc = -1;` |
|        43 | 2190 | `				else if( nA > nB ) rc = 1;` |
|        27 | 2191 | `			}` |
|         - | 2192 | `		}` |
|         - | 2193 | `	}` |
|       251 | 2194 | `	return rc;` |
|         1 | 2195 | `}` |
|         - | 2196 | `/*` |
|         - | 2197 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|         - | 2198 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|         - | 2199 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|         - | 2200 | ` */` |
|       116 | 2201 | `static int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|         1 | 2202 | `{` |
|         - | 2203 | `	ph7_value sA,sB;` |
|         - | 2204 | `	sxi32 rc;` |
|       117 | 2205 | `	PH7_MemObjInit(pVm,&sA);` |
|       117 | 2206 | `	PH7_MemObjInit(pVm,&sB);` |
|       117 | 2207 | `	PH7_MemObjStore(pA,&sA);` |
|       117 | 2208 | `	PH7_MemObjStore(pB,&sB);` |
|       117 | 2209 | `	if( base == 0 ){` |
|        11 | 2210 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|         6 | 2211 | `	}else{` |
|       107 | 2212 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|         - | 2213 | `	}` |
|       117 | 2214 | `	PH7_MemObjRelease(&sA);` |
|       117 | 2215 | `	PH7_MemObjRelease(&sB);` |
|       117 | 2216 | `	return rc == 0;` |
|         1 | 2217 | `}` |
|         - | 2218 | `/*` |
|         - | 2219 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|         - | 2220 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|         - | 2221 | ` */` |
|       126 | 2222 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2223 | `{` |
|         - | 2224 | `	ph7_value sA,sB;` |
|       127 | 2225 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|       127 | 2226 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2227 | `	sxi32 rc;` |
|       127 | 2228 | `	if( base == 0 ){` |
|         - | 2229 | `		/* SORT_REGULAR */` |
|       ! 0 | 2230 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2231 | `	}` |
|       127 | 2232 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       127 | 2233 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       127 | 2234 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       127 | 2235 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       127 | 2236 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|       127 | 2237 | `	PH7_MemObjRelease(&sA);` |
|       127 | 2238 | `	PH7_MemObjRelease(&sB);` |
|       127 | 2239 | `	return rc;` |
|        64 | 2240 | `}` |
|     72458 | 2241 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2242 | `{` |
|     72463 | 2243 | `	if( pCmpData == 0 ){` |
|         - | 2244 | `		/* SORT_REGULAR fast path */` |
|     72375 | 2245 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2246 | `	}` |
|        89 | 2247 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     36208 | 2248 | `}` |
|         - | 2249 | `/*` |
|         - | 2250 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2251 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2252 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2253 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2254 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2255 | ` */` |
|         - | 2256 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2257 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2258 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        36 | 2259 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2260 | `{` |
|        38 | 2261 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        38 | 2262 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        38 | 2263 | `	if( rc == 0 ){` |
|       ! 0 | 2264 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2265 | `	}` |
|        38 | 2266 | `	return rc;` |
|         2 | 2267 | `}` |
|        58 | 2268 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2269 | `{` |
|         - | 2270 | `	sxi32 rc;` |
|        60 | 2271 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2272 | `		/* Perform a string comparison */` |
|        32 | 2273 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        20 | 2274 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        12 | 2275 | `	}else{` |
|         - | 2276 | `		SyString sStr;` |
|        39 | 2277 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2278 | `		int bNum = 1;` |
|        39 | 2279 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2280 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2281 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2282 | `				bNum = 0;` |
|         6 | 2283 | `			}else{` |
|       ! 0 | 2284 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2285 | `			}` |
|         6 | 2286 | `		}else{` |
|        29 | 2287 | `			iA = pA->xKey.iKey;` |
|         - | 2288 | `		}` |
|        39 | 2289 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2290 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2291 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2292 | `				bNum = 0;` |
|         4 | 2293 | `			}else{` |
|       ! 0 | 2294 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2295 | `			}` |
|         4 | 2296 | `		}else{` |
|        33 | 2297 | `			iB = pB->xKey.iKey;` |
|         - | 2298 | `		}` |
|        39 | 2299 | `		if( bNum ){` |
|        23 | 2300 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2301 | `		}else{` |
|         - | 2302 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2303 | `			char zNumA[24],zNumB[24];` |
|         - | 2304 | `			SyString sA,sB;` |
|        17 | 2305 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2306 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2307 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2308 | `			}else{` |
|        11 | 2309 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2310 | `			}` |
|        17 | 2311 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2312 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2313 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2314 | `			}else{` |
|         7 | 2315 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2316 | `			}` |
|        17 | 2317 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2318 | `		}` |
|         - | 2319 | `	}` |
|        60 | 2320 | `	return rc;` |
|         2 | 2321 | `}` |
|         - | 2322 | `/*` |
|         - | 2323 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|         - | 2324 | ` * -> string) for a flag-aware key comparison.` |
|         - | 2325 | ` */` |
|        36 | 2326 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|         1 | 2327 | `{` |
|        37 | 2328 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|        21 | 2329 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|        11 | 2330 | `	}else{` |
|        17 | 2331 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|        25 | 2332 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|         8 | 2333 | `			SyBlobLength(&pNode->xKey.sKey));` |
|         - | 2334 | `	}` |
|        37 | 2335 | `}` |
|         - | 2336 | `/*` |
|         - | 2337 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|         - | 2338 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|         - | 2339 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|         - | 2340 | ` */` |
|        18 | 2341 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2342 | `{` |
|         - | 2343 | `	ph7_value sA,sB;` |
|        19 | 2344 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|        19 | 2345 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2346 | `	sxi32 rc;` |
|        19 | 2347 | `	if( base == 0 ){` |
|       ! 0 | 2348 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2349 | `	}` |
|        19 | 2350 | `	HashmapNodeKeyToValue(pA,&sA);` |
|        19 | 2351 | `	HashmapNodeKeyToValue(pB,&sB);` |
|        19 | 2352 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|        19 | 2353 | `	PH7_MemObjRelease(&sA);` |
|        19 | 2354 | `	PH7_MemObjRelease(&sB);` |
|        19 | 2355 | `	return rc;` |
|        10 | 2356 | `}` |
|         - | 2357 | `/*` |
|         - | 2358 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2359 | ` * used-by: [ksort()]` |
|         - | 2360 | ` */` |
|        58 | 2361 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2362 | `{` |
|        60 | 2363 | `	if( pCmpData == 0 ){` |
|        46 | 2364 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2365 | `	}` |
|        15 | 2366 | `	return HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        31 | 2367 | `}` |
|         - | 2368 | `/*` |
|         - | 2369 | ` * Node comparison callback.` |
|         - | 2370 | ` * Used by: [rsort(),arsort()];` |
|         - | 2371 | ` */` |
|        96 | 2372 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2373 | `{` |
|        97 | 2374 | `	if( pCmpData == 0 ){` |
|         - | 2375 | `		/* SORT_REGULAR fast path, reversed */` |
|        59 | 2376 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2377 | `	}` |
|        39 | 2378 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        49 | 2379 | `}` |
|         - | 2380 | `/*` |
|         - | 2381 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2382 | ` * used-by: [usort(),uasort()]` |
|         - | 2383 | ` */` |
|       116 | 2384 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2385 | `{` |
|         - | 2386 | `	ph7_value sResult,*pCallback;` |
|         - | 2387 | `	ph7_value *pV1,*pV2;` |
|         - | 2388 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2389 | `	sxi32 rc;` |
|         - | 2390 | `	/* Point to the desired callback */` |
|       119 | 2391 | `	pCallback = (ph7_value *)pCmpData;` |
|       119 | 2392 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2393 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2394 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2395 | `		return 0;` |
|         - | 2396 | `	}` |
|         - | 2397 | `	/* initialize the result value */` |
|       113 | 2398 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2399 | `	/* Extract nodes values */` |
|       113 | 2400 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       113 | 2401 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       113 | 2402 | `	apArg[0] = pV1;` |
|       113 | 2403 | `	apArg[1] = pV2;` |
|         - | 2404 | `	/* Invoke the callback */` |
|       113 | 2405 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       113 | 2406 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2407 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2408 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2409 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2410 | `		rc = 0;` |
|       108 | 2411 | `	}else if( rc != SXRET_OK ){` |
|         - | 2412 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2413 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2414 | `	}else{` |
|         - | 2415 | `		/* Extract callback result */` |
|       104 | 2416 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2417 | `			/* Perform an int cast */` |
|       ! 0 | 2418 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2419 | `		}` |
|       104 | 2420 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2421 | `	}` |
|       113 | 2422 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2423 | `	/* Callback result */` |
|       113 | 2424 | `	return rc;` |
|        61 | 2425 | `}` |
|         - | 2426 | `/*` |
|         - | 2427 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2428 | ` * used-by: [krsort()]` |
|         - | 2429 | ` */` |
|        18 | 2430 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2431 | `{` |
|        19 | 2432 | `	if( pCmpData == 0 ){` |
|        15 | 2433 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         - | 2434 | `	}` |
|         5 | 2435 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        10 | 2436 | `}` |
|         - | 2437 | `/*` |
|         - | 2438 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2439 | ` * used-by: [uksort()]` |
|         - | 2440 | ` */` |
|         6 | 2441 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2442 | `{` |
|         - | 2443 | `	ph7_value sResult,*pCallback;` |
|         - | 2444 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2445 | `	ph7_value sK1,sK2;` |
|         - | 2446 | `	sxi32 rc;` |
|         - | 2447 | `	/* Point to the desired callback */` |
|         7 | 2448 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2449 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2450 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2451 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2452 | `		return 0;` |
|         - | 2453 | `	}` |
|         - | 2454 | `	/* initialize the result value */` |
|         7 | 2455 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2456 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2457 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2458 | `	/* Extract nodes keys */` |
|         7 | 2459 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2460 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2461 | `	apArg[0] = &sK1;` |
|         7 | 2462 | `	apArg[1] = &sK2;` |
|         - | 2463 | `	/* Mark keys as constants */` |
|         7 | 2464 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2465 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2466 | `	/* Invoke the callback */` |
|         7 | 2467 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2468 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2469 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2470 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2471 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2472 | `		rc = 0;` |
|         7 | 2473 | `	}else if( rc != SXRET_OK ){` |
|         - | 2474 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2475 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2476 | `	}else{` |
|         - | 2477 | `		/* Extract callback result */` |
|         7 | 2478 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2479 | `			/* Perform an int cast */` |
|       ! 0 | 2480 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2481 | `		}` |
|         7 | 2482 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2483 | `	}` |
|         7 | 2484 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2485 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2486 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2487 | `	/* Callback result */` |
|         7 | 2488 | `	return rc;` |
|         4 | 2489 | `}` |
|         - | 2490 | `/*` |
|         - | 2491 | ` * Node comparison callback: Random node comparison.` |
|         - | 2492 | ` * used-by: [shuffle()]` |
|         - | 2493 | ` */` |
|        20 | 2494 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2495 | `{` |
|         - | 2496 | `	sxu32 n;` |
|         9 | 2497 | `	SXUNUSED(pB); /* cc warning */` |
|         9 | 2498 | `	SXUNUSED(pCmpData);` |
|         - | 2499 | `	/* Grab a random number */` |
|        21 | 2500 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2501 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2502 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2503 | `	 */` |
|        21 | 2504 | `	return n&1 ? 1 : -1;` |
|         1 | 2505 | `}` |
|         - | 2506 | `/*` |
|         - | 2507 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2508 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2509 | ` */` |
|       698 | 2510 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2511 | `{` |
|         - | 2512 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2513 | `	sxu32 i;` |
|         - | 2514 | `	/* Rehash all entries */` |
|       703 | 2515 | `	pLast = p = pMap->pFirst;` |
|       703 | 2516 | `	pMap->iNextIdx = 0;` |
|       703 | 2517 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|       703 | 2518 | `	i = 0;` |
|      7322 | 2519 | `	for( ;; ){` |
|     14649 | 2520 | `		if( i >= pMap->nEntry ){` |
|       703 | 2521 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       703 | 2522 | `			break;` |
|         - | 2523 | `		}` |
|     13951 | 2524 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2525 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2526 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2527 | `			/* Change key type */` |
|         5 | 2528 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2529 | `		}` |
|     13951 | 2530 | `		HashmapRehashIntNode(p);` |
|         - | 2531 | `		/* Point to the next entry */` |
|     13951 | 2532 | `		i++;` |
|     13951 | 2533 | `		pLast = p;` |
|     13951 | 2534 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2535 | `	}` |
|       703 | 2536 | `}` |
|         - | 2537 | `/*` |
|         - | 2538 | ` * Array functions implementation.` |
|         - | 2539 | ` * Status:` |
|         - | 2540 | ` *  Stable.` |
|         - | 2541 | ` */` |
|         - | 2542 | `/*` |
|         - | 2543 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2544 | ` * Sort an array.` |
|         - | 2545 | ` * Parameters` |
|         - | 2546 | ` *  $array` |
|         - | 2547 | ` *   The input array.` |
|         - | 2548 | ` * $sort_flags` |
|         - | 2549 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2550 | ` *  Sorting type flags:` |
|         - | 2551 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2552 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2553 | ` *   SORT_STRING - compare items as strings` |
|         - | 2554 | ` * Return` |
|         - | 2555 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2556 | ` *` |
|         - | 2557 | ` */` |
|      1068 | 2558 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2559 | `{` |
|         - | 2560 | `	ph7_hashmap *pMap;` |
|         - | 2561 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1073 | 2562 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2563 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2564 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2565 | `		return PH7_OK;` |
|         - | 2566 | `	}` |
|         - | 2567 | `	/* Point to the internal representation of the input hashmap */` |
|      1073 | 2568 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1073 | 2569 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1073 | 2570 | `	if( pMap->nEntry > 1 ){` |
|       673 | 2571 | `		sxi32 iCmpFlags = 0;` |
|       673 | 2572 | `		if( nArg > 1 ){` |
|         - | 2573 | `			/* Extract comparison flags */` |
|        15 | 2574 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         7 | 2575 | `		}` |
|         - | 2576 | `		/* Do the merge sort */` |
|       673 | 2577 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2578 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       673 | 2579 | `		HashmapSortRehash(pMap);` |
|       334 | 2580 | `	}` |
|         - | 2581 | `	/* All done,return TRUE */` |
|      1073 | 2582 | `	ph7_result_bool(pCtx,1);` |
|      1073 | 2583 | `	return PH7_OK;` |
|       539 | 2584 | `}` |
|         - | 2585 | `/*` |
|         - | 2586 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2587 | ` *  Sort an array and maintain index association.` |
|         - | 2588 | ` * Parameters` |
|         - | 2589 | ` *  $array` |
|         - | 2590 | ` *   The input array.` |
|         - | 2591 | ` * $sort_flags` |
|         - | 2592 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2593 | ` *  Sorting type flags:` |
|         - | 2594 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2595 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2596 | ` *   SORT_STRING - compare items as strings` |
|         - | 2597 | ` * Return` |
|         - | 2598 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2599 | ` */` |
|        34 | 2600 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2601 | `{` |
|         - | 2602 | `	ph7_hashmap *pMap;` |
|         - | 2603 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        39 | 2604 | `	if( nArg < 1 ){` |
|       ! 0 | 2605 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2606 | `			"ArgumentCountError",` |
|         - | 2607 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2608 | `			);` |
|         - | 2609 | `	}` |
|         - | 2610 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        39 | 2611 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2612 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2613 | `			"TypeError",` |
|         - | 2614 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2615 | `			ph7_type_name(apArg[0])` |
|         - | 2616 | `			);` |
|         - | 2617 | `	}` |
|         - | 2618 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 2619 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        27 | 2620 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        27 | 2621 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2622 | `		sxi32 iCmpFlags = 0;` |
|        23 | 2623 | `		if( nArg > 1 ){` |
|         - | 2624 | `			/* Extract comparison flags */` |
|         7 | 2625 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2626 | `		}` |
|         - | 2627 | `		/* Do the merge sort */` |
|        23 | 2628 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2629 | `		/* Fix the last link broken by the merge */` |
|        55 | 2630 | `		while(pMap->pLast->pPrev){` |
|        33 | 2631 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2632 | `		}` |
|        11 | 2633 | `	}` |
|         - | 2634 | `	/* All done,return TRUE */` |
|        27 | 2635 | `	ph7_result_bool(pCtx,1);` |
|        27 | 2636 | `	return PH7_OK;` |
|        22 | 2637 | `}` |
|         - | 2638 | `/*` |
|         - | 2639 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2640 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2641 | ` * Parameters` |
|         - | 2642 | ` *  $array` |
|         - | 2643 | ` *   The input array.` |
|         - | 2644 | ` * $sort_flags` |
|         - | 2645 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2646 | ` *  Sorting type flags:` |
|         - | 2647 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2648 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2649 | ` *   SORT_STRING - compare items as strings` |
|         - | 2650 | ` * Return` |
|         - | 2651 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2652 | ` */` |
|        32 | 2653 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2654 | `{` |
|         - | 2655 | `	ph7_hashmap *pMap;` |
|         - | 2656 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2657 | `	if( nArg < 1 ){` |
|       ! 0 | 2658 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2659 | `			"ArgumentCountError",` |
|         - | 2660 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2661 | `			);` |
|         - | 2662 | `	}` |
|         - | 2663 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2664 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2665 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2666 | `			"TypeError",` |
|         - | 2667 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2668 | `			ph7_type_name(apArg[0])` |
|         - | 2669 | `			);` |
|         - | 2670 | `	}` |
|         - | 2671 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2672 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2673 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2674 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2675 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2676 | `		if( nArg > 1 ){` |
|         - | 2677 | `			/* Extract comparison flags */` |
|         7 | 2678 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2679 | `		}` |
|         - | 2680 | `		/* Do the merge sort */` |
|        21 | 2681 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2682 | `		/* Fix the last link broken by the merge */` |
|        37 | 2683 | `		while(pMap->pLast->pPrev){` |
|        17 | 2684 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2685 | `		}` |
|        10 | 2686 | `	}` |
|         - | 2687 | `	/* All done,return TRUE */` |
|        25 | 2688 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2689 | `	return PH7_OK;` |
|        21 | 2690 | `}` |
|         - | 2691 | `/*` |
|         - | 2692 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2693 | ` *  Sort an array by key.` |
|         - | 2694 | ` * Parameters` |
|         - | 2695 | ` *  $array` |
|         - | 2696 | ` *   The input array.` |
|         - | 2697 | ` * $sort_flags` |
|         - | 2698 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2699 | ` *  Sorting type flags:` |
|         - | 2700 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2701 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2702 | ` *   SORT_STRING - compare items as strings` |
|         - | 2703 | ` * Return` |
|         - | 2704 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2705 | ` */` |
|        18 | 2706 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2707 | `{` |
|         - | 2708 | `	ph7_hashmap *pMap;` |
|         - | 2709 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 2710 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2711 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2712 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2713 | `		return PH7_OK;` |
|         - | 2714 | `	}` |
|         - | 2715 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 2716 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        20 | 2717 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 2718 | `	if( pMap->nEntry > 1 ){` |
|        20 | 2719 | `		sxi32 iCmpFlags = 0;` |
|        20 | 2720 | `		if( nArg > 1 ){` |
|         - | 2721 | `			/* Extract comparison flags */` |
|         5 | 2722 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2723 | `		}` |
|         - | 2724 | `		/* Do the merge sort */` |
|        20 | 2725 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2726 | `		/* Fix the last link broken by the merge */` |
|        52 | 2727 | `		while(pMap->pLast->pPrev){` |
|        33 | 2728 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2729 | `		}` |
|         9 | 2730 | `	}` |
|         - | 2731 | `	/* All done,return TRUE */` |
|        20 | 2732 | `	ph7_result_bool(pCtx,1);` |
|        20 | 2733 | `	return PH7_OK;` |
|        11 | 2734 | `}` |
|         - | 2735 | `/*` |
|         - | 2736 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2737 | ` *  Sort an array by key in reverse order.` |
|         - | 2738 | ` * Parameters` |
|         - | 2739 | ` *  $array` |
|         - | 2740 | ` *   The input array.` |
|         - | 2741 | ` * $sort_flags` |
|         - | 2742 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2743 | ` *  Sorting type flags:` |
|         - | 2744 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2745 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2746 | ` *   SORT_STRING - compare items as strings` |
|         - | 2747 | ` * Return` |
|         - | 2748 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2749 | ` */` |
|         6 | 2750 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2751 | `{` |
|         - | 2752 | `	ph7_hashmap *pMap;` |
|         - | 2753 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2754 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2755 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2756 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2757 | `		return PH7_OK;` |
|         - | 2758 | `	}` |
|         - | 2759 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2760 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2761 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2762 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2763 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2764 | `		if( nArg > 1 ){` |
|         - | 2765 | `			/* Extract comparison flags */` |
|         3 | 2766 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         1 | 2767 | `		}` |
|         - | 2768 | `		/* Do the merge sort */` |
|         7 | 2769 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2770 | `		/* Fix the last link broken by the merge */` |
|        23 | 2771 | `		while(pMap->pLast->pPrev){` |
|        17 | 2772 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2773 | `		}` |
|         3 | 2774 | `	}` |
|         - | 2775 | `	/* All done,return TRUE */` |
|         7 | 2776 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2777 | `	return PH7_OK;` |
|         4 | 2778 | `}` |
|         - | 2779 | `/*` |
|         - | 2780 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2781 | ` * Sort an array in reverse order.` |
|         - | 2782 | ` * Parameters` |
|         - | 2783 | ` *  $array` |
|         - | 2784 | ` *   The input array.` |
|         - | 2785 | ` * $sort_flags` |
|         - | 2786 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2787 | ` *  Sorting type flags:` |
|         - | 2788 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2789 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2790 | ` *   SORT_STRING - compare items as strings` |
|         - | 2791 | ` * Return` |
|         - | 2792 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2793 | ` */` |
|         6 | 2794 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2795 | `{` |
|         - | 2796 | `	ph7_hashmap *pMap;` |
|         - | 2797 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2798 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2799 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2800 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2801 | `		return PH7_OK;` |
|         - | 2802 | `	}` |
|         - | 2803 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2804 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2805 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2806 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2807 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2808 | `		if( nArg > 1 ){` |
|         - | 2809 | `			/* Extract comparison flags */` |
|         5 | 2810 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2811 | `		}` |
|         - | 2812 | `		/* Do the merge sort */` |
|         7 | 2813 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2814 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         7 | 2815 | `		HashmapSortRehash(pMap);` |
|         3 | 2816 | `	}` |
|         - | 2817 | `	/* All done,return TRUE */` |
|         7 | 2818 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2819 | `	return PH7_OK;` |
|         4 | 2820 | `}` |
|         - | 2821 | `/*` |
|         - | 2822 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2823 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2824 | ` * Parameters` |
|         - | 2825 | ` *  $array` |
|         - | 2826 | ` *   The input array.` |
|         - | 2827 | ` * $cmp_function` |
|         - | 2828 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2829 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2830 | ` *  to, or greater than the second.` |
|         - | 2831 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2832 | ` * Return` |
|         - | 2833 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2834 | ` */` |
|        22 | 2835 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2836 | `{` |
|         - | 2837 | `	ph7_hashmap *pMap;` |
|         - | 2838 | `	/* Make sure we are dealing with a valid hashmap */` |
|        25 | 2839 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2840 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2841 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2842 | `		return PH7_OK;` |
|         - | 2843 | `	}` |
|        25 | 2844 | `	if( nArg > 1 ){` |
|         - | 2845 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2846 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2847 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        25 | 2848 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        25 | 2849 | `		if( rcCb != PH7_OK ){` |
|         3 | 2850 | `			return rcCb;` |
|         - | 2851 | `		}` |
|        10 | 2852 | `	}` |
|         - | 2853 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2854 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2855 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2856 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2857 | `		ph7_value *pCallback = 0;` |
|         - | 2858 | `		ProcNodeCmp xCmp;` |
|        23 | 2859 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        23 | 2860 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2861 | `			/* Point to the desired callback */` |
|        23 | 2862 | `			pCallback = apArg[1];` |
|        13 | 2863 | `		}else{` |
|         - | 2864 | `			/* Use the default comparison function */` |
|       ! 0 | 2865 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2866 | `		}` |
|         - | 2867 | `		/* Do the merge sort */` |
|        23 | 2868 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        23 | 2869 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2870 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        23 | 2871 | `		HashmapSortRehash(pMap);` |
|        23 | 2872 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2873 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2874 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2875 | `			return PH7_EXCEPTION;` |
|         - | 2876 | `		}` |
|         6 | 2877 | `	}` |
|         - | 2878 | `	/* All done,return TRUE */` |
|        14 | 2879 | `	ph7_result_bool(pCtx,1);` |
|        14 | 2880 | `	return PH7_OK;` |
|        14 | 2881 | `}` |
|         - | 2882 | `/*` |
|         - | 2883 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2884 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2885 | ` *  and maintain index association.` |
|         - | 2886 | ` * Parameters` |
|         - | 2887 | ` *  $array` |
|         - | 2888 | ` *   The input array.` |
|         - | 2889 | ` * $cmp_function` |
|         - | 2890 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2891 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2892 | ` *  to, or greater than the second.` |
|         - | 2893 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2894 | ` * Return` |
|         - | 2895 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2896 | ` */` |
|        12 | 2897 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2898 | `{` |
|         - | 2899 | `	ph7_hashmap *pMap;` |
|         - | 2900 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 2901 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2902 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2903 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2904 | `		return PH7_OK;` |
|         - | 2905 | `	}` |
|        13 | 2906 | `	if( nArg > 1 ){` |
|         - | 2907 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2908 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2909 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        13 | 2910 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        13 | 2911 | `		if( rcCb != PH7_OK ){` |
|         3 | 2912 | `			return rcCb;` |
|         - | 2913 | `		}` |
|         5 | 2914 | `	}` |
|         - | 2915 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2916 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2917 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2918 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2919 | `		ph7_value *pCallback = 0;` |
|         - | 2920 | `		ProcNodeCmp xCmp;` |
|        11 | 2921 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2922 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2923 | `			/* Point to the desired callback */` |
|        11 | 2924 | `			pCallback = apArg[1];` |
|         6 | 2925 | `		}else{` |
|         - | 2926 | `			/* Use the default comparison function */` |
|       ! 0 | 2927 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2928 | `		}` |
|         - | 2929 | `		/* Do the merge sort */` |
|        11 | 2930 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2931 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2932 | `		/* Fix the last link broken by the merge */` |
|        23 | 2933 | `		while(pMap->pLast->pPrev){` |
|        13 | 2934 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2935 | `		}` |
|        11 | 2936 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2937 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2938 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2939 | `			return PH7_EXCEPTION;` |
|         - | 2940 | `		}` |
|         5 | 2941 | `	}` |
|         - | 2942 | `	/* All done,return TRUE */` |
|        11 | 2943 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2944 | `	return PH7_OK;` |
|         7 | 2945 | `}` |
|         - | 2946 | `/*` |
|         - | 2947 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2948 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2949 | ` *  function and maintain index association.` |
|         - | 2950 | ` * Parameters` |
|         - | 2951 | ` *  $array` |
|         - | 2952 | ` *   The input array.` |
|         - | 2953 | ` * $cmp_function` |
|         - | 2954 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2955 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2956 | ` *  to, or greater than the second.` |
|         - | 2957 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2958 | ` * Return` |
|         - | 2959 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2960 | ` */` |
|         4 | 2961 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2962 | `{` |
|         - | 2963 | `	ph7_hashmap *pMap;` |
|         - | 2964 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2965 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2966 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2967 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2968 | `		return PH7_OK;` |
|         - | 2969 | `	}` |
|         5 | 2970 | `	if( nArg > 1 ){` |
|         - | 2971 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2972 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2973 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|         5 | 2974 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|         5 | 2975 | `		if( rcCb != PH7_OK ){` |
|         3 | 2976 | `			return rcCb;` |
|         - | 2977 | `		}` |
|         1 | 2978 | `	}` |
|         - | 2979 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2980 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2981 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2982 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2983 | `		ph7_value *pCallback = 0;` |
|         - | 2984 | `		ProcNodeCmp xCmp;` |
|         3 | 2985 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2986 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2987 | `			/* Point to the desired callback */` |
|         3 | 2988 | `			pCallback = apArg[1];` |
|         2 | 2989 | `		}else{` |
|         - | 2990 | `			/* Use the default comparison function */` |
|       ! 0 | 2991 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2992 | `		}` |
|         - | 2993 | `		/* Do the merge sort */` |
|         3 | 2994 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2995 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2996 | `		/* Fix the last link broken by the merge */` |
|         3 | 2997 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2998 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2999 | `		}` |
|         3 | 3000 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 3001 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 3002 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 3003 | `			return PH7_EXCEPTION;` |
|         - | 3004 | `		}` |
|         1 | 3005 | `	}` |
|         - | 3006 | `	/* All done,return TRUE */` |
|         3 | 3007 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3008 | `	return PH7_OK;` |
|         3 | 3009 | `}` |
|         - | 3010 | `/*` |
|         - | 3011 | ` * bool shuffle(array &$array)` |
|         - | 3012 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 3013 | ` * Parameters` |
|         - | 3014 | ` *  $array` |
|         - | 3015 | ` *   The input array.` |
|         - | 3016 | ` * Return` |
|         - | 3017 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3018 | ` *` |
|         - | 3019 | ` */` |
|         2 | 3020 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3021 | `{` |
|         - | 3022 | `	ph7_hashmap *pMap;` |
|         - | 3023 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3024 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3025 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3026 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3027 | `		return PH7_OK;` |
|         - | 3028 | `	}` |
|         - | 3029 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3030 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3031 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3032 | `	if( pMap->nEntry > 1 ){` |
|         - | 3033 | `		/* Do the merge sort */` |
|         3 | 3034 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 3035 | `		/* Fix the last link broken by the merge */` |
|         9 | 3036 | `		while(pMap->pLast->pPrev){` |
|         7 | 3037 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 3038 | `		}` |
|         1 | 3039 | `	}` |
|         - | 3040 | `	/* All done,return TRUE */` |
|         3 | 3041 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3042 | `	return PH7_OK;` |
|         2 | 3043 | `}` |
|         - | 3044 | `/*` |
|         - | 3045 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 3046 | ` *   Count all elements in an array, or something in an object.` |
|         - | 3047 | ` * Parameters` |
|         - | 3048 | ` *  $var` |
|         - | 3049 | ` *   The array or the object.` |
|         - | 3050 | ` * $mode` |
|         - | 3051 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 3052 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 3053 | ` *  all the elements of a multidimensional array.` |
|         - | 3054 | ` * Return` |
|         - | 3055 | ` *  Returns the number of elements in the array.` |
|         - | 3056 | ` */` |
|      1978 | 3057 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3058 | `{` |
|      1983 | 3059 | `	int bRecursive = FALSE;` |
|      1983 | 3060 | `	int bCycleDetected = FALSE;` |
|         - | 3061 | `	sxi64 iCount;` |
|      1983 | 3062 | `	if( nArg < 1 ){` |
|       ! 0 | 3063 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3064 | `			"ArgumentCountError",` |
|         - | 3065 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 3066 | `			);` |
|         - | 3067 | `	}` |
|      1983 | 3068 | `	if( nArg > 2 ){` |
|         4 | 3069 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3070 | `			"ArgumentCountError",` |
|         - | 3071 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3072 | `			nArg` |
|         - | 3073 | `			);` |
|         - | 3074 | `	}` |
|         - | 3075 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3076 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3077 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1981 | 3078 | `	if( nArg > 1 ){` |
|        44 | 3079 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 3080 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 3081 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3082 | `				"ValueError",` |
|         - | 3083 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3084 | `				);` |
|         - | 3085 | `		}` |
|        34 | 3086 | `		bRecursive = iMode == 1;` |
|        16 | 3087 | `	}` |
|      1973 | 3088 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3089 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 3090 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 3091 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 3092 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 3093 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 3094 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3095 | `					"count",sizeof("count")-1);` |
|        59 | 3096 | `				if( pMeth ){` |
|         - | 3097 | `					ph7_value sResult;` |
|        59 | 3098 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 3099 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3100 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3101 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3102 | `					return PH7_OK;` |
|         - | 3103 | `				}` |
|       ! 0 | 3104 | `			}` |
|         1 | 3105 | `		}` |
|        22 | 3106 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3107 | `			"TypeError",` |
|         - | 3108 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3109 | `			ph7_type_name(apArg[0])` |
|         - | 3110 | `			);` |
|         - | 3111 | `	}` |
|         - | 3112 | `	/* Count */` |
|      1905 | 3113 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1905 | 3114 | `	if( bCycleDetected ){` |
|         3 | 3115 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3116 | `	}` |
|      1905 | 3117 | `	ph7_result_int64(pCtx,iCount);` |
|      1905 | 3118 | `	return PH7_OK;` |
|       994 | 3119 | `}` |
|         - | 3120 | `/*` |
|         - | 3121 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3122 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3123 | ` * Parameters` |
|         - | 3124 | ` * $key` |
|         - | 3125 | ` *   Value to check.` |
|         - | 3126 | ` * $search` |
|         - | 3127 | ` *  An array with keys to check.` |
|         - | 3128 | ` * Return` |
|         - | 3129 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3130 | ` */` |
|        94 | 3131 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3132 | `{` |
|         - | 3133 | `	sxi32 rc;` |
|        99 | 3134 | `	if( nArg != 2 ){` |
|         - | 3135 | `		/* PHP requires exactly two arguments */` |
|         4 | 3136 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3137 | `			"ArgumentCountError",` |
|         - | 3138 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3139 | `			nArg` |
|         - | 3140 | `			);` |
|         - | 3141 | `	}` |
|         - | 3142 | `	/* Make sure we are dealing with a valid hashmap */` |
|        97 | 3143 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3144 | `		/* Type mismatch -> TypeError */` |
|         8 | 3145 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3146 | `			"TypeError",` |
|         - | 3147 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3148 | `			ph7_type_name(apArg[1])` |
|         - | 3149 | `			);` |
|         - | 3150 | `	}` |
|         - | 3151 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        92 | 3152 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3153 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3154 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3155 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3156 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3157 | `			"use an empty string instead"` |
|         - | 3158 | `			);` |
|        91 | 3159 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3160 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3161 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3162 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3163 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3164 | `				,rVal` |
|         - | 3165 | `				);` |
|         1 | 3166 | `		}` |
|         1 | 3167 | `	}` |
|         - | 3168 | `	/* Perform the lookup */` |
|        92 | 3169 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3170 | `	/* lookup result */` |
|        92 | 3171 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        92 | 3172 | `	return PH7_OK;` |
|        52 | 3173 | `}` |
|         - | 3174 | `/*` |
|         - | 3175 | ` * value array_pop(array $array)` |
|         - | 3176 | ` *   POP the last inserted element from the array.` |
|         - | 3177 | ` * Parameter` |
|         - | 3178 | ` *  The array to get the value from.` |
|         - | 3179 | ` * Return` |
|         - | 3180 | ` *  Poped value or NULL on failure.` |
|         - | 3181 | ` */` |
|       102 | 3182 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3183 | `{` |
|         - | 3184 | `	ph7_hashmap *pMap;` |
|         - | 3185 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       106 | 3186 | `	if( nArg != 1 ){` |
|         4 | 3187 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3188 | `			"ArgumentCountError",` |
|         - | 3189 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3190 | `			nArg` |
|         - | 3191 | `			);` |
|         - | 3192 | `	}` |
|         - | 3193 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3194 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3195 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3196 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3197 | `			"Error",` |
|         - | 3198 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3199 | `			);` |
|         - | 3200 | `	}` |
|         - | 3201 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3202 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3203 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3204 | `			"TypeError",` |
|         - | 3205 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3206 | `			ph7_type_name(apArg[0])` |
|         - | 3207 | `			);` |
|         - | 3208 | `	}` |
|        95 | 3209 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3210 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3211 | `	if( pMap->nEntry < 1 ){` |
|         - | 3212 | `		/* Nothing to pop,return NULL */` |
|         3 | 3213 | `		ph7_result_null(pCtx);` |
|         2 | 3214 | `	}else{` |
|        93 | 3215 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3216 | `		ph7_value *pObj;` |
|        93 | 3217 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3218 | `		if( pObj ){` |
|         - | 3219 | `			/* Node value */` |
|        93 | 3220 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3221 | `			/* Unlink the node */` |
|        93 | 3222 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3223 | `		}else{` |
|       ! 0 | 3224 | `			ph7_result_null(pCtx);` |
|         - | 3225 | `		}` |
|         - | 3226 | `		/* Reset the cursor */` |
|        93 | 3227 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3228 | `	}` |
|        95 | 3229 | `	return PH7_OK;` |
|        55 | 3230 | `}` |
|         - | 3231 | `/*` |
|         - | 3232 | ` * int array_push($array,$var,...)` |
|         - | 3233 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3234 | ` * Parameters` |
|         - | 3235 | ` *  array` |
|         - | 3236 | ` *    The input array.` |
|         - | 3237 | ` *  var` |
|         - | 3238 | ` *   On or more value to push.` |
|         - | 3239 | ` * Return` |
|         - | 3240 | ` *  New array count (including old items).` |
|         - | 3241 | ` */` |
|        22 | 3242 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3243 | `{` |
|         - | 3244 | `	ph7_hashmap *pMap;` |
|         - | 3245 | `	sxi32 rc;` |
|         - | 3246 | `	int i;` |
|        26 | 3247 | `	if( nArg < 1 ){` |
|       ! 0 | 3248 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3249 | `			"ArgumentCountError",` |
|         - | 3250 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3251 | `			nArg` |
|         - | 3252 | `			);` |
|         - | 3253 | `	}` |
|         - | 3254 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3255 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3256 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3257 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3258 | `			"Error",` |
|         - | 3259 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3260 | `			);` |
|         - | 3261 | `	}` |
|         - | 3262 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3263 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3264 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3265 | `			"TypeError",` |
|         - | 3266 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3267 | `			ph7_type_name(apArg[0])` |
|         - | 3268 | `			);` |
|         - | 3269 | `	}` |
|         - | 3270 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3271 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3272 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3273 | `	/* Start pushing given values */` |
|        34 | 3274 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3275 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3276 | `		if( rc != SXRET_OK ){` |
|         3 | 3277 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3278 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3279 | `				return rc;` |
|         - | 3280 | `			}` |
|       ! 0 | 3281 | `			break;` |
|         - | 3282 | `		}` |
|         9 | 3283 | `	}` |
|         - | 3284 | `	/* Return the new count */` |
|        15 | 3285 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3286 | `	return PH7_OK;` |
|        15 | 3287 | `}` |
|         - | 3288 | `/*` |
|         - | 3289 | ` * value array_shift(array $array)` |
|         - | 3290 | ` *   Shift an element off the beginning of array.` |
|         - | 3291 | ` * Parameter` |
|         - | 3292 | ` *  The array to get the value from.` |
|         - | 3293 | ` * Return` |
|         - | 3294 | ` *  Shifted value or NULL on failure.` |
|         - | 3295 | ` */` |
|        44 | 3296 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3297 | `{` |
|         - | 3298 | `	ph7_hashmap *pMap;` |
|         - | 3299 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3300 | `	if( nArg != 1 ){` |
|         4 | 3301 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3302 | `			"ArgumentCountError",` |
|         - | 3303 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3304 | `			nArg` |
|         - | 3305 | `			);` |
|         - | 3306 | `	}` |
|         - | 3307 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3308 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3309 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3310 | `			"Error",` |
|         - | 3311 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3312 | `			);` |
|         - | 3313 | `	}` |
|         - | 3314 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3315 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3316 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3317 | `			"TypeError",` |
|         - | 3318 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3319 | `			ph7_type_name(apArg[0])` |
|         - | 3320 | `			);` |
|         - | 3321 | `	}` |
|         - | 3322 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3323 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3324 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3325 | `	if( pMap->nEntry < 1 ){` |
|         - | 3326 | `		/* Empty hashmap,return NULL */` |
|         3 | 3327 | `		ph7_result_null(pCtx);` |
|         2 | 3328 | `	}else{` |
|        39 | 3329 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3330 | `		ph7_value *pObj;` |
|         - | 3331 | `		sxu32 n;` |
|        39 | 3332 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3333 | `		if( pObj ){` |
|         - | 3334 | `			/* Node value */` |
|        39 | 3335 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3336 | `			/* Unlink the first node */` |
|        39 | 3337 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3338 | `		}else{` |
|       ! 0 | 3339 | `			ph7_result_null(pCtx);` |
|         - | 3340 | `		}` |
|         - | 3341 | `		/* Rehash all int keys */` |
|        39 | 3342 | `		n = pMap->nEntry;` |
|        39 | 3343 | `		pEntry = pMap->pFirst;` |
|        39 | 3344 | `		pMap->iNextIdx = 0;` |
|        39 | 3345 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3346 | `		for(;;){` |
|        99 | 3347 | `			if( n < 1 ){` |
|        39 | 3348 | `				break;` |
|         - | 3349 | `			}` |
|        65 | 3350 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3351 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3352 | `			}` |
|         - | 3353 | `			/* Point to the next entry */` |
|        65 | 3354 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3355 | `			n--;` |
|         5 | 3356 | `		}` |
|         - | 3357 | `		/* Reset the cursor */` |
|        39 | 3358 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3359 | `	}` |
|        41 | 3360 | `	return PH7_OK;` |
|        27 | 3361 | `}` |
|         - | 3362 | `/*` |
|         - | 3363 | ` * Extract the node cursor value.` |
|         - | 3364 | ` */` |
|      1094 | 3365 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3366 | `{` |
|      1095 | 3367 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3368 | `	ph7_value *pVal;` |
|      1095 | 3369 | `	if( pCur == 0 ){` |
|         - | 3370 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3371 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3372 | `		return PH7_OK;` |
|         - | 3373 | `	}` |
|      1057 | 3374 | `	if( iDirection != 0 ){` |
|       201 | 3375 | `		if( iDirection > 0 ){` |
|         - | 3376 | `			/* Point to the next entry */` |
|       199 | 3377 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3378 | `			pCur = pMap->pCur;` |
|       100 | 3379 | `		}else{` |
|         - | 3380 | `			/* Point to the previous entry */` |
|         3 | 3381 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3382 | `			pCur = pMap->pCur;` |
|         - | 3383 | `		}` |
|       201 | 3384 | `		if( pCur == 0 ){` |
|         - | 3385 | `			/* End of input reached,return FALSE */` |
|        83 | 3386 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3387 | `			return PH7_OK;` |
|         - | 3388 | `		}` |
|        59 | 3389 | `	}` |
|         - | 3390 | `	/* Point to the desired element */` |
|       975 | 3391 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3392 | `	if( pVal ){` |
|       975 | 3393 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3394 | `	}else{` |
|       ! 0 | 3395 | `		ph7_result_bool(pCtx,0);` |
|         - | 3396 | `	}` |
|       975 | 3397 | `	return PH7_OK;` |
|       548 | 3398 | `}` |
|         - | 3399 | `/*` |
|         - | 3400 | ` * value current(array $array)` |
|         - | 3401 | ` *  Return the current element in an array.` |
|         - | 3402 | ` * Parameter` |
|         - | 3403 | ` *  $input: The input array.` |
|         - | 3404 | ` * Return` |
|         - | 3405 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3406 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3407 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3408 | ` *  is empty, current() returns FALSE.` |
|         - | 3409 | ` */` |
|       302 | 3410 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3411 | `{` |
|       303 | 3412 | `	if( nArg < 1 ){` |
|         - | 3413 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3414 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3415 | `		return PH7_OK;` |
|         - | 3416 | `	}` |
|         - | 3417 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3419 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3420 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3421 | `		return PH7_OK;` |
|         - | 3422 | `	}` |
|       303 | 3423 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3424 | `	return PH7_OK;` |
|       152 | 3425 | `}` |
|         - | 3426 | `/*` |
|         - | 3427 | ` * value next(array $input)` |
|         - | 3428 | ` *  Advance the internal array pointer of an array.` |
|         - | 3429 | ` * Parameter` |
|         - | 3430 | ` *  $input: The input array.` |
|         - | 3431 | ` * Return` |
|         - | 3432 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3433 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3434 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3435 | ` */` |
|       198 | 3436 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3437 | `{` |
|       199 | 3438 | `	if( nArg < 1 ){` |
|         - | 3439 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3440 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3441 | `		return PH7_OK;` |
|         - | 3442 | `	}` |
|         - | 3443 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3444 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3445 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3446 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3447 | `		return PH7_OK;` |
|         - | 3448 | `	}` |
|       199 | 3449 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3450 | `	return PH7_OK;` |
|       100 | 3451 | `}` |
|         - | 3452 | `/*` |
|         - | 3453 | ` * value prev(array $input)` |
|         - | 3454 | ` *  Rewind the internal array pointer.` |
|         - | 3455 | ` * Parameter` |
|         - | 3456 | ` *  $input: The input array.` |
|         - | 3457 | ` * Return` |
|         - | 3458 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3459 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3460 | ` *  elements.` |
|         - | 3461 | ` */` |
|         2 | 3462 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3463 | `{` |
|         3 | 3464 | `	if( nArg < 1 ){` |
|         - | 3465 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3466 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3467 | `		return PH7_OK;` |
|         - | 3468 | `	}` |
|         - | 3469 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3470 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3471 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3472 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3473 | `		return PH7_OK;` |
|         - | 3474 | `	}` |
|         3 | 3475 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3476 | `	return PH7_OK;` |
|         2 | 3477 | `}` |
|         - | 3478 | `/*` |
|         - | 3479 | ` * value end(array $input)` |
|         - | 3480 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3481 | ` * Parameter` |
|         - | 3482 | ` *  $input: The input array.` |
|         - | 3483 | ` * Return` |
|         - | 3484 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3485 | ` */` |
|       348 | 3486 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3487 | `{` |
|         - | 3488 | `	ph7_hashmap *pMap;` |
|       349 | 3489 | `	if( nArg < 1 ){` |
|         - | 3490 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3491 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3492 | `		return PH7_OK;` |
|         - | 3493 | `	}` |
|         - | 3494 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3495 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3496 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3497 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3498 | `		return PH7_OK;` |
|         - | 3499 | `	}` |
|         - | 3500 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3501 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3502 | `	/* Point to the last node */` |
|       349 | 3503 | `	pMap->pCur = pMap->pLast;` |
|         - | 3504 | `	/* Return the last node value */` |
|       349 | 3505 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3506 | `	return PH7_OK;` |
|       175 | 3507 | `}` |
|         - | 3508 | `/*` |
|         - | 3509 | ` * value reset(array $array )` |
|         - | 3510 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3511 | ` * Parameter` |
|         - | 3512 | ` *  $input: The input array.` |
|         - | 3513 | ` * Return` |
|         - | 3514 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3515 | ` */` |
|       244 | 3516 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3517 | `{` |
|         - | 3518 | `	ph7_hashmap *pMap;` |
|       245 | 3519 | `	if( nArg < 1 ){` |
|         - | 3520 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3521 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3522 | `		return PH7_OK;` |
|         - | 3523 | `	}` |
|         - | 3524 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3525 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3526 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3527 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3528 | `		return PH7_OK;` |
|         - | 3529 | `	}` |
|         - | 3530 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3531 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3532 | `	/* Point to the first node */` |
|       245 | 3533 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3534 | `	/* Return the last node value if available */` |
|       245 | 3535 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3536 | `	return PH7_OK;` |
|       123 | 3537 | `}` |
|         - | 3538 | `/*` |
|         - | 3539 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3540 | ` * array_key_first() and array_key_last().` |
|         - | 3541 | ` */` |
|       672 | 3542 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3543 | `{` |
|       673 | 3544 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3545 | `		/* Key is integer */` |
|       283 | 3546 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3547 | `	}else{` |
|         - | 3548 | `		/* Key is blob */` |
|       586 | 3549 | `		ph7_result_string(pCtx,` |
|       390 | 3550 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3551 | `	}` |
|       673 | 3552 | `}` |
|         - | 3553 | `/*` |
|         - | 3554 | ` * value key(array $array)` |
|         - | 3555 | ` *   Fetch a key from an array` |
|         - | 3556 | ` * Parameter` |
|         - | 3557 | ` *  $input` |
|         - | 3558 | ` *   The input array.` |
|         - | 3559 | ` * Return` |
|         - | 3560 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3561 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3562 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3563 | ` *  is empty, key() returns NULL.` |
|         - | 3564 | ` */` |
|       776 | 3565 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3566 | `{` |
|         - | 3567 | `	ph7_hashmap_node *pCur;` |
|         - | 3568 | `	ph7_hashmap *pMap;` |
|       777 | 3569 | `	if( nArg < 1 ){` |
|         - | 3570 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3571 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3572 | `		return PH7_OK;` |
|         - | 3573 | `	}` |
|         - | 3574 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3575 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3576 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3577 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3578 | `		return PH7_OK;` |
|         - | 3579 | `	}` |
|       777 | 3580 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3581 | `	pCur = pMap->pCur;` |
|       777 | 3582 | `	if( pCur == 0 ){` |
|         - | 3583 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3584 | `		ph7_result_null(pCtx);` |
|       121 | 3585 | `		return PH7_OK;` |
|         - | 3586 | `	}` |
|       657 | 3587 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3588 | `	return PH7_OK;` |
|       389 | 3589 | `}` |
|         - | 3590 | `/*` |
|         - | 3591 | ` * array each(array $input)` |
|         - | 3592 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3593 | ` * Parameter` |
|         - | 3594 | ` *  $input` |
|         - | 3595 | ` *    The input array.` |
|         - | 3596 | ` * Return` |
|         - | 3597 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3598 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3599 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3600 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3601 | ` *  each() returns FALSE.` |
|         - | 3602 | ` */` |
|        22 | 3603 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3604 | `{` |
|         - | 3605 | `	ph7_hashmap_node *pCur;` |
|         - | 3606 | `	ph7_hashmap *pMap;` |
|         - | 3607 | `	ph7_value *pArray;` |
|         - | 3608 | `	ph7_value *pVal;` |
|         - | 3609 | `	ph7_value sKey;` |
|        23 | 3610 | `	if( nArg < 1 ){` |
|         - | 3611 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3612 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3613 | `		return PH7_OK;` |
|         - | 3614 | `	}` |
|         - | 3615 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3616 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3617 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3618 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3619 | `		return PH7_OK;` |
|         - | 3620 | `	}` |
|         - | 3621 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3622 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3623 | `	if( pMap->pCur == 0 ){` |
|         - | 3624 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3625 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3626 | `		return PH7_OK;` |
|         - | 3627 | `	}` |
|        15 | 3628 | `	pCur = pMap->pCur;` |
|         - | 3629 | `	/* Create a new array */` |
|        15 | 3630 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3631 | `	if( pArray == 0 ){` |
|       ! 0 | 3632 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3633 | `		return PH7_OK;` |
|         - | 3634 | `	}` |
|        15 | 3635 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3636 | `	/* Insert the current value */` |
|        15 | 3637 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3638 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3639 | `	/* Make the key */` |
|        15 | 3640 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3641 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3642 | `	}else{` |
|         9 | 3643 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3644 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3645 | `	}` |
|         - | 3646 | `	/* Insert the current key */` |
|        15 | 3647 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3648 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3649 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3650 | `	/* Advance the cursor */` |
|        15 | 3651 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3652 | `	/* Return the current entry */` |
|        15 | 3653 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3654 | `	return PH7_OK;` |
|        12 | 3655 | `}` |
|         - | 3656 | `/*` |
|         - | 3657 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3658 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3659 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3660 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3661 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3662 | ` */` |
|         - | 3663 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3664 | `/*` |
|         - | 3665 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3666 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3667 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3668 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3669 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3670 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3671 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3672 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3673 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3674 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3675 | ` */` |
|         - | 3676 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3677 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3678 | `/*` |
|         - | 3679 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3680 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3681 | ` */` |
|       ! 0 | 3682 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3683 | `{` |
|       ! 0 | 3684 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3685 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3686 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3687 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3688 | `		zBuf[n] = 0;` |
|       ! 0 | 3689 | `		return zBuf;` |
|         - | 3690 | `	}` |
|       ! 0 | 3691 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3692 | `}` |
|         - | 3693 | `/*` |
|         - | 3694 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3695 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3696 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3697 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3698 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3699 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3700 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3701 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3702 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3703 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3704 | ` */` |
|       156 | 3705 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3706 | `{` |
|       157 | 3707 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3708 | `	sxu64 uVal = 0;` |
|       157 | 3709 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3710 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3711 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3712 | `		bNeg = (z[0] == '-');` |
|         3 | 3713 | `		z++;` |
|         1 | 3714 | `	}` |
|       237 | 3715 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3716 | `		int d = z[0] - '0';` |
|         - | 3717 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3718 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3719 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3720 | `			bOverflow = 1;` |
|       ! 0 | 3721 | `		}else{` |
|        81 | 3722 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3723 | `		}` |
|        81 | 3724 | `		bDigit = 1;` |
|        81 | 3725 | `		z++;` |
|         1 | 3726 | `	}` |
|       157 | 3727 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3728 | `		bReal = 1;` |
|         3 | 3729 | `		z++;` |
|         5 | 3730 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3731 | `			bDigit = 1;` |
|         3 | 3732 | `			z++;` |
|         1 | 3733 | `		}` |
|         1 | 3734 | `	}` |
|         - | 3735 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3736 | `	if( !bDigit ){` |
|        61 | 3737 | `		return RANGE_IN_ERROR;` |
|         - | 3738 | `	}` |
|         - | 3739 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3740 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3741 | `		z++;` |
|         9 | 3742 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3743 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3744 | `			return RANGE_IN_ERROR;` |
|         - | 3745 | `		}` |
|         9 | 3746 | `		bReal = 1;` |
|        17 | 3747 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3748 | `	}` |
|         - | 3749 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3750 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3751 | `	if( z != zEnd ){` |
|        13 | 3752 | `		return RANGE_IN_ERROR;` |
|         - | 3753 | `	}` |
|        84 | 3754 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3755 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3756 | `		bReal = 1;` |
|        84 | 3757 | `	}` |
|        43 | 3758 | `	if( bReal ){` |
|        11 | 3759 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3760 | `		return RANGE_IN_DOUBLE;` |
|         - | 3761 | `	}` |
|         - | 3762 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3763 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3764 | `	return RANGE_IN_LONG;` |
|        58 | 3765 | `}` |
|         - | 3766 | `/*` |
|         - | 3767 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3768 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3769 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3770 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3771 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3772 | ` */` |
|       328 | 3773 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3774 | `{` |
|         - | 3775 | `	char zMsg[160];` |
|       329 | 3776 | `	*pRc = PH7_OK;` |
|       329 | 3777 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3778 | `		char zType[80];` |
|       ! 0 | 3779 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3780 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3781 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3782 | `		return FALSE;` |
|         - | 3783 | `	}` |
|       329 | 3784 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3785 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3786 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3787 | `			iArg,zName);` |
|         5 | 3788 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3789 | `		*pbNullCoerced = TRUE;` |
|         2 | 3790 | `	}` |
|       329 | 3791 | `	return TRUE;` |
|       165 | 3792 | `}` |
|         - | 3793 | `/*` |
|         - | 3794 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3795 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3796 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3797 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3798 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3799 | ` */` |
|        60 | 3800 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3801 | `{` |
|        61 | 3802 | `	*pRc = PH7_OK;` |
|        61 | 3803 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3804 | `		char zType[80];` |
|       ! 0 | 3805 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3806 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3807 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3808 | `		return RANGE_IN_ERROR;` |
|         - | 3809 | `	}` |
|        61 | 3810 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3811 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3812 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3813 | `		*pLong = 0;` |
|         3 | 3814 | `		return RANGE_IN_LONG;` |
|         - | 3815 | `	}` |
|        59 | 3816 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3817 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3818 | `		return RANGE_IN_DOUBLE;` |
|         - | 3819 | `	}` |
|        35 | 3820 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3821 | `		const char *zStr;` |
|         - | 3822 | `		int nLen;` |
|         - | 3823 | `		sxu8 iKind;` |
|         3 | 3824 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3825 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3826 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3827 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3828 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3829 | `		}` |
|         3 | 3830 | `		return iKind;` |
|         - | 3831 | `	}` |
|         - | 3832 | `	/* int / bool */` |
|        33 | 3833 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3834 | `	return RANGE_IN_LONG;` |
|        31 | 3835 | `}` |
|         - | 3836 | `/*` |
|         - | 3837 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3838 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3839 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3840 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3841 | ` */` |
|       296 | 3842 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3843 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3844 | `{` |
|         - | 3845 | `	char zMsg[160];` |
|         - | 3846 | `	double r;` |
|       297 | 3847 | `	*pRc = PH7_OK;` |
|       297 | 3848 | `	if( bNullCoerced ){` |
|         - | 3849 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3850 | `		*pLong = 0;` |
|         5 | 3851 | `		*pDouble = 0.0;` |
|         5 | 3852 | `		return RANGE_IN_LONG;` |
|         - | 3853 | `	}` |
|       293 | 3854 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3855 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3856 | `check_dval:` |
|        25 | 3857 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3858 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3859 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3860 | `			return RANGE_IN_ERROR;` |
|         - | 3861 | `		}` |
|        21 | 3862 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3863 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3864 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3865 | `			return RANGE_IN_ERROR;` |
|         - | 3866 | `		}` |
|        17 | 3867 | `		*pDouble = r;` |
|        17 | 3868 | `		return RANGE_IN_DOUBLE;` |
|         - | 3869 | `	}` |
|       273 | 3870 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3871 | `		const char *zStr;` |
|         - | 3872 | `		int nLen;` |
|         - | 3873 | `		sxu8 iKind;` |
|        81 | 3874 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3875 | `		if( nLen == 0 ){` |
|         7 | 3876 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3877 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3878 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3879 | `			*pLong = 0;` |
|         5 | 3880 | `			*pDouble = 0.0;` |
|        41 | 3881 | `			return RANGE_IN_LONG;` |
|         - | 3882 | `		}` |
|        77 | 3883 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3884 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3885 | `			r = *pDouble;` |
|         5 | 3886 | `			goto check_dval;` |
|         - | 3887 | `		}` |
|        73 | 3888 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3889 | `			*pDouble = (double)*pLong;` |
|        23 | 3890 | `			if( nLen == 1 ){` |
|         - | 3891 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3892 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3893 | `				return RANGE_IN_DIGIT;` |
|         - | 3894 | `			}` |
|        15 | 3895 | `			return RANGE_IN_LONG;` |
|         - | 3896 | `		}` |
|        51 | 3897 | `		if( nLen != 1 ){` |
|        10 | 3898 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3899 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3900 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3901 | `		}` |
|        51 | 3902 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3903 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3904 | `		*pLong = 0;` |
|        51 | 3905 | `		*pDouble = 0.0;` |
|        51 | 3906 | `		return RANGE_IN_STRING;` |
|         - | 3907 | `	}` |
|         - | 3908 | `	/* int / bool */` |
|       193 | 3909 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3910 | `	*pDouble = (double)*pLong;` |
|       193 | 3911 | `	return RANGE_IN_LONG;` |
|       149 | 3912 | `}` |
|         - | 3913 | `/*` |
|         - | 3914 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3915 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3916 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3917 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3918 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3919 | ` * exactly like php's two macros.` |
|         - | 3920 | ` */` |
|         6 | 3921 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3922 | `{` |
|        10 | 3923 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3924 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3925 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3926 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3927 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3928 | `}` |
|         6 | 3929 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3930 | `{` |
|         - | 3931 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3932 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3933 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3934 | `	const unsigned int nBuf = 1500;` |
|         7 | 3935 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3936 | `	if( zMsg == 0 ){` |
|       ! 0 | 3937 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3938 | `	}` |
|         7 | 3939 | `	snprintf(zMsg,nBuf,` |
|         - | 3940 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3941 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3942 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3943 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3944 | `}` |
|         - | 3945 | `/*` |
|         - | 3946 | ` * Set the element container to the next range element and append it to the` |
|         - | 3947 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3948 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3949 | ` * below stay one line per iteration.` |
|         - | 3950 | ` */` |
|      1680 | 3951 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3952 | `{` |
|      1681 | 3953 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3954 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3955 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3956 | `	}` |
|      1681 | 3957 | `	return PH7_OK;` |
|       841 | 3958 | `}` |
|        70 | 3959 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3960 | `{` |
|        71 | 3961 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3962 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3963 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3964 | `	}` |
|        71 | 3965 | `	return PH7_OK;` |
|        36 | 3966 | `}` |
|       168 | 3967 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3968 | `{` |
|       169 | 3969 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3970 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3971 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3972 | `	}` |
|       169 | 3973 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3974 | `	return PH7_OK;` |
|        85 | 3975 | `}` |
|         - | 3976 | `/*` |
|         - | 3977 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3978 | ` *  Create an array containing a range of elements.` |
|         - | 3979 | ` * Return` |
|         - | 3980 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3981 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3982 | ` */` |
|       166 | 3983 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3984 | `{` |
|         - | 3985 | `	ph7_value *pValue,*pArray;` |
|       167 | 3986 | `	sxi32 rc = PH7_OK;` |
|       167 | 3987 | `	int is_step_double = 0,is_step_negative = 0;` |
|       167 | 3988 | `	double step_double = 1.0;` |
|       167 | 3989 | `	sxi64 step = 1;` |
|         - | 3990 | `	sxu8 start_type,end_type;` |
|       167 | 3991 | `	sxi64 start_long = 0,end_long = 0;` |
|       167 | 3992 | `	double start_double = 0.0,end_double = 0.0;` |
|       167 | 3993 | `	unsigned char cStart = 0,cEnd = 0;` |
|       167 | 3994 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3995 | `	sxu32 i,size;` |
|         - | 3996 |  |
|         - | 3997 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       167 | 3998 | `	if( nArg > 3 ){` |
|         4 | 3999 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 4000 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 4001 | `	}` |
|       165 | 4002 | `	if( nArg < 2 ){` |
|         - | 4003 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 4004 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 4005 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 4006 | `	}` |
|         - | 4007 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 4008 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       165 | 4009 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 4010 | `		return rc;` |
|         - | 4011 | `	}` |
|       165 | 4012 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 4013 | `		return rc;` |
|         - | 4014 | `	}` |
|       165 | 4015 | `	if( nArg > 2 ){` |
|        61 | 4016 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 4017 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 4018 | `			return rc;` |
|         - | 4019 | `		}` |
|        59 | 4020 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 4021 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 4022 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4023 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 4024 | `			}` |
|        23 | 4025 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 4026 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4027 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 4028 | `			}` |
|         - | 4029 | `			/* We only want positive step values. */` |
|        21 | 4030 | `			if( step_double < 0.0 ){` |
|       ! 0 | 4031 | `				is_step_negative = 1;` |
|       ! 0 | 4032 | `				step_double *= -1;` |
|       ! 0 | 4033 | `			}` |
|         - | 4034 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 4035 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 4036 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 4037 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 4038 | `				step = (sxi64)step_double;` |
|        19 | 4039 | `				if( (double)step != step_double ){` |
|        17 | 4040 | `					is_step_double = 1;` |
|         8 | 4041 | `				}` |
|        10 | 4042 | `			}else{` |
|         - | 4043 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 4044 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 4045 | `				is_step_double = 1;` |
|         - | 4046 | `			}` |
|        11 | 4047 | `		}else{` |
|         - | 4048 | `			/* We only want positive step values. */` |
|        35 | 4049 | `			if( step < 0 ){` |
|        11 | 4050 | `				if( step == SMALLEST_INT64 ){` |
|         - | 4051 | `					/* -step would overflow */` |
|         4 | 4052 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 4053 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 4054 | `				}` |
|         9 | 4055 | `				is_step_negative = 1;` |
|         9 | 4056 | `				step = -step;` |
|         4 | 4057 | `			}` |
|        33 | 4058 | `			step_double = (double)step;` |
|         - | 4059 | `		}` |
|        53 | 4060 | `		if( step_double == 0.0 ){` |
|         7 | 4061 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4062 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 4063 | `		}` |
|        23 | 4064 | `	}` |
|       151 | 4065 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 4066 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 4067 | `		return rc;` |
|         - | 4068 | `	}` |
|       147 | 4069 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 4070 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 4071 | `		return rc;` |
|         - | 4072 | `	}` |
|         - | 4073 | `	/* Element container + result array */` |
|       143 | 4074 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 4075 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 4076 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4077 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4078 | `	}` |
|         - | 4079 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 4080 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4081 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4082 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4083 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4084 | `			 * and the range is numeric. */` |
|        15 | 4085 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4086 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4087 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4088 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4089 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4090 | `				}` |
|         7 | 4091 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4092 | `			}else{` |
|         9 | 4093 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4094 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4095 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4096 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4097 | `				}` |
|         9 | 4098 | `				start_type = RANGE_IN_LONG;` |
|         - | 4099 | `			}` |
|        15 | 4100 | `			goto handle_numeric_inputs;` |
|         - | 4101 | `		}` |
|        23 | 4102 | `		if( is_step_double ){` |
|         - | 4103 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4104 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4105 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4106 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4107 | `					" of characters, inputs converted to 0");` |
|         1 | 4108 | `			}` |
|         5 | 4109 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4110 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4111 | `			goto handle_numeric_inputs;` |
|         - | 4112 | `		}` |
|         - | 4113 | `		/* Generate an array of characters */` |
|        19 | 4114 | `		if( cStart > cEnd ){` |
|         - | 4115 | `			/* Decreasing char range */` |
|         - | 4116 | `			int iCur;` |
|         3 | 4117 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4118 | `				goto boundary_error;` |
|         - | 4119 | `			}` |
|        17 | 4120 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4121 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4122 | `					return rc;` |
|         - | 4123 | `				}` |
|         8 | 4124 | `			}` |
|        18 | 4125 | `		}else if( cEnd > cStart ){` |
|         - | 4126 | `			/* Increasing char range */` |
|         - | 4127 | `			int iCur;` |
|        15 | 4128 | `			if( is_step_negative ){` |
|         3 | 4129 | `				goto negative_step_error;` |
|         - | 4130 | `			}` |
|        13 | 4131 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4132 | `				goto boundary_error;` |
|         - | 4133 | `			}` |
|       163 | 4134 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4135 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4136 | `					return rc;` |
|         - | 4137 | `				}` |
|        77 | 4138 | `			}` |
|         6 | 4139 | `		}else{` |
|         3 | 4140 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4141 | `				return rc;` |
|         - | 4142 | `			}` |
|         - | 4143 | `		}` |
|        15 | 4144 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4145 | `		return PH7_OK;` |
|         - | 4146 | `	}` |
|        53 | 4147 | `handle_numeric_inputs:` |
|       133 | 4148 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4149 | `		/* Float range */` |
|         - | 4150 | `		double elem,calc;` |
|        25 | 4151 | `		if( start_double > end_double ){` |
|         - | 4152 | `			/* Decreasing float range */` |
|         7 | 4153 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4154 | `				goto boundary_error;` |
|         - | 4155 | `			}` |
|         7 | 4156 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4157 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4158 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4159 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4160 | `			}` |
|         5 | 4161 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4162 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4163 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4164 | `					return rc;` |
|         - | 4165 | `				}` |
|         8 | 4166 | `			}` |
|        21 | 4167 | `		}else if( end_double > start_double ){` |
|         - | 4168 | `			/* Increasing float range */` |
|        17 | 4169 | `			if( is_step_negative ){` |
|       ! 0 | 4170 | `				goto negative_step_error;` |
|         - | 4171 | `			}` |
|        17 | 4172 | `			if( end_double - start_double < step_double ){` |
|         3 | 4173 | `				goto boundary_error;` |
|         - | 4174 | `			}` |
|        15 | 4175 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4176 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4177 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4178 | `			}` |
|        11 | 4179 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4180 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4181 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4182 | `					return rc;` |
|         - | 4183 | `				}` |
|        28 | 4184 | `			}` |
|         6 | 4185 | `		}else{` |
|         3 | 4186 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4187 | `				return rc;` |
|         - | 4188 | `			}` |
|         - | 4189 | `		}` |
|         9 | 4190 | `	}else{` |
|         - | 4191 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4192 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4193 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4194 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4195 | `		sxu64 calc;` |
|       101 | 4196 | `		if( start_long > end_long ){` |
|         - | 4197 | `			/* Decreasing int range */` |
|        19 | 4198 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4199 | `				goto boundary_error;` |
|         - | 4200 | `			}` |
|        17 | 4201 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4202 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4203 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4204 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4205 | `			}` |
|        15 | 4206 | `			size = (sxu32)(calc + 1);` |
|       101 | 4207 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4208 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4209 | `					return rc;` |
|         - | 4210 | `				}` |
|        44 | 4211 | `			}` |
|        90 | 4212 | `		}else if( end_long > start_long ){` |
|         - | 4213 | `			/* Increasing int range */` |
|        77 | 4214 | `			if( is_step_negative ){` |
|         3 | 4215 | `				goto negative_step_error;` |
|         - | 4216 | `			}` |
|        75 | 4217 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4218 | `				goto boundary_error;` |
|         - | 4219 | `			}` |
|        73 | 4220 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4221 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4222 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4223 | `			}` |
|        69 | 4224 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4225 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4226 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4227 | `					return rc;` |
|         - | 4228 | `				}` |
|       795 | 4229 | `			}` |
|        35 | 4230 | `		}else{` |
|         7 | 4231 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4232 | `				return rc;` |
|         - | 4233 | `			}` |
|         - | 4234 | `		}` |
|         - | 4235 | `	}` |
|         - | 4236 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4237 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4238 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4239 | `	return PH7_OK;` |
|         2 | 4240 | `negative_step_error:` |
|         5 | 4241 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4242 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4243 | `boundary_error:` |
|         9 | 4244 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4245 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        84 | 4246 | `}` |
|         - | 4247 | `/*` |
|         - | 4248 | ` * array array_values(array $array)` |
|         - | 4249 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4250 | ` * Parameters` |
|         - | 4251 | ` *  $array` |
|         - | 4252 | ` *   The input array.` |
|         - | 4253 | ` * Return` |
|         - | 4254 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4255 | ` */` |
|        48 | 4256 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4257 | `{` |
|         - | 4258 | `	ph7_hashmap_node *pNode;` |
|         - | 4259 | `	ph7_hashmap *pMap;` |
|         - | 4260 | `	ph7_value *pArray;` |
|         - | 4261 | `	ph7_value *pObj;` |
|         - | 4262 | `	sxu32 n;` |
|        51 | 4263 | `	if( nArg != 1 ){` |
|         - | 4264 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4265 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4266 | `			"ArgumentCountError",` |
|         - | 4267 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4268 | `			nArg` |
|         - | 4269 | `			);` |
|         - | 4270 | `	}` |
|         - | 4271 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4272 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4273 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4274 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4275 | `			"TypeError",` |
|         - | 4276 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4277 | `			ph7_type_name(apArg[0])` |
|         - | 4278 | `			);` |
|         - | 4279 | `	}` |
|         - | 4280 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4281 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4282 | `	/* Create a new array */` |
|        46 | 4283 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4284 | `	if( pArray == 0 ){` |
|       ! 0 | 4285 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4286 | `		return PH7_OK;` |
|         - | 4287 | `	}` |
|         - | 4288 | `	/* Perform the requested operation */` |
|        46 | 4289 | `	pNode = pMap->pFirst;` |
|       144 | 4290 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4291 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4292 | `		if( pObj ){` |
|         - | 4293 | `			/* perform the insertion */` |
|       100 | 4294 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4295 | `		}` |
|         - | 4296 | `		/* Point to the next entry */` |
|       100 | 4297 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4298 | `	}` |
|         - | 4299 | `	/* return the new array */` |
|        46 | 4300 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4301 | `	return PH7_OK;` |
|        27 | 4302 | `}` |
|         - | 4303 | `/*` |
|         - | 4304 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4305 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4306 | ` * Parameters` |
|         - | 4307 | ` *  $input` |
|         - | 4308 | ` *   An array containing keys to return.` |
|         - | 4309 | ` * $search_value` |
|         - | 4310 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4311 | ` * $strict` |
|         - | 4312 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4313 | ` * Return` |
|         - | 4314 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4315 | ` */` |
|       166 | 4316 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4317 | `{` |
|         - | 4318 | `	ph7_hashmap_node *pNode;` |
|         - | 4319 | `	ph7_hashmap *pMap;` |
|         - | 4320 | `	ph7_value *pArray;` |
|         - | 4321 | `	ph7_value sObj;` |
|         - | 4322 | `	ph7_value sVal;` |
|         - | 4323 | `	SyString sKey;` |
|         - | 4324 | `	int bStrict;` |
|         - | 4325 | `	sxi32 rc;` |
|         - | 4326 | `	sxu32 n;` |
|       170 | 4327 | `	if( nArg < 1 ){` |
|         - | 4328 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4329 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4330 | `			"ArgumentCountError",` |
|         - | 4331 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4332 | `			);` |
|         - | 4333 | `	}` |
|         - | 4334 | `	/* Make sure we are dealing with a valid hashmap */` |
|       170 | 4335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4336 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4337 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4338 | `			"TypeError",` |
|         - | 4339 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4340 | `			ph7_type_name(apArg[0])` |
|         - | 4341 | `			);` |
|         - | 4342 | `	}` |
|         - | 4343 | `	/* Point to the internal representation of the input hashmap */` |
|       167 | 4344 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4345 | `	/* Create a new array */` |
|       167 | 4346 | `	pArray = ph7_context_new_array(pCtx);` |
|       167 | 4347 | `	if( pArray == 0 ){` |
|       ! 0 | 4348 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4349 | `		return PH7_OK;` |
|         - | 4350 | `	}` |
|       167 | 4351 | `	bStrict = FALSE;` |
|       167 | 4352 | `	if( nArg > 2 ){` |
|         - | 4353 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4354 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4355 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4356 | `				"TypeError",` |
|         - | 4357 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4358 | `				ph7_type_name(apArg[2])` |
|         - | 4359 | `				);` |
|         - | 4360 | `		}` |
|         9 | 4361 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4362 | `	}` |
|         - | 4363 | `	/* Perform the requested operation */` |
|       167 | 4364 | `	pNode = pMap->pFirst;` |
|       167 | 4365 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1489 | 4366 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1325 | 4367 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       193 | 4368 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        98 | 4369 | `		}else{` |
|      1134 | 4370 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1134 | 4371 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4372 | `		}` |
|      1325 | 4373 | `		rc = 0;` |
|      1325 | 4374 | `		if( nArg > 1 ){` |
|        65 | 4375 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4376 | `			if( pValue ){` |
|         - | 4377 | `				ph7_value sNeedle;` |
|        65 | 4378 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4379 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4380 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4381 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4382 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4383 | `				 * corrupt every later comparison. */` |
|        65 | 4384 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4385 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4386 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4387 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4388 | `			}` |
|        32 | 4389 | `		}` |
|      1325 | 4390 | `		if( rc == 0 ){` |
|         - | 4391 | `			/* Perform the insertion */` |
|      1293 | 4392 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       645 | 4393 | `		}` |
|      1325 | 4394 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4395 | `		/* Point to the next entry */` |
|      1325 | 4396 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       664 | 4397 | `	}` |
|         - | 4398 | `	/* return the new array */` |
|       167 | 4399 | `	ph7_result_value(pCtx,pArray);` |
|       167 | 4400 | `	return PH7_OK;` |
|        87 | 4401 | `}` |
|         - | 4402 | `/*` |
|         - | 4403 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4404 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4405 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4406 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4407 | ` * Parameters` |
|         - | 4408 | ` *  $arr1` |
|         - | 4409 | ` *   First array` |
|         - | 4410 | ` *  $arr2` |
|         - | 4411 | ` *   Second array` |
|         - | 4412 | ` * Return` |
|         - | 4413 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4414 | ` * Note` |
|         - | 4415 | ` *  This function is a symisc eXtension.` |
|         - | 4416 | ` */` |
|         4 | 4417 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4418 | `{` |
|         - | 4419 | `	ph7_hashmap *p1,*p2;` |
|         - | 4420 | `	int rc;` |
|         5 | 4421 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4422 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4423 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4424 | `		return PH7_OK;` |
|         - | 4425 | `	}` |
|         - | 4426 | `	/* Point to the hashmaps */` |
|         5 | 4427 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4428 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4429 | `	rc = (p1 == p2);` |
|         - | 4430 | `	/* Same instance? */` |
|         5 | 4431 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4432 | `	return PH7_OK;` |
|         3 | 4433 | `}` |
|         - | 4434 | `/*` |
|         - | 4435 | ` * array array_merge(array ...$arrays)` |
|         - | 4436 | ` *  Merge one or more arrays.` |
|         - | 4437 | ` * Parameters` |
|         - | 4438 | ` *  ...$arrays` |
|         - | 4439 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4440 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4441 | ` * Return` |
|         - | 4442 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4443 | ` *  with no arguments.` |
|         - | 4444 | ` */` |
|      1096 | 4445 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4446 | `{` |
|         - | 4447 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4448 | `	ph7_value *pArray;` |
|         - | 4449 | `	int i;` |
|         - | 4450 | `	/* Create a new array */` |
|      1101 | 4451 | `	pArray = ph7_context_new_array(pCtx);` |
|      1101 | 4452 | `	if( pArray == 0 ){` |
|       ! 0 | 4453 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4454 | `		return PH7_OK;` |
|         - | 4455 | `	}` |
|         - | 4456 | `	/* Point to the internal representation of the hashmap */` |
|      1101 | 4457 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4458 | `	/* Start merging */` |
|      3283 | 4459 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4460 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2191 | 4461 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4462 | `			/* Type mismatch -> TypeError */` |
|         8 | 4463 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4464 | `				"TypeError",` |
|         - | 4465 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4466 | `				i + 1,` |
|         4 | 4467 | `				ph7_type_name(apArg[i])` |
|         - | 4468 | `				);` |
|       ! 0 | 4469 | `		}else{` |
|      2187 | 4470 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4471 | `			/* Merge the two hashmaps */` |
|      2187 | 4472 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4473 | `		}` |
|      1096 | 4474 | `	}` |
|         - | 4475 | `	/* Return the freshly created array */` |
|      1097 | 4476 | `	ph7_result_value(pCtx,pArray);` |
|      1097 | 4477 | `	return PH7_OK;` |
|       553 | 4478 | `}` |
|         - | 4479 | `/*` |
|         - | 4480 | ` * array array_copy(array $source)` |
|         - | 4481 | ` *  Make a blind copy of the target array.` |
|         - | 4482 | ` * Parameters` |
|         - | 4483 | ` *  $source` |
|         - | 4484 | ` *   Target array` |
|         - | 4485 | ` * Return` |
|         - | 4486 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4487 | ` * Note` |
|         - | 4488 | ` *  This function is a symisc eXtension.` |
|         - | 4489 | ` */` |
|        18 | 4490 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4491 | `{` |
|         - | 4492 | `	ph7_hashmap *pMap;` |
|         - | 4493 | `	ph7_value *pArray;` |
|        19 | 4494 | `	if( nArg < 1 ){` |
|         - | 4495 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4496 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4497 | `		return PH7_OK;` |
|         - | 4498 | `	}` |
|         - | 4499 | `	/* Create a new array */` |
|        19 | 4500 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4501 | `	if( pArray == 0 ){` |
|       ! 0 | 4502 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4503 | `		return PH7_OK;` |
|         - | 4504 | `	}` |
|         - | 4505 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4506 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4507 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4508 | `		/* Point to the internal representation of the source */` |
|        19 | 4509 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4510 | `		/* Perform the copy */` |
|        19 | 4511 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4512 | `	}else{` |
|         - | 4513 | `		/* Simple insertion */` |
|       ! 0 | 4514 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4515 | `	}` |
|         - | 4516 | `	/* Return the duplicated array */` |
|        19 | 4517 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4518 | `	return PH7_OK;` |
|        10 | 4519 | `}` |
|         - | 4520 | `/*` |
|         - | 4521 | ` * bool array_erase(array $source)` |
|         - | 4522 | ` *  Remove all elements from a given array.` |
|         - | 4523 | ` * Parameters` |
|         - | 4524 | ` *  $source` |
|         - | 4525 | ` *   Target array` |
|         - | 4526 | ` * Return` |
|         - | 4527 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4528 | ` * Note` |
|         - | 4529 | ` *  This function is a symisc eXtension.` |
|         - | 4530 | ` */` |
|        26 | 4531 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4532 | `{` |
|         - | 4533 | `	ph7_hashmap *pMap;` |
|        28 | 4534 | `	if( nArg < 1 ){` |
|         - | 4535 | `		/* Missing arguments */` |
|       ! 0 | 4536 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4537 | `		return PH7_OK;` |
|         - | 4538 | `	}` |
|         - | 4539 | `	/* Point to the target hashmap */` |
|        28 | 4540 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4541 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4542 | `	/* Erase */` |
|        28 | 4543 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4544 | `	return PH7_OK;` |
|        15 | 4545 | `}` |
|         - | 4546 | `/*` |
|         - | 4547 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4548 | ` *  Extract a slice of the array.` |
|         - | 4549 | ` * Parameters` |
|         - | 4550 | ` *  $array` |
|         - | 4551 | ` *    The input array.` |
|         - | 4552 | ` * $offset` |
|         - | 4553 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4554 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4555 | ` * $length (optional, nullable)` |
|         - | 4556 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4557 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4558 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4559 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4560 | ` * $preserve_keys (optional)` |
|         - | 4561 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4562 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4563 | ` * Return` |
|         - | 4564 | ` *   The new slice.` |
|         - | 4565 | ` */` |
|        66 | 4566 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4567 | `{` |
|         - | 4568 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4569 | `	ph7_hashmap_node *pCur;` |
|         - | 4570 | `	ph7_value *pArray;` |
|         - | 4571 | `	int iLength,iOfft;` |
|         - | 4572 | `	int bPreserve;` |
|         - | 4573 | `	sxi32 rc;` |
|        71 | 4574 | `	if( nArg < 2 ){` |
|       ! 0 | 4575 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4576 | `			"ArgumentCountError",` |
|         - | 4577 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4578 | `			nArg` |
|         - | 4579 | `			);` |
|         - | 4580 | `	}` |
|        71 | 4581 | `	if( nArg > 4 ){` |
|         4 | 4582 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4583 | `			"ArgumentCountError",` |
|         - | 4584 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4585 | `			nArg` |
|         - | 4586 | `			);` |
|         - | 4587 | `	}` |
|        69 | 4588 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4589 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4590 | `			"TypeError",` |
|         - | 4591 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4592 | `			ph7_type_name(apArg[0])` |
|         - | 4593 | `			);` |
|         - | 4594 | `	}` |
|         - | 4595 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        92 | 4596 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        95 | 4597 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4598 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4599 | `			"TypeError",` |
|         - | 4600 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4601 | `			ph7_type_name(apArg[1])` |
|         - | 4602 | `			);` |
|         - | 4603 | `	}` |
|         - | 4604 | `	/* Validate $length type if provided: nullable int */` |
|        65 | 4605 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        56 | 4606 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        56 | 4607 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4608 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4609 | `				"TypeError",` |
|         - | 4610 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4611 | `				ph7_type_name(apArg[2])` |
|         - | 4612 | `				);` |
|         - | 4613 | `		}` |
|        18 | 4614 | `	}` |
|         - | 4615 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        63 | 4616 | `	if( nArg > 3 ){` |
|         7 | 4617 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4618 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4619 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4620 | `				"TypeError",` |
|         - | 4621 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4622 | `				ph7_type_name(apArg[3])` |
|         - | 4623 | `				);` |
|         - | 4624 | `		}` |
|         2 | 4625 | `	}` |
|         - | 4626 | `	/* Point the internal representation of the target array */` |
|        63 | 4627 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        63 | 4628 | `	bPreserve = FALSE;` |
|         - | 4629 | `	/* Get the offset */` |
|         - | 4630 | `	{` |
|        63 | 4631 | `		sxi64 iTmp = 0;` |
|        63 | 4632 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        63 | 4633 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4634 | `			return rcArg;` |
|         - | 4635 | `		}` |
|        63 | 4636 | `		iOfft = (int)iTmp;` |
|         - | 4637 | `	}` |
|        63 | 4638 | `	if( iOfft < 0 ){` |
|         5 | 4639 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4640 | `		if( iOfft < 0 ){` |
|         3 | 4641 | `			iOfft = 0;` |
|         1 | 4642 | `		}` |
|         2 | 4643 | `	}` |
|        63 | 4644 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4645 | `		/* Offset past end of array, return empty array */` |
|         5 | 4646 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4647 | `		if( pArray == 0 ){` |
|       ! 0 | 4648 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4649 | `			return PH7_OK;` |
|         - | 4650 | `		}` |
|         5 | 4651 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4652 | `		return PH7_OK;` |
|         - | 4653 | `	}` |
|         - | 4654 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        59 | 4655 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        59 | 4656 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        37 | 4657 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        37 | 4658 | `		if( iLength < 0 ){` |
|         5 | 4659 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4660 | `		}` |
|        37 | 4661 | `		if( iLength < 0 ){` |
|         3 | 4662 | `			iLength = 0;` |
|         1 | 4663 | `		}` |
|        37 | 4664 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4665 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4666 | `		}` |
|        18 | 4667 | `	}` |
|        59 | 4668 | `	if( nArg > 3 ){` |
|         5 | 4669 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4670 | `	}` |
|         - | 4671 | `	/* Create a new array */` |
|        59 | 4672 | `	pArray = ph7_context_new_array(pCtx);` |
|        59 | 4673 | `	if( pArray == 0 ){` |
|       ! 0 | 4674 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4675 | `		return PH7_OK;` |
|         - | 4676 | `	}` |
|        59 | 4677 | `	if( iLength < 1 ){` |
|         - | 4678 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4679 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4680 | `		return PH7_OK;` |
|         - | 4681 | `	}` |
|         - | 4682 | `	/* Point to the desired entry */` |
|        55 | 4683 | `	pCur = pSrc->pFirst;` |
|        54 | 4684 | `	for(;;){` |
|       113 | 4685 | `		if( iOfft < 1 ){` |
|        55 | 4686 | `			break;` |
|         - | 4687 | `		}` |
|         - | 4688 | `		/* Point to the next entry */` |
|        63 | 4689 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        63 | 4690 | `		iOfft--;` |
|         5 | 4691 | `	}` |
|         - | 4692 | `	/* Point to the internal representation of the hashmap */` |
|        55 | 4693 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       106 | 4694 | `	for(;;){` |
|       217 | 4695 | `		if( iLength < 1 ){` |
|        55 | 4696 | `			break;` |
|         - | 4697 | `		}` |
|         - | 4698 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4699 | `		{` |
|       167 | 4700 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|       167 | 4701 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4702 | `		}` |
|       167 | 4703 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4704 | `			break;` |
|         - | 4705 | `		}` |
|         - | 4706 | `		/* Point to the next entry */` |
|       167 | 4707 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       167 | 4708 | `		iLength--;` |
|         5 | 4709 | `	}` |
|         - | 4710 | `	/* Return the freshly created array */` |
|        55 | 4711 | `	ph7_result_value(pCtx,pArray);` |
|        55 | 4712 | `	return PH7_OK;` |
|        38 | 4713 | `}` |
|         - | 4714 | `/*` |
|         - | 4715 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4716 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4717 | ` * beginning (becomes the new pFirst).` |
|         - | 4718 | ` */` |
|        38 | 4719 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4720 | `{` |
|         - | 4721 | `	ph7_hashmap_node *pNode;` |
|         - | 4722 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4723 | `	pNode = pMap->pLast;` |
|        39 | 4724 | `	if( pNode == 0 ){` |
|       ! 0 | 4725 | `		return;` |
|         - | 4726 | `	}` |
|        39 | 4727 | `	if( pNode->pNext == 0 ){` |
|         - | 4728 | `		/* Only node in the list, nothing to move */` |
|         5 | 4729 | `		return;` |
|         - | 4730 | `	}` |
|        35 | 4731 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4732 | `		/* Already in the correct position */` |
|         9 | 4733 | `		return;` |
|         - | 4734 | `	}` |
|         - | 4735 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4736 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4737 | `	pMap->pLast->pPrev = 0;` |
|         - | 4738 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4739 | `	if( pAfter == 0 ){` |
|         - | 4740 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4741 | `		pNode->pNext = 0;` |
|         3 | 4742 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4743 | `		if( pMap->pFirst ){` |
|         3 | 4744 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4745 | `		}` |
|         3 | 4746 | `		pMap->pFirst = pNode;` |
|         2 | 4747 | `	}else{` |
|        25 | 4748 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4749 | `		pNode->pPrev = pOldNext;` |
|        25 | 4750 | `		pNode->pNext = pAfter;` |
|        25 | 4751 | `		pAfter->pPrev = pNode;` |
|        25 | 4752 | `		if( pOldNext ){` |
|        25 | 4753 | `			pOldNext->pNext = pNode;` |
|        13 | 4754 | `		}else{` |
|       ! 0 | 4755 | `			pMap->pLast = pNode;` |
|         - | 4756 | `		}` |
|         - | 4757 | `	}` |
|        20 | 4758 | `}` |
|         - | 4759 | `/*` |
|         - | 4760 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4761 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4762 | ` * Parameters` |
|         - | 4763 | ` *  $array` |
|         - | 4764 | ` *    The input array.` |
|         - | 4765 | ` *  $offset` |
|         - | 4766 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4767 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4768 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4769 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4770 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4771 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4772 | ` *  $length (optional)` |
|         - | 4773 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4774 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4775 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4776 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4777 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4778 | ` *  $replacement (optional)` |
|         - | 4779 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4780 | ` *    with elements from this array.` |
|         - | 4781 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4782 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4783 | ` *    offset.` |
|         - | 4784 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4785 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4786 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4787 | ` * Return` |
|         - | 4788 | ` *   A new array consisting of the extracted elements.` |
|         - | 4789 | ` */` |
|        64 | 4790 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4791 | `{` |
|         - | 4792 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4793 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4794 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4795 | `	int iLength,iOfft,i;` |
|         - | 4796 | `	sxi32 rc;` |
|        66 | 4797 | `	if( nArg < 2 ){` |
|       ! 0 | 4798 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4799 | `			"ArgumentCountError",` |
|         - | 4800 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4801 | `			nArg` |
|         - | 4802 | `			);` |
|         - | 4803 | `	}` |
|        66 | 4804 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4805 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4806 | `			"TypeError",` |
|         - | 4807 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4808 | `			ph7_type_name(apArg[0])` |
|         - | 4809 | `			);` |
|         - | 4810 | `	}` |
|         - | 4811 | `	/* Point to the internal representation of the target array */` |
|        63 | 4812 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4813 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4814 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4815 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4816 | `	if( iOfft < 0 ){` |
|         9 | 4817 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4818 | `		if( iOfft < 0 ){` |
|         3 | 4819 | `			iOfft = 0;` |
|         2 | 4820 | `		}` |
|        59 | 4821 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4822 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4823 | `	}` |
|         - | 4824 | `	/* Get the length and clamp to valid range.` |
|         - | 4825 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4826 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4827 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4828 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4829 | `		if( iLength < 0 ){` |
|         7 | 4830 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4831 | `			if( iLength < 0 ){` |
|         3 | 4832 | `				iLength = 0;` |
|         1 | 4833 | `			}` |
|         3 | 4834 | `		}` |
|        45 | 4835 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4836 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4837 | `		}` |
|        22 | 4838 | `	}` |
|         - | 4839 | `	/* Create the result array for removed elements */` |
|        63 | 4840 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4841 | `	if( pArray == 0 ){` |
|       ! 0 | 4842 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4843 | `		return PH7_OK;` |
|         - | 4844 | `	}` |
|         - | 4845 | `	/* Get replacement array if provided */` |
|        63 | 4846 | `	pRep = 0;` |
|        63 | 4847 | `	if( nArg > 3 ){` |
|        27 | 4848 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4849 | `			/* Perform an array cast */` |
|         3 | 4850 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4851 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4852 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4853 | `			}` |
|         2 | 4854 | `		}else{` |
|        25 | 4855 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4856 | `		}` |
|        27 | 4857 | `		if( pRep ){` |
|         - | 4858 | `			/* Reset the loop cursor */` |
|        27 | 4859 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4860 | `		}` |
|        13 | 4861 | `	}` |
|         - | 4862 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4863 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4864 | `	/* Navigate to the offset position */` |
|        63 | 4865 | `	pCur = pSrc->pFirst;` |
|       131 | 4866 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4867 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4868 | `	}` |
|         - | 4869 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4870 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4871 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4872 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4873 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4874 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4875 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4876 | `		pPrev = pCur->pPrev;` |
|        79 | 4877 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4878 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4879 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4880 | `			break;` |
|         - | 4881 | `		}` |
|        79 | 4882 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4883 | `	}` |
|         - | 4884 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4885 | `	if( pRep ){` |
|         - | 4886 | `		ph7_value sSafeVal;` |
|        78 | 4887 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4888 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4889 | `			if( pRvalue ){` |
|         - | 4890 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4891 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4892 | `				 * since it points into that same pool. */` |
|        39 | 4893 | `				sSafeVal = *pRvalue;` |
|        39 | 4894 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4895 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4896 | `					pNewNode = pSrc->pLast;` |
|        39 | 4897 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4898 | `					pInsertAfter = pNewNode;` |
|        19 | 4899 | `				}` |
|        19 | 4900 | `			}` |
|         1 | 4901 | `		}` |
|        13 | 4902 | `	}` |
|         - | 4903 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4904 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4905 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4906 | `	 * and removals left gaps. */` |
|         - | 4907 | `	{` |
|        63 | 4908 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4909 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4910 | `		pSrc->iNextIdx = 0;` |
|       233 | 4911 | `		while( n > 0 ){` |
|       171 | 4912 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4913 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4914 | `			}` |
|       171 | 4915 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4916 | `			n--;` |
|         1 | 4917 | `		}` |
|        63 | 4918 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4919 | `	}` |
|         - | 4920 | `	/* Return the freshly created array */` |
|        63 | 4921 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4922 | `	return PH7_OK;` |
|        34 | 4923 | `}` |
|         - | 4924 | `/*` |
|         - | 4925 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4926 | ` *  Checks if a value exists in an array.` |
|         - | 4927 | ` * Parameters` |
|         - | 4928 | ` *  $needle` |
|         - | 4929 | ` *   The searched value.` |
|         - | 4930 | ` *   Note:` |
|         - | 4931 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4932 | ` * $haystack` |
|         - | 4933 | ` *  The target array.` |
|         - | 4934 | ` * $strict` |
|         - | 4935 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4936 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4937 | ` */` |
|     33068 | 4938 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4939 | `{` |
|         - | 4940 | `	ph7_value *pNeedle;` |
|         - | 4941 | `	int bStrict;` |
|         - | 4942 | `	int rc;` |
|     33073 | 4943 | `	if( nArg < 2 ){` |
|         - | 4944 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4945 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4946 | `		return PH7_OK;` |
|         - | 4947 | `	}` |
|     33073 | 4948 | `	pNeedle = apArg[0];` |
|     33073 | 4949 | `	bStrict = 0;` |
|     33073 | 4950 | `	if( nArg > 2 ){` |
|        53 | 4951 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4952 | `	}` |
|     33073 | 4953 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4954 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4955 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4956 | `		/* Set the comparison result */` |
|       ! 0 | 4957 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4958 | `		return PH7_OK;` |
|         - | 4959 | `	}` |
|         - | 4960 | `	/* Perform the lookup */` |
|     33073 | 4961 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4962 | `	/* Lookup result */` |
|     33073 | 4963 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33073 | 4964 | `	return PH7_OK;` |
|     16539 | 4965 | `}` |
|         - | 4966 | `/*` |
|         - | 4967 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4968 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4969 | ` * Parameters` |
|         - | 4970 | ` * $needle` |
|         - | 4971 | ` *   The searched value.` |
|         - | 4972 | ` * $haystack` |
|         - | 4973 | ` *   The array.` |
|         - | 4974 | ` * $strict` |
|         - | 4975 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4976 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4977 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4978 | ` * Return` |
|         - | 4979 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4980 | ` */` |
|        26 | 4981 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4982 | `{` |
|         - | 4983 | `	ph7_hashmap_node *pEntry;` |
|         - | 4984 | `	ph7_value *pVal,sNeedle;` |
|         - | 4985 | `	ph7_hashmap *pMap;` |
|         - | 4986 | `	ph7_value sVal;` |
|         - | 4987 | `	int bStrict;` |
|         - | 4988 | `	sxu32 n;` |
|         - | 4989 | `	int rc;` |
|        28 | 4990 | `	if( nArg < 2 ){` |
|         - | 4991 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4992 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4993 | `			"ArgumentCountError",` |
|         - | 4994 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 4995 | `			nArg` |
|         - | 4996 | `			);` |
|         - | 4997 | `	}` |
|        28 | 4998 | `	bStrict = FALSE;` |
|        28 | 4999 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 5000 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 5001 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5002 | `			"TypeError",` |
|         - | 5003 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 5004 | `			ph7_type_name(apArg[1])` |
|         - | 5005 | `			);` |
|         - | 5006 | `	}` |
|        25 | 5007 | `	if( nArg > 2 ){` |
|         - | 5008 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        11 | 5009 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 5010 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5011 | `				"TypeError",` |
|         - | 5012 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 5013 | `				ph7_type_name(apArg[2])` |
|         - | 5014 | `				);` |
|         - | 5015 | `		}` |
|        11 | 5016 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 5017 | `	}` |
|         - | 5018 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 5019 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 5020 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 5021 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 5022 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 5023 | `	pEntry = pMap->pFirst;` |
|        25 | 5024 | `	n = pMap->nEntry;` |
|        28 | 5025 | `	for(;;){` |
|        57 | 5026 | `		if( !n ){` |
|         9 | 5027 | `			break;` |
|         - | 5028 | `		}` |
|         - | 5029 | `		/* Extract node value */` |
|        49 | 5030 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5031 | `		if( pVal ){` |
|         - | 5032 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 5033 | `			 * can change their type.` |
|         - | 5034 | `			 */` |
|        49 | 5035 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 5036 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 5037 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 5038 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 5039 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 5040 | `			if( rc == 0 ){` |
|         - | 5041 | `				/* Match found,return key */` |
|        17 | 5042 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 5043 | `					/* INT key */` |
|        11 | 5044 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 5045 | `				}else{` |
|         7 | 5046 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5047 | `					/* Blob key */` |
|         7 | 5048 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 5049 | `				}` |
|        17 | 5050 | `				return PH7_OK;` |
|         - | 5051 | `			}` |
|        16 | 5052 | `		}` |
|         - | 5053 | `		/* Point to the next entry */` |
|        33 | 5054 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5055 | `		n--;` |
|         1 | 5056 | `	}` |
|         - | 5057 | `	/* No such value,return FALSE */` |
|         9 | 5058 | `	ph7_result_bool(pCtx,0);` |
|         9 | 5059 | `	return PH7_OK;` |
|        15 | 5060 | `}` |
|         - | 5061 | `/*` |
|         - | 5062 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 5063 | ` *  Computes the difference of arrays.` |
|         - | 5064 | ` * Parameters` |
|         - | 5065 | ` *  $array1` |
|         - | 5066 | ` *    The array to compare from` |
|         - | 5067 | ` *  $array2` |
|         - | 5068 | ` *    An array to compare against` |
|         - | 5069 | ` *  $...` |
|         - | 5070 | ` *   More arrays to compare against` |
|         - | 5071 | ` * Return` |
|         - | 5072 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5073 | ` *  are not present in any of the other arrays.` |
|         - | 5074 | ` */` |
|        20 | 5075 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5076 | `{` |
|         - | 5077 | `	ph7_hashmap_node *pEntry;` |
|         - | 5078 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5079 | `	ph7_value *pArray;` |
|         - | 5080 | `	ph7_value *pVal;` |
|         - | 5081 | `	sxi32 rc;` |
|         - | 5082 | `	sxu32 n;` |
|         - | 5083 | `	int i;` |
|         - | 5084 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5085 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5086 | `	 * debugging difficult. */` |
|        23 | 5087 | `	if( nArg < 1 ){` |
|       ! 0 | 5088 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5089 | `			"ArgumentCountError",` |
|         - | 5090 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5091 | `			nArg` |
|         - | 5092 | `			);` |
|         - | 5093 | `	}` |
|        23 | 5094 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5095 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5096 | `			"TypeError",` |
|         - | 5097 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5098 | `			ph7_type_name(apArg[0])` |
|         - | 5099 | `			);` |
|         - | 5100 | `	}` |
|        36 | 5101 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5102 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5103 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5104 | `				"TypeError",` |
|         - | 5105 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5106 | `				i + 1,` |
|         2 | 5107 | `				ph7_type_name(apArg[i])` |
|         - | 5108 | `				);` |
|         - | 5109 | `		}` |
|         9 | 5110 | `	}` |
|        17 | 5111 | `	if( nArg == 1 ){` |
|         - | 5112 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5113 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5114 | `		return PH7_OK;` |
|         - | 5115 | `	}` |
|         - | 5116 | `	/* Create a new array */` |
|        15 | 5117 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5118 | `	if( pArray == 0 ){` |
|       ! 0 | 5119 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5120 | `		return PH7_OK;` |
|         - | 5121 | `	}` |
|         - | 5122 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5123 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5124 | `	/* Perform the diff */` |
|        15 | 5125 | `	pEntry = pSrc->pFirst;` |
|        15 | 5126 | `	n = pSrc->nEntry;` |
|        27 | 5127 | `	for(;;){` |
|        55 | 5128 | `		if( n < 1 ){` |
|        15 | 5129 | `			break;` |
|         - | 5130 | `		}` |
|         - | 5131 | `		/* Extract the node value */` |
|        41 | 5132 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5133 | `		if( pVal ){` |
|        69 | 5134 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5135 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5136 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5137 | `				/* Perform the lookup */` |
|        45 | 5138 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5139 | `				if( rc == SXRET_OK ){` |
|         - | 5140 | `					/* Value exist */` |
|        17 | 5141 | `					break;` |
|         - | 5142 | `				}` |
|        15 | 5143 | `			}` |
|        41 | 5144 | `			if( i >= nArg ){` |
|         - | 5145 | `				/* Perform the insertion */` |
|        25 | 5146 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5147 | `			}` |
|        20 | 5148 | `		}` |
|         - | 5149 | `		/* Point to the next entry */` |
|        41 | 5150 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5151 | `		n--;` |
|         1 | 5152 | `	}` |
|         - | 5153 | `	/* Return the freshly created array */` |
|        15 | 5154 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5155 | `	return PH7_OK;` |
|        13 | 5156 | `}` |
|         - | 5157 | `/*` |
|         - | 5158 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5159 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5160 | ` * Parameters` |
|         - | 5161 | ` *  $array1` |
|         - | 5162 | ` *    The array to compare from` |
|         - | 5163 | ` *  $array2` |
|         - | 5164 | ` *    An array to compare against` |
|         - | 5165 | ` *  $...` |
|         - | 5166 | ` *   More arrays to compare against.` |
|         - | 5167 | ` * $callback` |
|         - | 5168 | ` *  The callback comparison function.` |
|         - | 5169 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5170 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5171 | ` *  than the second.` |
|         - | 5172 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5173 | ` * Return` |
|         - | 5174 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5175 | ` *  are not present in any of the other arrays.` |
|         - | 5176 | ` */` |
|        20 | 5177 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5178 | `{` |
|         - | 5179 | `	ph7_hashmap_node *pEntry;` |
|         - | 5180 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5181 | `	ph7_value *pCallback;` |
|         - | 5182 | `	ph7_value *pArray;` |
|         - | 5183 | `	ph7_value *pVal;` |
|         - | 5184 | `	sxi32 rc;` |
|         - | 5185 | `	sxu32 n;` |
|         - | 5186 | `	int i;` |
|         - | 5187 |  |
|         - | 5188 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5189 | `	if( nArg < 2 ){` |
|       ! 0 | 5190 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5191 | `			"ArgumentCountError",` |
|         - | 5192 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5193 | `			nArg` |
|         - | 5194 | `			);` |
|         - | 5195 | `	}` |
|        25 | 5196 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5197 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5198 | `			"TypeError",` |
|         - | 5199 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5200 | `			ph7_type_name(apArg[0])` |
|         - | 5201 | `			);` |
|         - | 5202 | `	}` |
|         - | 5203 |  |
|        23 | 5204 | `	if( nArg == 2 ){` |
|         - | 5205 | `		/* Only the original array and the callback were provided. */` |
|         - | 5206 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5207 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5208 | `		 * validation order.` |
|         - | 5209 | `		 */` |
|         4 | 5210 | `	} else {` |
|         - | 5211 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5212 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5213 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5214 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5215 | `					"TypeError",` |
|         - | 5216 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5217 | `					i + 1,` |
|         6 | 5218 | `					ph7_type_name(apArg[i])` |
|         - | 5219 | `					);` |
|         - | 5220 | `			}` |
|         7 | 5221 | `		}` |
|         - | 5222 | `	}` |
|         - | 5223 |  |
|         - | 5224 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5225 | `	pCallback = apArg[nArg - 1];` |
|         - | 5226 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5227 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5228 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5229 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5230 | `				"TypeError",` |
|         - | 5231 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5232 | `				nArg` |
|         - | 5233 | `				);` |
|         - | 5234 | `		}` |
|         6 | 5235 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5236 | `			int len;` |
|         3 | 5237 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5238 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5239 | `				"TypeError",` |
|         - | 5240 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5241 | `				nArg,` |
|         1 | 5242 | `				zName` |
|         - | 5243 | `				);` |
|         - | 5244 | `		}` |
|         4 | 5245 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5246 | `			"TypeError",` |
|         - | 5247 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5248 | `			nArg` |
|         - | 5249 | `			);` |
|         - | 5250 | `	}` |
|         - | 5251 |  |
|         7 | 5252 | `	if( nArg == 2 ){` |
|         - | 5253 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5254 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5255 | `		return PH7_OK;` |
|         - | 5256 | `	}` |
|         - | 5257 |  |
|         - | 5258 | `	/* Create a new array */` |
|         5 | 5259 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5260 | `	if( pArray == 0 ){` |
|       ! 0 | 5261 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5262 | `		return PH7_OK;` |
|         - | 5263 | `	}` |
|         - | 5264 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5265 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5266 | `	/* Perform the diff */` |
|         5 | 5267 | `	pEntry = pSrc->pFirst;` |
|         5 | 5268 | `	n = pSrc->nEntry;` |
|         5 | 5269 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5270 | `	for(;;){` |
|        11 | 5271 | `		if( n < 1 ){` |
|         3 | 5272 | `			break;` |
|         - | 5273 | `		}` |
|         - | 5274 | `		/* Extract the node value */` |
|         9 | 5275 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5276 | `		if( pVal ){` |
|        15 | 5277 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5278 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5279 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5280 | `				/* Perform the lookup */` |
|         9 | 5281 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5282 | `				if( rc == SXRET_OK ){` |
|         - | 5283 | `					/* Value exist */` |
|         3 | 5284 | `					break;` |
|         - | 5285 | `				}` |
|         4 | 5286 | `			}` |
|         9 | 5287 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5288 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5289 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5290 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5291 | `				return PH7_EXCEPTION;` |
|         - | 5292 | `			}` |
|         7 | 5293 | `			if( i >= (nArg - 1)){` |
|         - | 5294 | `				/* Perform the insertion */` |
|         5 | 5295 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5296 | `			}` |
|         3 | 5297 | `		}` |
|         - | 5298 | `		/* Point to the next entry */` |
|         7 | 5299 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5300 | `		n--;` |
|         1 | 5301 | `	}` |
|         - | 5302 | `	/* Return the freshly created array */` |
|         3 | 5303 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5304 | `	return PH7_OK;` |
|        15 | 5305 | `}` |
|         - | 5306 | `/*` |
|         - | 5307 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5308 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5309 | ` * Parameters` |
|         - | 5310 | ` *  $array1` |
|         - | 5311 | ` *    The array to compare from` |
|         - | 5312 | ` *  $array2` |
|         - | 5313 | ` *    An array to compare against` |
|         - | 5314 | ` *  $...` |
|         - | 5315 | ` *   More arrays to compare against` |
|         - | 5316 | ` * Return` |
|         - | 5317 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5318 | ` *  are not present in any of the other arrays.` |
|         - | 5319 | ` */` |
|        20 | 5320 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5321 | `{` |
|         - | 5322 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5323 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5324 | `	ph7_value *pArray;` |
|         - | 5325 | `	ph7_value *pVal;` |
|         - | 5326 | `	sxi32 rc;` |
|         - | 5327 | `	sxu32 n;` |
|         - | 5328 | `	int i;` |
|         - | 5329 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5330 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5331 | `	 * accompanying integration tests to pass. */` |
|        24 | 5332 | `	if( nArg < 1 ){` |
|       ! 0 | 5333 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5334 | `			"ArgumentCountError",` |
|         - | 5335 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5336 | `			nArg` |
|         - | 5337 | `			);` |
|         - | 5338 | `	}` |
|        24 | 5339 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5340 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5341 | `			"TypeError",` |
|         - | 5342 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5343 | `			ph7_type_name(apArg[0])` |
|         - | 5344 | `			);` |
|         - | 5345 | `	}` |
|        37 | 5346 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5347 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5348 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5349 | `				"TypeError",` |
|         - | 5350 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5351 | `				i + 1,` |
|         4 | 5352 | `				ph7_type_name(apArg[i])` |
|         - | 5353 | `				);` |
|         - | 5354 | `		}` |
|        10 | 5355 | `	}` |
|        15 | 5356 | `	if( nArg == 1 ){` |
|         - | 5357 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5358 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5359 | `		return PH7_OK;` |
|         - | 5360 | `	}` |
|         - | 5361 | `	/* Create a new array */` |
|        13 | 5362 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5363 | `	if( pArray == 0 ){` |
|       ! 0 | 5364 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5365 | `		return PH7_OK;` |
|         - | 5366 | `	}` |
|         - | 5367 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5368 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5369 | `	/* Perform the diff */` |
|        13 | 5370 | `	pEntry = pSrc->pFirst;` |
|        13 | 5371 | `	n = pSrc->nEntry;` |
|        13 | 5372 | `	pN1 = pN2 = 0;` |
|        34 | 5373 | `	for(;;){` |
|         - | 5374 | `		int keep;` |
|        41 | 5375 | `		if( n < 1 ){` |
|        13 | 5376 | `			break;` |
|         - | 5377 | `		}` |
|         - | 5378 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5379 | `		keep = 1;` |
|        47 | 5380 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5381 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5382 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5383 | `			/* Perform a key lookup first */` |
|        33 | 5384 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5385 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5386 | `			}else{` |
|        21 | 5387 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5388 | `			}` |
|        33 | 5389 | `			if( rc != SXRET_OK ){` |
|         - | 5390 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5391 | `				continue;` |
|         - | 5392 | `			}` |
|         - | 5393 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5394 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5395 | `			if( pVal ){` |
|         - | 5396 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5397 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5398 | `				if( pVal2 ){` |
|         - | 5399 | `					ph7_value sV1,sV2;` |
|         - | 5400 | `					sxi32 cmp;` |
|         - | 5401 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5402 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5403 | `					 * null element used to come back bool(false) in the` |
|         - | 5404 | `					 * caller's array). */` |
|        17 | 5405 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5406 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5407 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5408 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5409 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5410 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5411 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5412 | `					if( cmp == 0 ){` |
|         - | 5413 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5414 | `						keep = 0;` |
|        15 | 5415 | `						break;` |
|         - | 5416 | `					}` |
|         1 | 5417 | `				}` |
|         1 | 5418 | `			}` |
|         2 | 5419 | `		}` |
|        29 | 5420 | `		if( keep ){` |
|         - | 5421 | `			/* Perform the insertion */` |
|        15 | 5422 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5423 | `		}` |
|         - | 5424 | `		/* Point to the next entry */` |
|        29 | 5425 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5426 | `		n--;` |
|         1 | 5427 | `	}` |
|         - | 5428 | `	/* Return the freshly created array */` |
|        13 | 5429 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5430 | `	return PH7_OK;` |
|        14 | 5431 | `}` |
|         - | 5432 | `/*` |
|         - | 5433 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5434 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5435 | ` *  by a user supplied callback function.` |
|         - | 5436 | ` * Parameters` |
|         - | 5437 | ` *  $array1` |
|         - | 5438 | ` *    The array to compare from` |
|         - | 5439 | ` *  $array2` |
|         - | 5440 | ` *    An array to compare against` |
|         - | 5441 | ` *  $...` |
|         - | 5442 | ` *   More arrays to compare against.` |
|         - | 5443 | ` *  $key_compare_func` |
|         - | 5444 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5445 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5446 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5447 | ` * Return` |
|         - | 5448 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5449 | ` *  are not present in any of the other arrays.` |
|         - | 5450 | ` */` |
|        22 | 5451 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5452 | `{` |
|         - | 5453 | `	ph7_hashmap_node *pEntry;` |
|         - | 5454 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5455 | `	ph7_value *pCallback;` |
|         - | 5456 | `	ph7_value *pArray;` |
|         - | 5457 | `	sxi32 rc;` |
|         - | 5458 | `	sxu32 n;` |
|         - | 5459 | `	int i;` |
|         - | 5460 |  |
|         - | 5461 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5462 | `	if( nArg < 2 ){` |
|       ! 0 | 5463 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5464 | `			"ArgumentCountError",` |
|         - | 5465 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5466 | `			nArg` |
|         - | 5467 | `			);` |
|         - | 5468 | `	}` |
|        26 | 5469 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5470 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5471 | `			"TypeError",` |
|         - | 5472 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5473 | `			ph7_type_name(apArg[0])` |
|         - | 5474 | `			);` |
|         - | 5475 | `	}` |
|         - | 5476 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5477 | `	 * expected to be a callback. */` |
|        38 | 5478 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5479 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5480 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5481 | `				"TypeError",` |
|         - | 5482 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5483 | `				i + 1,` |
|         2 | 5484 | `				ph7_type_name(apArg[i])` |
|         - | 5485 | `				);` |
|         - | 5486 | `		}` |
|         9 | 5487 | `	}` |
|         - | 5488 | `	/* Point to the callback value */` |
|        22 | 5489 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5490 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5491 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5492 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5493 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5494 | `		 * string given" which we also reproduce. */` |
|         9 | 5495 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5496 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5497 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5498 | `				"TypeError",` |
|         - | 5499 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5500 | `				nArg` |
|         - | 5501 | `				);` |
|         - | 5502 | `		}` |
|         6 | 5503 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5504 | `			/* neither array nor string */` |
|         8 | 5505 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5506 | `				"TypeError",` |
|         - | 5507 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5508 | `				nArg` |
|         - | 5509 | `				);` |
|         - | 5510 | `		}` |
|         - | 5511 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5512 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5513 | `			"TypeError",` |
|         - | 5514 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5515 | `			nArg,` |
|       ! 0 | 5516 | `			ph7_type_name(pCallback)` |
|         - | 5517 | `			);` |
|         - | 5518 | `	}` |
|        13 | 5519 | `	if( nArg == 2 ){` |
|         - | 5520 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5521 | `		 * input array. */` |
|         3 | 5522 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5523 | `		return PH7_OK;` |
|         - | 5524 | `	}` |
|         - | 5525 | `	/* Create a new array */` |
|        11 | 5526 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5527 | `	if( pArray == 0 ){` |
|       ! 0 | 5528 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5529 | `		return PH7_OK;` |
|         - | 5530 | `	}` |
|         - | 5531 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5532 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5533 | `	/* Perform the diff */` |
|        11 | 5534 | `	pEntry = pSrc->pFirst;` |
|        11 | 5535 | `	n = pSrc->nEntry;` |
|        21 | 5536 | `	for(;;){` |
|         - | 5537 | `		int keep;` |
|        27 | 5538 | `		if( n < 1 ){` |
|         9 | 5539 | `			break;` |
|         - | 5540 | `		}` |
|        19 | 5541 | `		keep = 1;` |
|        31 | 5542 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5543 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5544 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5545 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5546 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5547 | `			while( pIt ){` |
|         - | 5548 | `				/* build temporary key values for callback */` |
|         - | 5549 | `				ph7_value key1, key2, result;` |
|         - | 5550 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5551 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5552 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5553 | `				}else{` |
|         - | 5554 | `					SyString sStr;` |
|        33 | 5555 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5556 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5557 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5558 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5559 | `				}` |
|        33 | 5560 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5561 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5562 | `				}else{` |
|         - | 5563 | `					SyString sStr;` |
|        33 | 5564 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5565 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5566 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5567 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5568 | `				}` |
|        33 | 5569 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5570 | `				/* call user callback with (key1, key2) */` |
|         - | 5571 | `				{` |
|         - | 5572 | `					ph7_value *apK[2];` |
|        33 | 5573 | `					apK[0] = &key1;` |
|        33 | 5574 | `					apK[1] = &key2;` |
|        33 | 5575 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5576 | `				}` |
|        33 | 5577 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5578 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5579 | `					 * array_uintersect (which signal back from` |
|         - | 5580 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5581 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5582 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5583 | `					PH7_MemObjRelease(&result);` |
|         3 | 5584 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5585 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5586 | `					return PH7_EXCEPTION;` |
|         - | 5587 | `				}` |
|        31 | 5588 | `				if( rc == SXRET_OK ){` |
|        31 | 5589 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5590 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5591 | `					}` |
|        31 | 5592 | `					if( result.x.iVal == 0 ){` |
|         - | 5593 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5594 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5595 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5596 | `						if( pVal1 && pVal2 ){` |
|         - | 5597 | `							ph7_value sV1,sV2;` |
|         - | 5598 | `							sxi32 cmp;` |
|         - | 5599 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5600 | `							 * place and these are LIVE array elements. */` |
|        13 | 5601 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5602 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5603 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5604 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5605 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5606 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5607 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5608 | `							if( cmp == 0 ){` |
|         9 | 5609 | `								keep = 0;` |
|         9 | 5610 | `								PH7_MemObjRelease(&result);` |
|         - | 5611 | `								/* release keys too before breaking */` |
|         9 | 5612 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5613 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5614 | `								break;` |
|         - | 5615 | `							}` |
|         2 | 5616 | `						}` |
|         2 | 5617 | `					}` |
|        11 | 5618 | `				}` |
|        23 | 5619 | `				PH7_MemObjRelease(&result);` |
|        23 | 5620 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5621 | `				PH7_MemObjRelease(&key2);` |
|         - | 5622 | `				/* move to next node */` |
|        23 | 5623 | `				pIt = pIt->pPrev;` |
|        23 | 5624 | `				if( keep == 0 ) break;` |
|         1 | 5625 | `			}` |
|        21 | 5626 | `			if( keep == 0 ) break;` |
|         7 | 5627 | `		}` |
|        17 | 5628 | `		if( keep ){` |
|         - | 5629 | `			/* Perform the insertion */` |
|         9 | 5630 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5631 | `		}` |
|         - | 5632 | `		/* Point to the next entry */` |
|        17 | 5633 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5634 | `		n--;` |
|         1 | 5635 | `	}` |
|         - | 5636 | `	/* Return the freshly created array */` |
|         9 | 5637 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5638 | `	return PH7_OK;` |
|        15 | 5639 | `}` |
|         - | 5640 | `/*` |
|         - | 5641 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5642 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5643 | ` * Parameters` |
|         - | 5644 | ` *  $array1` |
|         - | 5645 | ` *    The array to compare from` |
|         - | 5646 | ` *  $array2` |
|         - | 5647 | ` *    An array to compare against` |
|         - | 5648 | ` *  $...` |
|         - | 5649 | ` *   More arrays to compare against` |
|         - | 5650 | ` * Return` |
|         - | 5651 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5652 | ` *  in any of the other arrays.` |
|         - | 5653 | ` * Note that NULL is returned on failure.` |
|         - | 5654 | ` */` |
|        12 | 5655 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5656 | `{` |
|         - | 5657 | `	ph7_hashmap_node *pEntry;` |
|         - | 5658 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5659 | `	ph7_value *pArray;` |
|         - | 5660 | `	sxi32 rc;` |
|         - | 5661 | `	sxu32 n;` |
|         - | 5662 | `	int i;` |
|         - | 5663 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5664 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5665 | `	 * helpers. */` |
|        15 | 5666 | `	if( nArg < 1 ){` |
|       ! 0 | 5667 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5668 | `			"ArgumentCountError",` |
|         - | 5669 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5670 | `			nArg` |
|         - | 5671 | `			);` |
|         - | 5672 | `	}` |
|        15 | 5673 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5674 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5675 | `			"TypeError",` |
|         - | 5676 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5677 | `			ph7_type_name(apArg[0])` |
|         - | 5678 | `			);` |
|         - | 5679 | `	}` |
|        20 | 5680 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5681 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5682 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5683 | `				"TypeError",` |
|         - | 5684 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5685 | `				i + 1,` |
|         2 | 5686 | `				ph7_type_name(apArg[i])` |
|         - | 5687 | `				);` |
|         - | 5688 | `		}` |
|         5 | 5689 | `	}` |
|         9 | 5690 | `	if( nArg == 1 ){` |
|         - | 5691 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5692 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5693 | `		return PH7_OK;` |
|         - | 5694 | `	}` |
|         - | 5695 | `	/* Create a new array */` |
|         7 | 5696 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5697 | `	if( pArray == 0 ){` |
|       ! 0 | 5698 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5699 | `		return PH7_OK;` |
|         - | 5700 | `	}` |
|         - | 5701 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5702 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5703 | `	/* Perfrom the diff */` |
|         7 | 5704 | `	pEntry = pSrc->pFirst;` |
|         7 | 5705 | `	n = pSrc->nEntry;` |
|        12 | 5706 | `	for(;;){` |
|        25 | 5707 | `		if( n < 1 ){` |
|         7 | 5708 | `			break;` |
|         - | 5709 | `		}` |
|        31 | 5710 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5711 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5712 | `				/* ignore */` |
|       ! 0 | 5713 | `				continue;` |
|         - | 5714 | `			}` |
|        23 | 5715 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5716 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5717 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5718 | `				/* Blob lookup */` |
|        17 | 5719 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5720 | `			}else{` |
|         - | 5721 | `				/* Int lookup */` |
|         7 | 5722 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5723 | `			}` |
|        23 | 5724 | `			if( rc == SXRET_OK ){` |
|         - | 5725 | `				/* Key exists,break immediately */` |
|        11 | 5726 | `				break;` |
|         - | 5727 | `			}` |
|         7 | 5728 | `		}` |
|        19 | 5729 | `		if( i >= nArg ){` |
|         - | 5730 | `			/* Perform the insertion */` |
|         9 | 5731 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5732 | `		}` |
|         - | 5733 | `		/* Point to the next entry */` |
|        19 | 5734 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5735 | `		n--;` |
|         1 | 5736 | `	}` |
|         - | 5737 | `	/* Return the freshly created array */` |
|         7 | 5738 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5739 | `	return PH7_OK;` |
|         9 | 5740 | `}` |
|         - | 5741 | `/*` |
|         - | 5742 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5743 | ` *  Computes the intersection of arrays.` |
|         - | 5744 | ` * Parameters` |
|         - | 5745 | ` *  $array1` |
|         - | 5746 | ` *    The array to compare from` |
|         - | 5747 | ` *  $array2` |
|         - | 5748 | ` *    An array to compare against` |
|         - | 5749 | ` *  $...` |
|         - | 5750 | ` *   More arrays to compare against` |
|         - | 5751 | ` * Return` |
|         - | 5752 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5753 | ` *  in all of the parameters.` |
|         - | 5754 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5755 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5756 | ` */` |
|        20 | 5757 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5758 | `{` |
|         - | 5759 | `	ph7_hashmap_node *pEntry;` |
|         - | 5760 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5761 | `	ph7_value *pArray;` |
|         - | 5762 | `	ph7_value *pVal;` |
|         - | 5763 | `	sxi32 rc;` |
|         - | 5764 | `	sxu32 n;` |
|         - | 5765 | `	int i;` |
|        23 | 5766 | `	if( nArg < 1 ){` |
|       ! 0 | 5767 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5768 | `			"ArgumentCountError",` |
|         - | 5769 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5770 | `			nArg` |
|         - | 5771 | `			);` |
|         - | 5772 | `	}` |
|        23 | 5773 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5774 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5775 | `			"TypeError",` |
|         - | 5776 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5777 | `			ph7_type_name(apArg[0])` |
|         - | 5778 | `			);` |
|         - | 5779 | `	}` |
|        36 | 5780 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5781 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5782 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5783 | `				"TypeError",` |
|         - | 5784 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5785 | `				i + 1,` |
|         2 | 5786 | `				ph7_type_name(apArg[i])` |
|         - | 5787 | `				);` |
|         - | 5788 | `		}` |
|         9 | 5789 | `	}` |
|        17 | 5790 | `	if( nArg == 1 ){` |
|         - | 5791 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5792 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5793 | `		return PH7_OK;` |
|         - | 5794 | `	}` |
|         - | 5795 | `	/* Create a new array */` |
|        15 | 5796 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5797 | `	if( pArray == 0 ){` |
|       ! 0 | 5798 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5799 | `		return PH7_OK;` |
|         - | 5800 | `	}` |
|         - | 5801 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5802 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5803 | `	/* Perform the intersection */` |
|        15 | 5804 | `	pEntry = pSrc->pFirst;` |
|        15 | 5805 | `	n = pSrc->nEntry;` |
|        31 | 5806 | `	for(;;){` |
|        63 | 5807 | `		if( n < 1 ){` |
|        15 | 5808 | `			break;` |
|         - | 5809 | `		}` |
|         - | 5810 | `		/* Extract the node value */` |
|        49 | 5811 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5812 | `		if( pVal ){` |
|        79 | 5813 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5814 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5815 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5816 | `				/* Perform the lookup */` |
|        55 | 5817 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5818 | `				if( rc != SXRET_OK ){` |
|         - | 5819 | `					/* Value does not exist */` |
|        25 | 5820 | `					break;` |
|         - | 5821 | `				}` |
|        16 | 5822 | `			}` |
|        49 | 5823 | `			if( i >= nArg ){` |
|         - | 5824 | `				/* Perform the insertion */` |
|        25 | 5825 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5826 | `			}` |
|        24 | 5827 | `		}` |
|         - | 5828 | `		/* Point to the next entry */` |
|        49 | 5829 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5830 | `		n--;` |
|         1 | 5831 | `	}` |
|         - | 5832 | `	/* Return the freshly created array */` |
|        15 | 5833 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5834 | `	return PH7_OK;` |
|        13 | 5835 | `}` |
|         - | 5836 | `/*` |
|         - | 5837 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5838 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5839 | ` * Parameters` |
|         - | 5840 | ` *  $array1` |
|         - | 5841 | ` *    The array to compare from` |
|         - | 5842 | ` *  $array2` |
|         - | 5843 | ` *    An array to compare against` |
|         - | 5844 | ` *  $...` |
|         - | 5845 | ` *   More arrays to compare against` |
|         - | 5846 | ` * Return` |
|         - | 5847 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5848 | ` *  in all the arguments, with matching keys.` |
|         - | 5849 | ` */` |
|        20 | 5850 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5851 | `{` |
|         - | 5852 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5853 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5854 | `	ph7_value *pArray;` |
|         - | 5855 | `	ph7_value *pVal;` |
|         - | 5856 | `	sxi32 rc;` |
|         - | 5857 | `	sxu32 n;` |
|         - | 5858 | `	int i;` |
|        23 | 5859 | `	if( nArg < 1 ){` |
|       ! 0 | 5860 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5861 | `			"ArgumentCountError",` |
|         - | 5862 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5863 | `			nArg` |
|         - | 5864 | `			);` |
|         - | 5865 | `	}` |
|        23 | 5866 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5867 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5868 | `			"TypeError",` |
|         - | 5869 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5870 | `			ph7_type_name(apArg[0])` |
|         - | 5871 | `			);` |
|         - | 5872 | `	}` |
|        36 | 5873 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5874 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5875 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5876 | `				"TypeError",` |
|         - | 5877 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5878 | `				i + 1,` |
|         2 | 5879 | `				ph7_type_name(apArg[i])` |
|         - | 5880 | `				);` |
|         - | 5881 | `		}` |
|         9 | 5882 | `	}` |
|        17 | 5883 | `	if( nArg == 1 ){` |
|         - | 5884 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5885 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5886 | `		return PH7_OK;` |
|         - | 5887 | `	}` |
|         - | 5888 | `	/* Create a new array */` |
|        15 | 5889 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5890 | `	if( pArray == 0 ){` |
|       ! 0 | 5891 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5892 | `		return PH7_OK;` |
|         - | 5893 | `	}` |
|         - | 5894 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5895 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5896 | `	/* Perform the intersection */` |
|        15 | 5897 | `	pEntry = pSrc->pFirst;` |
|        15 | 5898 | `	n = pSrc->nEntry;` |
|        15 | 5899 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5900 | `	for(;;){` |
|        47 | 5901 | `		if( n < 1 ){` |
|        15 | 5902 | `			break;` |
|         - | 5903 | `		}` |
|         - | 5904 | `		/* Extract the node value */` |
|        33 | 5905 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5906 | `		if( pVal ){` |
|        53 | 5907 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5908 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5909 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5910 | `				/* Perform a key lookup first */` |
|        37 | 5911 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5912 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5913 | `				}else{` |
|        23 | 5914 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5915 | `				}` |
|        37 | 5916 | `				if( rc != SXRET_OK ){` |
|         - | 5917 | `					/* No such key,break immediately */` |
|         7 | 5918 | `					break;` |
|         - | 5919 | `				}` |
|         - | 5920 | `				/* Perform the lookup */` |
|        31 | 5921 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5922 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5923 | `					/* Value does not exist */` |
|         6 | 5924 | `					break;` |
|         - | 5925 | `				}` |
|        11 | 5926 | `			}` |
|        33 | 5927 | `			if( i >= nArg ){` |
|         - | 5928 | `				/* Perform the insertion */` |
|        17 | 5929 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5930 | `			}` |
|        16 | 5931 | `		}` |
|         - | 5932 | `		/* Point to the next entry */` |
|        33 | 5933 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5934 | `		n--;` |
|         1 | 5935 | `	}` |
|         - | 5936 | `	/* Return the freshly created array */` |
|        15 | 5937 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5938 | `	return PH7_OK;` |
|        13 | 5939 | `}` |
|         - | 5940 | `/*` |
|         - | 5941 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5942 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5943 | ` * Parameters` |
|         - | 5944 | ` *  $array1` |
|         - | 5945 | ` *    The array to compare from` |
|         - | 5946 | ` *  $...` |
|         - | 5947 | ` *   More arrays to compare against` |
|         - | 5948 | ` * Return` |
|         - | 5949 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5950 | ` *  have keys that are present in all arguments.` |
|         - | 5951 | ` * Note that NULL is returned on failure.` |
|         - | 5952 | ` */` |
|        20 | 5953 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5954 | `{` |
|         - | 5955 | `	ph7_hashmap_node *pEntry;` |
|         - | 5956 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5957 | `	ph7_value *pArray;` |
|         - | 5958 | `	sxi32 rc;` |
|         - | 5959 | `	sxu32 n;` |
|         - | 5960 | `	int i;` |
|        23 | 5961 | `	if( nArg < 1 ){` |
|       ! 0 | 5962 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5963 | `			"ArgumentCountError",` |
|         - | 5964 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5965 | `			nArg` |
|         - | 5966 | `			);` |
|         - | 5967 | `	}` |
|        23 | 5968 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5969 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5970 | `			"TypeError",` |
|         - | 5971 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5972 | `			ph7_type_name(apArg[0])` |
|         - | 5973 | `			);` |
|         - | 5974 | `	}` |
|        36 | 5975 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5976 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5977 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5978 | `				"TypeError",` |
|         - | 5979 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5980 | `				i + 1,` |
|         2 | 5981 | `				ph7_type_name(apArg[i])` |
|         - | 5982 | `				);` |
|         - | 5983 | `		}` |
|         9 | 5984 | `	}` |
|        17 | 5985 | `	if( nArg == 1 ){` |
|         - | 5986 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5987 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5988 | `		return PH7_OK;` |
|         - | 5989 | `	}` |
|         - | 5990 | `	/* Create a new array */` |
|        15 | 5991 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5992 | `	if( pArray == 0 ){` |
|       ! 0 | 5993 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5994 | `		return PH7_OK;` |
|         - | 5995 | `	}` |
|         - | 5996 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5997 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5998 | `	/* Perform the intersection */` |
|        15 | 5999 | `	pEntry = pSrc->pFirst;` |
|        15 | 6000 | `	n = pSrc->nEntry;` |
|        24 | 6001 | `	for(;;){` |
|        49 | 6002 | `		if( n < 1 ){` |
|        15 | 6003 | `			break;` |
|         - | 6004 | `		}` |
|        57 | 6005 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 6006 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 6007 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 6008 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 6009 | `				/* Blob lookup */` |
|        27 | 6010 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 6011 | `			}else{` |
|         - | 6012 | `				/* Int key */` |
|        13 | 6013 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 6014 | `			}` |
|        39 | 6015 | `			if( rc != SXRET_OK ){` |
|         - | 6016 | `				/* Key does not exist, break immediately */` |
|        17 | 6017 | `				break;` |
|         - | 6018 | `			}` |
|        12 | 6019 | `		}` |
|        35 | 6020 | `		if( i >= nArg ){` |
|         - | 6021 | `			/* Perform the insertion */` |
|        19 | 6022 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 6023 | `		}` |
|         - | 6024 | `		/* Point to the next entry */` |
|        35 | 6025 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6026 | `		n--;` |
|         1 | 6027 | `	}` |
|         - | 6028 | `	/* Return the freshly created array */` |
|        15 | 6029 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 6030 | `	return PH7_OK;` |
|        13 | 6031 | `}` |
|         - | 6032 | `/*` |
|         - | 6033 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 6034 | ` *  Computes the intersection of arrays.` |
|         - | 6035 | ` * Parameters` |
|         - | 6036 | ` *  $array1` |
|         - | 6037 | ` *    The array to compare from` |
|         - | 6038 | ` *  $array2` |
|         - | 6039 | ` *    An array to compare against` |
|         - | 6040 | ` *  $...` |
|         - | 6041 | ` *   More arrays to compare against` |
|         - | 6042 | ` * $callback` |
|         - | 6043 | ` *  The callback comparison function.` |
|         - | 6044 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 6045 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 6046 | ` *  than the second.` |
|         - | 6047 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 6048 | ` * Return` |
|         - | 6049 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 6050 | ` *  in all of the parameters. .` |
|         - | 6051 | ` * Note that NULL is returned on failure.` |
|         - | 6052 | ` */` |
|        24 | 6053 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6054 | `{` |
|         - | 6055 | `	ph7_hashmap_node *pEntry;` |
|         - | 6056 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 6057 | `	ph7_value *pCallback;` |
|         - | 6058 | `	ph7_value *pArray;` |
|         - | 6059 | `	ph7_value *pVal;` |
|         - | 6060 | `	sxi32 rc;` |
|         - | 6061 | `	sxu32 n;` |
|         - | 6062 | `	int i;` |
|         - | 6063 |  |
|         - | 6064 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 6065 | `	if( nArg < 2 ){` |
|       ! 0 | 6066 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6067 | `			"ArgumentCountError",` |
|         - | 6068 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 6069 | `			nArg` |
|         - | 6070 | `			);` |
|         - | 6071 | `	}` |
|        29 | 6072 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6073 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6074 | `			"TypeError",` |
|         - | 6075 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6076 | `			ph7_type_name(apArg[0])` |
|         - | 6077 | `			);` |
|         - | 6078 | `	}` |
|         - | 6079 |  |
|        27 | 6080 | `	if( nArg == 2 ){` |
|         - | 6081 | `		/* Only the original array and the callback were provided. */` |
|         - | 6082 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6083 | `		 * validation ordering. */` |
|         3 | 6084 | `	} else {` |
|         - | 6085 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6086 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6087 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6088 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6089 | `					"TypeError",` |
|         - | 6090 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6091 | `					i + 1,` |
|         2 | 6092 | `					ph7_type_name(apArg[i])` |
|         - | 6093 | `					);` |
|         - | 6094 | `			}` |
|        13 | 6095 | `		}` |
|         - | 6096 | `	}` |
|         - | 6097 |  |
|         - | 6098 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6099 | `	pCallback = apArg[nArg - 1];` |
|         - | 6100 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6101 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6102 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6103 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6104 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6105 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6106 | `			 */` |
|         9 | 6107 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6108 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6109 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6110 | `					"TypeError",` |
|         - | 6111 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6112 | `					nArg` |
|         - | 6113 | `					);` |
|         - | 6114 | `			}` |
|         - | 6115 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6116 | `			{` |
|         6 | 6117 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6118 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6119 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6120 | `					int nMethodLen;` |
|         6 | 6121 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6122 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6123 | `					if( pClass ){` |
|         - | 6124 | `						/* Class exists but method is missing. */` |
|         4 | 6125 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6126 | `							"TypeError",` |
|         - | 6127 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6128 | `							nArg,` |
|         1 | 6129 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6130 | `							zMethod` |
|         - | 6131 | `							);` |
|         - | 6132 | `					}` |
|         - | 6133 | `					/* Class not found */` |
|         - | 6134 | `					{` |
|         - | 6135 | `						int nName;` |
|         3 | 6136 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6137 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6138 | `							"TypeError",` |
|         - | 6139 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6140 | `							nArg,` |
|         1 | 6141 | `							zName` |
|         - | 6142 | `							);` |
|         - | 6143 | `					}` |
|         - | 6144 | `				}` |
|         - | 6145 | `			}` |
|         - | 6146 | `			/* Fallback message */` |
|       ! 0 | 6147 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6148 | `				"TypeError",` |
|         - | 6149 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6150 | `				nArg` |
|         - | 6151 | `				);` |
|         - | 6152 | `		}` |
|         6 | 6153 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6154 | `			int len;` |
|         3 | 6155 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6156 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6157 | `				"TypeError",` |
|         - | 6158 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6159 | `				nArg,` |
|         1 | 6160 | `				zName` |
|         - | 6161 | `				);` |
|         - | 6162 | `		}` |
|         4 | 6163 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6164 | `			"TypeError",` |
|         - | 6165 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6166 | `			nArg` |
|         - | 6167 | `			);` |
|         - | 6168 | `	}` |
|         - | 6169 |  |
|        11 | 6170 | `	if( nArg == 2 ){` |
|         - | 6171 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6172 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6173 | `		return PH7_OK;` |
|         - | 6174 | `	}` |
|         - | 6175 |  |
|         - | 6176 | `	/* Create a new array */` |
|         7 | 6177 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6178 | `	if( pArray == 0 ){` |
|       ! 0 | 6179 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6180 | `		return PH7_OK;` |
|         - | 6181 | `	}` |
|         - | 6182 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6183 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6184 | `	/* Perform the intersection */` |
|         7 | 6185 | `	pEntry = pSrc->pFirst;` |
|         7 | 6186 | `	n = pSrc->nEntry;` |
|         7 | 6187 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6188 | `	for(;;){` |
|        19 | 6189 | `		if( n < 1 ){` |
|         5 | 6190 | `			break;` |
|         - | 6191 | `		}` |
|         - | 6192 | `		/* Extract the node value */` |
|        15 | 6193 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6194 | `		if( pVal ){` |
|        23 | 6195 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6196 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6197 | `					/* ignore */` |
|       ! 0 | 6198 | `					continue;` |
|         - | 6199 | `				}` |
|         - | 6200 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6201 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6202 | `				/* Perform the lookup */` |
|        15 | 6203 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6204 | `				if( rc != SXRET_OK ){` |
|         - | 6205 | `					/* Value does not exist */` |
|         7 | 6206 | `					break;` |
|         - | 6207 | `				}` |
|         5 | 6208 | `			}` |
|        15 | 6209 | `			if( i >= (nArg-1) ){` |
|         - | 6210 | `				/* Perform the insertion */` |
|         9 | 6211 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6212 | `			}` |
|         7 | 6213 | `		}` |
|        15 | 6214 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6215 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6216 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6217 | `			return PH7_EXCEPTION;` |
|         - | 6218 | `		}` |
|         - | 6219 | `		/* Point to the next entry */` |
|        13 | 6220 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6221 | `		n--;` |
|         1 | 6222 | `	}` |
|         - | 6223 | `	/* Return the freshly created array */` |
|         5 | 6224 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6225 | `	return PH7_OK;` |
|        17 | 6226 | `}` |
|         - | 6227 | `/*` |
|         - | 6228 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6229 | ` *  Fill an array with values.` |
|         - | 6230 | ` * Parameters` |
|         - | 6231 | ` *  $start_index` |
|         - | 6232 | ` *    The first index of the returned array.` |
|         - | 6233 | ` *  $num` |
|         - | 6234 | ` *   Number of elements to insert.` |
|         - | 6235 | ` *  $value` |
|         - | 6236 | ` *    Value to use for filling.` |
|         - | 6237 | ` * Return` |
|         - | 6238 | ` *  The filled array or null on failure.` |
|         - | 6239 | ` */` |
|       240 | 6240 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6241 | `{` |
|         - | 6242 | `	ph7_value *pArray;` |
|         - | 6243 | `	int i,nEntry;` |
|         - | 6244 |  |
|         - | 6245 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6246 | `	if( nArg != 3 ){` |
|         - | 6247 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6248 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6249 | `			"ArgumentCountError",` |
|         - | 6250 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6251 | `			nArg` |
|         - | 6252 | `			);` |
|         - | 6253 | `	}` |
|         - | 6254 |  |
|         - | 6255 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6256 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6257 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6258 | `	 * and NULLs are rejected outright. */` |
|       357 | 6259 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6260 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6261 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6262 | `			"TypeError",` |
|         - | 6263 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6264 | `			ph7_type_name(apArg[0])` |
|         - | 6265 | `			);` |
|         - | 6266 | `	}` |
|       242 | 6267 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6268 | `		int len;` |
|         8 | 6269 | `		sxu8 bReal = FALSE;` |
|         8 | 6270 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6271 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6272 | `			/* Non‑numeric string is an error. */` |
|         3 | 6273 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6274 | `				"TypeError",` |
|         - | 6275 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6276 | `				);` |
|         - | 6277 | `		}` |
|         5 | 6278 | `		if( bReal ){` |
|         - | 6279 | `			/* float-string -> deprecation warning */` |
|         4 | 6280 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6281 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6282 | `				zStr` |
|         - | 6283 | `				);` |
|         1 | 6284 | `		}` |
|         2 | 6285 | `	}` |
|         - | 6286 |  |
|         - | 6287 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6288 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6289 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6290 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6291 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6292 | `			"TypeError",` |
|         - | 6293 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6294 | `			ph7_type_name(apArg[1])` |
|         - | 6295 | `			);` |
|         - | 6296 | `	}` |
|       239 | 6297 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6298 | `		int len;` |
|         3 | 6299 | `		sxu8 bReal = FALSE;` |
|         3 | 6300 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6301 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6302 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6303 | `				"TypeError",` |
|         - | 6304 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6305 | `				);` |
|         - | 6306 | `		}` |
|       ! 0 | 6307 | `	}` |
|         - | 6308 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6309 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6310 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6311 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6312 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6313 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6314 | `		if( d != (double)i64 ){` |
|         7 | 6315 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6316 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6317 | `				d` |
|         - | 6318 | `				);` |
|         2 | 6319 | `		}` |
|         2 | 6320 | `	}` |
|         - | 6321 |  |
|         - | 6322 | `	/* Total number of entries to insert */` |
|       236 | 6323 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6324 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6325 | `	if( nEntry < 0 ){` |
|         3 | 6326 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6327 | `			"ValueError",` |
|         - | 6328 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6329 | `			);` |
|         - | 6330 | `	}` |
|         - | 6331 |  |
|         - | 6332 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6333 | `	if( nEntry == 0 ){` |
|         7 | 6334 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6335 | `		return PH7_OK;` |
|         - | 6336 | `	}` |
|         - | 6337 |  |
|         - | 6338 | `	/* Create a new array */` |
|       227 | 6339 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6340 | `	if( pArray == 0 ){` |
|       ! 0 | 6341 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6342 | `	}` |
|         - | 6343 |  |
|         - | 6344 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6345 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6346 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6347 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6348 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6349 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6350 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6351 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6352 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6353 | `		}` |
|   1058803 | 6354 | `	}` |
|         - | 6355 | `	/* Return the filled array */` |
|       227 | 6356 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6357 | `	return PH7_OK;` |
|       124 | 6358 | `}` |
|         - | 6359 | `/*` |
|         - | 6360 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6361 | ` *  Fill an array with values, specifying keys.` |
|         - | 6362 | ` * Parameters` |
|         - | 6363 | ` *  $input` |
|         - | 6364 | ` *   Array of values that will be used as key.` |
|         - | 6365 | ` *  $value` |
|         - | 6366 | ` *    Value to use for filling.` |
|         - | 6367 | ` * Return` |
|         - | 6368 | ` *  The filled array.` |
|         - | 6369 | ` * Throws` |
|         - | 6370 | ` *  ValueError if $input is not an array.` |
|         - | 6371 | ` */` |
|        22 | 6372 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6373 | `{` |
|         - | 6374 | `	ph7_hashmap_node *pEntry;` |
|         - | 6375 | `	ph7_hashmap *pSrc;` |
|         - | 6376 | `	ph7_value *pArray;` |
|         - | 6377 | `	sxu32 n;` |
|         - | 6378 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6379 | `	if( nArg != 2 ){` |
|         4 | 6380 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6381 | `			"ArgumentCountError",` |
|         - | 6382 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6383 | `			nArg` |
|         - | 6384 | `			);` |
|         - | 6385 | `	}` |
|         - | 6386 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6387 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6388 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6389 | `			"TypeError",` |
|         - | 6390 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6391 | `			ph7_type_name(apArg[0])` |
|         - | 6392 | `			);` |
|         - | 6393 | `	}` |
|         - | 6394 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6395 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6396 | `	/* Create a new array */` |
|        17 | 6397 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6398 | `	if( pArray == 0 ){` |
|       ! 0 | 6399 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6400 | `		return PH7_OK;` |
|         - | 6401 | `	}` |
|         - | 6402 | `	/* Perform the requested operation */` |
|        17 | 6403 | `	pEntry = pSrc->pFirst;` |
|        45 | 6404 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6405 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6406 | `		/* Point to the next entry */` |
|        29 | 6407 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6408 | `	}` |
|         - | 6409 | `	/* Return the filled array */` |
|        17 | 6410 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6411 | `	return PH7_OK;` |
|        14 | 6412 | `}` |
|         - | 6413 | `/*` |
|         - | 6414 | ` * array array_combine(array $keys,array $values)` |
|         - | 6415 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6416 | ` * Parameters` |
|         - | 6417 | ` *  $keys` |
|         - | 6418 | ` *    Array of keys to be used.` |
|         - | 6419 | ` * $values` |
|         - | 6420 | ` *   Array of values to be used.` |
|         - | 6421 | ` * Return` |
|         - | 6422 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6423 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6424 | ` *  not an array.` |
|         - | 6425 | ` */` |
|        16 | 6426 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6427 | `{` |
|         - | 6428 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6429 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6430 | `	ph7_value *pArray;` |
|         - | 6431 | `	sxu32 n;` |
|         - | 6432 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6433 | `	if( nArg != 2 ){` |
|         - | 6434 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6435 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6436 | `			"ArgumentCountError",` |
|         - | 6437 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6438 | `			nArg` |
|         - | 6439 | `			);` |
|         - | 6440 | `	}` |
|         - | 6441 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6442 | `	 * argument index in the error message. */` |
|        20 | 6443 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6444 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6445 | `			"TypeError",` |
|         - | 6446 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6447 | `			ph7_type_name(apArg[0])` |
|         - | 6448 | `			);` |
|         - | 6449 | `	}` |
|        17 | 6450 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6451 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6452 | `			"TypeError",` |
|         - | 6453 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6454 | `			ph7_type_name(apArg[1])` |
|         - | 6455 | `			);` |
|         - | 6456 | `	}` |
|         - | 6457 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6458 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6459 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6460 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6461 | `		/* Length mismatch -> ValueError */` |
|         3 | 6462 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6463 | `			"ValueError",` |
|         - | 6464 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6465 | `			);` |
|         - | 6466 | `	}` |
|         - | 6467 | `	/* Create a new array */` |
|        11 | 6468 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6469 | `	if( pArray == 0 ){` |
|       ! 0 | 6470 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6471 | `		return PH7_OK;` |
|         - | 6472 | `	}` |
|         - | 6473 | `	/* Perform the requested operation */` |
|        11 | 6474 | `	pKe = pKey->pFirst;` |
|        11 | 6475 | `	pVe = pValue->pFirst;` |
|        33 | 6476 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6477 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6478 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6479 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6480 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6481 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6482 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6483 | `		 * original array must not be mutated. */` |
|        23 | 6484 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6485 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6486 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6487 | `			if( pTmpKey ){` |
|         5 | 6488 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6489 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6490 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6491 | `				pKeyCopy = pTmpKey;` |
|         2 | 6492 | `			}` |
|         2 | 6493 | `		}` |
|        23 | 6494 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6495 | `		/* Point to the next entry */` |
|        23 | 6496 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6497 | `		pVe = pVe->pPrev;` |
|        12 | 6498 | `	}` |
|         - | 6499 | `	/* Return the filled array */` |
|        11 | 6500 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6501 | `	return PH7_OK;` |
|        12 | 6502 | `}` |
|         - | 6503 | `/*` |
|         - | 6504 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6505 | ` *  Return an array with elements in reverse order.` |
|         - | 6506 | ` * Parameters` |
|         - | 6507 | ` *  $array` |
|         - | 6508 | ` *   The input array.` |
|         - | 6509 | ` *  $preserve_keys (optional)` |
|         - | 6510 | ` *   If set to TRUE keys are preserved.` |
|         - | 6511 | ` * Return` |
|         - | 6512 | ` *  The reversed array.` |
|         - | 6513 | ` */` |
|        18 | 6514 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6515 | `{` |
|         - | 6516 | `	ph7_hashmap_node *pEntry;` |
|         - | 6517 | `	ph7_hashmap *pSrc;` |
|         - | 6518 | `	ph7_value *pArray;` |
|         - | 6519 | `	int bPreserve;` |
|         - | 6520 | `	sxu32 n;` |
|        20 | 6521 | `	if( nArg < 1 ){` |
|       ! 0 | 6522 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6523 | `			"ArgumentCountError",` |
|         - | 6524 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6525 | `			nArg` |
|         - | 6526 | `			);` |
|         - | 6527 | `	}` |
|         - | 6528 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6529 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6530 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6531 | `			"TypeError",` |
|         - | 6532 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6533 | `			ph7_type_name(apArg[0])` |
|         - | 6534 | `			);` |
|         - | 6535 | `	}` |
|        17 | 6536 | `	bPreserve = FALSE;` |
|        17 | 6537 | `	if( nArg > 1 ){` |
|         7 | 6538 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6539 | `	}` |
|         - | 6540 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6541 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6542 | `	/* Create a new array */` |
|        17 | 6543 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6544 | `	if( pArray == 0 ){` |
|       ! 0 | 6545 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6546 | `		return PH7_OK;` |
|         - | 6547 | `	}` |
|         - | 6548 | `	/* Perform the requested operation */` |
|        17 | 6549 | `	pEntry = pSrc->pLast;` |
|        55 | 6550 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6551 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6552 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6553 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6554 | `		/* Point to the previous entry */` |
|        39 | 6555 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6556 | `	}` |
|        17 | 6557 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6558 | `	return PH7_OK;` |
|        11 | 6559 | `}` |
|         - | 6560 | `/*` |
|         - | 6561 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6562 | ` *  Removes duplicate values from an array.` |
|         - | 6563 | ` * Parameters` |
|         - | 6564 | ` *  $array` |
|         - | 6565 | ` *   The input array.` |
|         - | 6566 | ` *  $flags` |
|         - | 6567 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6568 | ` *   behavior using these values:` |
|         - | 6569 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6570 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6571 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6572 | ` * Return` |
|         - | 6573 | ` *  The filtered array.` |
|         - | 6574 | ` */` |
|        36 | 6575 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6576 | `{` |
|         - | 6577 | `	ph7_hashmap_node *pEntry;` |
|         - | 6578 | `	ph7_value *pNeedle;` |
|         - | 6579 | `	ph7_hashmap *pSrc;` |
|         - | 6580 | `	ph7_value *pArray;` |
|         - | 6581 | `	int iFlags,base,bFold;` |
|         - | 6582 | `	sxu32 n;` |
|        39 | 6583 | `	if( nArg < 1 ){` |
|         - | 6584 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6585 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6586 | `			"ArgumentCountError",` |
|         - | 6587 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6588 | `			);` |
|         - | 6589 | `	}` |
|        39 | 6590 | `	if( nArg > 2 ){` |
|         - | 6591 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6592 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6593 | `			"ArgumentCountError",` |
|         - | 6594 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6595 | `			nArg` |
|         - | 6596 | `			);` |
|         - | 6597 | `	}` |
|         - | 6598 | `	/* Make sure we are dealing with a valid hashmap */` |
|        36 | 6599 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6600 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6601 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6602 | `			"TypeError",` |
|         - | 6603 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6604 | `			ph7_type_name(apArg[0])` |
|         - | 6605 | `			);` |
|         - | 6606 | `	}` |
|         - | 6607 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|         - | 6608 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|        33 | 6609 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|        33 | 6610 | `	base = iFlags & ~8;` |
|        33 | 6611 | `	bFold = (iFlags & 8) != 0;` |
|         - | 6612 | `	/* Point to the internal representation of the input hashmap */` |
|        33 | 6613 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6614 | `	/* Create a new array */` |
|        33 | 6615 | `	pArray = ph7_context_new_array(pCtx);` |
|        33 | 6616 | `	if( pArray == 0 ){` |
|       ! 0 | 6617 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6618 | `		return PH7_OK;` |
|         - | 6619 | `	}` |
|         - | 6620 | `	/* Perform the requested operation */` |
|        33 | 6621 | `	pEntry = pSrc->pFirst;` |
|       145 | 6622 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       113 | 6623 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|       113 | 6624 | `		if( pNeedle ){` |
|         - | 6625 | `			/* Keep this element unless a flag-equal one is already present. */` |
|       113 | 6626 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|       113 | 6627 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|       113 | 6628 | `			int bDup = 0;` |
|         - | 6629 | `			sxu32 i;` |
|         - | 6630 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|         - | 6631 | `			 * loop over pSrc). */` |
|       177 | 6632 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|       117 | 6633 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|       117 | 6634 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|        53 | 6635 | `					bDup = 1;` |
|        53 | 6636 | `					break;` |
|         - | 6637 | `				}` |
|        65 | 6638 | `				pK = pK->pPrev;` |
|        33 | 6639 | `			}` |
|       113 | 6640 | `			if( !bDup ){` |
|        61 | 6641 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|        30 | 6642 | `			}` |
|        56 | 6643 | `		}` |
|         - | 6644 | `		/* Point to the next entry */` |
|       113 | 6645 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        57 | 6646 | `	}` |
|         - | 6647 | `	/* Return the freshly created array */` |
|        33 | 6648 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 6649 | `	return PH7_OK;` |
|        21 | 6650 | `}` |
|         - | 6651 | `/*` |
|         - | 6652 | ` * array array_flip(array $input)` |
|         - | 6653 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6654 | ` * Parameter` |
|         - | 6655 | ` *  $input` |
|         - | 6656 | ` *   Input array.` |
|         - | 6657 | ` * Return` |
|         - | 6658 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6659 | ` */` |
|        30 | 6660 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6661 | `{` |
|         - | 6662 | `	ph7_hashmap_node *pEntry;` |
|         - | 6663 | `	ph7_hashmap *pSrc;` |
|         - | 6664 | `	ph7_value *pArray;` |
|         - | 6665 | `	ph7_value *pKey;` |
|         - | 6666 | `	ph7_value sVal;` |
|         - | 6667 | `	sxu32 n;` |
|         - | 6668 |  |
|         - | 6669 | `	/* PHP requires exactly one argument */` |
|        33 | 6670 | `	if( nArg != 1 ){` |
|         - | 6671 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6672 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6673 | `			"ArgumentCountError",` |
|         - | 6674 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6675 | `			nArg` |
|         - | 6676 | `			);` |
|         - | 6677 | `	}` |
|         - | 6678 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6679 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6680 | `		/* Type mismatch -> TypeError */` |
|         4 | 6681 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6682 | `			"TypeError",` |
|         - | 6683 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6684 | `			ph7_type_name(apArg[0])` |
|         - | 6685 | `			);` |
|         - | 6686 | `	}` |
|         - | 6687 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6688 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6689 | `	/* Create a new array */` |
|        27 | 6690 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6691 | `	if( pArray == 0 ){` |
|       ! 0 | 6692 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6693 | `		return PH7_OK;` |
|         - | 6694 | `	}` |
|         - | 6695 | `	/* Start processing */` |
|        27 | 6696 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6697 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6698 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6699 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6700 | `		if( pKey ){` |
|         - | 6701 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6702 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6703 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6704 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6705 | `					);` |
|     22236 | 6706 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6707 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6708 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6709 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6710 | `				}else{` |
|         - | 6711 | `					SyString sStr;` |
|      2227 | 6712 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6713 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6714 | `				}` |
|         - | 6715 | `				/* Perform the insertion */` |
|     22227 | 6716 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6717 | `				/* Safely release the value because each inserted entry` |
|         - | 6718 | `				 * has its own private copy of the value.` |
|         - | 6719 | `				 */` |
|     22227 | 6720 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6721 | `			}else{` |
|         - | 6722 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6723 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6724 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6725 | `					);` |
|         - | 6726 | `			}` |
|     11118 | 6727 | `		}` |
|         - | 6728 | `		/* Point to the next entry */` |
|     22237 | 6729 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6730 | `	}` |
|         - | 6731 | `	/* Return the freshly created array */` |
|        27 | 6732 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6733 | `	return PH7_OK;` |
|        18 | 6734 | `}` |
|         - | 6735 | `/*` |
|         - | 6736 | ` * number array_sum(array $array )` |
|         - | 6737 | ` *  Calculate the sum of values in an array.` |
|         - | 6738 | ` * Parameters` |
|         - | 6739 | ` *  $array: The input array.` |
|         - | 6740 | ` * Return` |
|         - | 6741 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6742 | ` */` |
|        24 | 6743 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6744 | `{` |
|         - | 6745 | `	ph7_hashmap_node *pEntry;` |
|         - | 6746 | `	ph7_value *pObj;` |
|        26 | 6747 | `	double dSum = 0;` |
|         - | 6748 | `	sxu32 n;` |
|        26 | 6749 | `	pEntry = pMap->pFirst;` |
|        92 | 6750 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6751 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6752 | `		if( pObj ){` |
|        68 | 6753 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6754 | `				dSum += pObj->rVal;` |
|        54 | 6755 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6756 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6757 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6758 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6759 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6760 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6761 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6762 | `						"Addition is not supported on type string");` |
|        14 | 6763 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6764 | `					double dv = 0;` |
|        13 | 6765 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6766 | `					dSum += dv;` |
|         8 | 6767 | `				}` |
|        12 | 6768 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6769 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6770 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6771 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6772 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6773 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6774 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6775 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6776 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6777 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6778 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6779 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6780 | `			}` |
|         - | 6781 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6782 | `		}` |
|         - | 6783 | `		/* Point to the next entry */` |
|        68 | 6784 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6785 | `	}` |
|         - | 6786 | `	/* Return sum */` |
|        26 | 6787 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6788 | `}` |
|       688 | 6789 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6790 | `{` |
|         - | 6791 | `	ph7_hashmap_node *pEntry;` |
|         - | 6792 | `	ph7_value *pObj;` |
|       690 | 6793 | `	sxi64 nSum = 0;` |
|         - | 6794 | `	sxu32 n;` |
|       690 | 6795 | `	pEntry = pMap->pFirst;` |
|      4702 | 6796 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6797 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6798 | `		if( pObj ){` |
|      4014 | 6799 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6800 | `				nSum += pObj->x.iVal;` |
|      2018 | 6801 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6802 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6803 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6804 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6805 | `						"Addition is not supported on type string");` |
|        10 | 6806 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6807 | `					sxi64 nv = 0;` |
|         8 | 6808 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6809 | `					nSum += nv;` |
|         5 | 6810 | `				}` |
|        17 | 6811 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6812 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6813 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6814 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6815 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6816 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6817 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6818 | `					"Addition is not supported on type %s",` |
|         2 | 6819 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6820 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6821 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6822 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6823 | `			}` |
|         - | 6824 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6825 | `		}` |
|         - | 6826 | `		/* Point to the next entry */` |
|      4014 | 6827 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6828 | `	}` |
|         - | 6829 | `	/* Return sum */` |
|       690 | 6830 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6831 | `}` |
|         - | 6832 | `/* number array_sum(array $array )` |
|         - | 6833 | ` * (See block-coment above)` |
|         - | 6834 | ` */` |
|       724 | 6835 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6836 | `{` |
|         - | 6837 | `	ph7_hashmap_node *pEntry;` |
|         - | 6838 | `	ph7_hashmap *pMap;` |
|         - | 6839 | `	ph7_value *pObj;` |
|       728 | 6840 | `	int useDouble = 0;` |
|         - | 6841 | `	sxu32 n;` |
|         - | 6842 | `	/* PHP requires exactly one argument */` |
|       728 | 6843 | `	if( nArg != 1 ){` |
|         4 | 6844 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6845 | `			"ArgumentCountError",` |
|         - | 6846 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6847 | `			nArg` |
|         - | 6848 | `			);` |
|         - | 6849 | `	}` |
|         - | 6850 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6851 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6852 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6853 | `		char zBuf[64];` |
|         8 | 6854 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6855 | `			"TypeError",` |
|         - | 6856 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6857 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6858 | `			);` |
|         - | 6859 | `	}` |
|       720 | 6860 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6861 | `	if( pMap->nEntry < 1 ){` |
|         - | 6862 | `		/* Nothing to compute,return 0 */` |
|         7 | 6863 | `		ph7_result_int(pCtx,0);` |
|         7 | 6864 | `		return PH7_OK;` |
|         - | 6865 | `	}` |
|         - | 6866 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6867 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6868 | `	 */` |
|       714 | 6869 | `	pEntry = pMap->pFirst;` |
|      4734 | 6870 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6871 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6872 | `		if( pObj ){` |
|      4046 | 6873 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6874 | `				useDouble = 1;` |
|        20 | 6875 | `				break;` |
|         - | 6876 | `			}` |
|      4028 | 6877 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6878 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6879 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6880 | `				sxu32 i;` |
|        32 | 6881 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6882 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6883 | `						useDouble = 1;` |
|         7 | 6884 | `						break;` |
|         - | 6885 | `					}` |
|         9 | 6886 | `				}` |
|        18 | 6887 | `				if( useDouble ){` |
|         7 | 6888 | `					break;` |
|         - | 6889 | `				}` |
|         5 | 6890 | `			}` |
|      2010 | 6891 | `		}` |
|      4022 | 6892 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6893 | `	}` |
|       714 | 6894 | `	if( useDouble ){` |
|        26 | 6895 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6896 | `	}else{` |
|       690 | 6897 | `		Int64Sum(pCtx,pMap);` |
|         - | 6898 | `	}` |
|       714 | 6899 | `	return PH7_OK;` |
|       366 | 6900 | `}` |
|         - | 6901 | `/*` |
|         - | 6902 | ` * number array_product(array $array )` |
|         - | 6903 | ` *  Calculate the product of values in an array.` |
|         - | 6904 | ` * Parameters` |
|         - | 6905 | ` *  $array: The input array.` |
|         - | 6906 | ` * Return` |
|         - | 6907 | ` *  Returns the product of values as an integer or float.` |
|         - | 6908 | ` */` |
|         2 | 6909 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6910 | `{` |
|         - | 6911 | `	ph7_hashmap_node *pEntry;` |
|         - | 6912 | `	ph7_value *pObj;` |
|         - | 6913 | `	double dProd;` |
|         - | 6914 | `	sxu32 n;` |
|         3 | 6915 | `	pEntry = pMap->pFirst;` |
|         3 | 6916 | `	dProd = 1;` |
|         7 | 6917 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6918 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6919 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6920 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6921 | `				dProd *= pObj->rVal;` |
|         4 | 6922 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6923 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6924 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6925 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6926 | `					double dv = 0;` |
|       ! 0 | 6927 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6928 | `					dProd *= dv;` |
|       ! 0 | 6929 | `				}` |
|       ! 0 | 6930 | `			}` |
|         2 | 6931 | `		}` |
|         - | 6932 | `		/* Point to the next entry */` |
|         5 | 6933 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6934 | `	}` |
|         - | 6935 | `	/* Return product */` |
|         3 | 6936 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6937 | `}` |
|         2 | 6938 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6939 | `{` |
|         - | 6940 | `	ph7_hashmap_node *pEntry;` |
|         - | 6941 | `	ph7_value *pObj;` |
|         - | 6942 | `	sxi64 nProd;` |
|         - | 6943 | `	sxu32 n;` |
|         3 | 6944 | `	pEntry = pMap->pFirst;` |
|         3 | 6945 | `	nProd = 1;` |
|         9 | 6946 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6947 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6948 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6949 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6950 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6951 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6952 | `				nProd *= pObj->x.iVal;` |
|         3 | 6953 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6954 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6955 | `					sxi64 nv = 0;` |
|       ! 0 | 6956 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6957 | `					nProd *= nv;` |
|       ! 0 | 6958 | `				}` |
|       ! 0 | 6959 | `			}` |
|         3 | 6960 | `		}` |
|         - | 6961 | `		/* Point to the next entry */` |
|         7 | 6962 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6963 | `	}` |
|         - | 6964 | `	/* Return product */` |
|         3 | 6965 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6966 | `}` |
|         - | 6967 | `/* number array_product(array $array )` |
|         - | 6968 | ` * (See block-block comment above)` |
|         - | 6969 | ` */` |
|        16 | 6970 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6971 | `{` |
|         - | 6972 | `	ph7_hashmap *pMap;` |
|         - | 6973 | `	ph7_value *pObj;` |
|        17 | 6974 | `	if( nArg < 1 ){` |
|         - | 6975 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6976 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6977 | `		return PH7_OK;` |
|         - | 6978 | `	}` |
|         - | 6979 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 6980 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6981 | `		char zBuf[64];` |
|        16 | 6982 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6983 | `			"TypeError",` |
|         - | 6984 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 6985 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6986 | `			);` |
|         - | 6987 | `	}` |
|         7 | 6988 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6989 | `	if( pMap->nEntry < 1 ){` |
|         - | 6990 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6991 | `		ph7_result_int(pCtx,1);` |
|         3 | 6992 | `		return PH7_OK;` |
|         - | 6993 | `	}` |
|         - | 6994 | `	/* If the first element is of type float,then perform floating` |
|         - | 6995 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6996 | `	 */` |
|         5 | 6997 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6998 | `	if( pObj == 0 ){` |
|       ! 0 | 6999 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 7000 | `		return PH7_OK;` |
|         - | 7001 | `	}` |
|         5 | 7002 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 7003 | `		DoubleProd(pCtx,pMap);` |
|         2 | 7004 | `	}else{` |
|         3 | 7005 | `		Int64Prod(pCtx,pMap);` |
|         - | 7006 | `	}` |
|         5 | 7007 | `	return PH7_OK;` |
|         9 | 7008 | `}` |
|         - | 7009 | `/*` |
|         - | 7010 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 7011 | ` *  Pick one or more random entries out of an array.` |
|         - | 7012 | ` * Parameters` |
|         - | 7013 | ` * $input` |
|         - | 7014 | ` *  The input array.` |
|         - | 7015 | ` * $num_req` |
|         - | 7016 | ` *  Specifies how many entries you want to pick.` |
|         - | 7017 | ` * Return` |
|         - | 7018 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 7019 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 7020 | ` *  NULL is returned on failure.` |
|         - | 7021 | ` */` |
|        36 | 7022 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7023 | `{` |
|         - | 7024 | `	ph7_hashmap_node *pNode;` |
|         - | 7025 | `	ph7_hashmap *pMap;` |
|        37 | 7026 | `	int nItem = 1;` |
|        37 | 7027 | `	if( nArg < 1 ){` |
|         - | 7028 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7029 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7030 | `		return PH7_OK;` |
|         - | 7031 | `	}` |
|         - | 7032 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 7033 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7034 | `		char zBuf[64];` |
|        10 | 7035 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7036 | `			"TypeError",` |
|         - | 7037 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7038 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7039 | `			);` |
|         - | 7040 | `	}` |
|         - | 7041 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 7042 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 7043 | `	if( nArg > 1 ){` |
|        23 | 7044 | `		ph7_value *pNum = apArg[1];` |
|        22 | 7045 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 7046 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 7047 | `			char zBuf[64];` |
|       ! 0 | 7048 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7049 | `				"TypeError",` |
|         - | 7050 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 7051 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 7052 | `				);` |
|         - | 7053 | `		}` |
|        23 | 7054 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 7055 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 7056 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 7057 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 7058 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 7059 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 7060 | `			int len;` |
|         9 | 7061 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 7062 | `			sxi64 iLong; double dReal;` |
|         9 | 7063 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 7064 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 7065 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7066 | `					"TypeError",` |
|         - | 7067 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 7068 | `					);` |
|         - | 7069 | `			}` |
|         - | 7070 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 7071 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 7072 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 7073 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 7074 | `			}` |
|         3 | 7075 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 7076 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 7077 | `			nItem = (int)iLong;` |
|         2 | 7078 | `		}else{` |
|        15 | 7079 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 7080 | `		}` |
|         8 | 7081 | `	}` |
|         - | 7082 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 7083 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7084 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 7085 | `	if( pMap->nEntry < 1 ){` |
|         5 | 7086 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7087 | `			"ValueError",` |
|         - | 7088 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7089 | `			);` |
|         - | 7090 | `	}` |
|         - | 7091 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7092 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7093 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7094 | `			"ValueError",` |
|         - | 7095 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7096 | `			);` |
|         - | 7097 | `	}` |
|        13 | 7098 | `	if( nItem < 2 ){` |
|         - | 7099 | `		sxu32 nEntry;` |
|         - | 7100 | `		/* Select a random number */` |
|         9 | 7101 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7102 | `		/* Extract the desired entry.` |
|         - | 7103 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7104 | `		 */` |
|         9 | 7105 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         2 | 7106 | `			pNode = pMap->pLast;` |
|         2 | 7107 | `			nEntry = pMap->nEntry - nEntry;` |
|         2 | 7108 | `			if( nEntry > 1 ){` |
|       ! 0 | 7109 | `				for(;;){` |
|       ! 0 | 7110 | `					if( nEntry == 0 ){` |
|       ! 0 | 7111 | `						break;` |
|         - | 7112 | `					}` |
|         - | 7113 | `					/* Point to the previous entry */` |
|       ! 0 | 7114 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7115 | `					nEntry--;` |
|       ! 0 | 7116 | `				}` |
|       ! 0 | 7117 | `			}` |
|         1 | 7118 | `		}else{` |
|         8 | 7119 | `			pNode = pMap->pFirst;` |
|         5 | 7120 | `			for(;;){` |
|        10 | 7121 | `				if( nEntry == 0 ){` |
|         8 | 7122 | `					break;` |
|         - | 7123 | `				}` |
|         - | 7124 | `				/* Point to the next entry */` |
|         2 | 7125 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         2 | 7126 | `				nEntry--;` |
|       ! 0 | 7127 | `			}` |
|         - | 7128 | `		}` |
|         9 | 7129 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7130 | `			/* Int key */` |
|         7 | 7131 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7132 | `		}else{` |
|         - | 7133 | `			/* Blob key */` |
|         3 | 7134 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7135 | `		}` |
|         5 | 7136 | `	}else{` |
|         - | 7137 | `		ph7_value sKey,*pArray;` |
|         - | 7138 | `		ph7_hashmap *pDest;` |
|         - | 7139 | `		/* Create a new array */` |
|         5 | 7140 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7141 | `		if( pArray == 0 ){` |
|       ! 0 | 7142 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7143 | `			return PH7_OK;` |
|         - | 7144 | `		}` |
|         - | 7145 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7146 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7147 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7148 | `		/* Copy the first n items */` |
|         5 | 7149 | `		pNode = pMap->pFirst;` |
|         5 | 7150 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7151 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7152 | `		}` |
|        15 | 7153 | `		while( nItem > 0){` |
|        11 | 7154 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7155 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7156 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7157 | `			/* Point to the next entry */` |
|        11 | 7158 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7159 | `			nItem--;` |
|         1 | 7160 | `		}` |
|         - | 7161 | `		/* Shuffle the array */` |
|         5 | 7162 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7163 | `		/* Rehash node */` |
|         5 | 7164 | `		HashmapSortRehash(pDest);` |
|         - | 7165 | `		/* Return the random array */` |
|         5 | 7166 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7167 | `	}` |
|        13 | 7168 | `	return PH7_OK;` |
|        19 | 7169 | `}` |
|         - | 7170 | `/*` |
|         - | 7171 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7172 | ` *  Split an array into chunks.` |
|         - | 7173 | ` * Parameters` |
|         - | 7174 | ` * $input` |
|         - | 7175 | ` *   The array to work on` |
|         - | 7176 | ` * $size` |
|         - | 7177 | ` *   The size of each chunk` |
|         - | 7178 | ` * $preserve_keys` |
|         - | 7179 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7180 | ` *   the chunk numerically.` |
|         - | 7181 | ` * Return` |
|         - | 7182 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7183 | ` *  zero, with each dimension containing size elements.` |
|         - | 7184 | ` */` |
|        36 | 7185 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7186 | `{` |
|         - | 7187 | `	ph7_value *pArray,*pChunk;` |
|         - | 7188 | `	ph7_hashmap_node *pEntry;` |
|         - | 7189 | `	ph7_hashmap *pMap;` |
|         - | 7190 | `	int bPreserve;` |
|         - | 7191 | `	sxu32 nChunk;` |
|         - | 7192 | `	sxu32 nSize;` |
|         - | 7193 | `	sxu32 n;` |
|         - | 7194 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7195 | `	if( nArg < 2 ){` |
|         - | 7196 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7197 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7198 | `			"ArgumentCountError",` |
|         - | 7199 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7200 | `			nArg` |
|         - | 7201 | `			);` |
|         - | 7202 | `	}` |
|        41 | 7203 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7204 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7205 | `			"TypeError",` |
|         - | 7206 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7207 | `			ph7_type_name(apArg[0])` |
|         - | 7208 | `			);` |
|         - | 7209 | `	}` |
|         - | 7210 | `	/* Create a new array */` |
|        38 | 7211 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7212 | `	if( pArray == 0 ){` |
|       ! 0 | 7213 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7214 | `		return PH7_OK;` |
|         - | 7215 | `	}` |
|         - | 7216 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7217 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7218 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7219 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7220 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7221 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7222 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7224 | `			"TypeError",` |
|         - | 7225 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7226 | `			ph7_type_name(apArg[1])` |
|         - | 7227 | `			);` |
|         - | 7228 | `	}` |
|         - | 7229 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7230 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7231 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7232 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7233 | `		int len;` |
|         3 | 7234 | `		sxu8 bReal = FALSE;` |
|         3 | 7235 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7236 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7237 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7238 | `				"TypeError",` |
|         - | 7239 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7240 | `				);` |
|         - | 7241 | `		}` |
|       ! 0 | 7242 | `		if( bReal ){` |
|         - | 7243 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7244 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7245 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7246 | `				zStr` |
|         - | 7247 | `				);` |
|       ! 0 | 7248 | `		}` |
|       ! 0 | 7249 | `	}` |
|         - | 7250 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7251 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7252 | `	 * later via ph7_value_to_int. */` |
|        35 | 7253 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7254 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7255 | `		sxi64 i = (sxi64)d;` |
|         3 | 7256 | `		if( d != (double)i ){` |
|         4 | 7257 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7258 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7259 | `				d` |
|         - | 7260 | `				);` |
|         1 | 7261 | `		}` |
|         1 | 7262 | `	}` |
|         - | 7263 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7264 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7265 | `	{` |
|        35 | 7266 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7267 | `		if( nSizeSigned < 1 ){` |
|         - | 7268 | `			/* size <= 0 -> ValueError */` |
|         6 | 7269 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7270 | `				"ValueError",` |
|         - | 7271 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7272 | `				);` |
|         - | 7273 | `		}` |
|        29 | 7274 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7275 | `	}` |
|        29 | 7276 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7277 | `		/* Return the whole array */` |
|         3 | 7278 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7279 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7280 | `		return PH7_OK;` |
|         - | 7281 | `	}` |
|        27 | 7282 | `	bPreserve = 0;` |
|        27 | 7283 | `	if( nArg > 2 ){` |
|         - | 7284 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7285 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7286 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7287 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7288 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7289 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7290 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7291 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7292 | `				"TypeError",` |
|         - | 7293 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7294 | `				ph7_type_name(apArg[2])` |
|         - | 7295 | `				);` |
|         - | 7296 | `		}` |
|        21 | 7297 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7298 | `	}` |
|         - | 7299 | `	/* Start processing */` |
|        27 | 7300 | `	pEntry = pMap->pFirst;` |
|        27 | 7301 | `	nChunk = 0;` |
|        27 | 7302 | `	pChunk = 0;` |
|        27 | 7303 | `	n = pMap->nEntry;` |
|        56 | 7304 | `	for( ;; ){` |
|       113 | 7305 | `		if( n < 1 ){` |
|         - | 7306 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7307 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7308 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7309 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7310 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7311 | `			 * exists. */` |
|        27 | 7312 | `			if( pChunk ){` |
|        27 | 7313 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7314 | `			}` |
|        27 | 7315 | `			break;` |
|         - | 7316 | `		}` |
|        87 | 7317 | `		if( nChunk < 1 ){` |
|        71 | 7318 | `			if( pChunk ){` |
|         - | 7319 | `				/* Put the first chunk */` |
|        45 | 7320 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7321 | `			}` |
|         - | 7322 | `			/* Create a new dimension */` |
|        71 | 7323 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7324 | `												   * will be automatically released as soon we return` |
|         - | 7325 | `												   * from this function */` |
|        71 | 7326 | `			if( pChunk == 0 ){` |
|       ! 0 | 7327 | `				break;` |
|         - | 7328 | `			}` |
|        71 | 7329 | `			nChunk = nSize;` |
|        35 | 7330 | `		}` |
|         - | 7331 | `		/* Insert the entry */` |
|        87 | 7332 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7333 | `		/* Point to the next entry */` |
|        87 | 7334 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7335 | `		nChunk--;` |
|        87 | 7336 | `		n--;` |
|         1 | 7337 | `	}` |
|         - | 7338 | `	/* Return the multidimensional array */` |
|        27 | 7339 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7340 | `	return PH7_OK;` |
|        23 | 7341 | `}` |
|         - | 7342 | `/*` |
|         - | 7343 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7344 | ` *  Pad array to the specified length with a value.` |
|         - | 7345 | ` * $input` |
|         - | 7346 | ` *   Initial array of values to pad.` |
|         - | 7347 | ` * $pad_size` |
|         - | 7348 | ` *   New size of the array.` |
|         - | 7349 | ` * $pad_value` |
|         - | 7350 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7351 | ` */` |
|         - | 7352 | `/*` |
|         - | 7353 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7354 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7355 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7356 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7357 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7358 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7359 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7360 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7361 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7362 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7363 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7364 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7365 | ` */` |
|        50 | 7366 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7367 | `	ph7_context *pCtx,` |
|         - | 7368 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7369 | `	int iArg,              /* 1-based argument position */` |
|         - | 7370 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7371 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7372 | `	)` |
|         1 | 7373 | `{` |
|        51 | 7374 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7375 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7376 | `			"ValueError",` |
|         - | 7377 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7378 | `			zFunc,iArg,zParam` |
|         - | 7379 | `			);` |
|         - | 7380 | `	}` |
|        37 | 7381 | `	return SXRET_OK;` |
|        26 | 7382 | `}` |
|        62 | 7383 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7384 | `{` |
|         - | 7385 | `	ph7_hashmap *pMap;` |
|         - | 7386 | `	ph7_value *pArray;` |
|         - | 7387 | `	sxi64 iLen,iAbs;` |
|         - | 7388 | `	int nEntry;` |
|         - | 7389 | `	sxi32 rc;` |
|        65 | 7390 | `	if( nArg != 3 ){` |
|         4 | 7391 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7392 | `			"ArgumentCountError",` |
|         - | 7393 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7394 | `			nArg` |
|         - | 7395 | `			);` |
|         - | 7396 | `	}` |
|        62 | 7397 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7398 | `		char zBuf[64];` |
|        11 | 7399 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7400 | `			"TypeError",` |
|         - | 7401 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7402 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7403 | `			);` |
|         - | 7404 | `	}` |
|         - | 7405 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7406 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7407 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7408 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7409 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7410 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7411 | `		char zBuf[64];` |
|       ! 0 | 7412 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7413 | `			"TypeError",` |
|         - | 7414 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7415 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7416 | `			);` |
|         - | 7417 | `	}` |
|        55 | 7418 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7419 | `		int nStr;` |
|        11 | 7420 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7421 | `		sxi64 iLong; double dReal;` |
|        11 | 7422 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7423 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7424 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7425 | `				"TypeError",` |
|         - | 7426 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7427 | `				);` |
|         - | 7428 | `		}` |
|         7 | 7429 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7430 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7431 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7432 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7433 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7434 | `					"TypeError",` |
|         - | 7435 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7436 | `					);` |
|         - | 7437 | `			}` |
|         3 | 7438 | `			iLen = (sxi64)dReal;` |
|         3 | 7439 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7440 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7441 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7442 | `					zStr` |
|         - | 7443 | `					);` |
|       ! 0 | 7444 | `			}` |
|         2 | 7445 | `		}else{` |
|         5 | 7446 | `			iLen = iLong;` |
|         - | 7447 | `		}` |
|         4 | 7448 | `	}else{` |
|        45 | 7449 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7450 | `	}` |
|         - | 7451 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7452 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7453 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7454 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7455 | `	 * overflow). */` |
|        51 | 7456 | `	iAbs = iLen;` |
|        51 | 7457 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7458 | `		iAbs = -iAbs;` |
|         7 | 7459 | `	}` |
|        51 | 7460 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7461 | `	if( rc != SXRET_OK ){` |
|        15 | 7462 | `		return rc;` |
|         - | 7463 | `	}` |
|        37 | 7464 | `	nEntry = (int)iLen;` |
|         - | 7465 | `	/* Create a new array */` |
|        37 | 7466 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7467 | `	if( pArray == 0 ){` |
|       ! 0 | 7468 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7469 | `	}` |
|        37 | 7470 | `	if( nEntry < 0 ){` |
|        11 | 7471 | `		nEntry = -nEntry;` |
|        11 | 7472 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7473 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7474 | `			/* Insert given items first */` |
|        25 | 7475 | `			while( nEntry > 0 ){` |
|        19 | 7476 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7477 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7478 | `				}` |
|        19 | 7479 | `				nEntry--;` |
|         1 | 7480 | `			}` |
|         - | 7481 | `			/* Merge the two arrays */` |
|         7 | 7482 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7483 | `		}else{` |
|         5 | 7484 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7485 | `		}` |
|        32 | 7486 | `	}else if( nEntry > 0 ){` |
|        25 | 7487 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7488 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7489 | `			/* Merge the two arrays first */` |
|        19 | 7490 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7491 | `			/* Insert given items */` |
|       275 | 7492 | `			while( nEntry > 0 ){` |
|       257 | 7493 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7494 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7495 | `				}` |
|       257 | 7496 | `				nEntry--;` |
|         1 | 7497 | `			}` |
|        10 | 7498 | `		}else{` |
|         7 | 7499 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7500 | `		}` |
|        13 | 7501 | `	}else{` |
|         - | 7502 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7503 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7504 | `	}` |
|         - | 7505 | `	/* Return the new array */` |
|        37 | 7506 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7507 | `	return PH7_OK;` |
|        34 | 7508 | `}` |
|         - | 7509 | `/*` |
|         - | 7510 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7511 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7512 | ` * Parameters` |
|         - | 7513 | ` * $array` |
|         - | 7514 | ` *   The array in which elements are replaced.` |
|         - | 7515 | ` * $array1` |
|         - | 7516 | ` *   The array from which elements will be extracted.` |
|         - | 7517 | ` * ....` |
|         - | 7518 | ` *  More arrays from which elements will be extracted.` |
|         - | 7519 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7520 | ` * Return` |
|         - | 7521 | ` *  Returns an array.` |
|         - | 7522 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7523 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7524 | ` */` |
|        20 | 7525 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7526 | `{` |
|         - | 7527 | `	ph7_hashmap *pMap;` |
|         - | 7528 | `	ph7_value *pArray;` |
|         - | 7529 | `	int i;` |
|        23 | 7530 | `	if( nArg < 1 ){` |
|       ! 0 | 7531 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7532 | `			"ArgumentCountError",` |
|         - | 7533 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7534 | `			);` |
|         - | 7535 | `	}` |
|        23 | 7536 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7537 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7538 | `			"TypeError",` |
|         - | 7539 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7540 | `			ph7_type_name(apArg[0])` |
|         - | 7541 | `			);` |
|         - | 7542 | `	}` |
|         - | 7543 | `	/* Create a new array */` |
|        20 | 7544 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7545 | `	if( pArray == 0 ){` |
|       ! 0 | 7546 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7547 | `		return PH7_OK;` |
|         - | 7548 | `	}` |
|         - | 7549 | `	/* Overwrite from the first array */` |
|        20 | 7550 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7551 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7552 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7553 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7554 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7555 | `			/* Type mismatch -> TypeError */` |
|         4 | 7556 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7557 | `				"TypeError",` |
|         - | 7558 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7559 | `				i + 1,` |
|         2 | 7560 | `				ph7_type_name(apArg[i])` |
|         - | 7561 | `				);` |
|         - | 7562 | `		}` |
|         - | 7563 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7564 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7565 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7566 | `	}` |
|         - | 7567 | `	/* Return the new array */` |
|        17 | 7568 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7569 | `	return PH7_OK;` |
|        13 | 7570 | `}` |
|         - | 7571 | `/*` |
|         - | 7572 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7573 | ` *  Filters elements of an array using a callback function.` |
|         - | 7574 | ` * Parameters` |
|         - | 7575 | ` *  $input` |
|         - | 7576 | ` *    The array to iterate over` |
|         - | 7577 | ` * $callback` |
|         - | 7578 | ` *    The callback function to use` |
|         - | 7579 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7580 | ` *    will be removed.` |
|         - | 7581 | ` * Return` |
|         - | 7582 | ` *  The filtered array.` |
|         - | 7583 | ` */` |
|        30 | 7584 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7585 | `{` |
|         - | 7586 | `	ph7_hashmap_node *pEntry;` |
|         - | 7587 | `	ph7_hashmap *pMap;` |
|         - | 7588 | `	ph7_value *pArray;` |
|         - | 7589 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7590 | `	ph7_value *pValue;` |
|         - | 7591 | `	sxi32 rc;` |
|         - | 7592 | `	int keep;` |
|         - | 7593 | `	sxu32 n;` |
|        32 | 7594 | `	if( nArg < 1 ){` |
|         - | 7595 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7596 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7597 | `		return PH7_OK;` |
|         - | 7598 | `	}` |
|         - | 7599 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7600 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7601 | `		char zBuf[64];` |
|        19 | 7602 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7603 | `			"TypeError",` |
|         - | 7604 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7605 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7606 | `			);` |
|         - | 7607 | `	}` |
|         - | 7608 | `	/* Create a new array */` |
|        20 | 7609 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7610 | `	if( pArray == 0 ){` |
|       ! 0 | 7611 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7612 | `		return PH7_OK;` |
|         - | 7613 | `	}` |
|         - | 7614 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7615 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7616 | `	pEntry = pMap->pFirst;` |
|        20 | 7617 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7618 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7619 | `	/* Perform the requested operation */` |
|        78 | 7620 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7621 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7622 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7623 | `		if( pValue == 0 ){` |
|         - | 7624 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7625 | `			keep = FALSE;` |
|        64 | 7626 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7627 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7628 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7629 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7630 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7631 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7632 | `					int len;` |
|         3 | 7633 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7634 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7635 | `						"TypeError",` |
|         - | 7636 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7637 | `						zName` |
|         - | 7638 | `						);` |
|       ! 0 | 7639 | `				}else{` |
|       ! 0 | 7640 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7641 | `						"TypeError",` |
|         - | 7642 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7643 | `						ph7_type_name(apArg[1])` |
|         - | 7644 | `						);` |
|         - | 7645 | `				}` |
|         - | 7646 | `			}` |
|        33 | 7647 | `			keep = FALSE;` |
|        33 | 7648 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7649 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7650 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7651 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7652 | `				return PH7_EXCEPTION;` |
|         - | 7653 | `			}` |
|        31 | 7654 | `			if( rc == SXRET_OK ){` |
|         - | 7655 | `				/* Perform a boolean cast */` |
|        31 | 7656 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7657 | `			}` |
|        31 | 7658 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7659 | `		}else{` |
|         - | 7660 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7661 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7662 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7663 | `			 */` |
|        29 | 7664 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7665 | `		}` |
|        59 | 7666 | `		if( keep ){` |
|         - | 7667 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7668 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7669 | `		}` |
|         - | 7670 | `		/* Point to the next entry */` |
|        59 | 7671 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7672 | `	}` |
|        15 | 7673 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7674 | `	return PH7_OK;` |
|        17 | 7675 | `}` |
|         - | 7676 | `/*` |
|         - | 7677 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7678 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7679 | ` * Parameters` |
|         - | 7680 | ` *  $callback` |
|         - | 7681 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7682 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7683 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7684 | ` *   are zipped together.` |
|         - | 7685 | ` *  $array` |
|         - | 7686 | ` *   The first array to run through the callback function.` |
|         - | 7687 | ` *  $arrays` |
|         - | 7688 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7689 | ` * Return` |
|         - | 7690 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7691 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7692 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7693 | ` *  padding shorter arrays with NULL.` |
|         - | 7694 | ` */` |
|        62 | 7695 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7696 | `{` |
|         - | 7697 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7698 | `	ph7_hashmap_node *pEntry;` |
|         - | 7699 | `	ph7_hashmap *pMap;` |
|         - | 7700 | `	ph7_vm *pVm;` |
|         - | 7701 | `	int bNullCallback;` |
|         - | 7702 | `	sxi32 rc;` |
|         - | 7703 | `	int i;` |
|         - | 7704 | `	sxu32 n;` |
|        66 | 7705 | `	if( nArg < 2 ){` |
|       ! 0 | 7706 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7707 | `			"ArgumentCountError",` |
|         - | 7708 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7709 | `			nArg` |
|         - | 7710 | `			);` |
|         - | 7711 | `	}` |
|        66 | 7712 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        66 | 7713 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         8 | 7714 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         6 | 7715 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         8 | 7716 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7717 | `				"TypeError",` |
|         - | 7718 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7719 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7720 | `				zFunc` |
|         - | 7721 | `				);` |
|         - | 7722 | `		}` |
|         3 | 7723 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7724 | `			"TypeError",` |
|         - | 7725 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7726 | `			"no array or string given"` |
|         - | 7727 | `			);` |
|         - | 7728 | `	}` |
|         - | 7729 | `	/* Every remaining argument must be an array */` |
|       125 | 7730 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        71 | 7731 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7732 | `			if( i == 1 ){` |
|         4 | 7733 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7734 | `					"TypeError",` |
|         - | 7735 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7736 | `					ph7_type_name(apArg[1])` |
|         - | 7737 | `					);` |
|         - | 7738 | `			}` |
|       ! 0 | 7739 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7740 | `				"TypeError",` |
|         - | 7741 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7742 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7743 | `				);` |
|         - | 7744 | `		}` |
|        36 | 7745 | `	}` |
|        57 | 7746 | `	pVm = pCtx->pVm;` |
|         - | 7747 | `	/* Create a new array */` |
|        57 | 7748 | `	pArray = ph7_context_new_array(pCtx);` |
|        57 | 7749 | `	if( pArray == 0 ){` |
|       ! 0 | 7750 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7751 | `		return PH7_OK;` |
|         - | 7752 | `	}` |
|        57 | 7753 | `	PH7_MemObjInit(pVm,&sResult);` |
|        57 | 7754 | `	PH7_MemObjInit(pVm,&sKey);` |
|        57 | 7755 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7756 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7757 | `	if( nArg == 2 ){` |
|         - | 7758 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        47 | 7759 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        47 | 7760 | `		pEntry = pMap->pFirst;` |
|       143 | 7761 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7762 | `			/* Extract the node value */` |
|       103 | 7763 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|       103 | 7764 | `			if( pValue ){` |
|         - | 7765 | `				/* Extract the node key */` |
|       103 | 7766 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       103 | 7767 | `				if( bNullCallback ){` |
|         - | 7768 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7769 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7770 | `				}else{` |
|         - | 7771 | `					/* Invoke the supplied callback */` |
|        93 | 7772 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        93 | 7773 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7774 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7775 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7776 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7777 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7778 | `						return PH7_EXCEPTION;` |
|         - | 7779 | `					}` |
|         - | 7780 | `					/* Insert the callback return value */` |
|        89 | 7781 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7782 | `				}` |
|        99 | 7783 | `				PH7_MemObjRelease(&sKey);` |
|        99 | 7784 | `				PH7_MemObjRelease(&sResult);` |
|        48 | 7785 | `			}` |
|         - | 7786 | `			/* Point to the next entry */` |
|        99 | 7787 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        51 | 7788 | `		}` |
|        23 | 7789 | `	}else{` |
|         - | 7790 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7791 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7792 | `		int nArrays = nArg - 1;` |
|         - | 7793 | `		ph7_hashmap_node **apCur;` |
|         - | 7794 | `		ph7_value **apCallArg;` |
|         - | 7795 | `		ph7_value sNull;` |
|        11 | 7796 | `		sxu32 nMax = 0;` |
|        11 | 7797 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7798 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7799 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7800 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7801 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7802 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7803 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7804 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7805 | `			return PH7_OK;` |
|         - | 7806 | `		}` |
|        11 | 7807 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7808 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7809 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7810 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7811 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7812 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7813 | `				nMax = pMap->nEntry;` |
|         6 | 7814 | `			}` |
|        12 | 7815 | `		}` |
|        35 | 7816 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7817 | `			ph7_value *pZip = 0;` |
|        25 | 7818 | `			if( bNullCallback ){` |
|         - | 7819 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7820 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7821 | `			}` |
|        79 | 7822 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7823 | `				ph7_value *pv = &sNull;` |
|        55 | 7824 | `				if( apCur[i] ){` |
|        53 | 7825 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7826 | `					if( pNodeVal ){` |
|        53 | 7827 | `						pv = pNodeVal;` |
|        26 | 7828 | `					}` |
|        53 | 7829 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7830 | `				}` |
|        55 | 7831 | `				if( bNullCallback ){` |
|         9 | 7832 | `					if( pZip ){` |
|         9 | 7833 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7834 | `					}` |
|         5 | 7835 | `				}else{` |
|        47 | 7836 | `					apCallArg[i] = pv;` |
|         - | 7837 | `				}` |
|        28 | 7838 | `			}` |
|        25 | 7839 | `			if( bNullCallback ){` |
|         5 | 7840 | `				if( pZip ){` |
|         5 | 7841 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7842 | `				}` |
|         3 | 7843 | `			}else{` |
|        21 | 7844 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7845 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7846 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7847 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7848 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7849 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7850 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7851 | `					return PH7_EXCEPTION;` |
|         - | 7852 | `				}` |
|        21 | 7853 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7854 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7855 | `			}` |
|        13 | 7856 | `		}` |
|        11 | 7857 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7858 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7859 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7860 | `	}` |
|        53 | 7861 | `	PH7_MemObjRelease(&sKey);` |
|        53 | 7862 | `	PH7_MemObjRelease(&sResult);` |
|        53 | 7863 | `	ph7_result_value(pCtx,pArray);` |
|        53 | 7864 | `	return PH7_OK;` |
|        35 | 7865 | `}` |
|         - | 7866 | `/*` |
|         - | 7867 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7868 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7869 | ` * Parameters` |
|         - | 7870 | ` *  $array` |
|         - | 7871 | ` *   The input array.` |
|         - | 7872 | ` *  $callback` |
|         - | 7873 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7874 | ` *  $initial` |
|         - | 7875 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7876 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7877 | ` * Return` |
|         - | 7878 | ` *  Returns the resulting value.` |
|         - | 7879 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7880 | ` */` |
|        30 | 7881 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7882 | `{` |
|         - | 7883 | `	ph7_hashmap_node *pEntry;` |
|         - | 7884 | `	ph7_hashmap *pMap;` |
|         - | 7885 | `	ph7_value *pValue;` |
|         - | 7886 | `	ph7_value sResult;` |
|         - | 7887 | `	sxi32 rc;` |
|         - | 7888 | `	sxu32 n;` |
|        35 | 7889 | `	if( nArg < 2 ){` |
|       ! 0 | 7890 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7891 | `			"ArgumentCountError",` |
|         - | 7892 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7893 | `			nArg` |
|         - | 7894 | `			);` |
|         - | 7895 | `	}` |
|        35 | 7896 | `	if( nArg > 3 ){` |
|         4 | 7897 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7898 | `			"ArgumentCountError",` |
|         - | 7899 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7900 | `			nArg` |
|         - | 7901 | `			);` |
|         - | 7902 | `	}` |
|        33 | 7903 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7904 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7905 | `			"TypeError",` |
|         - | 7906 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7907 | `			ph7_type_name(apArg[0])` |
|         - | 7908 | `			);` |
|         - | 7909 | `	}` |
|        31 | 7910 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7911 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7912 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7913 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7914 | `				"TypeError",` |
|         - | 7915 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7916 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7917 | `				zFunc` |
|         - | 7918 | `				);` |
|         - | 7919 | `		}` |
|         9 | 7920 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7921 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7922 | `				"TypeError",` |
|         - | 7923 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7924 | `				"array callback must have exactly two members"` |
|         - | 7925 | `				);` |
|         - | 7926 | `		}` |
|         6 | 7927 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7928 | `			"TypeError",` |
|         - | 7929 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7930 | `			"no array or string given"` |
|         - | 7931 | `			);` |
|         - | 7932 | `	}` |
|         - | 7933 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7934 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7935 | `	/* Assume a NULL initial value */` |
|        19 | 7936 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7937 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7938 | `	if( nArg > 2 ){` |
|         - | 7939 | `		/* Set the initial value */` |
|        13 | 7940 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7941 | `	}` |
|         - | 7942 | `	/* Perform the requested operation */` |
|        19 | 7943 | `	pEntry = pMap->pFirst;` |
|        55 | 7944 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7945 | `		/* Extract the node value */` |
|        39 | 7946 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7947 | `		/* Invoke the supplied callback */` |
|        39 | 7948 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7949 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7950 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7951 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7952 | `			return PH7_EXCEPTION;` |
|         - | 7953 | `		}` |
|         - | 7954 | `		/* Point to the next entry */` |
|        37 | 7955 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7956 | `	}` |
|        17 | 7957 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7958 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7959 | `	return PH7_OK;` |
|        20 | 7960 | `}` |
|         - | 7961 | `/*` |
|         - | 7962 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7963 | ` *  Apply a user function to every member of an array.` |
|         - | 7964 | ` * Parameters` |
|         - | 7965 | ` *  $array` |
|         - | 7966 | ` *   The input array.` |
|         - | 7967 | ` *  $funcname` |
|         - | 7968 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7969 | ` *   the first, and the key/index second.` |
|         - | 7970 | ` * Note:` |
|         - | 7971 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7972 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7973 | ` *  be made in the original array itself.` |
|         - | 7974 | ` *  $userdata` |
|         - | 7975 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7976 | ` *   to the callback funcname.` |
|         - | 7977 | ` * Return` |
|         - | 7978 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7979 | ` */` |
|        36 | 7980 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7981 | `{` |
|         - | 7982 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7983 | `	ph7_hashmap_node *pEntry;` |
|         - | 7984 | `	ph7_hashmap *pMap;` |
|         - | 7985 | `	sxu32 n;` |
|        41 | 7986 | `	if( nArg < 2 ){` |
|       ! 0 | 7987 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7988 | `			"ArgumentCountError",` |
|         - | 7989 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7990 | `			nArg` |
|         - | 7991 | `			);` |
|         - | 7992 | `	}` |
|        41 | 7993 | `	if( nArg > 3 ){` |
|         4 | 7994 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7995 | `			"ArgumentCountError",` |
|         - | 7996 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7997 | `			nArg` |
|         - | 7998 | `			);` |
|         - | 7999 | `	}` |
|        39 | 8000 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8001 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8002 | `			"TypeError",` |
|         - | 8003 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8004 | `			ph7_type_name(apArg[0])` |
|         - | 8005 | `			);` |
|         - | 8006 | `	}` |
|        37 | 8007 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        17 | 8008 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         6 | 8009 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         8 | 8010 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8011 | `				"TypeError",` |
|         - | 8012 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8013 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 8014 | `				zFunc` |
|         - | 8015 | `				);` |
|         - | 8016 | `		}` |
|        12 | 8017 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8018 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8019 | `				"TypeError",` |
|         - | 8020 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8021 | `				"array callback must have exactly two members"` |
|         - | 8022 | `				);` |
|         - | 8023 | `		}` |
|         6 | 8024 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8025 | `			"TypeError",` |
|         - | 8026 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8027 | `			"no array or string given"` |
|         - | 8028 | `			);` |
|         - | 8029 | `	}` |
|        21 | 8030 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 8031 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 8032 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 8033 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8034 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 8035 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 8036 | `	/* Perform the desired operation */` |
|        21 | 8037 | `	pEntry = pMap->pFirst;` |
|        61 | 8038 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8039 | `		/* Extract the node value */` |
|        43 | 8040 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 8041 | `		if( pValue ){` |
|         - | 8042 | `			sxi32 rcW;` |
|         - | 8043 | `			/* Extract the entry key */` |
|        43 | 8044 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8045 | `			/* Invoke the supplied callback */` |
|        43 | 8046 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 8047 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 8048 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 8049 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 8050 | `				return PH7_EXCEPTION;` |
|         - | 8051 | `			}` |
|        20 | 8052 | `		}` |
|         - | 8053 | `		/* Point to the next entry */` |
|        41 | 8054 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 8055 | `	}` |
|         - | 8056 | `	/* All done, return TRUE */` |
|        19 | 8057 | `	ph7_result_bool(pCtx,1);` |
|        19 | 8058 | `	return PH7_OK;` |
|        23 | 8059 | `}` |
|         - | 8060 | `/*` |
|         - | 8061 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 8062 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 8063 | ` */` |
|        22 | 8064 | `static sxi32 HashmapWalkRecursive(` |
|         - | 8065 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 8066 | `	ph7_value *pCallback, /* User callback */` |
|         - | 8067 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 8068 | `	int iNest             /* Nesting level */` |
|         - | 8069 | `	)` |
|         1 | 8070 | `{` |
|         - | 8071 | `	ph7_hashmap_node *pEntry;` |
|         - | 8072 | `	ph7_value *pValue,sKey;` |
|         - | 8073 | `	sxi32 rc;` |
|         - | 8074 | `	sxu32 n;` |
|         - | 8075 | `	/* Iterate through hashmap entries */` |
|        23 | 8076 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 8077 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 8078 | `	pEntry = pMap->pFirst;` |
|        59 | 8079 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8080 | `		/* Extract the node value */` |
|        37 | 8081 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 8082 | `		if( pValue ){` |
|        37 | 8083 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 8084 | `				if( iNest < 32 ){` |
|         - | 8085 | `					/* Recurse */` |
|        11 | 8086 | `					iNest++;` |
|        11 | 8087 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8088 | `					iNest--;` |
|        11 | 8089 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8090 | `						return PH7_EXCEPTION;` |
|         - | 8091 | `					}` |
|         5 | 8092 | `				}` |
|         6 | 8093 | `			}else{` |
|         - | 8094 | `				/* Extract the node key */` |
|        27 | 8095 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8096 | `				/* Invoke the supplied callback */` |
|        27 | 8097 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8098 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8099 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8100 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8101 | `					return PH7_EXCEPTION;` |
|         - | 8102 | `				}` |
|         - | 8103 | `			}` |
|        18 | 8104 | `		}` |
|         - | 8105 | `		/* Point to the next entry */` |
|        37 | 8106 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8107 | `	}` |
|        23 | 8108 | `	return PH7_OK;` |
|        12 | 8109 | `}` |
|         - | 8110 | `/*` |
|         - | 8111 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8112 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8113 | ` * Parameters` |
|         - | 8114 | ` *  $array` |
|         - | 8115 | ` *   The input array.` |
|         - | 8116 | ` *  $funcname` |
|         - | 8117 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8118 | ` *   the first, and the key/index second.` |
|         - | 8119 | ` * Note:` |
|         - | 8120 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8121 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8122 | ` *  be made in the original array itself.` |
|         - | 8123 | ` *  $userdata` |
|         - | 8124 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8125 | ` *   to the callback funcname.` |
|         - | 8126 | ` * Return` |
|         - | 8127 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8128 | ` */` |
|        26 | 8129 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8130 | `{` |
|         - | 8131 | `	ph7_hashmap *pMap;` |
|        31 | 8132 | `	if( nArg < 2 ){` |
|       ! 0 | 8133 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8134 | `			"ArgumentCountError",` |
|         - | 8135 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8136 | `			nArg` |
|         - | 8137 | `			);` |
|         - | 8138 | `	}` |
|        31 | 8139 | `	if( nArg > 3 ){` |
|         4 | 8140 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8141 | `			"ArgumentCountError",` |
|         - | 8142 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8143 | `			nArg` |
|         - | 8144 | `			);` |
|         - | 8145 | `	}` |
|        29 | 8146 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8147 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8148 | `			"TypeError",` |
|         - | 8149 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8150 | `			ph7_type_name(apArg[0])` |
|         - | 8151 | `			);` |
|         - | 8152 | `	}` |
|        27 | 8153 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8154 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8155 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8156 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8157 | `				"TypeError",` |
|         - | 8158 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8159 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8160 | `				zFunc` |
|         - | 8161 | `				);` |
|         - | 8162 | `		}` |
|        12 | 8163 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8164 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8165 | `				"TypeError",` |
|         - | 8166 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8167 | `				"array callback must have exactly two members"` |
|         - | 8168 | `				);` |
|         - | 8169 | `		}` |
|         6 | 8170 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8171 | `			"TypeError",` |
|         - | 8172 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8173 | `			"no array or string given"` |
|         - | 8174 | `			);` |
|         - | 8175 | `	}` |
|         - | 8176 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8177 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8178 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8179 | `	/* Perform the desired operation */` |
|        13 | 8180 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8181 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8182 | `		return PH7_EXCEPTION;` |
|         - | 8183 | `	}` |
|         - | 8184 | `	/* All done, return TRUE */` |
|        13 | 8185 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8186 | `	return PH7_OK;` |
|        18 | 8187 | `}` |
|         - | 8188 | `/*` |
|         - | 8189 | ` * bool array_is_list(array $array)` |
|         - | 8190 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8191 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8192 | ` * Return` |
|         - | 8193 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8194 | ` */` |
|         - | 8195 | `/*` |
|         - | 8196 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8197 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8198 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8199 | ` */` |
|       314 | 8200 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8201 | `{` |
|       315 | 8202 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       315 | 8203 | `	sxi64 iExpect = 0;` |
|         - | 8204 | `	sxu32 n;` |
|       721 | 8205 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       541 | 8206 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8207 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       135 | 8208 | `			return 0;` |
|         - | 8209 | `		}` |
|       407 | 8210 | `		++iExpect;` |
|       407 | 8211 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       204 | 8212 | `	}` |
|       181 | 8213 | `	return 1;` |
|       158 | 8214 | `}` |
|        12 | 8215 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8216 | `{` |
|        13 | 8217 | `	if( nArg < 1 ){` |
|       ! 0 | 8218 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8219 | `			"ArgumentCountError",` |
|         - | 8220 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8221 | `			);` |
|         - | 8222 | `	}` |
|        13 | 8223 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8224 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8225 | `			"TypeError",` |
|         - | 8226 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8227 | `			ph7_type_name(apArg[0])` |
|         - | 8228 | `			);` |
|         - | 8229 | `	}` |
|        13 | 8230 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8231 | `	return PH7_OK;` |
|         7 | 8232 | `}` |
|         - | 8233 | `/*` |
|         - | 8234 | ` * mixed array_first(array $array)` |
|         - | 8235 | ` * mixed array_last(array $array)` |
|         - | 8236 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8237 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8238 | ` *  untouched (unlike reset()/end()).` |
|         - | 8239 | ` */` |
|        18 | 8240 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8241 | `{` |
|         - | 8242 | `	ph7_hashmap *pMap;` |
|         - | 8243 | `	ph7_hashmap_node *pNode;` |
|         - | 8244 | `	ph7_value *pVal;` |
|        19 | 8245 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8246 | `	if( nArg < 1 ){` |
|       ! 0 | 8247 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8248 | `			"ArgumentCountError",` |
|         - | 8249 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8250 | `			zName` |
|         - | 8251 | `			);` |
|         - | 8252 | `	}` |
|        19 | 8253 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8254 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8255 | `			"TypeError",` |
|         - | 8256 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8257 | `			zName,` |
|         1 | 8258 | `			ph7_type_name(apArg[0])` |
|         - | 8259 | `			);` |
|         - | 8260 | `	}` |
|        17 | 8261 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8262 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8263 | `	if( pNode == 0 ){` |
|         - | 8264 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8265 | `		ph7_result_null(pCtx);` |
|         5 | 8266 | `		return PH7_OK;` |
|         - | 8267 | `	}` |
|        13 | 8268 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8269 | `	if( pVal ){` |
|        13 | 8270 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8271 | `	}else{` |
|       ! 0 | 8272 | `		ph7_result_null(pCtx);` |
|         - | 8273 | `	}` |
|        13 | 8274 | `	return PH7_OK;` |
|        10 | 8275 | `}` |
|         8 | 8276 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8277 | `{` |
|         9 | 8278 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8279 | `}` |
|        10 | 8280 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8281 | `{` |
|        11 | 8282 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8283 | `}` |
|         - | 8284 | `/*` |
|         - | 8285 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8286 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8287 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8288 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8289 | ` *  untouched.` |
|         - | 8290 | ` */` |
|        22 | 8291 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8292 | `{` |
|         - | 8293 | `	ph7_hashmap *pMap;` |
|         - | 8294 | `	ph7_hashmap_node *pNode;` |
|        23 | 8295 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8296 | `	if( nArg < 1 ){` |
|       ! 0 | 8297 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8298 | `			"ArgumentCountError",` |
|         - | 8299 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8300 | `			zName` |
|         - | 8301 | `			);` |
|         - | 8302 | `	}` |
|        23 | 8303 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8304 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8305 | `			"TypeError",` |
|         - | 8306 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8307 | `			zName,` |
|         1 | 8308 | `			ph7_type_name(apArg[0])` |
|         - | 8309 | `			);` |
|         - | 8310 | `	}` |
|        21 | 8311 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8312 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8313 | `	if( pNode == 0 ){` |
|         - | 8314 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8315 | `		ph7_result_null(pCtx);` |
|         5 | 8316 | `		return PH7_OK;` |
|         - | 8317 | `	}` |
|        17 | 8318 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8319 | `	return PH7_OK;` |
|        12 | 8320 | `}` |
|        10 | 8321 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8322 | `{` |
|        11 | 8323 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8324 | `}` |
|        12 | 8325 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8326 | `{` |
|        13 | 8327 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8328 | `}` |
|         - | 8329 | `/*` |
|         - | 8330 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8331 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8332 | ` * array_column() for both the column value and the index key.` |
|         - | 8333 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8334 | ` * container or the key is absent.` |
|         - | 8335 | ` */` |
|        32 | 8336 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8337 | `{` |
|        33 | 8338 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8339 | `		ph7_hashmap_node *pNode;` |
|        25 | 8340 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8341 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8342 | `		}` |
|        11 | 8343 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8344 | `		ph7_value sName;` |
|         - | 8345 | `		const char *zName;` |
|         - | 8346 | `		ph7_value *pAttr;` |
|         - | 8347 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8348 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8349 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8350 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8351 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8352 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8353 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8354 | `		return pAttr;` |
|         - | 8355 | `	}` |
|         5 | 8356 | `	return 0;` |
|        17 | 8357 | `}` |
|         - | 8358 | `/*` |
|         - | 8359 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8360 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8361 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8362 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8363 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8364 | ` *  Each row may be an array or an object.` |
|         - | 8365 | ` */` |
|        12 | 8366 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8367 | `{` |
|         - | 8368 | `	ph7_hashmap_node *pNode;` |
|         - | 8369 | `	ph7_hashmap *pMap;` |
|         - | 8370 | `	ph7_value *pArray;` |
|         - | 8371 | `	ph7_value *pRow;` |
|         - | 8372 | `	ph7_value *pCol;` |
|         - | 8373 | `	ph7_value *pIdx;` |
|         - | 8374 | `	int bWantCol;` |
|         - | 8375 | `	int bWantIdx;` |
|         - | 8376 | `	sxu32 n;` |
|        13 | 8377 | `	if( nArg < 2 ){` |
|       ! 0 | 8378 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8379 | `			"ArgumentCountError",` |
|         - | 8380 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8381 | `			nArg` |
|         - | 8382 | `			);` |
|         - | 8383 | `	}` |
|        13 | 8384 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8385 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8386 | `			"TypeError",` |
|         - | 8387 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8388 | `			ph7_type_name(apArg[0])` |
|         - | 8389 | `			);` |
|         - | 8390 | `	}` |
|        13 | 8391 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8392 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8393 | `	if( pArray == 0 ){` |
|       ! 0 | 8394 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8395 | `		return PH7_OK;` |
|         - | 8396 | `	}` |
|         - | 8397 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8398 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8399 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8400 | `	pNode = pMap->pFirst;` |
|        33 | 8401 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8402 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8403 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8404 | `		if( pRow == 0 ){` |
|       ! 0 | 8405 | `			continue;` |
|         - | 8406 | `		}` |
|        21 | 8407 | `		if( bWantCol ){` |
|        19 | 8408 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8409 | `			if( pCol == 0 ){` |
|         - | 8410 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8411 | `				continue;` |
|         - | 8412 | `			}` |
|         9 | 8413 | `		}else{` |
|         3 | 8414 | `			pCol = pRow;` |
|         - | 8415 | `		}` |
|        19 | 8416 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8417 | `		if( pIdx ){` |
|        13 | 8418 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8419 | `		}else{` |
|         7 | 8420 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8421 | `		}` |
|        10 | 8422 | `	}` |
|        13 | 8423 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8424 | `	return PH7_OK;` |
|         7 | 8425 | `}` |
|         - | 8426 | `/*` |
|         - | 8427 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8428 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8429 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8430 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8431 | ` */` |
|        28 | 8432 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8433 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8434 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8435 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8436 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8437 | `	)` |
|         1 | 8438 | `{` |
|         - | 8439 | `	ph7_hashmap_node *pEntry;` |
|         - | 8440 | `	ph7_hashmap *pMap;` |
|         - | 8441 | `	ph7_value *pValue;` |
|         - | 8442 | `	ph7_value *apCbArg[2];` |
|         - | 8443 | `	ph7_value sKey;` |
|         - | 8444 | `	ph7_value sResult;` |
|         - | 8445 | `	sxi32 rc;` |
|         - | 8446 | `	sxu32 n;` |
|        29 | 8447 | `	*ppMatch = 0;` |
|        29 | 8448 | `	if( nArg < 2 ){` |
|       ! 0 | 8449 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8450 | `			"ArgumentCountError",` |
|         - | 8451 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8452 | `			zName,nArg` |
|         - | 8453 | `			);` |
|         - | 8454 | `	}` |
|        29 | 8455 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8456 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8457 | `			"TypeError",` |
|         - | 8458 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8459 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8460 | `			);` |
|         - | 8461 | `	}` |
|        29 | 8462 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8463 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8464 | `			"TypeError",` |
|         - | 8465 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8466 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8467 | `			);` |
|         - | 8468 | `	}` |
|        29 | 8469 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8470 | `	pEntry = pMap->pFirst;` |
|        29 | 8471 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8472 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8473 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8474 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8475 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8476 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8477 | `		if( pValue ){` |
|         - | 8478 | `			/* The callback receives ($value, $key). */` |
|        59 | 8479 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8480 | `			apCbArg[0] = pValue;` |
|        59 | 8481 | `			apCbArg[1] = &sKey;` |
|        59 | 8482 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8483 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8484 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8485 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8486 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8487 | `				return PH7_EXCEPTION;` |
|         - | 8488 | `			}` |
|        59 | 8489 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8490 | `				*ppMatch = pEntry;` |
|        15 | 8491 | `				break;` |
|         - | 8492 | `			}` |
|        22 | 8493 | `		}` |
|        45 | 8494 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8495 | `	}` |
|        29 | 8496 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8497 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8498 | `	return PH7_OK;` |
|        15 | 8499 | `}` |
|         - | 8500 | `/*` |
|         - | 8501 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8502 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8503 | ` *  is truthy, or NULL if none match.` |
|         - | 8504 | ` */` |
|         6 | 8505 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8506 | `{` |
|         - | 8507 | `	ph7_hashmap_node *pMatch;` |
|         - | 8508 | `	ph7_value *pVal;` |
|         - | 8509 | `	sxi32 rc;` |
|         7 | 8510 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8511 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8512 | `		return rc;` |
|         - | 8513 | `	}` |
|         7 | 8514 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8515 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8516 | `	}else{` |
|         3 | 8517 | `		ph7_result_null(pCtx);` |
|         - | 8518 | `	}` |
|         7 | 8519 | `	return PH7_OK;` |
|         4 | 8520 | `}` |
|         - | 8521 | `/*` |
|         - | 8522 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8523 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8524 | ` *  is truthy, or NULL if none match.` |
|         - | 8525 | ` */` |
|         6 | 8526 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8527 | `{` |
|         - | 8528 | `	ph7_hashmap_node *pMatch;` |
|         - | 8529 | `	sxi32 rc;` |
|         7 | 8530 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8531 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8532 | `		return rc;` |
|         - | 8533 | `	}` |
|         7 | 8534 | `	if( pMatch == 0 ){` |
|         3 | 8535 | `		ph7_result_null(pCtx);` |
|         6 | 8536 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8537 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8538 | `	}else{` |
|         4 | 8539 | `		ph7_result_string(pCtx,` |
|         2 | 8540 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8541 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8542 | `	}` |
|         7 | 8543 | `	return PH7_OK;` |
|         4 | 8544 | `}` |
|         - | 8545 | `/*` |
|         - | 8546 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8547 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8548 | ` *  FALSE for an empty array.` |
|         - | 8549 | ` */` |
|         8 | 8550 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8551 | `{` |
|         - | 8552 | `	ph7_hashmap_node *pMatch;` |
|         - | 8553 | `	sxi32 rc;` |
|         9 | 8554 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8555 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8556 | `		return rc;` |
|         - | 8557 | `	}` |
|         9 | 8558 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8559 | `	return PH7_OK;` |
|         5 | 8560 | `}` |
|         - | 8561 | `/*` |
|         - | 8562 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8563 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8564 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8565 | ` */` |
|         8 | 8566 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8567 | `{` |
|         - | 8568 | `	ph7_hashmap_node *pMatch;` |
|         - | 8569 | `	sxi32 rc;` |
|         9 | 8570 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8571 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8572 | `		return rc;` |
|         - | 8573 | `	}` |
|         9 | 8574 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8575 | `	return PH7_OK;` |
|         5 | 8576 | `}` |
|         - | 8577 | `/*` |
|         - | 8578 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8579 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8580 | ` */` |
|         - | 8581 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8582 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8583 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8584 | `{` |
|        84 | 8585 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8586 | `	(void)pVm;` |
|        84 | 8587 | `	p->nCount++;` |
|        84 | 8588 | `	if( p->pArray ){` |
|         - | 8589 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8590 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8591 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8592 | `	}` |
|        84 | 8593 | `	return SXRET_OK;` |
|         4 | 8594 | `}` |
|         - | 8595 | `/*` |
|         - | 8596 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8597 | ` */` |
|        30 | 8598 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8599 | `{` |
|         - | 8600 | `	struct IterCollect sCol;` |
|         - | 8601 | `	ph7_value *pArray;` |
|         - | 8602 | `	sxi32 rc;` |
|        34 | 8603 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8604 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8605 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8606 | `	sCol.pArray = pArray;` |
|        34 | 8607 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8608 | `	sCol.nCount = 0;` |
|        34 | 8609 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8610 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8611 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8612 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8613 | `		sxu32 n;` |
|         9 | 8614 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8615 | `			ph7_value sKey, *pVal;` |
|         7 | 8616 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8617 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8618 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8619 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8620 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8621 | `			pEntry = pEntry->pPrev;` |
|         4 | 8622 | `		}` |
|         3 | 8623 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8624 | `		return PH7_OK;` |
|         - | 8625 | `	}` |
|        32 | 8626 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8627 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8628 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8629 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8630 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8631 | `			ph7_type_name(apArg[0]));` |
|         - | 8632 | `	}` |
|        30 | 8633 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8634 | `	return PH7_OK;` |
|        19 | 8635 | `}` |
|         - | 8636 | `/*` |
|         - | 8637 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8638 | ` */` |
|         8 | 8639 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8640 | `{` |
|         - | 8641 | `	struct IterCollect sCol;` |
|         - | 8642 | `	sxi32 rc;` |
|         9 | 8643 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8644 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8645 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8646 | `		return PH7_OK;` |
|         - | 8647 | `	}` |
|         7 | 8648 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8649 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8650 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8651 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8652 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8653 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8654 | `			ph7_type_name(apArg[0]));` |
|         - | 8655 | `	}` |
|         7 | 8656 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8657 | `	return PH7_OK;` |
|         5 | 8658 | `}` |
|         - | 8659 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8660 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8661 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8662 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8663 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8664 | `{` |
|        33 | 8665 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8666 | `	ph7_value sResult;` |
|         - | 8667 | `	SySet aArg;` |
|         - | 8668 | `	sxi32 rc;` |
|         - | 8669 | `	int bContinue;` |
|        16 | 8670 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8671 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8672 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8673 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8674 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8675 | `		sxu32 n;` |
|        17 | 8676 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8677 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8678 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8679 | `			pEntry = pEntry->pPrev;` |
|         5 | 8680 | `		}` |
|         4 | 8681 | `	}` |
|        33 | 8682 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8683 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8684 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8685 | `	SySetRelease(&aArg);` |
|        33 | 8686 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8687 | `	p->nCount++;` |
|        31 | 8688 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8689 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8690 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8691 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8692 | `}` |
|         - | 8693 | `/*` |
|         - | 8694 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8695 | ` */` |
|        12 | 8696 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8697 | `{` |
|         - | 8698 | `	struct IterApply sApp;` |
|         - | 8699 | `	sxi32 rc;` |
|        13 | 8700 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8701 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8702 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8703 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8704 | `	}` |
|        13 | 8705 | `	sApp.pCallback = apArg[1];` |
|        13 | 8706 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8707 | `	sApp.nCount = 0;` |
|        13 | 8708 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8709 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8710 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8711 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8712 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8713 | `			ph7_type_name(apArg[0]));` |
|         - | 8714 | `	}` |
|        11 | 8715 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8716 | `	return PH7_OK;` |
|         7 | 8717 | `}` |
|         - | 8718 | `/*` |
|         - | 8719 | ` * Table of hashmap functions.` |
|         - | 8720 | ` */` |
|         - | 8721 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8722 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8723 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8724 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8725 | `	{"count",             ph7_hashmap_count },` |
|         - | 8726 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8727 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8728 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8729 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8730 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8731 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8732 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8733 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8734 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8735 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8736 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8737 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8738 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8739 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8740 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8741 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8742 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8743 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8744 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8745 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8746 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8747 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8748 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8749 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8750 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8751 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8752 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8753 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8754 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8755 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8756 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8757 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8758 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8759 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8760 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8761 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8762 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8763 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8764 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8765 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8766 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8767 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8768 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8769 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8770 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8771 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8772 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8773 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8774 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8775 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8776 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8777 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8778 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8779 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8780 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8781 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8782 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8783 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8784 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8785 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8786 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8787 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8788 | `	{"current",           ph7_hashmap_current },` |
|         - | 8789 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8790 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8791 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8792 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8793 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8794 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8795 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8796 | `};` |
|         - | 8797 | `/*` |
|         - | 8798 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8799 | ` */` |
|      3356 | 8800 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8801 | `{` |
|         - | 8802 | `	sxu32 n;` |
|    251705 | 8803 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    248349 | 8804 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    124177 | 8805 | `	}` |
|      3361 | 8806 | `}` |
|         - | 8807 | `/*` |
|         - | 8808 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8809 | ` * the BLOB given as the first argument.` |
|         - | 8810 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8811 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8812 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8813 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8814 | ` */` |
|         - | 8815 | `/*` |
|         - | 8816 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8817 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8818 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8819 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8820 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8821 | ` */` |
|       120 | 8822 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8823 | `{` |
|       122 | 8824 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8825 | `	ph7_value *pObj;` |
|       122 | 8826 | `	sxu32 n = 0;` |
|         - | 8827 | `	int isRef;` |
|       122 | 8828 | `	sxi32 rc = SXRET_OK;` |
|         - | 8829 | `	int i;` |
|       195 | 8830 | `	for(;;){` |
|       392 | 8831 | `		if( n >= pMap->nEntry ){` |
|       122 | 8832 | `			break;` |
|         - | 8833 | `		}` |
|       272 | 8834 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8835 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8836 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       540 | 8837 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       270 | 8838 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       272 | 8839 | `		if( ShowType ){` |
|         - | 8840 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8841 | `			 * on the next line at the same indent (php). */` |
|       104 | 8842 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        70 | 8843 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        36 | 8844 | `			}` |
|        36 | 8845 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8846 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8847 | `			}else{` |
|        20 | 8848 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8849 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8850 | `			}` |
|        36 | 8851 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        36 | 8852 | `			if( pObj ){` |
|        36 | 8853 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        36 | 8854 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8855 | `					break;` |
|         - | 8856 | `				}` |
|        17 | 8857 | `			}` |
|        19 | 8858 | `		}else{` |
|         - | 8859 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8860 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8861 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8862 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8863 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8864 | `			}` |
|       238 | 8865 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8866 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8867 | `			}else{` |
|       170 | 8868 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8869 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8870 | `			}` |
|       236 | 8871 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8872 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8873 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8874 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8875 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8876 | `					break;` |
|         - | 8877 | `				}` |
|        13 | 8878 | `			}else{` |
|       214 | 8879 | `				if( pObj ){` |
|       214 | 8880 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8881 | `				}` |
|       214 | 8882 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8883 | `			}` |
|         - | 8884 | `		}` |
|         - | 8885 | `		/* Point to the next entry */` |
|       272 | 8886 | `		n++;` |
|       272 | 8887 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8888 | `	}` |
|       122 | 8889 | `	return rc;` |
|         2 | 8890 | `}` |
|       116 | 8891 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8892 | `{` |
|         - | 8893 | `	sxi32 rc;` |
|         - | 8894 | `	int i;` |
|       118 | 8895 | `	if( nDepth > 31 ){` |
|         - | 8896 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8897 | `		/* Nesting limit reached */` |
|       ! 0 | 8898 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8899 | `		return SXERR_LIMIT;` |
|         - | 8900 | `	}` |
|       118 | 8901 | `	if( ShowType ){` |
|         - | 8902 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8903 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8904 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8905 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8906 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8907 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8908 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8909 | `		}` |
|        14 | 8910 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8911 | `		return rc;` |
|         - | 8912 | `	}` |
|         - | 8913 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8914 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8915 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8916 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8917 | `	}` |
|       105 | 8918 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8919 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8920 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8921 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8922 | `	}` |
|       105 | 8923 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8924 | `	return rc;` |
|        60 | 8925 | `}` |
|         - | 8926 | `/*` |
|         - | 8927 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8928 | ` * retrieved entry.` |
|         - | 8929 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8930 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8931 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8932 | ` * a value different from PH7_OK.` |
|         - | 8933 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8934 | ` */` |
|     34222 | 8935 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8936 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8937 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8938 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8939 | `	)` |
|         5 | 8940 | `{` |
|         - | 8941 | `	ph7_hashmap_node *pEntry;` |
|         - | 8942 | `	ph7_value sKey,sValue;` |
|         - | 8943 | `	sxi32 rc;` |
|         - | 8944 | `	sxu32 n;` |
|         - | 8945 | `	/* Initialize walker parameter */` |
|     34227 | 8946 | `	rc = SXRET_OK;` |
|     34227 | 8947 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34227 | 8948 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34227 | 8949 | `	n = pMap->nEntry;` |
|     34227 | 8950 | `	pEntry = pMap->pFirst;` |
|         - | 8951 | `	/* Start the iteration process */` |
|     93720 | 8952 | `	for(;;){` |
|    187445 | 8953 | `		if( n < 1 ){` |
|     34227 | 8954 | `			break;` |
|         - | 8955 | `		}` |
|         - | 8956 | `		/* Extract a copy of the key and a copy the current value */` |
|    153223 | 8957 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    153223 | 8958 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8959 | `		/* Invoke the user callback */` |
|    153223 | 8960 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8961 | `		/* Release the copy of the key and the value */` |
|    153223 | 8962 | `		PH7_MemObjRelease(&sKey);` |
|    153223 | 8963 | `		PH7_MemObjRelease(&sValue);` |
|    153223 | 8964 | `		if( rc != PH7_OK ){` |
|         - | 8965 | `			/* Callback request an operation abort */` |
|       ! 0 | 8966 | `			return SXERR_ABORT;` |
|         - | 8967 | `		}` |
|         - | 8968 | `		/* Point to the next entry */` |
|    153223 | 8969 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    153223 | 8970 | `		n--;` |
|         5 | 8971 | `	}` |
|         - | 8972 | `	/* All done */` |
|     34227 | 8973 | `	return SXRET_OK;` |
|     17116 | 8974 | `}` |
|         - | 8975 |  |
