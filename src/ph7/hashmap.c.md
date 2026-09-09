# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3947/4430 lines (89.10%)

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
|   7492016 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7492021 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7492021 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    634158 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    634163 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    634163 |   35 | `	sxu32 nH = 5381;` |
|    634163 |   36 | `	zEnd = &zIn[nLen];` |
|    717122 |   37 | `	for(;;){` |
|   1434249 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1227633 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1099717 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    957753 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    634163 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      2046 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      2051 |   54 | `	sxi64 iCount = 0;` |
|      2051 |   55 | `	if( !bRecursive ){` |
|      1877 |   56 | `		iCount = pMap->nEntry;` |
|       941 |   57 | `	}else{` |
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
|      2051 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3188302 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3188307 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3188307 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3188307 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3188307 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3188307 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3188307 |  112 | `	pNode->nHash = nHash;` |
|   3188307 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3188307 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3188307 |  115 | `	return pNode;` |
|   1594156 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    262836 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    262841 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    262841 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    262841 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    262841 |  133 | `	pNode->pMap  = &(*pMap);` |
|    262841 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    262841 |  135 | `	pNode->nHash = nHash;` |
|    262841 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    262841 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    262841 |  138 | `	pNode->nValIdx = nValIdx;` |
|    262841 |  139 | `	return pNode;` |
|    131423 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3451138 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3451143 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2976219 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2976219 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1488107 |  150 | `	}` |
|   3451143 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3451143 |  153 | `	if( pMap->pFirst == 0 ){` |
|     90115 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     90115 |  156 | `		pMap->pCur = pNode;` |
|     45060 |  157 | `	}else{` |
|   3361033 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3451143 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3451143 |  174 | `	++pMap->nEntry;` |
|   3451143 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7892 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7897 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7897 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7897 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7429 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3717 |  187 | `	}else{` |
|       470 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7897 |  190 | `	if( pNode->pNextCollide ){` |
|      5035 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2516 |  192 | `	}` |
|      7897 |  193 | `	if( pMap->pFirst == pNode ){` |
|       171 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        83 |  195 | `	}` |
|      7897 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       203 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        99 |  199 | `	}` |
|      7897 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7897 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7897 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       209 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       209 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       209 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       102 |  218 | `		}` |
|       102 |  219 | `	}` |
|      7897 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7659 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3827 |  222 | `	}` |
|      7897 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7897 |  224 | `	pMap->nEntry--;` |
|      7897 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|        99 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        99 |  228 | `		pMap->apBucket = 0;` |
|        99 |  229 | `		pMap->nSize = 0;` |
|        99 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        47 |  231 | `	}` |
|      7897 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3451138 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3451143 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     95383 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     95383 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     95383 |  245 | `		if( nNew < 1 ){` |
|     90115 |  246 | `			nNew = 16;` |
|     45055 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     95383 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     95383 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     95383 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     95383 |  260 | `		pMap->apBucket = apNew;` |
|     95383 |  261 | `		pMap->nSize = nNew;` |
|     95383 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     90115 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5273 |  267 | `		pEntry = pMap->pFirst;` |
|      5273 |  268 | `		n = 0;` |
|   2129370 |  269 | `		for( ;; ){` |
|   4258745 |  270 | `			if( n >= pMap->nEntry ){` |
|      5273 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4253477 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4253477 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4253477 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3613633 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3613633 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1806814 |  280 | `			}` |
|   4253477 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4253477 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4253477 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5273 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2634 |  288 | `	}` |
|   3361033 |  289 | `	return SXRET_OK;` |
|   1725574 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3188302 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3188307 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3188265 |  310 | `		if( pValue ){` |
|   3188259 |  311 | `			sSafeVal = *pValue;` |
|   3188259 |  312 | `			pValue = &sSafeVal;` |
|   1594127 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3188265 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3188265 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3188265 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3188259 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1594127 |  322 | `		}` |
|   3188265 |  323 | `		nIdx = pObj->nIdx;` |
|   1594135 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3188307 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3188307 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3188307 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3188307 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3188307 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3188307 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3188307 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3188307 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3188307 |  349 | `	return SXRET_OK;` |
|   1594156 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    262836 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    262841 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    216895 |  370 | `		if( pValue ){` |
|    216585 |  371 | `			sSafeVal = *pValue;` |
|    216585 |  372 | `			pValue = &sSafeVal;` |
|    108290 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    216895 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    216895 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    216895 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    216585 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    108290 |  382 | `		}` |
|    216895 |  383 | `		nIdx = pObj->nIdx;` |
|    108450 |  384 | `	}else{` |
|     45951 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    262841 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    262841 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    262841 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    262841 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     45951 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     22973 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    262841 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    262841 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    262841 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    262841 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    262841 |  409 | `	return SXRET_OK;` |
|    131423 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4290338 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4290343 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       767 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4289581 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4289581 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110566772 |  433 | `	for(;;){` |
| 221133549 |  434 | `		if( pNode == 0 ){` |
|   4284363 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216849186 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216846174 |  438 | `			&& pNode->nHash == nHash` |
| 108424195 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      5223 |  441 | `				if( ppNode ){` |
|      5201 |  442 | `					*ppNode = pNode;` |
|      2598 |  443 | `				}` |
|      5223 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216843970 |  447 | `		pNode = pNode->pNextCollide;` |
|         2 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4284363 |  450 | `	return SXERR_NOTFOUND;` |
|   2145174 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    405922 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    405927 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     34605 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    371327 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    371327 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    305645 |  475 | `	for(;;){` |
|    611295 |  476 | `		if( pNode == 0 ){` |
|    306653 |  477 | `			break;` |
|         - |  478 | `		}` |
|    304642 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    303131 |  480 | `			&& pNode->nHash == nHash` |
|    183197 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     64779 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     64679 |  484 | `				if( ppNode ){` |
|     64651 |  485 | `					*ppNode = pNode;` |
|     32323 |  486 | `				}` |
|     64679 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    239973 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    306653 |  493 | `	return SXERR_NOTFOUND;` |
|    202966 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    406054 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    406059 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    406059 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    406059 |  504 | `	int isNeg = FALSE, nDigit;` |
|    406059 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    406037 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    406033 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    406033 |  516 | `	zDigit = zIn;` |
|    203448 |  517 | `	for(;;){` |
|    406901 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    406651 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    405783 |  523 | `			return FALSE;` |
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
|    203032 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    148296 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    148301 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    148301 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    143209 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    143209 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    143195 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    143195 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|         7 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      5111 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        27 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        13 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      5111 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     74148 |  575 | `result:` |
|    148301 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     69015 |  578 | `		if( ppNode ){` |
|     68963 |  579 | `			*ppNode = pNode;` |
|     34479 |  580 | `		}` |
|     69015 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     79291 |  584 | `	return SXERR_NOTFOUND;` |
|     74153 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  588 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  589 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  590 | ` * via HashmapAppendIndexBusy.` |
|         - |  591 | ` */` |
|   2142564 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2142569 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       811 |  596 | `		pMap->bIntKeySeen = 1;` |
|       811 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       811 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|       811 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2141763 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141517 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2141517 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1070756 |  609 | `	}` |
|   1071287 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   1045364 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1045369 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1045363 |  623 | `	return FALSE;` |
|    522687 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3404748 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3404753 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3404753 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3404753 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    216897 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    216897 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       229 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    325001 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    108332 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|    216183 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    216053 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    216053 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1593928 |  683 | `IntKey:` |
|   3188089 |  684 | `	if( pKey ){` |
|   2142759 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       259 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       129 |  688 | `		}` |
|   2142759 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   2142565 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2142563 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2142563 |  712 | `		if( rc == SXRET_OK ){` |
|   2142563 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1071279 |  714 | `		}` |
|   1071284 |  715 | `	}else{` |
|   1045335 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1045333 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1045327 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1045327 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1045325 |  726 | `			++pMap->iNextIdx;` |
|    522660 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3187885 |  730 | `	return rc;` |
|   1702379 |  731 | `}` |
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
|     45998 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     46003 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     46003 |  766 | `	sxi32 rc = SXRET_OK;` |
|     46003 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     45963 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     45963 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     68939 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     22978 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     45951 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     45951 |  790 | `		return rc;` |
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
|     23004 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1478458 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1478463 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1478463 |  832 | `	return pObj;` |
|         5 |  833 | `}` |
|         - |  834 | `/*` |
|         - |  835 | ` * Insert a node in the given hashmap.` |
|         - |  836 | ` * If a node with the given key already exists in the database` |
|         - |  837 | ` * then this function overwrite the old value.` |
|         - |  838 | ` */` |
|       544 |  839 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  840 | `{` |
|         - |  841 | `	ph7_value *pObj;` |
|         - |  842 | `	sxi32 rc;` |
|         - |  843 | `	/* Extract the node value */` |
|       549 |  844 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       549 |  845 | `	if( pObj == 0 ){` |
|       ! 0 |  846 | `		return SXERR_EMPTY;` |
|         - |  847 | `	}` |
|       544 |  848 | `	if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|       548 |  849 | `	 \|\| PH7_VmSlotIsReferenced(pMap->pVm,pNode->nValIdx) ){` |
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
|       547 |  873 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  874 | `		/* Int64 key */` |
|       415 |  875 | `		if( !bPreserve ){` |
|         - |  876 | `			/* Assign an automatic index */` |
|       267 |  877 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       136 |  878 | `		}else{` |
|       149 |  879 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  880 | `		}` |
|       210 |  881 | `	}else{` |
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
|       547 |  892 | `	return rc;` |
|       277 |  893 | `}` |
|         - |  894 | `/*` |
|         - |  895 | ` * Compare two node values.` |
|         - |  896 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  897 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  898 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  899 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  900 | ` * documenation.` |
|         - |  901 | ` */` |
|     72285 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     72290 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     72290 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     72290 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     72290 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     72290 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     72290 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     72290 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     72290 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     72290 |  921 | `	return rc;` |
|     36143 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     14138 |  927 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     14143 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     14143 |  932 | `	if( pEntry->pPrevCollide ){` |
|     11372 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5694 |  934 | `	}else{` |
|      2776 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     14143 |  937 | `	if( pEntry->pNextCollide ){` |
|       975 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       509 |  939 | `	}` |
|     14143 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     14143 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     14143 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     14143 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     14143 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14143 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     11682 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5848 |  949 | `	}` |
|     14143 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14143 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     14143 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     14143 |  956 | `		pMap->iNextIdx++;` |
|      7069 |  957 | `	}` |
|     14143 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     33182 |  966 | `static int HashmapFindValue(` |
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
|     33187 |  979 | `	pEntry = pMap->pFirst;` |
|     33187 |  980 | `	n = pMap->nEntry;` |
|     33187 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33187 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     79112 |  983 | `	for(;;){` |
|    158229 |  984 | `		if( n < 1 ){` |
|       115 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    158115 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    158115 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    158115 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    158115 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    158115 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    158115 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    158115 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    158115 | 1001 | `			if( rc == 0 ){` |
|     33073 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     33073 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     62521 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    125047 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    125047 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|       115 | 1014 | `	return SXERR_NOTFOUND;` |
|     16596 | 1015 | `}` |
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
|    688894 | 1201 | `static sxi32 HashmapDuplicateNode(` |
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
|    688894 | 1212 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    688896 | 1213 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
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
|    688891 | 1238 | `	sSafeVal = *pVal;` |
|         - | 1239 |  |
|    688891 | 1240 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1241 | `		/* Blob key insertion */` |
|      3987 | 1242 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      3987 | 1243 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      3987 | 1244 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      3987 | 1245 | `		PH7_MemObjRelease(&sKey);` |
|      1996 | 1246 | `	}else{` |
|         - | 1247 | `		/* Int key */` |
|    684909 | 1248 | `		if( iAction == 0 ){ /* Merge */` |
|    684663 | 1249 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    342579 | 1250 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1251 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1252 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1253 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1254 | `		}else{ /* Dup */` |
|       220 | 1255 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1256 | `		}` |
|         - | 1257 | `	}` |
|    688891 | 1258 | `	return rc;` |
|    344452 | 1259 | `}` |
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
|      2828 | 1272 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1273 | `{` |
|         - | 1274 | `	ph7_hashmap_node *pEntry;` |
|         - | 1275 | `	ph7_value *pVal;` |
|         - | 1276 | `	sxi32 rc;` |
|         - | 1277 | `	sxu32 n;` |
|      2833 | 1278 | `	if( pSrc == pDest ){` |
|         - | 1279 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1280 | `		 * Unlike the zend engine.` |
|         - | 1281 | `		 */` |
|       ! 0 | 1282 | `		return SXRET_OK;` |
|         - | 1283 | `	}` |
|         - | 1284 | `	/* Point to the first inserted entry in the source */` |
|      2833 | 1285 | `	pEntry = pSrc->pFirst;` |
|         - | 1286 | `	/* Perform the merge */` |
|    687551 | 1287 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1288 | `		/* Extract the node value */` |
|    684723 | 1289 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    684723 | 1290 | `		if( pVal ){` |
|         - | 1291 | `			/* Make a local copy of the value.` |
|         - | 1292 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1293 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1294 | `			 * to the old pool.` |
|         - | 1295 | `			 */` |
|    684723 | 1296 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    342364 | 1297 | `		}else{` |
|       ! 0 | 1298 | `			rc = SXRET_OK;` |
|         - | 1299 | `		}` |
|    684723 | 1300 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1301 | `			return rc;` |
|         - | 1302 | `		}` |
|         - | 1303 | `		/* Point to the next entry */` |
|    684723 | 1304 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    342364 | 1305 | `	}` |
|      2833 | 1306 | `	return SXRET_OK;` |
|      1419 | 1307 | `}` |
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
|    236596 | 1463 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1464 | `{` |
|    236601 | 1465 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1466 | `	ph7_hashmap *pNew;` |
|         - | 1467 | `	ph7_value *pBacking;` |
|         - | 1468 | `	sxu32 nValIdx;` |
|         - | 1469 | `	int bValueInPool;` |
|    236601 | 1470 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    236601 | 1471 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1472 | `		/* Sole owner, no separation needed */` |
|    233869 | 1473 | `		return pMap;` |
|         - | 1474 | `	}` |
|      2737 | 1475 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1476 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1477 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1478 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1479 | `		return pMap;` |
|         - | 1480 | `	}` |
|         - | 1481 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1482 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1483 | `	 * frame is popped. */` |
|      2611 | 1484 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2611 | 1485 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2606 | 1486 | `		if( pBacking && pBacking != pValue` |
|      2581 | 1487 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2561 | 1488 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1489 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2561 | 1490 | `			pMap->iRef--;` |
|      2561 | 1491 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1492 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2515 | 1493 | `				pMap->iRef++;` |
|      2515 | 1494 | `				return pMap;` |
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
|    118303 | 1557 | `}` |
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
|    144378 | 1649 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1650 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1651 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1652 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1653 | `	)` |
|         5 | 1654 | `{` |
|         - | 1655 | `	ph7_hashmap *pMap;` |
|         - | 1656 | `	/* Allocate a new instance */` |
|    144383 | 1657 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    144383 | 1658 | `	if( pMap == 0 ){` |
|       ! 0 | 1659 | `		return 0;` |
|         - | 1660 | `	}` |
|         - | 1661 | `	/* Zero the structure */` |
|    144383 | 1662 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1663 | `	/* Fill in the structure */` |
|    144383 | 1664 | `	pMap->pVm = &(*pVm);` |
|    144383 | 1665 | `	pMap->iRef = 1;` |
|         - | 1666 | `	/* Default hash functions */` |
|    144383 | 1667 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    144383 | 1668 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    144383 | 1669 | `	return pMap;` |
|     72194 | 1670 | `}` |
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
|    102450 | 1762 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1763 | `{` |
|         - | 1764 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    102455 | 1765 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1766 | `	sxu32 n;` |
|    102455 | 1767 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1768 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1769 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1770 | `		return SXRET_OK;` |
|         - | 1771 | `	}` |
|    102455 | 1772 | `	if( pMap->pActiveSteps ){` |
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
|    102455 | 1785 | `	n = 0;` |
|    102455 | 1786 | `	pEntry = pMap->pFirst;` |
|   1734406 | 1787 | `	for(;;){` |
|   3468817 | 1788 | `		if( n >= pMap->nEntry ){` |
|    102455 | 1789 | `			break;` |
|         - | 1790 | `		}` |
|   3366367 | 1791 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1792 | `		/* Remove the reference from the foreign table */` |
|   3366367 | 1793 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3366367 | 1794 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1795 | `			/* Restore the ph7_value to the free list */` |
|   3366307 | 1796 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1683151 | 1797 | `		}` |
|         - | 1798 | `		/* Release the node */` |
|   3366367 | 1799 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    189375 | 1800 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     94685 | 1801 | `		}` |
|   3366367 | 1802 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1803 | `		/* Point to the next entry */` |
|   3366367 | 1804 | `		pEntry = pNext;` |
|   3366367 | 1805 | `		n++;` |
|         5 | 1806 | `	}` |
|    102455 | 1807 | `	if( pMap->nEntry > 0 ){` |
|         - | 1808 | `		/* Release the hash bucket */` |
|     75179 | 1809 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     37587 | 1810 | `	}` |
|    102455 | 1811 | `	if( FreeDS ){` |
|         - | 1812 | `		/* Free the whole instance */` |
|    102429 | 1813 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     51217 | 1814 | `	}else{` |
|         - | 1815 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1816 | `		pMap->apBucket = 0;` |
|        28 | 1817 | `		pMap->iNextIdx = 0;` |
|        28 | 1818 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1819 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1820 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1821 | `	}` |
|    102455 | 1822 | `	return SXRET_OK;` |
|     51230 | 1823 | `}` |
|         - | 1824 | `/*` |
|         - | 1825 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1826 | ` * If the count reaches zero which mean no more variables` |
|         - | 1827 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1828 | ` */` |
|    842514 | 1829 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1830 | `{` |
|    842519 | 1831 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1832 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    842519 | 1833 | `	pMap->iRef--;` |
|    842519 | 1834 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    102409 | 1835 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     51202 | 1836 | `	}` |
|    842519 | 1837 | `}` |
|         - | 1838 | `/*` |
|         - | 1839 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1840 | ` * Write a pointer to the target node on success.` |
|         - | 1841 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1842 | ` */` |
|    148484 | 1843 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1844 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1845 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1846 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1847 | `	)` |
|         5 | 1848 | `{` |
|         - | 1849 | `	sxi32 rc;` |
|    148489 | 1850 | `	if( pMap->nEntry < 1 ){` |
|         - | 1851 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1852 | `		 */` |
|       193 | 1853 | `		return SXERR_NOTFOUND;` |
|         - | 1854 | `	}` |
|    148301 | 1855 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    148301 | 1856 | `	return rc;` |
|     74247 | 1857 | `}` |
|         - | 1858 | `/*` |
|         - | 1859 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1860 | ` * hashmap.` |
|         - | 1861 | ` * If a node with the given key already exists in the database` |
|         - | 1862 | ` * then this function overwrite the old value.` |
|         - | 1863 | ` */` |
|   2719756 | 1864 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2719761 | 1875 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2719761 | 1876 | `	return rc;` |
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
|     45988 | 1915 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1916 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1917 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1918 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1919 | `	)` |
|         5 | 1920 | `{` |
|         - | 1921 | `	sxi32 rc;` |
|     45993 | 1922 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1923 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1924 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1925 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1926 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1927 | `		return PH7_ABORT;` |
|         - | 1928 | `	}` |
|     45993 | 1929 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     45993 | 1930 | `	return rc;` |
|     22999 | 1931 | `}` |
|         - | 1932 | `/*` |
|         - | 1933 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1934 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1935 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1936 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1937 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1938 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1939 | ` */` |
|     19130 | 1940 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1941 | `{` |
|     19135 | 1942 | `	pStep->pCursor = pMap->pFirst;` |
|     19135 | 1943 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     19135 | 1944 | `	pMap->pActiveSteps = pStep;` |
|     19135 | 1945 | `}` |
|         - | 1946 | `/*` |
|         - | 1947 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1948 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1949 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1950 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1951 | ` */` |
|     19028 | 1952 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1953 | `{` |
|     19033 | 1954 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     19033 | 1955 | `	while( *ppLink ){` |
|     19033 | 1956 | `		if( *ppLink == pStep ){` |
|     19033 | 1957 | `			*ppLink = pStep->pNextActive;` |
|     19033 | 1958 | `			pStep->pNextActive = 0;` |
|     19033 | 1959 | `			return;` |
|         - | 1960 | `		}` |
|       ! 0 | 1961 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1962 | `	}` |
|      9519 | 1963 | `}` |
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
|    597076 | 1984 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1985 | `{` |
|    597081 | 1986 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    597081 | 1987 | `	if( pEntry ){` |
|    597081 | 1988 | `		if( bStore ){` |
|    237591 | 1989 | `			PH7_MemObjStore(pEntry,pValue);` |
|    118798 | 1990 | `		}else{` |
|    359495 | 1991 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1992 | `		}` |
|    298534 | 1993 | `	}else{` |
|       ! 0 | 1994 | `		PH7_MemObjRelease(pValue);` |
|         - | 1995 | `	}` |
|    597081 | 1996 | `}` |
|         - | 1997 | `/*` |
|         - | 1998 | ` * Extract a node key.` |
|         - | 1999 | ` */` |
|    158796 | 2000 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2001 | `{` |
|         - | 2002 | `	/* Fill with the current key */` |
|    158801 | 2003 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    153501 | 2004 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2005 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2006 | `		}` |
|    153501 | 2007 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    153501 | 2008 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     76753 | 2009 | `	}else{` |
|      5305 | 2010 | `		SyBlobReset(&pKey->sBlob);` |
|      5305 | 2011 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5305 | 2012 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2013 | `	}` |
|    158801 | 2014 | `}` |
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
|     36738 | 2065 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2066 | `{` |
|         - | 2067 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2068 | `    /* Prevent compiler warning */` |
|     36743 | 2069 | `	result.pNext = result.pPrev = 0;` |
|     36743 | 2070 | `	pTail = &result;` |
|    109258 | 2071 | `	while( pA && pB ){` |
|     72520 | 2072 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47886 | 2073 | `			pTail->pPrev = pA;` |
|     47886 | 2074 | `			pA->pNext = pTail;` |
|     47886 | 2075 | `			pTail = pA;` |
|     47886 | 2076 | `			pA = pA->pPrev;` |
|     23889 | 2077 | `		}else{` |
|     24639 | 2078 | `			pTail->pPrev = pB;` |
|     24639 | 2079 | `			pB->pNext = pTail;` |
|     24639 | 2080 | `			pTail = pB;` |
|     24639 | 2081 | `			pB = pB->pPrev;` |
|         - | 2082 | `		}` |
|         5 | 2083 | `	}` |
|     36743 | 2084 | `	if( pA ){` |
|     26130 | 2085 | `		pTail->pPrev = pA;` |
|     26130 | 2086 | `		pA->pNext = pTail;` |
|     23715 | 2087 | `	}else if( pB ){` |
|     10392 | 2088 | `		pTail->pPrev = pB;` |
|     10392 | 2089 | `		pB->pNext = pTail;` |
|      5164 | 2090 | `	}else{` |
|       231 | 2091 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2092 | `	}` |
|     36743 | 2093 | `	return result.pPrev;` |
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
|       772 | 2107 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2108 | `{` |
|         - | 2109 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2110 | `	sxu32 i;` |
|       777 | 2111 | `	SyZero(a,sizeof(a));` |
|         - | 2112 | `	/* Point to the first inserted entry */` |
|       777 | 2113 | `	pIn = pMap->pFirst;` |
|     14939 | 2114 | `	while( pIn ){` |
|     14167 | 2115 | `		p = pIn;` |
|     14167 | 2116 | `		pIn = p->pPrev;` |
|     14167 | 2117 | `		p->pPrev = 0;` |
|     26973 | 2118 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26973 | 2119 | `			if( a[i]==0 ){` |
|     14167 | 2120 | `				a[i] = p;` |
|     14167 | 2121 | `				break;` |
|       ! 0 | 2122 | `			}else{` |
|     12811 | 2123 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12811 | 2124 | `				a[i] = 0;` |
|         - | 2125 | `			}` |
|      6408 | 2126 | `		}` |
|     14167 | 2127 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2128 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2129 | `			 * But that is impossible.` |
|         - | 2130 | `			 */` |
|       ! 0 | 2131 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2132 | `		}` |
|         5 | 2133 | `	}` |
|       777 | 2134 | `	p = a[0];` |
|     24709 | 2135 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     23937 | 2136 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11971 | 2137 | `	}` |
|       777 | 2138 | `	p->pNext = 0;` |
|         - | 2139 | `	/* Reflect the change */` |
|       777 | 2140 | `	pMap->pFirst = p;` |
|         - | 2141 | `	/* Reset the loop cursor */` |
|       777 | 2142 | `	pMap->pCur = pMap->pFirst;` |
|       777 | 2143 | `	return SXRET_OK;` |
|         5 | 2144 | `}` |
|         - | 2145 | `/* SPDX-SnippetEnd */` |
|         - | 2146 | `/*` |
|         - | 2147 | ` * Node comparison callback.` |
|         - | 2148 | ` * used-by: [sort(),asort(),...]` |
|         - | 2149 | ` */` |
|         - | 2150 | `/*` |
|         - | 2151 | ` * Compare two node VALUES under php's sort_flags. The flag carries a base type` |
|         - | 2152 | ` * in its low bits and the optional SORT_FLAG_CASE (8) modifier:` |
|         - | 2153 | ` *   SORT_REGULAR 0 · SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 ·` |
|         - | 2154 | ` *   SORT_NATURAL 6   (\| SORT_FLAG_CASE for a case-insensitive string/natural sort)` |
|         - | 2155 | ` * PHL has no locale tables, so SORT_LOCALE_STRING behaves like SORT_STRING.` |
|         - | 2156 | ` */` |
|       126 | 2157 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2158 | `{` |
|         - | 2159 | `	ph7_value sA,sB;` |
|       127 | 2160 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|       127 | 2161 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2162 | `	sxi32 rc;` |
|       127 | 2163 | `	if( base == 0 ){` |
|         - | 2164 | `		/* SORT_REGULAR */` |
|       ! 0 | 2165 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2166 | `	}` |
|       127 | 2167 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       127 | 2168 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       127 | 2169 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       127 | 2170 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       127 | 2171 | `	if( base == 1 ){` |
|         - | 2172 | `		/* SORT_NUMERIC */` |
|        47 | 2173 | `		PH7_MemObjToNumeric(&sA);` |
|        47 | 2174 | `		PH7_MemObjToNumeric(&sB);` |
|        47 | 2175 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|        24 | 2176 | `	}else{` |
|         - | 2177 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|         - | 2178 | `		const char *zA,*zB;` |
|         - | 2179 | `		sxu32 nA,nB,nMin,i;` |
|        81 | 2180 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(&sA); }` |
|        81 | 2181 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(&sB); }` |
|        81 | 2182 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        81 | 2183 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        81 | 2184 | `		nA = SyBlobLength(&sA.sBlob);` |
|        81 | 2185 | `		nB = SyBlobLength(&sB.sBlob);` |
|        81 | 2186 | `		if( base == 6 ){` |
|        21 | 2187 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|        11 | 2188 | `		}else{` |
|         - | 2189 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|        61 | 2190 | `			nMin = nA < nB ? nA : nB;` |
|        61 | 2191 | `			rc = 0;` |
|        77 | 2192 | `			for( i = 0 ; i < nMin ; ++i ){` |
|        65 | 2193 | `				int ca = (unsigned char)zA[i];` |
|        65 | 2194 | `				int cb = (unsigned char)zB[i];` |
|        65 | 2195 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|        65 | 2196 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|         9 | 2197 | `			}` |
|        61 | 2198 | `			if( rc == 0 ){` |
|        13 | 2199 | `				if( nA < nB ) rc = -1;` |
|         3 | 2200 | `				else if( nA > nB ) rc = 1;` |
|         6 | 2201 | `			}` |
|         - | 2202 | `		}` |
|         - | 2203 | `	}` |
|       127 | 2204 | `	PH7_MemObjRelease(&sA);` |
|       127 | 2205 | `	PH7_MemObjRelease(&sB);` |
|       127 | 2206 | `	return rc;` |
|        64 | 2207 | `}` |
|     72219 | 2208 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2209 | `{` |
|     72224 | 2210 | `	if( pCmpData == 0 ){` |
|         - | 2211 | `		/* SORT_REGULAR fast path */` |
|     72136 | 2212 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2213 | `	}` |
|        89 | 2214 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     36110 | 2215 | `}` |
|         - | 2216 | `/*` |
|         - | 2217 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2218 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2219 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2220 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2221 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2222 | ` */` |
|         - | 2223 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2224 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2225 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        36 | 2226 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2227 | `{` |
|        38 | 2228 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        38 | 2229 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        38 | 2230 | `	if( rc == 0 ){` |
|       ! 0 | 2231 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2232 | `	}` |
|        38 | 2233 | `	return rc;` |
|         2 | 2234 | `}` |
|        58 | 2235 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2236 | `{` |
|         - | 2237 | `	sxi32 rc;` |
|        60 | 2238 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2239 | `		/* Perform a string comparison */` |
|        32 | 2240 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        20 | 2241 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        12 | 2242 | `	}else{` |
|         - | 2243 | `		SyString sStr;` |
|        39 | 2244 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2245 | `		int bNum = 1;` |
|        39 | 2246 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2247 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2248 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2249 | `				bNum = 0;` |
|         6 | 2250 | `			}else{` |
|       ! 0 | 2251 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2252 | `			}` |
|         6 | 2253 | `		}else{` |
|        29 | 2254 | `			iA = pA->xKey.iKey;` |
|         - | 2255 | `		}` |
|        39 | 2256 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2257 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2258 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2259 | `				bNum = 0;` |
|         4 | 2260 | `			}else{` |
|       ! 0 | 2261 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2262 | `			}` |
|         4 | 2263 | `		}else{` |
|        33 | 2264 | `			iB = pB->xKey.iKey;` |
|         - | 2265 | `		}` |
|        39 | 2266 | `		if( bNum ){` |
|        23 | 2267 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2268 | `		}else{` |
|         - | 2269 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2270 | `			char zNumA[24],zNumB[24];` |
|         - | 2271 | `			SyString sA,sB;` |
|        17 | 2272 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2273 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2274 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2275 | `			}else{` |
|        11 | 2276 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2277 | `			}` |
|        17 | 2278 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2279 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2280 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2281 | `			}else{` |
|         7 | 2282 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2283 | `			}` |
|        17 | 2284 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2285 | `		}` |
|         - | 2286 | `	}` |
|        60 | 2287 | `	return rc;` |
|         2 | 2288 | `}` |
|         - | 2289 | `/*` |
|         - | 2290 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2291 | ` * used-by: [ksort()]` |
|         - | 2292 | ` */` |
|        44 | 2293 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2294 | `{` |
|        22 | 2295 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        46 | 2296 | `	return HashmapKeyNodeCmp(pA,pB);` |
|         2 | 2297 | `}` |
|         - | 2298 | `/*` |
|         - | 2299 | ` * Node comparison callback.` |
|         - | 2300 | ` * Used by: [rsort(),arsort()];` |
|         - | 2301 | ` */` |
|        96 | 2302 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2303 | `{` |
|        97 | 2304 | `	if( pCmpData == 0 ){` |
|         - | 2305 | `		/* SORT_REGULAR fast path, reversed */` |
|        59 | 2306 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2307 | `	}` |
|        39 | 2308 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        49 | 2309 | `}` |
|         - | 2310 | `/*` |
|         - | 2311 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2312 | ` * used-by: [usort(),uasort()]` |
|         - | 2313 | ` */` |
|       116 | 2314 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2315 | `{` |
|         - | 2316 | `	ph7_value sResult,*pCallback;` |
|         - | 2317 | `	ph7_value *pV1,*pV2;` |
|         - | 2318 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2319 | `	sxi32 rc;` |
|         - | 2320 | `	/* Point to the desired callback */` |
|       119 | 2321 | `	pCallback = (ph7_value *)pCmpData;` |
|       119 | 2322 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2323 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2324 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2325 | `		return 0;` |
|         - | 2326 | `	}` |
|         - | 2327 | `	/* initialize the result value */` |
|       113 | 2328 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2329 | `	/* Extract nodes values */` |
|       113 | 2330 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       113 | 2331 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       113 | 2332 | `	apArg[0] = pV1;` |
|       113 | 2333 | `	apArg[1] = pV2;` |
|         - | 2334 | `	/* Invoke the callback */` |
|       113 | 2335 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       113 | 2336 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2337 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2338 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2339 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2340 | `		rc = 0;` |
|       108 | 2341 | `	}else if( rc != SXRET_OK ){` |
|         - | 2342 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2343 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2344 | `	}else{` |
|         - | 2345 | `		/* Extract callback result */` |
|       104 | 2346 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2347 | `			/* Perform an int cast */` |
|       ! 0 | 2348 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2349 | `		}` |
|       104 | 2350 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2351 | `	}` |
|       113 | 2352 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2353 | `	/* Callback result */` |
|       113 | 2354 | `	return rc;` |
|        61 | 2355 | `}` |
|         - | 2356 | `/*` |
|         - | 2357 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2358 | ` * used-by: [krsort()]` |
|         - | 2359 | ` */` |
|        14 | 2360 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2361 | `{` |
|         7 | 2362 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        15 | 2363 | `	return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         1 | 2364 | `}` |
|         - | 2365 | `/*` |
|         - | 2366 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2367 | ` * used-by: [uksort()]` |
|         - | 2368 | ` */` |
|         6 | 2369 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2370 | `{` |
|         - | 2371 | `	ph7_value sResult,*pCallback;` |
|         - | 2372 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2373 | `	ph7_value sK1,sK2;` |
|         - | 2374 | `	sxi32 rc;` |
|         - | 2375 | `	/* Point to the desired callback */` |
|         7 | 2376 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2377 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2378 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2379 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2380 | `		return 0;` |
|         - | 2381 | `	}` |
|         - | 2382 | `	/* initialize the result value */` |
|         7 | 2383 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2384 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2385 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2386 | `	/* Extract nodes keys */` |
|         7 | 2387 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2388 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2389 | `	apArg[0] = &sK1;` |
|         7 | 2390 | `	apArg[1] = &sK2;` |
|         - | 2391 | `	/* Mark keys as constants */` |
|         7 | 2392 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2393 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2394 | `	/* Invoke the callback */` |
|         7 | 2395 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2396 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2397 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2398 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2399 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2400 | `		rc = 0;` |
|         7 | 2401 | `	}else if( rc != SXRET_OK ){` |
|         - | 2402 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2403 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2404 | `	}else{` |
|         - | 2405 | `		/* Extract callback result */` |
|         7 | 2406 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2407 | `			/* Perform an int cast */` |
|       ! 0 | 2408 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2409 | `		}` |
|         7 | 2410 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2411 | `	}` |
|         7 | 2412 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2413 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2414 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2415 | `	/* Callback result */` |
|         7 | 2416 | `	return rc;` |
|         4 | 2417 | `}` |
|         - | 2418 | `/*` |
|         - | 2419 | ` * Node comparison callback: Random node comparison.` |
|         - | 2420 | ` * used-by: [shuffle()]` |
|         - | 2421 | ` */` |
|        20 | 2422 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2423 | `{` |
|         - | 2424 | `	sxu32 n;` |
|        10 | 2425 | `	SXUNUSED(pB); /* cc warning */` |
|        10 | 2426 | `	SXUNUSED(pCmpData);` |
|         - | 2427 | `	/* Grab a random number */` |
|        21 | 2428 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2429 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2430 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2431 | `	 */` |
|        21 | 2432 | `	return n&1 ? 1 : -1;` |
|         1 | 2433 | `}` |
|         - | 2434 | `/*` |
|         - | 2435 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2436 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2437 | ` */` |
|       698 | 2438 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2439 | `{` |
|         - | 2440 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2441 | `	sxu32 i;` |
|         - | 2442 | `	/* Rehash all entries */` |
|       703 | 2443 | `	pLast = p = pMap->pFirst;` |
|       703 | 2444 | `	pMap->iNextIdx = 0;` |
|       703 | 2445 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|       703 | 2446 | `	i = 0;` |
|      7306 | 2447 | `	for( ;; ){` |
|     14617 | 2448 | `		if( i >= pMap->nEntry ){` |
|       703 | 2449 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       703 | 2450 | `			break;` |
|         - | 2451 | `		}` |
|     13919 | 2452 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2453 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2454 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2455 | `			/* Change key type */` |
|         5 | 2456 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2457 | `		}` |
|     13919 | 2458 | `		HashmapRehashIntNode(p);` |
|         - | 2459 | `		/* Point to the next entry */` |
|     13919 | 2460 | `		i++;` |
|     13919 | 2461 | `		pLast = p;` |
|     13919 | 2462 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2463 | `	}` |
|       703 | 2464 | `}` |
|         - | 2465 | `/*` |
|         - | 2466 | ` * Array functions implementation.` |
|         - | 2467 | ` * Status:` |
|         - | 2468 | ` *  Stable.` |
|         - | 2469 | ` */` |
|         - | 2470 | `/*` |
|         - | 2471 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2472 | ` * Sort an array.` |
|         - | 2473 | ` * Parameters` |
|         - | 2474 | ` *  $array` |
|         - | 2475 | ` *   The input array.` |
|         - | 2476 | ` * $sort_flags` |
|         - | 2477 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2478 | ` *  Sorting type flags:` |
|         - | 2479 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2480 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2481 | ` *   SORT_STRING - compare items as strings` |
|         - | 2482 | ` * Return` |
|         - | 2483 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2484 | ` *` |
|         - | 2485 | ` */` |
|      1052 | 2486 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2487 | `{` |
|         - | 2488 | `	ph7_hashmap *pMap;` |
|         - | 2489 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1057 | 2490 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2491 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2492 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2493 | `		return PH7_OK;` |
|         - | 2494 | `	}` |
|         - | 2495 | `	/* Point to the internal representation of the input hashmap */` |
|      1057 | 2496 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1057 | 2497 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1057 | 2498 | `	if( pMap->nEntry > 1 ){` |
|       673 | 2499 | `		sxi32 iCmpFlags = 0;` |
|       673 | 2500 | `		if( nArg > 1 ){` |
|         - | 2501 | `			/* Extract comparison flags */` |
|        15 | 2502 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         7 | 2503 | `		}` |
|         - | 2504 | `		/* Do the merge sort */` |
|       673 | 2505 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2506 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       673 | 2507 | `		HashmapSortRehash(pMap);` |
|       334 | 2508 | `	}` |
|         - | 2509 | `	/* All done,return TRUE */` |
|      1057 | 2510 | `	ph7_result_bool(pCtx,1);` |
|      1057 | 2511 | `	return PH7_OK;` |
|       531 | 2512 | `}` |
|         - | 2513 | `/*` |
|         - | 2514 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2515 | ` *  Sort an array and maintain index association.` |
|         - | 2516 | ` * Parameters` |
|         - | 2517 | ` *  $array` |
|         - | 2518 | ` *   The input array.` |
|         - | 2519 | ` * $sort_flags` |
|         - | 2520 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2521 | ` *  Sorting type flags:` |
|         - | 2522 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2523 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2524 | ` *   SORT_STRING - compare items as strings` |
|         - | 2525 | ` * Return` |
|         - | 2526 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2527 | ` */` |
|        34 | 2528 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2529 | `{` |
|         - | 2530 | `	ph7_hashmap *pMap;` |
|         - | 2531 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        39 | 2532 | `	if( nArg < 1 ){` |
|       ! 0 | 2533 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2534 | `			"ArgumentCountError",` |
|         - | 2535 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2536 | `			);` |
|         - | 2537 | `	}` |
|         - | 2538 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        39 | 2539 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2540 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2541 | `			"TypeError",` |
|         - | 2542 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2543 | `			ph7_type_name(apArg[0])` |
|         - | 2544 | `			);` |
|         - | 2545 | `	}` |
|         - | 2546 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 2547 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        27 | 2548 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        27 | 2549 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2550 | `		sxi32 iCmpFlags = 0;` |
|        23 | 2551 | `		if( nArg > 1 ){` |
|         - | 2552 | `			/* Extract comparison flags */` |
|         7 | 2553 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2554 | `		}` |
|         - | 2555 | `		/* Do the merge sort */` |
|        23 | 2556 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2557 | `		/* Fix the last link broken by the merge */` |
|        55 | 2558 | `		while(pMap->pLast->pPrev){` |
|        33 | 2559 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2560 | `		}` |
|        11 | 2561 | `	}` |
|         - | 2562 | `	/* All done,return TRUE */` |
|        27 | 2563 | `	ph7_result_bool(pCtx,1);` |
|        27 | 2564 | `	return PH7_OK;` |
|        22 | 2565 | `}` |
|         - | 2566 | `/*` |
|         - | 2567 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2568 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2569 | ` * Parameters` |
|         - | 2570 | ` *  $array` |
|         - | 2571 | ` *   The input array.` |
|         - | 2572 | ` * $sort_flags` |
|         - | 2573 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2574 | ` *  Sorting type flags:` |
|         - | 2575 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2576 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2577 | ` *   SORT_STRING - compare items as strings` |
|         - | 2578 | ` * Return` |
|         - | 2579 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2580 | ` */` |
|        32 | 2581 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2582 | `{` |
|         - | 2583 | `	ph7_hashmap *pMap;` |
|         - | 2584 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2585 | `	if( nArg < 1 ){` |
|       ! 0 | 2586 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2587 | `			"ArgumentCountError",` |
|         - | 2588 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2589 | `			);` |
|         - | 2590 | `	}` |
|         - | 2591 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2592 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2593 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2594 | `			"TypeError",` |
|         - | 2595 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2596 | `			ph7_type_name(apArg[0])` |
|         - | 2597 | `			);` |
|         - | 2598 | `	}` |
|         - | 2599 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2600 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2601 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2602 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2603 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2604 | `		if( nArg > 1 ){` |
|         - | 2605 | `			/* Extract comparison flags */` |
|         7 | 2606 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2607 | `		}` |
|         - | 2608 | `		/* Do the merge sort */` |
|        21 | 2609 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2610 | `		/* Fix the last link broken by the merge */` |
|        37 | 2611 | `		while(pMap->pLast->pPrev){` |
|        17 | 2612 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2613 | `		}` |
|        10 | 2614 | `	}` |
|         - | 2615 | `	/* All done,return TRUE */` |
|        25 | 2616 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2617 | `	return PH7_OK;` |
|        21 | 2618 | `}` |
|         - | 2619 | `/*` |
|         - | 2620 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2621 | ` *  Sort an array by key.` |
|         - | 2622 | ` * Parameters` |
|         - | 2623 | ` *  $array` |
|         - | 2624 | ` *   The input array.` |
|         - | 2625 | ` * $sort_flags` |
|         - | 2626 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2627 | ` *  Sorting type flags:` |
|         - | 2628 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2629 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2630 | ` *   SORT_STRING - compare items as strings` |
|         - | 2631 | ` * Return` |
|         - | 2632 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2633 | ` */` |
|        14 | 2634 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2635 | `{` |
|         - | 2636 | `	ph7_hashmap *pMap;` |
|         - | 2637 | `	/* Make sure we are dealing with a valid hashmap */` |
|        16 | 2638 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2639 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2640 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2641 | `		return PH7_OK;` |
|         - | 2642 | `	}` |
|         - | 2643 | `	/* Point to the internal representation of the input hashmap */` |
|        16 | 2644 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        16 | 2645 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        16 | 2646 | `	if( pMap->nEntry > 1 ){` |
|        16 | 2647 | `		sxi32 iCmpFlags = 0;` |
|        16 | 2648 | `		if( nArg > 1 ){` |
|         - | 2649 | `			/* Extract comparison flags */` |
|       ! 0 | 2650 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2651 | `		}` |
|         - | 2652 | `		/* Do the merge sort */` |
|        16 | 2653 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2654 | `		/* Fix the last link broken by the merge */` |
|        38 | 2655 | `		while(pMap->pLast->pPrev){` |
|        23 | 2656 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2657 | `		}` |
|         7 | 2658 | `	}` |
|         - | 2659 | `	/* All done,return TRUE */` |
|        16 | 2660 | `	ph7_result_bool(pCtx,1);` |
|        16 | 2661 | `	return PH7_OK;` |
|         9 | 2662 | `}` |
|         - | 2663 | `/*` |
|         - | 2664 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2665 | ` *  Sort an array by key in reverse order.` |
|         - | 2666 | ` * Parameters` |
|         - | 2667 | ` *  $array` |
|         - | 2668 | ` *   The input array.` |
|         - | 2669 | ` * $sort_flags` |
|         - | 2670 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2671 | ` *  Sorting type flags:` |
|         - | 2672 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2673 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2674 | ` *   SORT_STRING - compare items as strings` |
|         - | 2675 | ` * Return` |
|         - | 2676 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2677 | ` */` |
|         4 | 2678 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2679 | `{` |
|         - | 2680 | `	ph7_hashmap *pMap;` |
|         - | 2681 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2682 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2683 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2684 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2685 | `		return PH7_OK;` |
|         - | 2686 | `	}` |
|         - | 2687 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2688 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2689 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2690 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2691 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2692 | `		if( nArg > 1 ){` |
|         - | 2693 | `			/* Extract comparison flags */` |
|       ! 0 | 2694 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2695 | `		}` |
|         - | 2696 | `		/* Do the merge sort */` |
|         5 | 2697 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2698 | `		/* Fix the last link broken by the merge */` |
|        17 | 2699 | `		while(pMap->pLast->pPrev){` |
|        13 | 2700 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2701 | `		}` |
|         2 | 2702 | `	}` |
|         - | 2703 | `	/* All done,return TRUE */` |
|         5 | 2704 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2705 | `	return PH7_OK;` |
|         3 | 2706 | `}` |
|         - | 2707 | `/*` |
|         - | 2708 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2709 | ` * Sort an array in reverse order.` |
|         - | 2710 | ` * Parameters` |
|         - | 2711 | ` *  $array` |
|         - | 2712 | ` *   The input array.` |
|         - | 2713 | ` * $sort_flags` |
|         - | 2714 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2715 | ` *  Sorting type flags:` |
|         - | 2716 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2717 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2718 | ` *   SORT_STRING - compare items as strings` |
|         - | 2719 | ` * Return` |
|         - | 2720 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2721 | ` */` |
|         6 | 2722 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2723 | `{` |
|         - | 2724 | `	ph7_hashmap *pMap;` |
|         - | 2725 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2726 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2727 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2728 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2729 | `		return PH7_OK;` |
|         - | 2730 | `	}` |
|         - | 2731 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2732 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2733 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2734 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2735 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2736 | `		if( nArg > 1 ){` |
|         - | 2737 | `			/* Extract comparison flags */` |
|         5 | 2738 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2739 | `		}` |
|         - | 2740 | `		/* Do the merge sort */` |
|         7 | 2741 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2742 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         7 | 2743 | `		HashmapSortRehash(pMap);` |
|         3 | 2744 | `	}` |
|         - | 2745 | `	/* All done,return TRUE */` |
|         7 | 2746 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2747 | `	return PH7_OK;` |
|         4 | 2748 | `}` |
|         - | 2749 | `/*` |
|         - | 2750 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2751 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2752 | ` * Parameters` |
|         - | 2753 | ` *  $array` |
|         - | 2754 | ` *   The input array.` |
|         - | 2755 | ` * $cmp_function` |
|         - | 2756 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2757 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2758 | ` *  to, or greater than the second.` |
|         - | 2759 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2760 | ` * Return` |
|         - | 2761 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2762 | ` */` |
|        22 | 2763 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2764 | `{` |
|         - | 2765 | `	ph7_hashmap *pMap;` |
|         - | 2766 | `	/* Make sure we are dealing with a valid hashmap */` |
|        25 | 2767 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2768 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2769 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2770 | `		return PH7_OK;` |
|         - | 2771 | `	}` |
|        25 | 2772 | `	if( nArg > 1 ){` |
|         - | 2773 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2774 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2775 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        25 | 2776 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        25 | 2777 | `		if( rcCb != PH7_OK ){` |
|         3 | 2778 | `			return rcCb;` |
|         - | 2779 | `		}` |
|        10 | 2780 | `	}` |
|         - | 2781 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2782 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2783 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2784 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2785 | `		ph7_value *pCallback = 0;` |
|         - | 2786 | `		ProcNodeCmp xCmp;` |
|        23 | 2787 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        23 | 2788 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2789 | `			/* Point to the desired callback */` |
|        23 | 2790 | `			pCallback = apArg[1];` |
|        13 | 2791 | `		}else{` |
|         - | 2792 | `			/* Use the default comparison function */` |
|       ! 0 | 2793 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2794 | `		}` |
|         - | 2795 | `		/* Do the merge sort */` |
|        23 | 2796 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        23 | 2797 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2798 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        23 | 2799 | `		HashmapSortRehash(pMap);` |
|        23 | 2800 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2801 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2802 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2803 | `			return PH7_EXCEPTION;` |
|         - | 2804 | `		}` |
|         6 | 2805 | `	}` |
|         - | 2806 | `	/* All done,return TRUE */` |
|        14 | 2807 | `	ph7_result_bool(pCtx,1);` |
|        14 | 2808 | `	return PH7_OK;` |
|        14 | 2809 | `}` |
|         - | 2810 | `/*` |
|         - | 2811 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2812 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2813 | ` *  and maintain index association.` |
|         - | 2814 | ` * Parameters` |
|         - | 2815 | ` *  $array` |
|         - | 2816 | ` *   The input array.` |
|         - | 2817 | ` * $cmp_function` |
|         - | 2818 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2819 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2820 | ` *  to, or greater than the second.` |
|         - | 2821 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2822 | ` * Return` |
|         - | 2823 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2824 | ` */` |
|        12 | 2825 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2826 | `{` |
|         - | 2827 | `	ph7_hashmap *pMap;` |
|         - | 2828 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 2829 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2830 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2831 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2832 | `		return PH7_OK;` |
|         - | 2833 | `	}` |
|        13 | 2834 | `	if( nArg > 1 ){` |
|         - | 2835 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2836 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2837 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        13 | 2838 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        13 | 2839 | `		if( rcCb != PH7_OK ){` |
|         3 | 2840 | `			return rcCb;` |
|         - | 2841 | `		}` |
|         5 | 2842 | `	}` |
|         - | 2843 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2844 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2845 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2846 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2847 | `		ph7_value *pCallback = 0;` |
|         - | 2848 | `		ProcNodeCmp xCmp;` |
|        11 | 2849 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2850 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2851 | `			/* Point to the desired callback */` |
|        11 | 2852 | `			pCallback = apArg[1];` |
|         6 | 2853 | `		}else{` |
|         - | 2854 | `			/* Use the default comparison function */` |
|       ! 0 | 2855 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2856 | `		}` |
|         - | 2857 | `		/* Do the merge sort */` |
|        11 | 2858 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2859 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2860 | `		/* Fix the last link broken by the merge */` |
|        23 | 2861 | `		while(pMap->pLast->pPrev){` |
|        13 | 2862 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2863 | `		}` |
|        11 | 2864 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2865 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2866 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2867 | `			return PH7_EXCEPTION;` |
|         - | 2868 | `		}` |
|         5 | 2869 | `	}` |
|         - | 2870 | `	/* All done,return TRUE */` |
|        11 | 2871 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2872 | `	return PH7_OK;` |
|         7 | 2873 | `}` |
|         - | 2874 | `/*` |
|         - | 2875 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2876 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2877 | ` *  function and maintain index association.` |
|         - | 2878 | ` * Parameters` |
|         - | 2879 | ` *  $array` |
|         - | 2880 | ` *   The input array.` |
|         - | 2881 | ` * $cmp_function` |
|         - | 2882 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2883 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2884 | ` *  to, or greater than the second.` |
|         - | 2885 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2886 | ` * Return` |
|         - | 2887 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2888 | ` */` |
|         4 | 2889 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2890 | `{` |
|         - | 2891 | `	ph7_hashmap *pMap;` |
|         - | 2892 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2893 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2894 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2895 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2896 | `		return PH7_OK;` |
|         - | 2897 | `	}` |
|         5 | 2898 | `	if( nArg > 1 ){` |
|         - | 2899 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2900 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2901 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|         5 | 2902 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|         5 | 2903 | `		if( rcCb != PH7_OK ){` |
|         3 | 2904 | `			return rcCb;` |
|         - | 2905 | `		}` |
|         1 | 2906 | `	}` |
|         - | 2907 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2908 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2909 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2910 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2911 | `		ph7_value *pCallback = 0;` |
|         - | 2912 | `		ProcNodeCmp xCmp;` |
|         3 | 2913 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2914 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2915 | `			/* Point to the desired callback */` |
|         3 | 2916 | `			pCallback = apArg[1];` |
|         2 | 2917 | `		}else{` |
|         - | 2918 | `			/* Use the default comparison function */` |
|       ! 0 | 2919 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2920 | `		}` |
|         - | 2921 | `		/* Do the merge sort */` |
|         3 | 2922 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2923 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2924 | `		/* Fix the last link broken by the merge */` |
|         3 | 2925 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2926 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2927 | `		}` |
|         3 | 2928 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2929 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2930 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2931 | `			return PH7_EXCEPTION;` |
|         - | 2932 | `		}` |
|         1 | 2933 | `	}` |
|         - | 2934 | `	/* All done,return TRUE */` |
|         3 | 2935 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2936 | `	return PH7_OK;` |
|         3 | 2937 | `}` |
|         - | 2938 | `/*` |
|         - | 2939 | ` * bool shuffle(array &$array)` |
|         - | 2940 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2941 | ` * Parameters` |
|         - | 2942 | ` *  $array` |
|         - | 2943 | ` *   The input array.` |
|         - | 2944 | ` * Return` |
|         - | 2945 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2946 | ` *` |
|         - | 2947 | ` */` |
|         2 | 2948 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2949 | `{` |
|         - | 2950 | `	ph7_hashmap *pMap;` |
|         - | 2951 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2952 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2953 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2954 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2955 | `		return PH7_OK;` |
|         - | 2956 | `	}` |
|         - | 2957 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2958 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2959 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2960 | `	if( pMap->nEntry > 1 ){` |
|         - | 2961 | `		/* Do the merge sort */` |
|         3 | 2962 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2963 | `		/* Fix the last link broken by the merge */` |
|        10 | 2964 | `		while(pMap->pLast->pPrev){` |
|         8 | 2965 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2966 | `		}` |
|         1 | 2967 | `	}` |
|         - | 2968 | `	/* All done,return TRUE */` |
|         3 | 2969 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2970 | `	return PH7_OK;` |
|         2 | 2971 | `}` |
|         - | 2972 | `/*` |
|         - | 2973 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2974 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2975 | ` * Parameters` |
|         - | 2976 | ` *  $var` |
|         - | 2977 | ` *   The array or the object.` |
|         - | 2978 | ` * $mode` |
|         - | 2979 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2980 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2981 | ` *  all the elements of a multidimensional array.` |
|         - | 2982 | ` * Return` |
|         - | 2983 | ` *  Returns the number of elements in the array.` |
|         - | 2984 | ` */` |
|      1976 | 2985 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2986 | `{` |
|      1981 | 2987 | `	int bRecursive = FALSE;` |
|      1981 | 2988 | `	int bCycleDetected = FALSE;` |
|         - | 2989 | `	sxi64 iCount;` |
|      1981 | 2990 | `	if( nArg < 1 ){` |
|       ! 0 | 2991 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2992 | `			"ArgumentCountError",` |
|         - | 2993 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2994 | `			);` |
|         - | 2995 | `	}` |
|      1981 | 2996 | `	if( nArg > 2 ){` |
|         4 | 2997 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2998 | `			"ArgumentCountError",` |
|         - | 2999 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3000 | `			nArg` |
|         - | 3001 | `			);` |
|         - | 3002 | `	}` |
|         - | 3003 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3004 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3005 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1979 | 3006 | `	if( nArg > 1 ){` |
|        44 | 3007 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 3008 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 3009 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3010 | `				"ValueError",` |
|         - | 3011 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3012 | `				);` |
|         - | 3013 | `		}` |
|        34 | 3014 | `		bRecursive = iMode == 1;` |
|        16 | 3015 | `	}` |
|      1971 | 3016 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3017 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 3018 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 3019 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 3020 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 3021 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 3022 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3023 | `					"count",sizeof("count")-1);` |
|        59 | 3024 | `				if( pMeth ){` |
|         - | 3025 | `					ph7_value sResult;` |
|        59 | 3026 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 3027 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3028 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3029 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3030 | `					return PH7_OK;` |
|         - | 3031 | `				}` |
|       ! 0 | 3032 | `			}` |
|         1 | 3033 | `		}` |
|        22 | 3034 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3035 | `			"TypeError",` |
|         - | 3036 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3037 | `			ph7_type_name(apArg[0])` |
|         - | 3038 | `			);` |
|         - | 3039 | `	}` |
|         - | 3040 | `	/* Count */` |
|      1903 | 3041 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1903 | 3042 | `	if( bCycleDetected ){` |
|         3 | 3043 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3044 | `	}` |
|      1903 | 3045 | `	ph7_result_int64(pCtx,iCount);` |
|      1903 | 3046 | `	return PH7_OK;` |
|       993 | 3047 | `}` |
|         - | 3048 | `/*` |
|         - | 3049 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3050 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3051 | ` * Parameters` |
|         - | 3052 | ` * $key` |
|         - | 3053 | ` *   Value to check.` |
|         - | 3054 | ` * $search` |
|         - | 3055 | ` *  An array with keys to check.` |
|         - | 3056 | ` * Return` |
|         - | 3057 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3058 | ` */` |
|        94 | 3059 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3060 | `{` |
|         - | 3061 | `	sxi32 rc;` |
|        99 | 3062 | `	if( nArg != 2 ){` |
|         - | 3063 | `		/* PHP requires exactly two arguments */` |
|         4 | 3064 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3065 | `			"ArgumentCountError",` |
|         - | 3066 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3067 | `			nArg` |
|         - | 3068 | `			);` |
|         - | 3069 | `	}` |
|         - | 3070 | `	/* Make sure we are dealing with a valid hashmap */` |
|        97 | 3071 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3072 | `		/* Type mismatch -> TypeError */` |
|         8 | 3073 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3074 | `			"TypeError",` |
|         - | 3075 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3076 | `			ph7_type_name(apArg[1])` |
|         - | 3077 | `			);` |
|         - | 3078 | `	}` |
|         - | 3079 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        92 | 3080 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3081 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3082 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3083 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3084 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3085 | `			"use an empty string instead"` |
|         - | 3086 | `			);` |
|        91 | 3087 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3088 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3089 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3090 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3091 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3092 | `				,rVal` |
|         - | 3093 | `				);` |
|         1 | 3094 | `		}` |
|         1 | 3095 | `	}` |
|         - | 3096 | `	/* Perform the lookup */` |
|        92 | 3097 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3098 | `	/* lookup result */` |
|        92 | 3099 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        92 | 3100 | `	return PH7_OK;` |
|        52 | 3101 | `}` |
|         - | 3102 | `/*` |
|         - | 3103 | ` * value array_pop(array $array)` |
|         - | 3104 | ` *   POP the last inserted element from the array.` |
|         - | 3105 | ` * Parameter` |
|         - | 3106 | ` *  The array to get the value from.` |
|         - | 3107 | ` * Return` |
|         - | 3108 | ` *  Poped value or NULL on failure.` |
|         - | 3109 | ` */` |
|       102 | 3110 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3111 | `{` |
|         - | 3112 | `	ph7_hashmap *pMap;` |
|         - | 3113 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       106 | 3114 | `	if( nArg != 1 ){` |
|         4 | 3115 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3116 | `			"ArgumentCountError",` |
|         - | 3117 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3118 | `			nArg` |
|         - | 3119 | `			);` |
|         - | 3120 | `	}` |
|         - | 3121 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3122 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3123 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3124 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3125 | `			"Error",` |
|         - | 3126 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3127 | `			);` |
|         - | 3128 | `	}` |
|         - | 3129 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3130 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3131 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3132 | `			"TypeError",` |
|         - | 3133 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3134 | `			ph7_type_name(apArg[0])` |
|         - | 3135 | `			);` |
|         - | 3136 | `	}` |
|        95 | 3137 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3138 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3139 | `	if( pMap->nEntry < 1 ){` |
|         - | 3140 | `		/* Nothing to pop,return NULL */` |
|         3 | 3141 | `		ph7_result_null(pCtx);` |
|         2 | 3142 | `	}else{` |
|        93 | 3143 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3144 | `		ph7_value *pObj;` |
|        93 | 3145 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3146 | `		if( pObj ){` |
|         - | 3147 | `			/* Node value */` |
|        93 | 3148 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3149 | `			/* Unlink the node */` |
|        93 | 3150 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3151 | `		}else{` |
|       ! 0 | 3152 | `			ph7_result_null(pCtx);` |
|         - | 3153 | `		}` |
|         - | 3154 | `		/* Reset the cursor */` |
|        93 | 3155 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3156 | `	}` |
|        95 | 3157 | `	return PH7_OK;` |
|        55 | 3158 | `}` |
|         - | 3159 | `/*` |
|         - | 3160 | ` * int array_push($array,$var,...)` |
|         - | 3161 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3162 | ` * Parameters` |
|         - | 3163 | ` *  array` |
|         - | 3164 | ` *    The input array.` |
|         - | 3165 | ` *  var` |
|         - | 3166 | ` *   On or more value to push.` |
|         - | 3167 | ` * Return` |
|         - | 3168 | ` *  New array count (including old items).` |
|         - | 3169 | ` */` |
|        22 | 3170 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3171 | `{` |
|         - | 3172 | `	ph7_hashmap *pMap;` |
|         - | 3173 | `	sxi32 rc;` |
|         - | 3174 | `	int i;` |
|        26 | 3175 | `	if( nArg < 1 ){` |
|       ! 0 | 3176 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3177 | `			"ArgumentCountError",` |
|         - | 3178 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3179 | `			nArg` |
|         - | 3180 | `			);` |
|         - | 3181 | `	}` |
|         - | 3182 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3183 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3184 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3185 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3186 | `			"Error",` |
|         - | 3187 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3188 | `			);` |
|         - | 3189 | `	}` |
|         - | 3190 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3191 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3192 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3193 | `			"TypeError",` |
|         - | 3194 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3195 | `			ph7_type_name(apArg[0])` |
|         - | 3196 | `			);` |
|         - | 3197 | `	}` |
|         - | 3198 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3199 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3200 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3201 | `	/* Start pushing given values */` |
|        34 | 3202 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3203 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3204 | `		if( rc != SXRET_OK ){` |
|         3 | 3205 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3206 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3207 | `				return rc;` |
|         - | 3208 | `			}` |
|       ! 0 | 3209 | `			break;` |
|         - | 3210 | `		}` |
|         9 | 3211 | `	}` |
|         - | 3212 | `	/* Return the new count */` |
|        15 | 3213 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3214 | `	return PH7_OK;` |
|        15 | 3215 | `}` |
|         - | 3216 | `/*` |
|         - | 3217 | ` * value array_shift(array $array)` |
|         - | 3218 | ` *   Shift an element off the beginning of array.` |
|         - | 3219 | ` * Parameter` |
|         - | 3220 | ` *  The array to get the value from.` |
|         - | 3221 | ` * Return` |
|         - | 3222 | ` *  Shifted value or NULL on failure.` |
|         - | 3223 | ` */` |
|        44 | 3224 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3225 | `{` |
|         - | 3226 | `	ph7_hashmap *pMap;` |
|         - | 3227 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3228 | `	if( nArg != 1 ){` |
|         4 | 3229 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3230 | `			"ArgumentCountError",` |
|         - | 3231 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3232 | `			nArg` |
|         - | 3233 | `			);` |
|         - | 3234 | `	}` |
|         - | 3235 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3236 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3237 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3238 | `			"Error",` |
|         - | 3239 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3240 | `			);` |
|         - | 3241 | `	}` |
|         - | 3242 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3243 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3244 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3245 | `			"TypeError",` |
|         - | 3246 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3247 | `			ph7_type_name(apArg[0])` |
|         - | 3248 | `			);` |
|         - | 3249 | `	}` |
|         - | 3250 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3251 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3252 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3253 | `	if( pMap->nEntry < 1 ){` |
|         - | 3254 | `		/* Empty hashmap,return NULL */` |
|         3 | 3255 | `		ph7_result_null(pCtx);` |
|         2 | 3256 | `	}else{` |
|        39 | 3257 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3258 | `		ph7_value *pObj;` |
|         - | 3259 | `		sxu32 n;` |
|        39 | 3260 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3261 | `		if( pObj ){` |
|         - | 3262 | `			/* Node value */` |
|        39 | 3263 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3264 | `			/* Unlink the first node */` |
|        39 | 3265 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3266 | `		}else{` |
|       ! 0 | 3267 | `			ph7_result_null(pCtx);` |
|         - | 3268 | `		}` |
|         - | 3269 | `		/* Rehash all int keys */` |
|        39 | 3270 | `		n = pMap->nEntry;` |
|        39 | 3271 | `		pEntry = pMap->pFirst;` |
|        39 | 3272 | `		pMap->iNextIdx = 0;` |
|        39 | 3273 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3274 | `		for(;;){` |
|        99 | 3275 | `			if( n < 1 ){` |
|        39 | 3276 | `				break;` |
|         - | 3277 | `			}` |
|        65 | 3278 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3279 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3280 | `			}` |
|         - | 3281 | `			/* Point to the next entry */` |
|        65 | 3282 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3283 | `			n--;` |
|         5 | 3284 | `		}` |
|         - | 3285 | `		/* Reset the cursor */` |
|        39 | 3286 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3287 | `	}` |
|        41 | 3288 | `	return PH7_OK;` |
|        27 | 3289 | `}` |
|         - | 3290 | `/*` |
|         - | 3291 | ` * Extract the node cursor value.` |
|         - | 3292 | ` */` |
|      1094 | 3293 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3294 | `{` |
|      1095 | 3295 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3296 | `	ph7_value *pVal;` |
|      1095 | 3297 | `	if( pCur == 0 ){` |
|         - | 3298 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3299 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3300 | `		return PH7_OK;` |
|         - | 3301 | `	}` |
|      1057 | 3302 | `	if( iDirection != 0 ){` |
|       201 | 3303 | `		if( iDirection > 0 ){` |
|         - | 3304 | `			/* Point to the next entry */` |
|       199 | 3305 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3306 | `			pCur = pMap->pCur;` |
|       100 | 3307 | `		}else{` |
|         - | 3308 | `			/* Point to the previous entry */` |
|         3 | 3309 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3310 | `			pCur = pMap->pCur;` |
|         - | 3311 | `		}` |
|       201 | 3312 | `		if( pCur == 0 ){` |
|         - | 3313 | `			/* End of input reached,return FALSE */` |
|        83 | 3314 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3315 | `			return PH7_OK;` |
|         - | 3316 | `		}` |
|        59 | 3317 | `	}` |
|         - | 3318 | `	/* Point to the desired element */` |
|       975 | 3319 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3320 | `	if( pVal ){` |
|       975 | 3321 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3322 | `	}else{` |
|       ! 0 | 3323 | `		ph7_result_bool(pCtx,0);` |
|         - | 3324 | `	}` |
|       975 | 3325 | `	return PH7_OK;` |
|       548 | 3326 | `}` |
|         - | 3327 | `/*` |
|         - | 3328 | ` * value current(array $array)` |
|         - | 3329 | ` *  Return the current element in an array.` |
|         - | 3330 | ` * Parameter` |
|         - | 3331 | ` *  $input: The input array.` |
|         - | 3332 | ` * Return` |
|         - | 3333 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3334 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3335 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3336 | ` *  is empty, current() returns FALSE.` |
|         - | 3337 | ` */` |
|       302 | 3338 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3339 | `{` |
|       303 | 3340 | `	if( nArg < 1 ){` |
|         - | 3341 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3342 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3343 | `		return PH7_OK;` |
|         - | 3344 | `	}` |
|         - | 3345 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3346 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3347 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3348 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3349 | `		return PH7_OK;` |
|         - | 3350 | `	}` |
|       303 | 3351 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3352 | `	return PH7_OK;` |
|       152 | 3353 | `}` |
|         - | 3354 | `/*` |
|         - | 3355 | ` * value next(array $input)` |
|         - | 3356 | ` *  Advance the internal array pointer of an array.` |
|         - | 3357 | ` * Parameter` |
|         - | 3358 | ` *  $input: The input array.` |
|         - | 3359 | ` * Return` |
|         - | 3360 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3361 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3362 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3363 | ` */` |
|       198 | 3364 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3365 | `{` |
|       199 | 3366 | `	if( nArg < 1 ){` |
|         - | 3367 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3368 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3369 | `		return PH7_OK;` |
|         - | 3370 | `	}` |
|         - | 3371 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3372 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3373 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3374 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3375 | `		return PH7_OK;` |
|         - | 3376 | `	}` |
|       199 | 3377 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3378 | `	return PH7_OK;` |
|       100 | 3379 | `}` |
|         - | 3380 | `/*` |
|         - | 3381 | ` * value prev(array $input)` |
|         - | 3382 | ` *  Rewind the internal array pointer.` |
|         - | 3383 | ` * Parameter` |
|         - | 3384 | ` *  $input: The input array.` |
|         - | 3385 | ` * Return` |
|         - | 3386 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3387 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3388 | ` *  elements.` |
|         - | 3389 | ` */` |
|         2 | 3390 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3391 | `{` |
|         3 | 3392 | `	if( nArg < 1 ){` |
|         - | 3393 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3394 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3395 | `		return PH7_OK;` |
|         - | 3396 | `	}` |
|         - | 3397 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3398 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3399 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3400 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3401 | `		return PH7_OK;` |
|         - | 3402 | `	}` |
|         3 | 3403 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3404 | `	return PH7_OK;` |
|         2 | 3405 | `}` |
|         - | 3406 | `/*` |
|         - | 3407 | ` * value end(array $input)` |
|         - | 3408 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3409 | ` * Parameter` |
|         - | 3410 | ` *  $input: The input array.` |
|         - | 3411 | ` * Return` |
|         - | 3412 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3413 | ` */` |
|       348 | 3414 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3415 | `{` |
|         - | 3416 | `	ph7_hashmap *pMap;` |
|       349 | 3417 | `	if( nArg < 1 ){` |
|         - | 3418 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3419 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3420 | `		return PH7_OK;` |
|         - | 3421 | `	}` |
|         - | 3422 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3423 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3424 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3425 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3426 | `		return PH7_OK;` |
|         - | 3427 | `	}` |
|         - | 3428 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3429 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3430 | `	/* Point to the last node */` |
|       349 | 3431 | `	pMap->pCur = pMap->pLast;` |
|         - | 3432 | `	/* Return the last node value */` |
|       349 | 3433 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3434 | `	return PH7_OK;` |
|       175 | 3435 | `}` |
|         - | 3436 | `/*` |
|         - | 3437 | ` * value reset(array $array )` |
|         - | 3438 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3439 | ` * Parameter` |
|         - | 3440 | ` *  $input: The input array.` |
|         - | 3441 | ` * Return` |
|         - | 3442 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3443 | ` */` |
|       244 | 3444 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3445 | `{` |
|         - | 3446 | `	ph7_hashmap *pMap;` |
|       245 | 3447 | `	if( nArg < 1 ){` |
|         - | 3448 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3449 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3450 | `		return PH7_OK;` |
|         - | 3451 | `	}` |
|         - | 3452 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3453 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3454 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3455 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3456 | `		return PH7_OK;` |
|         - | 3457 | `	}` |
|         - | 3458 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3459 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3460 | `	/* Point to the first node */` |
|       245 | 3461 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3462 | `	/* Return the last node value if available */` |
|       245 | 3463 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3464 | `	return PH7_OK;` |
|       123 | 3465 | `}` |
|         - | 3466 | `/*` |
|         - | 3467 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3468 | ` * array_key_first() and array_key_last().` |
|         - | 3469 | ` */` |
|       672 | 3470 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3471 | `{` |
|       673 | 3472 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3473 | `		/* Key is integer */` |
|       283 | 3474 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3475 | `	}else{` |
|         - | 3476 | `		/* Key is blob */` |
|       586 | 3477 | `		ph7_result_string(pCtx,` |
|       390 | 3478 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3479 | `	}` |
|       673 | 3480 | `}` |
|         - | 3481 | `/*` |
|         - | 3482 | ` * value key(array $array)` |
|         - | 3483 | ` *   Fetch a key from an array` |
|         - | 3484 | ` * Parameter` |
|         - | 3485 | ` *  $input` |
|         - | 3486 | ` *   The input array.` |
|         - | 3487 | ` * Return` |
|         - | 3488 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3489 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3490 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3491 | ` *  is empty, key() returns NULL.` |
|         - | 3492 | ` */` |
|       776 | 3493 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3494 | `{` |
|         - | 3495 | `	ph7_hashmap_node *pCur;` |
|         - | 3496 | `	ph7_hashmap *pMap;` |
|       777 | 3497 | `	if( nArg < 1 ){` |
|         - | 3498 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3499 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3500 | `		return PH7_OK;` |
|         - | 3501 | `	}` |
|         - | 3502 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3503 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3504 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3505 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3506 | `		return PH7_OK;` |
|         - | 3507 | `	}` |
|       777 | 3508 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3509 | `	pCur = pMap->pCur;` |
|       777 | 3510 | `	if( pCur == 0 ){` |
|         - | 3511 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3512 | `		ph7_result_null(pCtx);` |
|       121 | 3513 | `		return PH7_OK;` |
|         - | 3514 | `	}` |
|       657 | 3515 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3516 | `	return PH7_OK;` |
|       389 | 3517 | `}` |
|         - | 3518 | `/*` |
|         - | 3519 | ` * array each(array $input)` |
|         - | 3520 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3521 | ` * Parameter` |
|         - | 3522 | ` *  $input` |
|         - | 3523 | ` *    The input array.` |
|         - | 3524 | ` * Return` |
|         - | 3525 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3526 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3527 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3528 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3529 | ` *  each() returns FALSE.` |
|         - | 3530 | ` */` |
|        22 | 3531 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3532 | `{` |
|         - | 3533 | `	ph7_hashmap_node *pCur;` |
|         - | 3534 | `	ph7_hashmap *pMap;` |
|         - | 3535 | `	ph7_value *pArray;` |
|         - | 3536 | `	ph7_value *pVal;` |
|         - | 3537 | `	ph7_value sKey;` |
|        23 | 3538 | `	if( nArg < 1 ){` |
|         - | 3539 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3540 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3541 | `		return PH7_OK;` |
|         - | 3542 | `	}` |
|         - | 3543 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3544 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3545 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3546 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3547 | `		return PH7_OK;` |
|         - | 3548 | `	}` |
|         - | 3549 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3550 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3551 | `	if( pMap->pCur == 0 ){` |
|         - | 3552 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3553 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3554 | `		return PH7_OK;` |
|         - | 3555 | `	}` |
|        15 | 3556 | `	pCur = pMap->pCur;` |
|         - | 3557 | `	/* Create a new array */` |
|        15 | 3558 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3559 | `	if( pArray == 0 ){` |
|       ! 0 | 3560 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3561 | `		return PH7_OK;` |
|         - | 3562 | `	}` |
|        15 | 3563 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3564 | `	/* Insert the current value */` |
|        15 | 3565 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3566 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3567 | `	/* Make the key */` |
|        15 | 3568 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3569 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3570 | `	}else{` |
|         9 | 3571 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3572 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3573 | `	}` |
|         - | 3574 | `	/* Insert the current key */` |
|        15 | 3575 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3576 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3577 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3578 | `	/* Advance the cursor */` |
|        15 | 3579 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3580 | `	/* Return the current entry */` |
|        15 | 3581 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3582 | `	return PH7_OK;` |
|        12 | 3583 | `}` |
|         - | 3584 | `/*` |
|         - | 3585 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3586 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3587 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3588 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3589 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3590 | ` */` |
|         - | 3591 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3592 | `/*` |
|         - | 3593 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3594 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3595 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3596 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3597 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3598 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3599 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3600 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3601 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3602 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3603 | ` */` |
|         - | 3604 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3605 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3606 | `/*` |
|         - | 3607 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3608 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3609 | ` */` |
|       ! 0 | 3610 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3611 | `{` |
|       ! 0 | 3612 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3613 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3614 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3615 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3616 | `		zBuf[n] = 0;` |
|       ! 0 | 3617 | `		return zBuf;` |
|         - | 3618 | `	}` |
|       ! 0 | 3619 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3620 | `}` |
|         - | 3621 | `/*` |
|         - | 3622 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3623 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3624 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3625 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3626 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3627 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3628 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3629 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3630 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3631 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3632 | ` */` |
|       156 | 3633 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3634 | `{` |
|       157 | 3635 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3636 | `	sxu64 uVal = 0;` |
|       157 | 3637 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3638 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3639 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3640 | `		bNeg = (z[0] == '-');` |
|         3 | 3641 | `		z++;` |
|         1 | 3642 | `	}` |
|       237 | 3643 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3644 | `		int d = z[0] - '0';` |
|         - | 3645 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3646 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3647 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3648 | `			bOverflow = 1;` |
|       ! 0 | 3649 | `		}else{` |
|        81 | 3650 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3651 | `		}` |
|        81 | 3652 | `		bDigit = 1;` |
|        81 | 3653 | `		z++;` |
|         1 | 3654 | `	}` |
|       157 | 3655 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3656 | `		bReal = 1;` |
|         3 | 3657 | `		z++;` |
|         5 | 3658 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3659 | `			bDigit = 1;` |
|         3 | 3660 | `			z++;` |
|         1 | 3661 | `		}` |
|         1 | 3662 | `	}` |
|         - | 3663 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3664 | `	if( !bDigit ){` |
|        61 | 3665 | `		return RANGE_IN_ERROR;` |
|         - | 3666 | `	}` |
|         - | 3667 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3668 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3669 | `		z++;` |
|         9 | 3670 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3671 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3672 | `			return RANGE_IN_ERROR;` |
|         - | 3673 | `		}` |
|         9 | 3674 | `		bReal = 1;` |
|        17 | 3675 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3676 | `	}` |
|         - | 3677 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3678 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3679 | `	if( z != zEnd ){` |
|        13 | 3680 | `		return RANGE_IN_ERROR;` |
|         - | 3681 | `	}` |
|        84 | 3682 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3683 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3684 | `		bReal = 1;` |
|        84 | 3685 | `	}` |
|        43 | 3686 | `	if( bReal ){` |
|        11 | 3687 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3688 | `		return RANGE_IN_DOUBLE;` |
|         - | 3689 | `	}` |
|         - | 3690 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3691 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3692 | `	return RANGE_IN_LONG;` |
|        58 | 3693 | `}` |
|         - | 3694 | `/*` |
|         - | 3695 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3696 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3697 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3698 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3699 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3700 | ` */` |
|       328 | 3701 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3702 | `{` |
|         - | 3703 | `	char zMsg[160];` |
|       329 | 3704 | `	*pRc = PH7_OK;` |
|       329 | 3705 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3706 | `		char zType[80];` |
|       ! 0 | 3707 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3708 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3709 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3710 | `		return FALSE;` |
|         - | 3711 | `	}` |
|       329 | 3712 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3713 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3714 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3715 | `			iArg,zName);` |
|         5 | 3716 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3717 | `		*pbNullCoerced = TRUE;` |
|         2 | 3718 | `	}` |
|       329 | 3719 | `	return TRUE;` |
|       165 | 3720 | `}` |
|         - | 3721 | `/*` |
|         - | 3722 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3723 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3724 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3725 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3726 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3727 | ` */` |
|        60 | 3728 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3729 | `{` |
|        61 | 3730 | `	*pRc = PH7_OK;` |
|        61 | 3731 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3732 | `		char zType[80];` |
|       ! 0 | 3733 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3734 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3735 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3736 | `		return RANGE_IN_ERROR;` |
|         - | 3737 | `	}` |
|        61 | 3738 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3739 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3740 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3741 | `		*pLong = 0;` |
|         3 | 3742 | `		return RANGE_IN_LONG;` |
|         - | 3743 | `	}` |
|        59 | 3744 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3745 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3746 | `		return RANGE_IN_DOUBLE;` |
|         - | 3747 | `	}` |
|        35 | 3748 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3749 | `		const char *zStr;` |
|         - | 3750 | `		int nLen;` |
|         - | 3751 | `		sxu8 iKind;` |
|         3 | 3752 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3753 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3754 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3755 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3756 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3757 | `		}` |
|         3 | 3758 | `		return iKind;` |
|         - | 3759 | `	}` |
|         - | 3760 | `	/* int / bool */` |
|        33 | 3761 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3762 | `	return RANGE_IN_LONG;` |
|        31 | 3763 | `}` |
|         - | 3764 | `/*` |
|         - | 3765 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3766 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3767 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3768 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3769 | ` */` |
|       296 | 3770 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3771 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3772 | `{` |
|         - | 3773 | `	char zMsg[160];` |
|         - | 3774 | `	double r;` |
|       297 | 3775 | `	*pRc = PH7_OK;` |
|       297 | 3776 | `	if( bNullCoerced ){` |
|         - | 3777 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3778 | `		*pLong = 0;` |
|         5 | 3779 | `		*pDouble = 0.0;` |
|         5 | 3780 | `		return RANGE_IN_LONG;` |
|         - | 3781 | `	}` |
|       293 | 3782 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3783 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3784 | `check_dval:` |
|        25 | 3785 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3786 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3787 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3788 | `			return RANGE_IN_ERROR;` |
|         - | 3789 | `		}` |
|        21 | 3790 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3791 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3792 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3793 | `			return RANGE_IN_ERROR;` |
|         - | 3794 | `		}` |
|        17 | 3795 | `		*pDouble = r;` |
|        17 | 3796 | `		return RANGE_IN_DOUBLE;` |
|         - | 3797 | `	}` |
|       273 | 3798 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3799 | `		const char *zStr;` |
|         - | 3800 | `		int nLen;` |
|         - | 3801 | `		sxu8 iKind;` |
|        81 | 3802 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3803 | `		if( nLen == 0 ){` |
|         7 | 3804 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3805 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3806 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3807 | `			*pLong = 0;` |
|         5 | 3808 | `			*pDouble = 0.0;` |
|        41 | 3809 | `			return RANGE_IN_LONG;` |
|         - | 3810 | `		}` |
|        77 | 3811 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3812 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3813 | `			r = *pDouble;` |
|         5 | 3814 | `			goto check_dval;` |
|         - | 3815 | `		}` |
|        73 | 3816 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3817 | `			*pDouble = (double)*pLong;` |
|        23 | 3818 | `			if( nLen == 1 ){` |
|         - | 3819 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3820 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3821 | `				return RANGE_IN_DIGIT;` |
|         - | 3822 | `			}` |
|        15 | 3823 | `			return RANGE_IN_LONG;` |
|         - | 3824 | `		}` |
|        51 | 3825 | `		if( nLen != 1 ){` |
|        10 | 3826 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3827 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3828 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3829 | `		}` |
|        51 | 3830 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3831 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3832 | `		*pLong = 0;` |
|        51 | 3833 | `		*pDouble = 0.0;` |
|        51 | 3834 | `		return RANGE_IN_STRING;` |
|         - | 3835 | `	}` |
|         - | 3836 | `	/* int / bool */` |
|       193 | 3837 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3838 | `	*pDouble = (double)*pLong;` |
|       193 | 3839 | `	return RANGE_IN_LONG;` |
|       149 | 3840 | `}` |
|         - | 3841 | `/*` |
|         - | 3842 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3843 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3844 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3845 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3846 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3847 | ` * exactly like php's two macros.` |
|         - | 3848 | ` */` |
|         6 | 3849 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3850 | `{` |
|        10 | 3851 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3852 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3853 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3854 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3855 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3856 | `}` |
|         6 | 3857 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3858 | `{` |
|         - | 3859 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3860 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3861 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3862 | `	const unsigned int nBuf = 1500;` |
|         7 | 3863 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3864 | `	if( zMsg == 0 ){` |
|       ! 0 | 3865 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3866 | `	}` |
|         7 | 3867 | `	snprintf(zMsg,nBuf,` |
|         - | 3868 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3869 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3870 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3871 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3872 | `}` |
|         - | 3873 | `/*` |
|         - | 3874 | ` * Set the element container to the next range element and append it to the` |
|         - | 3875 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3876 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3877 | ` * below stay one line per iteration.` |
|         - | 3878 | ` */` |
|      1680 | 3879 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3880 | `{` |
|      1681 | 3881 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3882 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3883 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3884 | `	}` |
|      1681 | 3885 | `	return PH7_OK;` |
|       841 | 3886 | `}` |
|        70 | 3887 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3888 | `{` |
|        71 | 3889 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3890 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3891 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3892 | `	}` |
|        71 | 3893 | `	return PH7_OK;` |
|        36 | 3894 | `}` |
|       168 | 3895 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3896 | `{` |
|       169 | 3897 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3898 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3899 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3900 | `	}` |
|       169 | 3901 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3902 | `	return PH7_OK;` |
|        85 | 3903 | `}` |
|         - | 3904 | `/*` |
|         - | 3905 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3906 | ` *  Create an array containing a range of elements.` |
|         - | 3907 | ` * Return` |
|         - | 3908 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3909 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3910 | ` */` |
|       166 | 3911 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3912 | `{` |
|         - | 3913 | `	ph7_value *pValue,*pArray;` |
|       167 | 3914 | `	sxi32 rc = PH7_OK;` |
|       167 | 3915 | `	int is_step_double = 0,is_step_negative = 0;` |
|       167 | 3916 | `	double step_double = 1.0;` |
|       167 | 3917 | `	sxi64 step = 1;` |
|         - | 3918 | `	sxu8 start_type,end_type;` |
|       167 | 3919 | `	sxi64 start_long = 0,end_long = 0;` |
|       167 | 3920 | `	double start_double = 0.0,end_double = 0.0;` |
|       167 | 3921 | `	unsigned char cStart = 0,cEnd = 0;` |
|       167 | 3922 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3923 | `	sxu32 i,size;` |
|         - | 3924 |  |
|         - | 3925 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       167 | 3926 | `	if( nArg > 3 ){` |
|         4 | 3927 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3928 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3929 | `	}` |
|       165 | 3930 | `	if( nArg < 2 ){` |
|         - | 3931 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3932 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3933 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3934 | `	}` |
|         - | 3935 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3936 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       165 | 3937 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 3938 | `		return rc;` |
|         - | 3939 | `	}` |
|       165 | 3940 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3941 | `		return rc;` |
|         - | 3942 | `	}` |
|       165 | 3943 | `	if( nArg > 2 ){` |
|        61 | 3944 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 3945 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 3946 | `			return rc;` |
|         - | 3947 | `		}` |
|        59 | 3948 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3949 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3950 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3951 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3952 | `			}` |
|        23 | 3953 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3954 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3955 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3956 | `			}` |
|         - | 3957 | `			/* We only want positive step values. */` |
|        21 | 3958 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3959 | `				is_step_negative = 1;` |
|       ! 0 | 3960 | `				step_double *= -1;` |
|       ! 0 | 3961 | `			}` |
|         - | 3962 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3963 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3964 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3965 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3966 | `				step = (sxi64)step_double;` |
|        19 | 3967 | `				if( (double)step != step_double ){` |
|        17 | 3968 | `					is_step_double = 1;` |
|         8 | 3969 | `				}` |
|        10 | 3970 | `			}else{` |
|         - | 3971 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3972 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3973 | `				is_step_double = 1;` |
|         - | 3974 | `			}` |
|        11 | 3975 | `		}else{` |
|         - | 3976 | `			/* We only want positive step values. */` |
|        35 | 3977 | `			if( step < 0 ){` |
|        11 | 3978 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3979 | `					/* -step would overflow */` |
|         4 | 3980 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3981 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3982 | `				}` |
|         9 | 3983 | `				is_step_negative = 1;` |
|         9 | 3984 | `				step = -step;` |
|         4 | 3985 | `			}` |
|        33 | 3986 | `			step_double = (double)step;` |
|         - | 3987 | `		}` |
|        53 | 3988 | `		if( step_double == 0.0 ){` |
|         7 | 3989 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3990 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3991 | `		}` |
|        23 | 3992 | `	}` |
|       151 | 3993 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 3994 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3995 | `		return rc;` |
|         - | 3996 | `	}` |
|       147 | 3997 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 3998 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3999 | `		return rc;` |
|         - | 4000 | `	}` |
|         - | 4001 | `	/* Element container + result array */` |
|       143 | 4002 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 4003 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 4004 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4005 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4006 | `	}` |
|         - | 4007 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 4008 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4009 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4010 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4011 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4012 | `			 * and the range is numeric. */` |
|        15 | 4013 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4014 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4015 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4016 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4017 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4018 | `				}` |
|         7 | 4019 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4020 | `			}else{` |
|         9 | 4021 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4022 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4023 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4024 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4025 | `				}` |
|         9 | 4026 | `				start_type = RANGE_IN_LONG;` |
|         - | 4027 | `			}` |
|        15 | 4028 | `			goto handle_numeric_inputs;` |
|         - | 4029 | `		}` |
|        23 | 4030 | `		if( is_step_double ){` |
|         - | 4031 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4032 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4033 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4034 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4035 | `					" of characters, inputs converted to 0");` |
|         1 | 4036 | `			}` |
|         5 | 4037 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4038 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4039 | `			goto handle_numeric_inputs;` |
|         - | 4040 | `		}` |
|         - | 4041 | `		/* Generate an array of characters */` |
|        19 | 4042 | `		if( cStart > cEnd ){` |
|         - | 4043 | `			/* Decreasing char range */` |
|         - | 4044 | `			int iCur;` |
|         3 | 4045 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4046 | `				goto boundary_error;` |
|         - | 4047 | `			}` |
|        17 | 4048 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4049 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4050 | `					return rc;` |
|         - | 4051 | `				}` |
|         8 | 4052 | `			}` |
|        18 | 4053 | `		}else if( cEnd > cStart ){` |
|         - | 4054 | `			/* Increasing char range */` |
|         - | 4055 | `			int iCur;` |
|        15 | 4056 | `			if( is_step_negative ){` |
|         3 | 4057 | `				goto negative_step_error;` |
|         - | 4058 | `			}` |
|        13 | 4059 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4060 | `				goto boundary_error;` |
|         - | 4061 | `			}` |
|       163 | 4062 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4063 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4064 | `					return rc;` |
|         - | 4065 | `				}` |
|        77 | 4066 | `			}` |
|         6 | 4067 | `		}else{` |
|         3 | 4068 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4069 | `				return rc;` |
|         - | 4070 | `			}` |
|         - | 4071 | `		}` |
|        15 | 4072 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4073 | `		return PH7_OK;` |
|         - | 4074 | `	}` |
|        53 | 4075 | `handle_numeric_inputs:` |
|       133 | 4076 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4077 | `		/* Float range */` |
|         - | 4078 | `		double elem,calc;` |
|        25 | 4079 | `		if( start_double > end_double ){` |
|         - | 4080 | `			/* Decreasing float range */` |
|         7 | 4081 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4082 | `				goto boundary_error;` |
|         - | 4083 | `			}` |
|         7 | 4084 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4085 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4086 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4087 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4088 | `			}` |
|         5 | 4089 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4090 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4091 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4092 | `					return rc;` |
|         - | 4093 | `				}` |
|         8 | 4094 | `			}` |
|        21 | 4095 | `		}else if( end_double > start_double ){` |
|         - | 4096 | `			/* Increasing float range */` |
|        17 | 4097 | `			if( is_step_negative ){` |
|       ! 0 | 4098 | `				goto negative_step_error;` |
|         - | 4099 | `			}` |
|        17 | 4100 | `			if( end_double - start_double < step_double ){` |
|         3 | 4101 | `				goto boundary_error;` |
|         - | 4102 | `			}` |
|        15 | 4103 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4104 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4105 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4106 | `			}` |
|        11 | 4107 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4108 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4109 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4110 | `					return rc;` |
|         - | 4111 | `				}` |
|        28 | 4112 | `			}` |
|         6 | 4113 | `		}else{` |
|         3 | 4114 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4115 | `				return rc;` |
|         - | 4116 | `			}` |
|         - | 4117 | `		}` |
|         9 | 4118 | `	}else{` |
|         - | 4119 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4120 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4121 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4122 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4123 | `		sxu64 calc;` |
|       101 | 4124 | `		if( start_long > end_long ){` |
|         - | 4125 | `			/* Decreasing int range */` |
|        19 | 4126 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4127 | `				goto boundary_error;` |
|         - | 4128 | `			}` |
|        17 | 4129 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4130 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4131 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4132 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4133 | `			}` |
|        15 | 4134 | `			size = (sxu32)(calc + 1);` |
|       101 | 4135 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4136 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4137 | `					return rc;` |
|         - | 4138 | `				}` |
|        44 | 4139 | `			}` |
|        90 | 4140 | `		}else if( end_long > start_long ){` |
|         - | 4141 | `			/* Increasing int range */` |
|        77 | 4142 | `			if( is_step_negative ){` |
|         3 | 4143 | `				goto negative_step_error;` |
|         - | 4144 | `			}` |
|        75 | 4145 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4146 | `				goto boundary_error;` |
|         - | 4147 | `			}` |
|        73 | 4148 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4149 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4150 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4151 | `			}` |
|        69 | 4152 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4153 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4154 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4155 | `					return rc;` |
|         - | 4156 | `				}` |
|       795 | 4157 | `			}` |
|        35 | 4158 | `		}else{` |
|         7 | 4159 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4160 | `				return rc;` |
|         - | 4161 | `			}` |
|         - | 4162 | `		}` |
|         - | 4163 | `	}` |
|         - | 4164 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4165 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4166 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4167 | `	return PH7_OK;` |
|         2 | 4168 | `negative_step_error:` |
|         5 | 4169 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4170 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4171 | `boundary_error:` |
|         9 | 4172 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4173 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        84 | 4174 | `}` |
|         - | 4175 | `/*` |
|         - | 4176 | ` * array array_values(array $array)` |
|         - | 4177 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4178 | ` * Parameters` |
|         - | 4179 | ` *  $array` |
|         - | 4180 | ` *   The input array.` |
|         - | 4181 | ` * Return` |
|         - | 4182 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4183 | ` */` |
|        48 | 4184 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4185 | `{` |
|         - | 4186 | `	ph7_hashmap_node *pNode;` |
|         - | 4187 | `	ph7_hashmap *pMap;` |
|         - | 4188 | `	ph7_value *pArray;` |
|         - | 4189 | `	ph7_value *pObj;` |
|         - | 4190 | `	sxu32 n;` |
|        51 | 4191 | `	if( nArg != 1 ){` |
|         - | 4192 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4193 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4194 | `			"ArgumentCountError",` |
|         - | 4195 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4196 | `			nArg` |
|         - | 4197 | `			);` |
|         - | 4198 | `	}` |
|         - | 4199 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4200 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4201 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4202 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4203 | `			"TypeError",` |
|         - | 4204 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4205 | `			ph7_type_name(apArg[0])` |
|         - | 4206 | `			);` |
|         - | 4207 | `	}` |
|         - | 4208 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4209 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4210 | `	/* Create a new array */` |
|        46 | 4211 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4212 | `	if( pArray == 0 ){` |
|       ! 0 | 4213 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4214 | `		return PH7_OK;` |
|         - | 4215 | `	}` |
|         - | 4216 | `	/* Perform the requested operation */` |
|        46 | 4217 | `	pNode = pMap->pFirst;` |
|       144 | 4218 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4219 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4220 | `		if( pObj ){` |
|         - | 4221 | `			/* perform the insertion */` |
|       100 | 4222 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4223 | `		}` |
|         - | 4224 | `		/* Point to the next entry */` |
|       100 | 4225 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4226 | `	}` |
|         - | 4227 | `	/* return the new array */` |
|        46 | 4228 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4229 | `	return PH7_OK;` |
|        27 | 4230 | `}` |
|         - | 4231 | `/*` |
|         - | 4232 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4233 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4234 | ` * Parameters` |
|         - | 4235 | ` *  $input` |
|         - | 4236 | ` *   An array containing keys to return.` |
|         - | 4237 | ` * $search_value` |
|         - | 4238 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4239 | ` * $strict` |
|         - | 4240 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4241 | ` * Return` |
|         - | 4242 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4243 | ` */` |
|       160 | 4244 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4245 | `{` |
|         - | 4246 | `	ph7_hashmap_node *pNode;` |
|         - | 4247 | `	ph7_hashmap *pMap;` |
|         - | 4248 | `	ph7_value *pArray;` |
|         - | 4249 | `	ph7_value sObj;` |
|         - | 4250 | `	ph7_value sVal;` |
|         - | 4251 | `	SyString sKey;` |
|         - | 4252 | `	int bStrict;` |
|         - | 4253 | `	sxi32 rc;` |
|         - | 4254 | `	sxu32 n;` |
|       164 | 4255 | `	if( nArg < 1 ){` |
|         - | 4256 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4257 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4258 | `			"ArgumentCountError",` |
|         - | 4259 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4260 | `			);` |
|         - | 4261 | `	}` |
|         - | 4262 | `	/* Make sure we are dealing with a valid hashmap */` |
|       164 | 4263 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4264 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4265 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4266 | `			"TypeError",` |
|         - | 4267 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4268 | `			ph7_type_name(apArg[0])` |
|         - | 4269 | `			);` |
|         - | 4270 | `	}` |
|         - | 4271 | `	/* Point to the internal representation of the input hashmap */` |
|       161 | 4272 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4273 | `	/* Create a new array */` |
|       161 | 4274 | `	pArray = ph7_context_new_array(pCtx);` |
|       161 | 4275 | `	if( pArray == 0 ){` |
|       ! 0 | 4276 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4277 | `		return PH7_OK;` |
|         - | 4278 | `	}` |
|       161 | 4279 | `	bStrict = FALSE;` |
|       161 | 4280 | `	if( nArg > 2 ){` |
|         - | 4281 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4282 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4283 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4284 | `				"TypeError",` |
|         - | 4285 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4286 | `				ph7_type_name(apArg[2])` |
|         - | 4287 | `				);` |
|         - | 4288 | `		}` |
|         9 | 4289 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4290 | `	}` |
|         - | 4291 | `	/* Perform the requested operation */` |
|       161 | 4292 | `	pNode = pMap->pFirst;` |
|       161 | 4293 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1463 | 4294 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1305 | 4295 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       185 | 4296 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        94 | 4297 | `		}else{` |
|      1122 | 4298 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4299 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4300 | `		}` |
|      1305 | 4301 | `		rc = 0;` |
|      1305 | 4302 | `		if( nArg > 1 ){` |
|        65 | 4303 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4304 | `			if( pValue ){` |
|         - | 4305 | `				ph7_value sNeedle;` |
|        65 | 4306 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4307 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4308 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4309 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4310 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4311 | `				 * corrupt every later comparison. */` |
|        65 | 4312 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4313 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4314 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4315 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4316 | `			}` |
|        32 | 4317 | `		}` |
|      1305 | 4318 | `		if( rc == 0 ){` |
|         - | 4319 | `			/* Perform the insertion */` |
|      1273 | 4320 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       635 | 4321 | `		}` |
|      1305 | 4322 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4323 | `		/* Point to the next entry */` |
|      1305 | 4324 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       654 | 4325 | `	}` |
|         - | 4326 | `	/* return the new array */` |
|       161 | 4327 | `	ph7_result_value(pCtx,pArray);` |
|       161 | 4328 | `	return PH7_OK;` |
|        84 | 4329 | `}` |
|         - | 4330 | `/*` |
|         - | 4331 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4332 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4333 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4334 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4335 | ` * Parameters` |
|         - | 4336 | ` *  $arr1` |
|         - | 4337 | ` *   First array` |
|         - | 4338 | ` *  $arr2` |
|         - | 4339 | ` *   Second array` |
|         - | 4340 | ` * Return` |
|         - | 4341 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4342 | ` * Note` |
|         - | 4343 | ` *  This function is a symisc eXtension.` |
|         - | 4344 | ` */` |
|         4 | 4345 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4346 | `{` |
|         - | 4347 | `	ph7_hashmap *p1,*p2;` |
|         - | 4348 | `	int rc;` |
|         5 | 4349 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4350 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4351 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4352 | `		return PH7_OK;` |
|         - | 4353 | `	}` |
|         - | 4354 | `	/* Point to the hashmaps */` |
|         5 | 4355 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4356 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4357 | `	rc = (p1 == p2);` |
|         - | 4358 | `	/* Same instance? */` |
|         5 | 4359 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4360 | `	return PH7_OK;` |
|         3 | 4361 | `}` |
|         - | 4362 | `/*` |
|         - | 4363 | ` * array array_merge(array ...$arrays)` |
|         - | 4364 | ` *  Merge one or more arrays.` |
|         - | 4365 | ` * Parameters` |
|         - | 4366 | ` *  ...$arrays` |
|         - | 4367 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4368 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4369 | ` * Return` |
|         - | 4370 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4371 | ` *  with no arguments.` |
|         - | 4372 | ` */` |
|      1080 | 4373 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4374 | `{` |
|         - | 4375 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4376 | `	ph7_value *pArray;` |
|         - | 4377 | `	int i;` |
|         - | 4378 | `	/* Create a new array */` |
|      1085 | 4379 | `	pArray = ph7_context_new_array(pCtx);` |
|      1085 | 4380 | `	if( pArray == 0 ){` |
|       ! 0 | 4381 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4382 | `		return PH7_OK;` |
|         - | 4383 | `	}` |
|         - | 4384 | `	/* Point to the internal representation of the hashmap */` |
|      1085 | 4385 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4386 | `	/* Start merging */` |
|      3235 | 4387 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4388 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2159 | 4389 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4390 | `			/* Type mismatch -> TypeError */` |
|         8 | 4391 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4392 | `				"TypeError",` |
|         - | 4393 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4394 | `				i + 1,` |
|         4 | 4395 | `				ph7_type_name(apArg[i])` |
|         - | 4396 | `				);` |
|       ! 0 | 4397 | `		}else{` |
|      2155 | 4398 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4399 | `			/* Merge the two hashmaps */` |
|      2155 | 4400 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4401 | `		}` |
|      1080 | 4402 | `	}` |
|         - | 4403 | `	/* Return the freshly created array */` |
|      1081 | 4404 | `	ph7_result_value(pCtx,pArray);` |
|      1081 | 4405 | `	return PH7_OK;` |
|       545 | 4406 | `}` |
|         - | 4407 | `/*` |
|         - | 4408 | ` * array array_copy(array $source)` |
|         - | 4409 | ` *  Make a blind copy of the target array.` |
|         - | 4410 | ` * Parameters` |
|         - | 4411 | ` *  $source` |
|         - | 4412 | ` *   Target array` |
|         - | 4413 | ` * Return` |
|         - | 4414 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4415 | ` * Note` |
|         - | 4416 | ` *  This function is a symisc eXtension.` |
|         - | 4417 | ` */` |
|        18 | 4418 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4419 | `{` |
|         - | 4420 | `	ph7_hashmap *pMap;` |
|         - | 4421 | `	ph7_value *pArray;` |
|        19 | 4422 | `	if( nArg < 1 ){` |
|         - | 4423 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4424 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4425 | `		return PH7_OK;` |
|         - | 4426 | `	}` |
|         - | 4427 | `	/* Create a new array */` |
|        19 | 4428 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4429 | `	if( pArray == 0 ){` |
|       ! 0 | 4430 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4431 | `		return PH7_OK;` |
|         - | 4432 | `	}` |
|         - | 4433 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4434 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4435 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4436 | `		/* Point to the internal representation of the source */` |
|        19 | 4437 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4438 | `		/* Perform the copy */` |
|        19 | 4439 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4440 | `	}else{` |
|         - | 4441 | `		/* Simple insertion */` |
|       ! 0 | 4442 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4443 | `	}` |
|         - | 4444 | `	/* Return the duplicated array */` |
|        19 | 4445 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4446 | `	return PH7_OK;` |
|        10 | 4447 | `}` |
|         - | 4448 | `/*` |
|         - | 4449 | ` * bool array_erase(array $source)` |
|         - | 4450 | ` *  Remove all elements from a given array.` |
|         - | 4451 | ` * Parameters` |
|         - | 4452 | ` *  $source` |
|         - | 4453 | ` *   Target array` |
|         - | 4454 | ` * Return` |
|         - | 4455 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4456 | ` * Note` |
|         - | 4457 | ` *  This function is a symisc eXtension.` |
|         - | 4458 | ` */` |
|        26 | 4459 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4460 | `{` |
|         - | 4461 | `	ph7_hashmap *pMap;` |
|        28 | 4462 | `	if( nArg < 1 ){` |
|         - | 4463 | `		/* Missing arguments */` |
|       ! 0 | 4464 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4465 | `		return PH7_OK;` |
|         - | 4466 | `	}` |
|         - | 4467 | `	/* Point to the target hashmap */` |
|        28 | 4468 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4469 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4470 | `	/* Erase */` |
|        28 | 4471 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4472 | `	return PH7_OK;` |
|        15 | 4473 | `}` |
|         - | 4474 | `/*` |
|         - | 4475 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4476 | ` *  Extract a slice of the array.` |
|         - | 4477 | ` * Parameters` |
|         - | 4478 | ` *  $array` |
|         - | 4479 | ` *    The input array.` |
|         - | 4480 | ` * $offset` |
|         - | 4481 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4482 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4483 | ` * $length (optional, nullable)` |
|         - | 4484 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4485 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4486 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4487 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4488 | ` * $preserve_keys (optional)` |
|         - | 4489 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4490 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4491 | ` * Return` |
|         - | 4492 | ` *   The new slice.` |
|         - | 4493 | ` */` |
|        66 | 4494 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4495 | `{` |
|         - | 4496 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4497 | `	ph7_hashmap_node *pCur;` |
|         - | 4498 | `	ph7_value *pArray;` |
|         - | 4499 | `	int iLength,iOfft;` |
|         - | 4500 | `	int bPreserve;` |
|         - | 4501 | `	sxi32 rc;` |
|        71 | 4502 | `	if( nArg < 2 ){` |
|       ! 0 | 4503 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4504 | `			"ArgumentCountError",` |
|         - | 4505 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4506 | `			nArg` |
|         - | 4507 | `			);` |
|         - | 4508 | `	}` |
|        71 | 4509 | `	if( nArg > 4 ){` |
|         4 | 4510 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4511 | `			"ArgumentCountError",` |
|         - | 4512 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4513 | `			nArg` |
|         - | 4514 | `			);` |
|         - | 4515 | `	}` |
|        69 | 4516 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4517 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4518 | `			"TypeError",` |
|         - | 4519 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4520 | `			ph7_type_name(apArg[0])` |
|         - | 4521 | `			);` |
|         - | 4522 | `	}` |
|         - | 4523 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        92 | 4524 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        95 | 4525 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4526 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4527 | `			"TypeError",` |
|         - | 4528 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4529 | `			ph7_type_name(apArg[1])` |
|         - | 4530 | `			);` |
|         - | 4531 | `	}` |
|         - | 4532 | `	/* Validate $length type if provided: nullable int */` |
|        65 | 4533 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        56 | 4534 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        56 | 4535 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4536 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4537 | `				"TypeError",` |
|         - | 4538 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4539 | `				ph7_type_name(apArg[2])` |
|         - | 4540 | `				);` |
|         - | 4541 | `		}` |
|        18 | 4542 | `	}` |
|         - | 4543 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        63 | 4544 | `	if( nArg > 3 ){` |
|         7 | 4545 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4546 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4547 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4548 | `				"TypeError",` |
|         - | 4549 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4550 | `				ph7_type_name(apArg[3])` |
|         - | 4551 | `				);` |
|         - | 4552 | `		}` |
|         2 | 4553 | `	}` |
|         - | 4554 | `	/* Point the internal representation of the target array */` |
|        63 | 4555 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        63 | 4556 | `	bPreserve = FALSE;` |
|         - | 4557 | `	/* Get the offset */` |
|         - | 4558 | `	{` |
|        63 | 4559 | `		sxi64 iTmp = 0;` |
|        63 | 4560 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        63 | 4561 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4562 | `			return rcArg;` |
|         - | 4563 | `		}` |
|        63 | 4564 | `		iOfft = (int)iTmp;` |
|         - | 4565 | `	}` |
|        63 | 4566 | `	if( iOfft < 0 ){` |
|         5 | 4567 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4568 | `		if( iOfft < 0 ){` |
|         3 | 4569 | `			iOfft = 0;` |
|         1 | 4570 | `		}` |
|         2 | 4571 | `	}` |
|        63 | 4572 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4573 | `		/* Offset past end of array, return empty array */` |
|         5 | 4574 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4575 | `		if( pArray == 0 ){` |
|       ! 0 | 4576 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4577 | `			return PH7_OK;` |
|         - | 4578 | `		}` |
|         5 | 4579 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4580 | `		return PH7_OK;` |
|         - | 4581 | `	}` |
|         - | 4582 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        59 | 4583 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        59 | 4584 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        37 | 4585 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        37 | 4586 | `		if( iLength < 0 ){` |
|         5 | 4587 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4588 | `		}` |
|        37 | 4589 | `		if( iLength < 0 ){` |
|         3 | 4590 | `			iLength = 0;` |
|         1 | 4591 | `		}` |
|        37 | 4592 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4593 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4594 | `		}` |
|        18 | 4595 | `	}` |
|        59 | 4596 | `	if( nArg > 3 ){` |
|         5 | 4597 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4598 | `	}` |
|         - | 4599 | `	/* Create a new array */` |
|        59 | 4600 | `	pArray = ph7_context_new_array(pCtx);` |
|        59 | 4601 | `	if( pArray == 0 ){` |
|       ! 0 | 4602 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4603 | `		return PH7_OK;` |
|         - | 4604 | `	}` |
|        59 | 4605 | `	if( iLength < 1 ){` |
|         - | 4606 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4607 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4608 | `		return PH7_OK;` |
|         - | 4609 | `	}` |
|         - | 4610 | `	/* Point to the desired entry */` |
|        55 | 4611 | `	pCur = pSrc->pFirst;` |
|        54 | 4612 | `	for(;;){` |
|       113 | 4613 | `		if( iOfft < 1 ){` |
|        55 | 4614 | `			break;` |
|         - | 4615 | `		}` |
|         - | 4616 | `		/* Point to the next entry */` |
|        63 | 4617 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        63 | 4618 | `		iOfft--;` |
|         5 | 4619 | `	}` |
|         - | 4620 | `	/* Point to the internal representation of the hashmap */` |
|        55 | 4621 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       106 | 4622 | `	for(;;){` |
|       217 | 4623 | `		if( iLength < 1 ){` |
|        55 | 4624 | `			break;` |
|         - | 4625 | `		}` |
|         - | 4626 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4627 | `		{` |
|       167 | 4628 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|       167 | 4629 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4630 | `		}` |
|       167 | 4631 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4632 | `			break;` |
|         - | 4633 | `		}` |
|         - | 4634 | `		/* Point to the next entry */` |
|       167 | 4635 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       167 | 4636 | `		iLength--;` |
|         5 | 4637 | `	}` |
|         - | 4638 | `	/* Return the freshly created array */` |
|        55 | 4639 | `	ph7_result_value(pCtx,pArray);` |
|        55 | 4640 | `	return PH7_OK;` |
|        38 | 4641 | `}` |
|         - | 4642 | `/*` |
|         - | 4643 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4644 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4645 | ` * beginning (becomes the new pFirst).` |
|         - | 4646 | ` */` |
|        38 | 4647 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4648 | `{` |
|         - | 4649 | `	ph7_hashmap_node *pNode;` |
|         - | 4650 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4651 | `	pNode = pMap->pLast;` |
|        39 | 4652 | `	if( pNode == 0 ){` |
|       ! 0 | 4653 | `		return;` |
|         - | 4654 | `	}` |
|        39 | 4655 | `	if( pNode->pNext == 0 ){` |
|         - | 4656 | `		/* Only node in the list, nothing to move */` |
|         5 | 4657 | `		return;` |
|         - | 4658 | `	}` |
|        35 | 4659 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4660 | `		/* Already in the correct position */` |
|         9 | 4661 | `		return;` |
|         - | 4662 | `	}` |
|         - | 4663 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4664 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4665 | `	pMap->pLast->pPrev = 0;` |
|         - | 4666 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4667 | `	if( pAfter == 0 ){` |
|         - | 4668 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4669 | `		pNode->pNext = 0;` |
|         3 | 4670 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4671 | `		if( pMap->pFirst ){` |
|         3 | 4672 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4673 | `		}` |
|         3 | 4674 | `		pMap->pFirst = pNode;` |
|         2 | 4675 | `	}else{` |
|        25 | 4676 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4677 | `		pNode->pPrev = pOldNext;` |
|        25 | 4678 | `		pNode->pNext = pAfter;` |
|        25 | 4679 | `		pAfter->pPrev = pNode;` |
|        25 | 4680 | `		if( pOldNext ){` |
|        25 | 4681 | `			pOldNext->pNext = pNode;` |
|        13 | 4682 | `		}else{` |
|       ! 0 | 4683 | `			pMap->pLast = pNode;` |
|         - | 4684 | `		}` |
|         - | 4685 | `	}` |
|        20 | 4686 | `}` |
|         - | 4687 | `/*` |
|         - | 4688 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4689 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4690 | ` * Parameters` |
|         - | 4691 | ` *  $array` |
|         - | 4692 | ` *    The input array.` |
|         - | 4693 | ` *  $offset` |
|         - | 4694 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4695 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4696 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4697 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4698 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4699 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4700 | ` *  $length (optional)` |
|         - | 4701 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4702 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4703 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4704 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4705 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4706 | ` *  $replacement (optional)` |
|         - | 4707 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4708 | ` *    with elements from this array.` |
|         - | 4709 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4710 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4711 | ` *    offset.` |
|         - | 4712 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4713 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4714 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4715 | ` * Return` |
|         - | 4716 | ` *   A new array consisting of the extracted elements.` |
|         - | 4717 | ` */` |
|        64 | 4718 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4719 | `{` |
|         - | 4720 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4721 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4722 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4723 | `	int iLength,iOfft,i;` |
|         - | 4724 | `	sxi32 rc;` |
|        66 | 4725 | `	if( nArg < 2 ){` |
|       ! 0 | 4726 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4727 | `			"ArgumentCountError",` |
|         - | 4728 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4729 | `			nArg` |
|         - | 4730 | `			);` |
|         - | 4731 | `	}` |
|        66 | 4732 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4733 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4734 | `			"TypeError",` |
|         - | 4735 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4736 | `			ph7_type_name(apArg[0])` |
|         - | 4737 | `			);` |
|         - | 4738 | `	}` |
|         - | 4739 | `	/* Point to the internal representation of the target array */` |
|        63 | 4740 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4741 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4742 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4743 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4744 | `	if( iOfft < 0 ){` |
|         9 | 4745 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4746 | `		if( iOfft < 0 ){` |
|         3 | 4747 | `			iOfft = 0;` |
|         2 | 4748 | `		}` |
|        59 | 4749 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4750 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4751 | `	}` |
|         - | 4752 | `	/* Get the length and clamp to valid range.` |
|         - | 4753 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4754 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4755 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4756 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4757 | `		if( iLength < 0 ){` |
|         7 | 4758 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4759 | `			if( iLength < 0 ){` |
|         3 | 4760 | `				iLength = 0;` |
|         1 | 4761 | `			}` |
|         3 | 4762 | `		}` |
|        45 | 4763 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4764 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4765 | `		}` |
|        22 | 4766 | `	}` |
|         - | 4767 | `	/* Create the result array for removed elements */` |
|        63 | 4768 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4769 | `	if( pArray == 0 ){` |
|       ! 0 | 4770 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4771 | `		return PH7_OK;` |
|         - | 4772 | `	}` |
|         - | 4773 | `	/* Get replacement array if provided */` |
|        63 | 4774 | `	pRep = 0;` |
|        63 | 4775 | `	if( nArg > 3 ){` |
|        27 | 4776 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4777 | `			/* Perform an array cast */` |
|         3 | 4778 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4779 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4780 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4781 | `			}` |
|         2 | 4782 | `		}else{` |
|        25 | 4783 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4784 | `		}` |
|        27 | 4785 | `		if( pRep ){` |
|         - | 4786 | `			/* Reset the loop cursor */` |
|        27 | 4787 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4788 | `		}` |
|        13 | 4789 | `	}` |
|         - | 4790 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4791 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4792 | `	/* Navigate to the offset position */` |
|        63 | 4793 | `	pCur = pSrc->pFirst;` |
|       131 | 4794 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4795 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4796 | `	}` |
|         - | 4797 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4798 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4799 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4800 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4801 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4802 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4803 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4804 | `		pPrev = pCur->pPrev;` |
|        79 | 4805 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4806 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4807 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4808 | `			break;` |
|         - | 4809 | `		}` |
|        79 | 4810 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4811 | `	}` |
|         - | 4812 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4813 | `	if( pRep ){` |
|         - | 4814 | `		ph7_value sSafeVal;` |
|        78 | 4815 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4816 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4817 | `			if( pRvalue ){` |
|         - | 4818 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4819 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4820 | `				 * since it points into that same pool. */` |
|        39 | 4821 | `				sSafeVal = *pRvalue;` |
|        39 | 4822 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4823 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4824 | `					pNewNode = pSrc->pLast;` |
|        39 | 4825 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4826 | `					pInsertAfter = pNewNode;` |
|        19 | 4827 | `				}` |
|        19 | 4828 | `			}` |
|         1 | 4829 | `		}` |
|        13 | 4830 | `	}` |
|         - | 4831 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4832 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4833 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4834 | `	 * and removals left gaps. */` |
|         - | 4835 | `	{` |
|        63 | 4836 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4837 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4838 | `		pSrc->iNextIdx = 0;` |
|       233 | 4839 | `		while( n > 0 ){` |
|       171 | 4840 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4841 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4842 | `			}` |
|       171 | 4843 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4844 | `			n--;` |
|         1 | 4845 | `		}` |
|        63 | 4846 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4847 | `	}` |
|         - | 4848 | `	/* Return the freshly created array */` |
|        63 | 4849 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4850 | `	return PH7_OK;` |
|        34 | 4851 | `}` |
|         - | 4852 | `/*` |
|         - | 4853 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4854 | ` *  Checks if a value exists in an array.` |
|         - | 4855 | ` * Parameters` |
|         - | 4856 | ` *  $needle` |
|         - | 4857 | ` *   The searched value.` |
|         - | 4858 | ` *   Note:` |
|         - | 4859 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4860 | ` * $haystack` |
|         - | 4861 | ` *  The target array.` |
|         - | 4862 | ` * $strict` |
|         - | 4863 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4864 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4865 | ` */` |
|     32990 | 4866 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4867 | `{` |
|         - | 4868 | `	ph7_value *pNeedle;` |
|         - | 4869 | `	int bStrict;` |
|         - | 4870 | `	int rc;` |
|     32995 | 4871 | `	if( nArg < 2 ){` |
|         - | 4872 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4873 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4874 | `		return PH7_OK;` |
|         - | 4875 | `	}` |
|     32995 | 4876 | `	pNeedle = apArg[0];` |
|     32995 | 4877 | `	bStrict = 0;` |
|     32995 | 4878 | `	if( nArg > 2 ){` |
|        53 | 4879 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4880 | `	}` |
|     32995 | 4881 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4882 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4883 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4884 | `		/* Set the comparison result */` |
|       ! 0 | 4885 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4886 | `		return PH7_OK;` |
|         - | 4887 | `	}` |
|         - | 4888 | `	/* Perform the lookup */` |
|     32995 | 4889 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4890 | `	/* Lookup result */` |
|     32995 | 4891 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32995 | 4892 | `	return PH7_OK;` |
|     16500 | 4893 | `}` |
|         - | 4894 | `/*` |
|         - | 4895 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4896 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4897 | ` * Parameters` |
|         - | 4898 | ` * $needle` |
|         - | 4899 | ` *   The searched value.` |
|         - | 4900 | ` * $haystack` |
|         - | 4901 | ` *   The array.` |
|         - | 4902 | ` * $strict` |
|         - | 4903 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4904 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4905 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4906 | ` * Return` |
|         - | 4907 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4908 | ` */` |
|        26 | 4909 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4910 | `{` |
|         - | 4911 | `	ph7_hashmap_node *pEntry;` |
|         - | 4912 | `	ph7_value *pVal,sNeedle;` |
|         - | 4913 | `	ph7_hashmap *pMap;` |
|         - | 4914 | `	ph7_value sVal;` |
|         - | 4915 | `	int bStrict;` |
|         - | 4916 | `	sxu32 n;` |
|         - | 4917 | `	int rc;` |
|        28 | 4918 | `	if( nArg < 2 ){` |
|         - | 4919 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4920 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4921 | `			"ArgumentCountError",` |
|         - | 4922 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 4923 | `			nArg` |
|         - | 4924 | `			);` |
|         - | 4925 | `	}` |
|        28 | 4926 | `	bStrict = FALSE;` |
|        28 | 4927 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4928 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4929 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4930 | `			"TypeError",` |
|         - | 4931 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4932 | `			ph7_type_name(apArg[1])` |
|         - | 4933 | `			);` |
|         - | 4934 | `	}` |
|        25 | 4935 | `	if( nArg > 2 ){` |
|         - | 4936 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        11 | 4937 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4938 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4939 | `				"TypeError",` |
|         - | 4940 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4941 | `				ph7_type_name(apArg[2])` |
|         - | 4942 | `				);` |
|         - | 4943 | `		}` |
|        11 | 4944 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4945 | `	}` |
|         - | 4946 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4947 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4948 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4949 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4950 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4951 | `	pEntry = pMap->pFirst;` |
|        25 | 4952 | `	n = pMap->nEntry;` |
|        28 | 4953 | `	for(;;){` |
|        57 | 4954 | `		if( !n ){` |
|         9 | 4955 | `			break;` |
|         - | 4956 | `		}` |
|         - | 4957 | `		/* Extract node value */` |
|        49 | 4958 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4959 | `		if( pVal ){` |
|         - | 4960 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4961 | `			 * can change their type.` |
|         - | 4962 | `			 */` |
|        49 | 4963 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4964 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4965 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4966 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4967 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4968 | `			if( rc == 0 ){` |
|         - | 4969 | `				/* Match found,return key */` |
|        17 | 4970 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4971 | `					/* INT key */` |
|        11 | 4972 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4973 | `				}else{` |
|         7 | 4974 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4975 | `					/* Blob key */` |
|         7 | 4976 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4977 | `				}` |
|        17 | 4978 | `				return PH7_OK;` |
|         - | 4979 | `			}` |
|        16 | 4980 | `		}` |
|         - | 4981 | `		/* Point to the next entry */` |
|        33 | 4982 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4983 | `		n--;` |
|         1 | 4984 | `	}` |
|         - | 4985 | `	/* No such value,return FALSE */` |
|         9 | 4986 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4987 | `	return PH7_OK;` |
|        15 | 4988 | `}` |
|         - | 4989 | `/*` |
|         - | 4990 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4991 | ` *  Computes the difference of arrays.` |
|         - | 4992 | ` * Parameters` |
|         - | 4993 | ` *  $array1` |
|         - | 4994 | ` *    The array to compare from` |
|         - | 4995 | ` *  $array2` |
|         - | 4996 | ` *    An array to compare against` |
|         - | 4997 | ` *  $...` |
|         - | 4998 | ` *   More arrays to compare against` |
|         - | 4999 | ` * Return` |
|         - | 5000 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5001 | ` *  are not present in any of the other arrays.` |
|         - | 5002 | ` */` |
|        20 | 5003 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5004 | `{` |
|         - | 5005 | `	ph7_hashmap_node *pEntry;` |
|         - | 5006 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5007 | `	ph7_value *pArray;` |
|         - | 5008 | `	ph7_value *pVal;` |
|         - | 5009 | `	sxi32 rc;` |
|         - | 5010 | `	sxu32 n;` |
|         - | 5011 | `	int i;` |
|         - | 5012 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5013 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5014 | `	 * debugging difficult. */` |
|        23 | 5015 | `	if( nArg < 1 ){` |
|       ! 0 | 5016 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5017 | `			"ArgumentCountError",` |
|         - | 5018 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5019 | `			nArg` |
|         - | 5020 | `			);` |
|         - | 5021 | `	}` |
|        23 | 5022 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5023 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5024 | `			"TypeError",` |
|         - | 5025 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5026 | `			ph7_type_name(apArg[0])` |
|         - | 5027 | `			);` |
|         - | 5028 | `	}` |
|        36 | 5029 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5030 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5031 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5032 | `				"TypeError",` |
|         - | 5033 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5034 | `				i + 1,` |
|         2 | 5035 | `				ph7_type_name(apArg[i])` |
|         - | 5036 | `				);` |
|         - | 5037 | `		}` |
|         9 | 5038 | `	}` |
|        17 | 5039 | `	if( nArg == 1 ){` |
|         - | 5040 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5041 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5042 | `		return PH7_OK;` |
|         - | 5043 | `	}` |
|         - | 5044 | `	/* Create a new array */` |
|        15 | 5045 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5046 | `	if( pArray == 0 ){` |
|       ! 0 | 5047 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5048 | `		return PH7_OK;` |
|         - | 5049 | `	}` |
|         - | 5050 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5051 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5052 | `	/* Perform the diff */` |
|        15 | 5053 | `	pEntry = pSrc->pFirst;` |
|        15 | 5054 | `	n = pSrc->nEntry;` |
|        27 | 5055 | `	for(;;){` |
|        55 | 5056 | `		if( n < 1 ){` |
|        15 | 5057 | `			break;` |
|         - | 5058 | `		}` |
|         - | 5059 | `		/* Extract the node value */` |
|        41 | 5060 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5061 | `		if( pVal ){` |
|        69 | 5062 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5063 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5064 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5065 | `				/* Perform the lookup */` |
|        45 | 5066 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5067 | `				if( rc == SXRET_OK ){` |
|         - | 5068 | `					/* Value exist */` |
|        17 | 5069 | `					break;` |
|         - | 5070 | `				}` |
|        15 | 5071 | `			}` |
|        41 | 5072 | `			if( i >= nArg ){` |
|         - | 5073 | `				/* Perform the insertion */` |
|        25 | 5074 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5075 | `			}` |
|        20 | 5076 | `		}` |
|         - | 5077 | `		/* Point to the next entry */` |
|        41 | 5078 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5079 | `		n--;` |
|         1 | 5080 | `	}` |
|         - | 5081 | `	/* Return the freshly created array */` |
|        15 | 5082 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5083 | `	return PH7_OK;` |
|        13 | 5084 | `}` |
|         - | 5085 | `/*` |
|         - | 5086 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5087 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5088 | ` * Parameters` |
|         - | 5089 | ` *  $array1` |
|         - | 5090 | ` *    The array to compare from` |
|         - | 5091 | ` *  $array2` |
|         - | 5092 | ` *    An array to compare against` |
|         - | 5093 | ` *  $...` |
|         - | 5094 | ` *   More arrays to compare against.` |
|         - | 5095 | ` * $callback` |
|         - | 5096 | ` *  The callback comparison function.` |
|         - | 5097 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5098 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5099 | ` *  than the second.` |
|         - | 5100 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5101 | ` * Return` |
|         - | 5102 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5103 | ` *  are not present in any of the other arrays.` |
|         - | 5104 | ` */` |
|        20 | 5105 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5106 | `{` |
|         - | 5107 | `	ph7_hashmap_node *pEntry;` |
|         - | 5108 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5109 | `	ph7_value *pCallback;` |
|         - | 5110 | `	ph7_value *pArray;` |
|         - | 5111 | `	ph7_value *pVal;` |
|         - | 5112 | `	sxi32 rc;` |
|         - | 5113 | `	sxu32 n;` |
|         - | 5114 | `	int i;` |
|         - | 5115 |  |
|         - | 5116 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5117 | `	if( nArg < 2 ){` |
|       ! 0 | 5118 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5119 | `			"ArgumentCountError",` |
|         - | 5120 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5121 | `			nArg` |
|         - | 5122 | `			);` |
|         - | 5123 | `	}` |
|        25 | 5124 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5125 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5126 | `			"TypeError",` |
|         - | 5127 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5128 | `			ph7_type_name(apArg[0])` |
|         - | 5129 | `			);` |
|         - | 5130 | `	}` |
|         - | 5131 |  |
|        23 | 5132 | `	if( nArg == 2 ){` |
|         - | 5133 | `		/* Only the original array and the callback were provided. */` |
|         - | 5134 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5135 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5136 | `		 * validation order.` |
|         - | 5137 | `		 */` |
|         4 | 5138 | `	} else {` |
|         - | 5139 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5140 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5141 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5142 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5143 | `					"TypeError",` |
|         - | 5144 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5145 | `					i + 1,` |
|         6 | 5146 | `					ph7_type_name(apArg[i])` |
|         - | 5147 | `					);` |
|         - | 5148 | `			}` |
|         7 | 5149 | `		}` |
|         - | 5150 | `	}` |
|         - | 5151 |  |
|         - | 5152 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5153 | `	pCallback = apArg[nArg - 1];` |
|         - | 5154 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5155 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5156 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5157 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5158 | `				"TypeError",` |
|         - | 5159 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5160 | `				nArg` |
|         - | 5161 | `				);` |
|         - | 5162 | `		}` |
|         6 | 5163 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5164 | `			int len;` |
|         3 | 5165 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5166 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5167 | `				"TypeError",` |
|         - | 5168 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5169 | `				nArg,` |
|         1 | 5170 | `				zName` |
|         - | 5171 | `				);` |
|         - | 5172 | `		}` |
|         4 | 5173 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5174 | `			"TypeError",` |
|         - | 5175 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5176 | `			nArg` |
|         - | 5177 | `			);` |
|         - | 5178 | `	}` |
|         - | 5179 |  |
|         7 | 5180 | `	if( nArg == 2 ){` |
|         - | 5181 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5182 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5183 | `		return PH7_OK;` |
|         - | 5184 | `	}` |
|         - | 5185 |  |
|         - | 5186 | `	/* Create a new array */` |
|         5 | 5187 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5188 | `	if( pArray == 0 ){` |
|       ! 0 | 5189 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5190 | `		return PH7_OK;` |
|         - | 5191 | `	}` |
|         - | 5192 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5193 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5194 | `	/* Perform the diff */` |
|         5 | 5195 | `	pEntry = pSrc->pFirst;` |
|         5 | 5196 | `	n = pSrc->nEntry;` |
|         5 | 5197 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5198 | `	for(;;){` |
|        11 | 5199 | `		if( n < 1 ){` |
|         3 | 5200 | `			break;` |
|         - | 5201 | `		}` |
|         - | 5202 | `		/* Extract the node value */` |
|         9 | 5203 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5204 | `		if( pVal ){` |
|        15 | 5205 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5206 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5207 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5208 | `				/* Perform the lookup */` |
|         9 | 5209 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5210 | `				if( rc == SXRET_OK ){` |
|         - | 5211 | `					/* Value exist */` |
|         3 | 5212 | `					break;` |
|         - | 5213 | `				}` |
|         4 | 5214 | `			}` |
|         9 | 5215 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5216 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5217 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5218 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5219 | `				return PH7_EXCEPTION;` |
|         - | 5220 | `			}` |
|         7 | 5221 | `			if( i >= (nArg - 1)){` |
|         - | 5222 | `				/* Perform the insertion */` |
|         5 | 5223 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5224 | `			}` |
|         3 | 5225 | `		}` |
|         - | 5226 | `		/* Point to the next entry */` |
|         7 | 5227 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5228 | `		n--;` |
|         1 | 5229 | `	}` |
|         - | 5230 | `	/* Return the freshly created array */` |
|         3 | 5231 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5232 | `	return PH7_OK;` |
|        15 | 5233 | `}` |
|         - | 5234 | `/*` |
|         - | 5235 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5236 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5237 | ` * Parameters` |
|         - | 5238 | ` *  $array1` |
|         - | 5239 | ` *    The array to compare from` |
|         - | 5240 | ` *  $array2` |
|         - | 5241 | ` *    An array to compare against` |
|         - | 5242 | ` *  $...` |
|         - | 5243 | ` *   More arrays to compare against` |
|         - | 5244 | ` * Return` |
|         - | 5245 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5246 | ` *  are not present in any of the other arrays.` |
|         - | 5247 | ` */` |
|        20 | 5248 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5249 | `{` |
|         - | 5250 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5251 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5252 | `	ph7_value *pArray;` |
|         - | 5253 | `	ph7_value *pVal;` |
|         - | 5254 | `	sxi32 rc;` |
|         - | 5255 | `	sxu32 n;` |
|         - | 5256 | `	int i;` |
|         - | 5257 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5258 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5259 | `	 * accompanying integration tests to pass. */` |
|        24 | 5260 | `	if( nArg < 1 ){` |
|       ! 0 | 5261 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5262 | `			"ArgumentCountError",` |
|         - | 5263 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5264 | `			nArg` |
|         - | 5265 | `			);` |
|         - | 5266 | `	}` |
|        24 | 5267 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5268 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5269 | `			"TypeError",` |
|         - | 5270 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5271 | `			ph7_type_name(apArg[0])` |
|         - | 5272 | `			);` |
|         - | 5273 | `	}` |
|        37 | 5274 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5275 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5276 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5277 | `				"TypeError",` |
|         - | 5278 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5279 | `				i + 1,` |
|         4 | 5280 | `				ph7_type_name(apArg[i])` |
|         - | 5281 | `				);` |
|         - | 5282 | `		}` |
|        10 | 5283 | `	}` |
|        15 | 5284 | `	if( nArg == 1 ){` |
|         - | 5285 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5286 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5287 | `		return PH7_OK;` |
|         - | 5288 | `	}` |
|         - | 5289 | `	/* Create a new array */` |
|        13 | 5290 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5291 | `	if( pArray == 0 ){` |
|       ! 0 | 5292 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5293 | `		return PH7_OK;` |
|         - | 5294 | `	}` |
|         - | 5295 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5296 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5297 | `	/* Perform the diff */` |
|        13 | 5298 | `	pEntry = pSrc->pFirst;` |
|        13 | 5299 | `	n = pSrc->nEntry;` |
|        13 | 5300 | `	pN1 = pN2 = 0;` |
|        34 | 5301 | `	for(;;){` |
|         - | 5302 | `		int keep;` |
|        41 | 5303 | `		if( n < 1 ){` |
|        13 | 5304 | `			break;` |
|         - | 5305 | `		}` |
|         - | 5306 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5307 | `		keep = 1;` |
|        47 | 5308 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5309 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5310 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5311 | `			/* Perform a key lookup first */` |
|        33 | 5312 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5313 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5314 | `			}else{` |
|        21 | 5315 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5316 | `			}` |
|        33 | 5317 | `			if( rc != SXRET_OK ){` |
|         - | 5318 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5319 | `				continue;` |
|         - | 5320 | `			}` |
|         - | 5321 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5322 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5323 | `			if( pVal ){` |
|         - | 5324 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5325 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5326 | `				if( pVal2 ){` |
|         - | 5327 | `					ph7_value sV1,sV2;` |
|         - | 5328 | `					sxi32 cmp;` |
|         - | 5329 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5330 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5331 | `					 * null element used to come back bool(false) in the` |
|         - | 5332 | `					 * caller's array). */` |
|        17 | 5333 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5334 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5335 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5336 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5337 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5338 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5339 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5340 | `					if( cmp == 0 ){` |
|         - | 5341 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5342 | `						keep = 0;` |
|        15 | 5343 | `						break;` |
|         - | 5344 | `					}` |
|         1 | 5345 | `				}` |
|         1 | 5346 | `			}` |
|         2 | 5347 | `		}` |
|        29 | 5348 | `		if( keep ){` |
|         - | 5349 | `			/* Perform the insertion */` |
|        15 | 5350 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5351 | `		}` |
|         - | 5352 | `		/* Point to the next entry */` |
|        29 | 5353 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5354 | `		n--;` |
|         1 | 5355 | `	}` |
|         - | 5356 | `	/* Return the freshly created array */` |
|        13 | 5357 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5358 | `	return PH7_OK;` |
|        14 | 5359 | `}` |
|         - | 5360 | `/*` |
|         - | 5361 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5362 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5363 | ` *  by a user supplied callback function.` |
|         - | 5364 | ` * Parameters` |
|         - | 5365 | ` *  $array1` |
|         - | 5366 | ` *    The array to compare from` |
|         - | 5367 | ` *  $array2` |
|         - | 5368 | ` *    An array to compare against` |
|         - | 5369 | ` *  $...` |
|         - | 5370 | ` *   More arrays to compare against.` |
|         - | 5371 | ` *  $key_compare_func` |
|         - | 5372 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5373 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5374 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5375 | ` * Return` |
|         - | 5376 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5377 | ` *  are not present in any of the other arrays.` |
|         - | 5378 | ` */` |
|        22 | 5379 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5380 | `{` |
|         - | 5381 | `	ph7_hashmap_node *pEntry;` |
|         - | 5382 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5383 | `	ph7_value *pCallback;` |
|         - | 5384 | `	ph7_value *pArray;` |
|         - | 5385 | `	sxi32 rc;` |
|         - | 5386 | `	sxu32 n;` |
|         - | 5387 | `	int i;` |
|         - | 5388 |  |
|         - | 5389 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5390 | `	if( nArg < 2 ){` |
|       ! 0 | 5391 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5392 | `			"ArgumentCountError",` |
|         - | 5393 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5394 | `			nArg` |
|         - | 5395 | `			);` |
|         - | 5396 | `	}` |
|        26 | 5397 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5398 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5399 | `			"TypeError",` |
|         - | 5400 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5401 | `			ph7_type_name(apArg[0])` |
|         - | 5402 | `			);` |
|         - | 5403 | `	}` |
|         - | 5404 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5405 | `	 * expected to be a callback. */` |
|        38 | 5406 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5407 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5408 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5409 | `				"TypeError",` |
|         - | 5410 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5411 | `				i + 1,` |
|         2 | 5412 | `				ph7_type_name(apArg[i])` |
|         - | 5413 | `				);` |
|         - | 5414 | `		}` |
|         9 | 5415 | `	}` |
|         - | 5416 | `	/* Point to the callback value */` |
|        22 | 5417 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5418 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5419 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5420 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5421 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5422 | `		 * string given" which we also reproduce. */` |
|         9 | 5423 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5424 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5425 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5426 | `				"TypeError",` |
|         - | 5427 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5428 | `				nArg` |
|         - | 5429 | `				);` |
|         - | 5430 | `		}` |
|         6 | 5431 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5432 | `			/* neither array nor string */` |
|         8 | 5433 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5434 | `				"TypeError",` |
|         - | 5435 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5436 | `				nArg` |
|         - | 5437 | `				);` |
|         - | 5438 | `		}` |
|         - | 5439 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5440 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5441 | `			"TypeError",` |
|         - | 5442 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5443 | `			nArg,` |
|       ! 0 | 5444 | `			ph7_type_name(pCallback)` |
|         - | 5445 | `			);` |
|         - | 5446 | `	}` |
|        13 | 5447 | `	if( nArg == 2 ){` |
|         - | 5448 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5449 | `		 * input array. */` |
|         3 | 5450 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5451 | `		return PH7_OK;` |
|         - | 5452 | `	}` |
|         - | 5453 | `	/* Create a new array */` |
|        11 | 5454 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5455 | `	if( pArray == 0 ){` |
|       ! 0 | 5456 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5457 | `		return PH7_OK;` |
|         - | 5458 | `	}` |
|         - | 5459 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5460 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5461 | `	/* Perform the diff */` |
|        11 | 5462 | `	pEntry = pSrc->pFirst;` |
|        11 | 5463 | `	n = pSrc->nEntry;` |
|        21 | 5464 | `	for(;;){` |
|         - | 5465 | `		int keep;` |
|        27 | 5466 | `		if( n < 1 ){` |
|         9 | 5467 | `			break;` |
|         - | 5468 | `		}` |
|        19 | 5469 | `		keep = 1;` |
|        31 | 5470 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5471 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5472 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5473 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5474 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5475 | `			while( pIt ){` |
|         - | 5476 | `				/* build temporary key values for callback */` |
|         - | 5477 | `				ph7_value key1, key2, result;` |
|         - | 5478 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5479 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5480 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5481 | `				}else{` |
|         - | 5482 | `					SyString sStr;` |
|        33 | 5483 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5484 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5485 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5486 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5487 | `				}` |
|        33 | 5488 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5489 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5490 | `				}else{` |
|         - | 5491 | `					SyString sStr;` |
|        33 | 5492 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5493 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5494 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5495 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5496 | `				}` |
|        33 | 5497 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5498 | `				/* call user callback with (key1, key2) */` |
|         - | 5499 | `				{` |
|         - | 5500 | `					ph7_value *apK[2];` |
|        33 | 5501 | `					apK[0] = &key1;` |
|        33 | 5502 | `					apK[1] = &key2;` |
|        33 | 5503 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5504 | `				}` |
|        33 | 5505 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5506 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5507 | `					 * array_uintersect (which signal back from` |
|         - | 5508 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5509 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5510 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5511 | `					PH7_MemObjRelease(&result);` |
|         3 | 5512 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5513 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5514 | `					return PH7_EXCEPTION;` |
|         - | 5515 | `				}` |
|        31 | 5516 | `				if( rc == SXRET_OK ){` |
|        31 | 5517 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5518 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5519 | `					}` |
|        31 | 5520 | `					if( result.x.iVal == 0 ){` |
|         - | 5521 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5522 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5523 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5524 | `						if( pVal1 && pVal2 ){` |
|         - | 5525 | `							ph7_value sV1,sV2;` |
|         - | 5526 | `							sxi32 cmp;` |
|         - | 5527 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5528 | `							 * place and these are LIVE array elements. */` |
|        13 | 5529 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5530 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5531 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5532 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5533 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5534 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5535 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5536 | `							if( cmp == 0 ){` |
|         9 | 5537 | `								keep = 0;` |
|         9 | 5538 | `								PH7_MemObjRelease(&result);` |
|         - | 5539 | `								/* release keys too before breaking */` |
|         9 | 5540 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5541 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5542 | `								break;` |
|         - | 5543 | `							}` |
|         2 | 5544 | `						}` |
|         2 | 5545 | `					}` |
|        11 | 5546 | `				}` |
|        23 | 5547 | `				PH7_MemObjRelease(&result);` |
|        23 | 5548 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5549 | `				PH7_MemObjRelease(&key2);` |
|         - | 5550 | `				/* move to next node */` |
|        23 | 5551 | `				pIt = pIt->pPrev;` |
|        23 | 5552 | `				if( keep == 0 ) break;` |
|         1 | 5553 | `			}` |
|        21 | 5554 | `			if( keep == 0 ) break;` |
|         7 | 5555 | `		}` |
|        17 | 5556 | `		if( keep ){` |
|         - | 5557 | `			/* Perform the insertion */` |
|         9 | 5558 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5559 | `		}` |
|         - | 5560 | `		/* Point to the next entry */` |
|        17 | 5561 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5562 | `		n--;` |
|         1 | 5563 | `	}` |
|         - | 5564 | `	/* Return the freshly created array */` |
|         9 | 5565 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5566 | `	return PH7_OK;` |
|        15 | 5567 | `}` |
|         - | 5568 | `/*` |
|         - | 5569 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5570 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5571 | ` * Parameters` |
|         - | 5572 | ` *  $array1` |
|         - | 5573 | ` *    The array to compare from` |
|         - | 5574 | ` *  $array2` |
|         - | 5575 | ` *    An array to compare against` |
|         - | 5576 | ` *  $...` |
|         - | 5577 | ` *   More arrays to compare against` |
|         - | 5578 | ` * Return` |
|         - | 5579 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5580 | ` *  in any of the other arrays.` |
|         - | 5581 | ` * Note that NULL is returned on failure.` |
|         - | 5582 | ` */` |
|        12 | 5583 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5584 | `{` |
|         - | 5585 | `	ph7_hashmap_node *pEntry;` |
|         - | 5586 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5587 | `	ph7_value *pArray;` |
|         - | 5588 | `	sxi32 rc;` |
|         - | 5589 | `	sxu32 n;` |
|         - | 5590 | `	int i;` |
|         - | 5591 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5592 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5593 | `	 * helpers. */` |
|        15 | 5594 | `	if( nArg < 1 ){` |
|       ! 0 | 5595 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5596 | `			"ArgumentCountError",` |
|         - | 5597 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5598 | `			nArg` |
|         - | 5599 | `			);` |
|         - | 5600 | `	}` |
|        15 | 5601 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5602 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5603 | `			"TypeError",` |
|         - | 5604 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5605 | `			ph7_type_name(apArg[0])` |
|         - | 5606 | `			);` |
|         - | 5607 | `	}` |
|        20 | 5608 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5609 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5610 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5611 | `				"TypeError",` |
|         - | 5612 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5613 | `				i + 1,` |
|         2 | 5614 | `				ph7_type_name(apArg[i])` |
|         - | 5615 | `				);` |
|         - | 5616 | `		}` |
|         5 | 5617 | `	}` |
|         9 | 5618 | `	if( nArg == 1 ){` |
|         - | 5619 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5620 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5621 | `		return PH7_OK;` |
|         - | 5622 | `	}` |
|         - | 5623 | `	/* Create a new array */` |
|         7 | 5624 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5625 | `	if( pArray == 0 ){` |
|       ! 0 | 5626 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5627 | `		return PH7_OK;` |
|         - | 5628 | `	}` |
|         - | 5629 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5630 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5631 | `	/* Perfrom the diff */` |
|         7 | 5632 | `	pEntry = pSrc->pFirst;` |
|         7 | 5633 | `	n = pSrc->nEntry;` |
|        12 | 5634 | `	for(;;){` |
|        25 | 5635 | `		if( n < 1 ){` |
|         7 | 5636 | `			break;` |
|         - | 5637 | `		}` |
|        31 | 5638 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5639 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5640 | `				/* ignore */` |
|       ! 0 | 5641 | `				continue;` |
|         - | 5642 | `			}` |
|        23 | 5643 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5644 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5645 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5646 | `				/* Blob lookup */` |
|        17 | 5647 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5648 | `			}else{` |
|         - | 5649 | `				/* Int lookup */` |
|         7 | 5650 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5651 | `			}` |
|        23 | 5652 | `			if( rc == SXRET_OK ){` |
|         - | 5653 | `				/* Key exists,break immediately */` |
|        11 | 5654 | `				break;` |
|         - | 5655 | `			}` |
|         7 | 5656 | `		}` |
|        19 | 5657 | `		if( i >= nArg ){` |
|         - | 5658 | `			/* Perform the insertion */` |
|         9 | 5659 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5660 | `		}` |
|         - | 5661 | `		/* Point to the next entry */` |
|        19 | 5662 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5663 | `		n--;` |
|         1 | 5664 | `	}` |
|         - | 5665 | `	/* Return the freshly created array */` |
|         7 | 5666 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5667 | `	return PH7_OK;` |
|         9 | 5668 | `}` |
|         - | 5669 | `/*` |
|         - | 5670 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5671 | ` *  Computes the intersection of arrays.` |
|         - | 5672 | ` * Parameters` |
|         - | 5673 | ` *  $array1` |
|         - | 5674 | ` *    The array to compare from` |
|         - | 5675 | ` *  $array2` |
|         - | 5676 | ` *    An array to compare against` |
|         - | 5677 | ` *  $...` |
|         - | 5678 | ` *   More arrays to compare against` |
|         - | 5679 | ` * Return` |
|         - | 5680 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5681 | ` *  in all of the parameters.` |
|         - | 5682 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5683 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5684 | ` */` |
|        20 | 5685 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5686 | `{` |
|         - | 5687 | `	ph7_hashmap_node *pEntry;` |
|         - | 5688 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5689 | `	ph7_value *pArray;` |
|         - | 5690 | `	ph7_value *pVal;` |
|         - | 5691 | `	sxi32 rc;` |
|         - | 5692 | `	sxu32 n;` |
|         - | 5693 | `	int i;` |
|        23 | 5694 | `	if( nArg < 1 ){` |
|       ! 0 | 5695 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5696 | `			"ArgumentCountError",` |
|         - | 5697 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5698 | `			nArg` |
|         - | 5699 | `			);` |
|         - | 5700 | `	}` |
|        23 | 5701 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5702 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5703 | `			"TypeError",` |
|         - | 5704 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5705 | `			ph7_type_name(apArg[0])` |
|         - | 5706 | `			);` |
|         - | 5707 | `	}` |
|        36 | 5708 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5709 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5710 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5711 | `				"TypeError",` |
|         - | 5712 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5713 | `				i + 1,` |
|         2 | 5714 | `				ph7_type_name(apArg[i])` |
|         - | 5715 | `				);` |
|         - | 5716 | `		}` |
|         9 | 5717 | `	}` |
|        17 | 5718 | `	if( nArg == 1 ){` |
|         - | 5719 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5720 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5721 | `		return PH7_OK;` |
|         - | 5722 | `	}` |
|         - | 5723 | `	/* Create a new array */` |
|        15 | 5724 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5725 | `	if( pArray == 0 ){` |
|       ! 0 | 5726 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5727 | `		return PH7_OK;` |
|         - | 5728 | `	}` |
|         - | 5729 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5730 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5731 | `	/* Perform the intersection */` |
|        15 | 5732 | `	pEntry = pSrc->pFirst;` |
|        15 | 5733 | `	n = pSrc->nEntry;` |
|        31 | 5734 | `	for(;;){` |
|        63 | 5735 | `		if( n < 1 ){` |
|        15 | 5736 | `			break;` |
|         - | 5737 | `		}` |
|         - | 5738 | `		/* Extract the node value */` |
|        49 | 5739 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5740 | `		if( pVal ){` |
|        79 | 5741 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5742 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5743 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5744 | `				/* Perform the lookup */` |
|        55 | 5745 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5746 | `				if( rc != SXRET_OK ){` |
|         - | 5747 | `					/* Value does not exist */` |
|        25 | 5748 | `					break;` |
|         - | 5749 | `				}` |
|        16 | 5750 | `			}` |
|        49 | 5751 | `			if( i >= nArg ){` |
|         - | 5752 | `				/* Perform the insertion */` |
|        25 | 5753 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5754 | `			}` |
|        24 | 5755 | `		}` |
|         - | 5756 | `		/* Point to the next entry */` |
|        49 | 5757 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5758 | `		n--;` |
|         1 | 5759 | `	}` |
|         - | 5760 | `	/* Return the freshly created array */` |
|        15 | 5761 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5762 | `	return PH7_OK;` |
|        13 | 5763 | `}` |
|         - | 5764 | `/*` |
|         - | 5765 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5766 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5767 | ` * Parameters` |
|         - | 5768 | ` *  $array1` |
|         - | 5769 | ` *    The array to compare from` |
|         - | 5770 | ` *  $array2` |
|         - | 5771 | ` *    An array to compare against` |
|         - | 5772 | ` *  $...` |
|         - | 5773 | ` *   More arrays to compare against` |
|         - | 5774 | ` * Return` |
|         - | 5775 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5776 | ` *  in all the arguments, with matching keys.` |
|         - | 5777 | ` */` |
|        20 | 5778 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5779 | `{` |
|         - | 5780 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5781 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5782 | `	ph7_value *pArray;` |
|         - | 5783 | `	ph7_value *pVal;` |
|         - | 5784 | `	sxi32 rc;` |
|         - | 5785 | `	sxu32 n;` |
|         - | 5786 | `	int i;` |
|        23 | 5787 | `	if( nArg < 1 ){` |
|       ! 0 | 5788 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5789 | `			"ArgumentCountError",` |
|         - | 5790 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5791 | `			nArg` |
|         - | 5792 | `			);` |
|         - | 5793 | `	}` |
|        23 | 5794 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5795 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5796 | `			"TypeError",` |
|         - | 5797 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5798 | `			ph7_type_name(apArg[0])` |
|         - | 5799 | `			);` |
|         - | 5800 | `	}` |
|        36 | 5801 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5802 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5803 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5804 | `				"TypeError",` |
|         - | 5805 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5806 | `				i + 1,` |
|         2 | 5807 | `				ph7_type_name(apArg[i])` |
|         - | 5808 | `				);` |
|         - | 5809 | `		}` |
|         9 | 5810 | `	}` |
|        17 | 5811 | `	if( nArg == 1 ){` |
|         - | 5812 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5813 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5814 | `		return PH7_OK;` |
|         - | 5815 | `	}` |
|         - | 5816 | `	/* Create a new array */` |
|        15 | 5817 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5818 | `	if( pArray == 0 ){` |
|       ! 0 | 5819 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5820 | `		return PH7_OK;` |
|         - | 5821 | `	}` |
|         - | 5822 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5823 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5824 | `	/* Perform the intersection */` |
|        15 | 5825 | `	pEntry = pSrc->pFirst;` |
|        15 | 5826 | `	n = pSrc->nEntry;` |
|        15 | 5827 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5828 | `	for(;;){` |
|        47 | 5829 | `		if( n < 1 ){` |
|        15 | 5830 | `			break;` |
|         - | 5831 | `		}` |
|         - | 5832 | `		/* Extract the node value */` |
|        33 | 5833 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5834 | `		if( pVal ){` |
|        53 | 5835 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5836 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5837 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5838 | `				/* Perform a key lookup first */` |
|        37 | 5839 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5840 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5841 | `				}else{` |
|        23 | 5842 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5843 | `				}` |
|        37 | 5844 | `				if( rc != SXRET_OK ){` |
|         - | 5845 | `					/* No such key,break immediately */` |
|         7 | 5846 | `					break;` |
|         - | 5847 | `				}` |
|         - | 5848 | `				/* Perform the lookup */` |
|        31 | 5849 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5850 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5851 | `					/* Value does not exist */` |
|         6 | 5852 | `					break;` |
|         - | 5853 | `				}` |
|        11 | 5854 | `			}` |
|        33 | 5855 | `			if( i >= nArg ){` |
|         - | 5856 | `				/* Perform the insertion */` |
|        17 | 5857 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5858 | `			}` |
|        16 | 5859 | `		}` |
|         - | 5860 | `		/* Point to the next entry */` |
|        33 | 5861 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5862 | `		n--;` |
|         1 | 5863 | `	}` |
|         - | 5864 | `	/* Return the freshly created array */` |
|        15 | 5865 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5866 | `	return PH7_OK;` |
|        13 | 5867 | `}` |
|         - | 5868 | `/*` |
|         - | 5869 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5870 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5871 | ` * Parameters` |
|         - | 5872 | ` *  $array1` |
|         - | 5873 | ` *    The array to compare from` |
|         - | 5874 | ` *  $...` |
|         - | 5875 | ` *   More arrays to compare against` |
|         - | 5876 | ` * Return` |
|         - | 5877 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5878 | ` *  have keys that are present in all arguments.` |
|         - | 5879 | ` * Note that NULL is returned on failure.` |
|         - | 5880 | ` */` |
|        20 | 5881 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5882 | `{` |
|         - | 5883 | `	ph7_hashmap_node *pEntry;` |
|         - | 5884 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5885 | `	ph7_value *pArray;` |
|         - | 5886 | `	sxi32 rc;` |
|         - | 5887 | `	sxu32 n;` |
|         - | 5888 | `	int i;` |
|        23 | 5889 | `	if( nArg < 1 ){` |
|       ! 0 | 5890 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5891 | `			"ArgumentCountError",` |
|         - | 5892 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5893 | `			nArg` |
|         - | 5894 | `			);` |
|         - | 5895 | `	}` |
|        23 | 5896 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5897 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5898 | `			"TypeError",` |
|         - | 5899 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5900 | `			ph7_type_name(apArg[0])` |
|         - | 5901 | `			);` |
|         - | 5902 | `	}` |
|        36 | 5903 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5904 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5905 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5906 | `				"TypeError",` |
|         - | 5907 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5908 | `				i + 1,` |
|         2 | 5909 | `				ph7_type_name(apArg[i])` |
|         - | 5910 | `				);` |
|         - | 5911 | `		}` |
|         9 | 5912 | `	}` |
|        17 | 5913 | `	if( nArg == 1 ){` |
|         - | 5914 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5915 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5916 | `		return PH7_OK;` |
|         - | 5917 | `	}` |
|         - | 5918 | `	/* Create a new array */` |
|        15 | 5919 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5920 | `	if( pArray == 0 ){` |
|       ! 0 | 5921 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5922 | `		return PH7_OK;` |
|         - | 5923 | `	}` |
|         - | 5924 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5925 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5926 | `	/* Perform the intersection */` |
|        15 | 5927 | `	pEntry = pSrc->pFirst;` |
|        15 | 5928 | `	n = pSrc->nEntry;` |
|        24 | 5929 | `	for(;;){` |
|        49 | 5930 | `		if( n < 1 ){` |
|        15 | 5931 | `			break;` |
|         - | 5932 | `		}` |
|        57 | 5933 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5934 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5935 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5936 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5937 | `				/* Blob lookup */` |
|        27 | 5938 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5939 | `			}else{` |
|         - | 5940 | `				/* Int key */` |
|        13 | 5941 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5942 | `			}` |
|        39 | 5943 | `			if( rc != SXRET_OK ){` |
|         - | 5944 | `				/* Key does not exist, break immediately */` |
|        17 | 5945 | `				break;` |
|         - | 5946 | `			}` |
|        12 | 5947 | `		}` |
|        35 | 5948 | `		if( i >= nArg ){` |
|         - | 5949 | `			/* Perform the insertion */` |
|        19 | 5950 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5951 | `		}` |
|         - | 5952 | `		/* Point to the next entry */` |
|        35 | 5953 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5954 | `		n--;` |
|         1 | 5955 | `	}` |
|         - | 5956 | `	/* Return the freshly created array */` |
|        15 | 5957 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5958 | `	return PH7_OK;` |
|        13 | 5959 | `}` |
|         - | 5960 | `/*` |
|         - | 5961 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5962 | ` *  Computes the intersection of arrays.` |
|         - | 5963 | ` * Parameters` |
|         - | 5964 | ` *  $array1` |
|         - | 5965 | ` *    The array to compare from` |
|         - | 5966 | ` *  $array2` |
|         - | 5967 | ` *    An array to compare against` |
|         - | 5968 | ` *  $...` |
|         - | 5969 | ` *   More arrays to compare against` |
|         - | 5970 | ` * $callback` |
|         - | 5971 | ` *  The callback comparison function.` |
|         - | 5972 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5973 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5974 | ` *  than the second.` |
|         - | 5975 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5976 | ` * Return` |
|         - | 5977 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5978 | ` *  in all of the parameters. .` |
|         - | 5979 | ` * Note that NULL is returned on failure.` |
|         - | 5980 | ` */` |
|        24 | 5981 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5982 | `{` |
|         - | 5983 | `	ph7_hashmap_node *pEntry;` |
|         - | 5984 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5985 | `	ph7_value *pCallback;` |
|         - | 5986 | `	ph7_value *pArray;` |
|         - | 5987 | `	ph7_value *pVal;` |
|         - | 5988 | `	sxi32 rc;` |
|         - | 5989 | `	sxu32 n;` |
|         - | 5990 | `	int i;` |
|         - | 5991 |  |
|         - | 5992 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 5993 | `	if( nArg < 2 ){` |
|       ! 0 | 5994 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5995 | `			"ArgumentCountError",` |
|         - | 5996 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 5997 | `			nArg` |
|         - | 5998 | `			);` |
|         - | 5999 | `	}` |
|        29 | 6000 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6001 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6002 | `			"TypeError",` |
|         - | 6003 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6004 | `			ph7_type_name(apArg[0])` |
|         - | 6005 | `			);` |
|         - | 6006 | `	}` |
|         - | 6007 |  |
|        27 | 6008 | `	if( nArg == 2 ){` |
|         - | 6009 | `		/* Only the original array and the callback were provided. */` |
|         - | 6010 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6011 | `		 * validation ordering. */` |
|         3 | 6012 | `	} else {` |
|         - | 6013 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6014 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6015 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6016 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6017 | `					"TypeError",` |
|         - | 6018 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6019 | `					i + 1,` |
|         2 | 6020 | `					ph7_type_name(apArg[i])` |
|         - | 6021 | `					);` |
|         - | 6022 | `			}` |
|        13 | 6023 | `		}` |
|         - | 6024 | `	}` |
|         - | 6025 |  |
|         - | 6026 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6027 | `	pCallback = apArg[nArg - 1];` |
|         - | 6028 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6029 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6030 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6031 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6032 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6033 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6034 | `			 */` |
|         9 | 6035 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6036 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6037 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6038 | `					"TypeError",` |
|         - | 6039 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6040 | `					nArg` |
|         - | 6041 | `					);` |
|         - | 6042 | `			}` |
|         - | 6043 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6044 | `			{` |
|         6 | 6045 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6046 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6047 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6048 | `					int nMethodLen;` |
|         6 | 6049 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6050 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6051 | `					if( pClass ){` |
|         - | 6052 | `						/* Class exists but method is missing. */` |
|         4 | 6053 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6054 | `							"TypeError",` |
|         - | 6055 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6056 | `							nArg,` |
|         1 | 6057 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6058 | `							zMethod` |
|         - | 6059 | `							);` |
|         - | 6060 | `					}` |
|         - | 6061 | `					/* Class not found */` |
|         - | 6062 | `					{` |
|         - | 6063 | `						int nName;` |
|         3 | 6064 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6065 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6066 | `							"TypeError",` |
|         - | 6067 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6068 | `							nArg,` |
|         1 | 6069 | `							zName` |
|         - | 6070 | `							);` |
|         - | 6071 | `					}` |
|         - | 6072 | `				}` |
|         - | 6073 | `			}` |
|         - | 6074 | `			/* Fallback message */` |
|       ! 0 | 6075 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6076 | `				"TypeError",` |
|         - | 6077 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6078 | `				nArg` |
|         - | 6079 | `				);` |
|         - | 6080 | `		}` |
|         6 | 6081 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6082 | `			int len;` |
|         3 | 6083 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6084 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6085 | `				"TypeError",` |
|         - | 6086 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6087 | `				nArg,` |
|         1 | 6088 | `				zName` |
|         - | 6089 | `				);` |
|         - | 6090 | `		}` |
|         4 | 6091 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6092 | `			"TypeError",` |
|         - | 6093 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6094 | `			nArg` |
|         - | 6095 | `			);` |
|         - | 6096 | `	}` |
|         - | 6097 |  |
|        11 | 6098 | `	if( nArg == 2 ){` |
|         - | 6099 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6100 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6101 | `		return PH7_OK;` |
|         - | 6102 | `	}` |
|         - | 6103 |  |
|         - | 6104 | `	/* Create a new array */` |
|         7 | 6105 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6106 | `	if( pArray == 0 ){` |
|       ! 0 | 6107 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6108 | `		return PH7_OK;` |
|         - | 6109 | `	}` |
|         - | 6110 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6111 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6112 | `	/* Perform the intersection */` |
|         7 | 6113 | `	pEntry = pSrc->pFirst;` |
|         7 | 6114 | `	n = pSrc->nEntry;` |
|         7 | 6115 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6116 | `	for(;;){` |
|        19 | 6117 | `		if( n < 1 ){` |
|         5 | 6118 | `			break;` |
|         - | 6119 | `		}` |
|         - | 6120 | `		/* Extract the node value */` |
|        15 | 6121 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6122 | `		if( pVal ){` |
|        23 | 6123 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6124 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6125 | `					/* ignore */` |
|       ! 0 | 6126 | `					continue;` |
|         - | 6127 | `				}` |
|         - | 6128 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6129 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6130 | `				/* Perform the lookup */` |
|        15 | 6131 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6132 | `				if( rc != SXRET_OK ){` |
|         - | 6133 | `					/* Value does not exist */` |
|         7 | 6134 | `					break;` |
|         - | 6135 | `				}` |
|         5 | 6136 | `			}` |
|        15 | 6137 | `			if( i >= (nArg-1) ){` |
|         - | 6138 | `				/* Perform the insertion */` |
|         9 | 6139 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6140 | `			}` |
|         7 | 6141 | `		}` |
|        15 | 6142 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6143 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6144 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6145 | `			return PH7_EXCEPTION;` |
|         - | 6146 | `		}` |
|         - | 6147 | `		/* Point to the next entry */` |
|        13 | 6148 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6149 | `		n--;` |
|         1 | 6150 | `	}` |
|         - | 6151 | `	/* Return the freshly created array */` |
|         5 | 6152 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6153 | `	return PH7_OK;` |
|        17 | 6154 | `}` |
|         - | 6155 | `/*` |
|         - | 6156 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6157 | ` *  Fill an array with values.` |
|         - | 6158 | ` * Parameters` |
|         - | 6159 | ` *  $start_index` |
|         - | 6160 | ` *    The first index of the returned array.` |
|         - | 6161 | ` *  $num` |
|         - | 6162 | ` *   Number of elements to insert.` |
|         - | 6163 | ` *  $value` |
|         - | 6164 | ` *    Value to use for filling.` |
|         - | 6165 | ` * Return` |
|         - | 6166 | ` *  The filled array or null on failure.` |
|         - | 6167 | ` */` |
|       240 | 6168 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6169 | `{` |
|         - | 6170 | `	ph7_value *pArray;` |
|         - | 6171 | `	int i,nEntry;` |
|         - | 6172 |  |
|         - | 6173 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6174 | `	if( nArg != 3 ){` |
|         - | 6175 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6176 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6177 | `			"ArgumentCountError",` |
|         - | 6178 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6179 | `			nArg` |
|         - | 6180 | `			);` |
|         - | 6181 | `	}` |
|         - | 6182 |  |
|         - | 6183 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6184 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6185 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6186 | `	 * and NULLs are rejected outright. */` |
|       357 | 6187 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6188 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6189 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6190 | `			"TypeError",` |
|         - | 6191 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6192 | `			ph7_type_name(apArg[0])` |
|         - | 6193 | `			);` |
|         - | 6194 | `	}` |
|       242 | 6195 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6196 | `		int len;` |
|         8 | 6197 | `		sxu8 bReal = FALSE;` |
|         8 | 6198 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6199 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6200 | `			/* Non‑numeric string is an error. */` |
|         3 | 6201 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6202 | `				"TypeError",` |
|         - | 6203 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6204 | `				);` |
|         - | 6205 | `		}` |
|         5 | 6206 | `		if( bReal ){` |
|         - | 6207 | `			/* float-string -> deprecation warning */` |
|         4 | 6208 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6209 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6210 | `				zStr` |
|         - | 6211 | `				);` |
|         1 | 6212 | `		}` |
|         2 | 6213 | `	}` |
|         - | 6214 |  |
|         - | 6215 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6216 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6217 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6218 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6219 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6220 | `			"TypeError",` |
|         - | 6221 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6222 | `			ph7_type_name(apArg[1])` |
|         - | 6223 | `			);` |
|         - | 6224 | `	}` |
|       239 | 6225 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6226 | `		int len;` |
|         3 | 6227 | `		sxu8 bReal = FALSE;` |
|         3 | 6228 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6229 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6230 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6231 | `				"TypeError",` |
|         - | 6232 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6233 | `				);` |
|         - | 6234 | `		}` |
|       ! 0 | 6235 | `	}` |
|         - | 6236 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6237 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6238 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6239 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6240 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6241 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6242 | `		if( d != (double)i64 ){` |
|         7 | 6243 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6244 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6245 | `				d` |
|         - | 6246 | `				);` |
|         2 | 6247 | `		}` |
|         2 | 6248 | `	}` |
|         - | 6249 |  |
|         - | 6250 | `	/* Total number of entries to insert */` |
|       236 | 6251 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6252 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6253 | `	if( nEntry < 0 ){` |
|         3 | 6254 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6255 | `			"ValueError",` |
|         - | 6256 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6257 | `			);` |
|         - | 6258 | `	}` |
|         - | 6259 |  |
|         - | 6260 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6261 | `	if( nEntry == 0 ){` |
|         7 | 6262 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6263 | `		return PH7_OK;` |
|         - | 6264 | `	}` |
|         - | 6265 |  |
|         - | 6266 | `	/* Create a new array */` |
|       227 | 6267 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6268 | `	if( pArray == 0 ){` |
|       ! 0 | 6269 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6270 | `	}` |
|         - | 6271 |  |
|         - | 6272 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6273 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6274 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6275 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6276 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6277 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6278 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6279 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6280 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6281 | `		}` |
|   1058803 | 6282 | `	}` |
|         - | 6283 | `	/* Return the filled array */` |
|       227 | 6284 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6285 | `	return PH7_OK;` |
|       124 | 6286 | `}` |
|         - | 6287 | `/*` |
|         - | 6288 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6289 | ` *  Fill an array with values, specifying keys.` |
|         - | 6290 | ` * Parameters` |
|         - | 6291 | ` *  $input` |
|         - | 6292 | ` *   Array of values that will be used as key.` |
|         - | 6293 | ` *  $value` |
|         - | 6294 | ` *    Value to use for filling.` |
|         - | 6295 | ` * Return` |
|         - | 6296 | ` *  The filled array.` |
|         - | 6297 | ` * Throws` |
|         - | 6298 | ` *  ValueError if $input is not an array.` |
|         - | 6299 | ` */` |
|        22 | 6300 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6301 | `{` |
|         - | 6302 | `	ph7_hashmap_node *pEntry;` |
|         - | 6303 | `	ph7_hashmap *pSrc;` |
|         - | 6304 | `	ph7_value *pArray;` |
|         - | 6305 | `	sxu32 n;` |
|         - | 6306 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6307 | `	if( nArg != 2 ){` |
|         4 | 6308 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6309 | `			"ArgumentCountError",` |
|         - | 6310 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6311 | `			nArg` |
|         - | 6312 | `			);` |
|         - | 6313 | `	}` |
|         - | 6314 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6315 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6316 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6317 | `			"TypeError",` |
|         - | 6318 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6319 | `			ph7_type_name(apArg[0])` |
|         - | 6320 | `			);` |
|         - | 6321 | `	}` |
|         - | 6322 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6323 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6324 | `	/* Create a new array */` |
|        17 | 6325 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6326 | `	if( pArray == 0 ){` |
|       ! 0 | 6327 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6328 | `		return PH7_OK;` |
|         - | 6329 | `	}` |
|         - | 6330 | `	/* Perform the requested operation */` |
|        17 | 6331 | `	pEntry = pSrc->pFirst;` |
|        45 | 6332 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6333 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6334 | `		/* Point to the next entry */` |
|        29 | 6335 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6336 | `	}` |
|         - | 6337 | `	/* Return the filled array */` |
|        17 | 6338 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6339 | `	return PH7_OK;` |
|        14 | 6340 | `}` |
|         - | 6341 | `/*` |
|         - | 6342 | ` * array array_combine(array $keys,array $values)` |
|         - | 6343 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6344 | ` * Parameters` |
|         - | 6345 | ` *  $keys` |
|         - | 6346 | ` *    Array of keys to be used.` |
|         - | 6347 | ` * $values` |
|         - | 6348 | ` *   Array of values to be used.` |
|         - | 6349 | ` * Return` |
|         - | 6350 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6351 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6352 | ` *  not an array.` |
|         - | 6353 | ` */` |
|        16 | 6354 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6355 | `{` |
|         - | 6356 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6357 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6358 | `	ph7_value *pArray;` |
|         - | 6359 | `	sxu32 n;` |
|         - | 6360 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6361 | `	if( nArg != 2 ){` |
|         - | 6362 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6363 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6364 | `			"ArgumentCountError",` |
|         - | 6365 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6366 | `			nArg` |
|         - | 6367 | `			);` |
|         - | 6368 | `	}` |
|         - | 6369 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6370 | `	 * argument index in the error message. */` |
|        20 | 6371 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6372 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6373 | `			"TypeError",` |
|         - | 6374 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6375 | `			ph7_type_name(apArg[0])` |
|         - | 6376 | `			);` |
|         - | 6377 | `	}` |
|        17 | 6378 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6379 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6380 | `			"TypeError",` |
|         - | 6381 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6382 | `			ph7_type_name(apArg[1])` |
|         - | 6383 | `			);` |
|         - | 6384 | `	}` |
|         - | 6385 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6386 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6387 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6388 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6389 | `		/* Length mismatch -> ValueError */` |
|         3 | 6390 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6391 | `			"ValueError",` |
|         - | 6392 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6393 | `			);` |
|         - | 6394 | `	}` |
|         - | 6395 | `	/* Create a new array */` |
|        11 | 6396 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6397 | `	if( pArray == 0 ){` |
|       ! 0 | 6398 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6399 | `		return PH7_OK;` |
|         - | 6400 | `	}` |
|         - | 6401 | `	/* Perform the requested operation */` |
|        11 | 6402 | `	pKe = pKey->pFirst;` |
|        11 | 6403 | `	pVe = pValue->pFirst;` |
|        33 | 6404 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6405 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6406 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6407 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6408 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6409 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6410 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6411 | `		 * original array must not be mutated. */` |
|        23 | 6412 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6413 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6414 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6415 | `			if( pTmpKey ){` |
|         5 | 6416 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6417 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6418 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6419 | `				pKeyCopy = pTmpKey;` |
|         2 | 6420 | `			}` |
|         2 | 6421 | `		}` |
|        23 | 6422 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6423 | `		/* Point to the next entry */` |
|        23 | 6424 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6425 | `		pVe = pVe->pPrev;` |
|        12 | 6426 | `	}` |
|         - | 6427 | `	/* Return the filled array */` |
|        11 | 6428 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6429 | `	return PH7_OK;` |
|        12 | 6430 | `}` |
|         - | 6431 | `/*` |
|         - | 6432 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6433 | ` *  Return an array with elements in reverse order.` |
|         - | 6434 | ` * Parameters` |
|         - | 6435 | ` *  $array` |
|         - | 6436 | ` *   The input array.` |
|         - | 6437 | ` *  $preserve_keys (optional)` |
|         - | 6438 | ` *   If set to TRUE keys are preserved.` |
|         - | 6439 | ` * Return` |
|         - | 6440 | ` *  The reversed array.` |
|         - | 6441 | ` */` |
|        18 | 6442 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6443 | `{` |
|         - | 6444 | `	ph7_hashmap_node *pEntry;` |
|         - | 6445 | `	ph7_hashmap *pSrc;` |
|         - | 6446 | `	ph7_value *pArray;` |
|         - | 6447 | `	int bPreserve;` |
|         - | 6448 | `	sxu32 n;` |
|        20 | 6449 | `	if( nArg < 1 ){` |
|       ! 0 | 6450 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6451 | `			"ArgumentCountError",` |
|         - | 6452 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6453 | `			nArg` |
|         - | 6454 | `			);` |
|         - | 6455 | `	}` |
|         - | 6456 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6457 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6458 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6459 | `			"TypeError",` |
|         - | 6460 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6461 | `			ph7_type_name(apArg[0])` |
|         - | 6462 | `			);` |
|         - | 6463 | `	}` |
|        17 | 6464 | `	bPreserve = FALSE;` |
|        17 | 6465 | `	if( nArg > 1 ){` |
|         7 | 6466 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6467 | `	}` |
|         - | 6468 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6469 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6470 | `	/* Create a new array */` |
|        17 | 6471 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6472 | `	if( pArray == 0 ){` |
|       ! 0 | 6473 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6474 | `		return PH7_OK;` |
|         - | 6475 | `	}` |
|         - | 6476 | `	/* Perform the requested operation */` |
|        17 | 6477 | `	pEntry = pSrc->pLast;` |
|        55 | 6478 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6479 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6480 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6481 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6482 | `		/* Point to the previous entry */` |
|        39 | 6483 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6484 | `	}` |
|        17 | 6485 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6486 | `	return PH7_OK;` |
|        11 | 6487 | `}` |
|         - | 6488 | `/*` |
|         - | 6489 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6490 | ` *  Removes duplicate values from an array.` |
|         - | 6491 | ` * Parameters` |
|         - | 6492 | ` *  $array` |
|         - | 6493 | ` *   The input array.` |
|         - | 6494 | ` *  $flags` |
|         - | 6495 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6496 | ` *   behavior using these values:` |
|         - | 6497 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6498 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6499 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6500 | ` * Return` |
|         - | 6501 | ` *  The filtered array.` |
|         - | 6502 | ` */` |
|        22 | 6503 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6504 | `{` |
|         - | 6505 | `	ph7_hashmap_node *pEntry;` |
|         - | 6506 | `	ph7_value *pNeedle;` |
|         - | 6507 | `	ph7_hashmap *pSrc;` |
|         - | 6508 | `	ph7_value *pArray;` |
|         - | 6509 | `	int bStrict;` |
|         - | 6510 | `	sxi32 rc;` |
|         - | 6511 | `	sxu32 n;` |
|        25 | 6512 | `	if( nArg < 1 ){` |
|         - | 6513 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6514 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6515 | `			"ArgumentCountError",` |
|         - | 6516 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6517 | `			);` |
|         - | 6518 | `	}` |
|        25 | 6519 | `	if( nArg > 2 ){` |
|         - | 6520 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6521 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6522 | `			"ArgumentCountError",` |
|         - | 6523 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6524 | `			nArg` |
|         - | 6525 | `			);` |
|         - | 6526 | `	}` |
|         - | 6527 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6528 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6529 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6530 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6531 | `			"TypeError",` |
|         - | 6532 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6533 | `			ph7_type_name(apArg[0])` |
|         - | 6534 | `			);` |
|         - | 6535 | `	}` |
|        19 | 6536 | `	bStrict = FALSE;` |
|         - | 6537 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6538 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6539 | `	/* Create a new array */` |
|        19 | 6540 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6541 | `	if( pArray == 0 ){` |
|       ! 0 | 6542 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6543 | `		return PH7_OK;` |
|         - | 6544 | `	}` |
|         - | 6545 | `	/* Perform the requested operation */` |
|        19 | 6546 | `	pEntry = pSrc->pFirst;` |
|        83 | 6547 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6548 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6549 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6550 | `		if( pNeedle ){` |
|        65 | 6551 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6552 | `		}` |
|        65 | 6553 | `		if( rc != SXRET_OK ){` |
|         - | 6554 | `			/* Perform the insertion */` |
|        37 | 6555 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6556 | `		}` |
|         - | 6557 | `		/* Point to the next entry */` |
|        65 | 6558 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6559 | `	}` |
|         - | 6560 | `	/* Return the freshly created array */` |
|        19 | 6561 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6562 | `	return PH7_OK;` |
|        14 | 6563 | `}` |
|         - | 6564 | `/*` |
|         - | 6565 | ` * array array_flip(array $input)` |
|         - | 6566 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6567 | ` * Parameter` |
|         - | 6568 | ` *  $input` |
|         - | 6569 | ` *   Input array.` |
|         - | 6570 | ` * Return` |
|         - | 6571 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6572 | ` */` |
|        30 | 6573 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6574 | `{` |
|         - | 6575 | `	ph7_hashmap_node *pEntry;` |
|         - | 6576 | `	ph7_hashmap *pSrc;` |
|         - | 6577 | `	ph7_value *pArray;` |
|         - | 6578 | `	ph7_value *pKey;` |
|         - | 6579 | `	ph7_value sVal;` |
|         - | 6580 | `	sxu32 n;` |
|         - | 6581 |  |
|         - | 6582 | `	/* PHP requires exactly one argument */` |
|        33 | 6583 | `	if( nArg != 1 ){` |
|         - | 6584 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6585 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6586 | `			"ArgumentCountError",` |
|         - | 6587 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6588 | `			nArg` |
|         - | 6589 | `			);` |
|         - | 6590 | `	}` |
|         - | 6591 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6592 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6593 | `		/* Type mismatch -> TypeError */` |
|         4 | 6594 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6595 | `			"TypeError",` |
|         - | 6596 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6597 | `			ph7_type_name(apArg[0])` |
|         - | 6598 | `			);` |
|         - | 6599 | `	}` |
|         - | 6600 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6601 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6602 | `	/* Create a new array */` |
|        27 | 6603 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6604 | `	if( pArray == 0 ){` |
|       ! 0 | 6605 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6606 | `		return PH7_OK;` |
|         - | 6607 | `	}` |
|         - | 6608 | `	/* Start processing */` |
|        27 | 6609 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6610 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6611 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6612 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6613 | `		if( pKey ){` |
|         - | 6614 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6615 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6616 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6617 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6618 | `					);` |
|     22236 | 6619 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6620 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6621 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6622 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6623 | `				}else{` |
|         - | 6624 | `					SyString sStr;` |
|      2227 | 6625 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6626 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6627 | `				}` |
|         - | 6628 | `				/* Perform the insertion */` |
|     22227 | 6629 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6630 | `				/* Safely release the value because each inserted entry` |
|         - | 6631 | `				 * has its own private copy of the value.` |
|         - | 6632 | `				 */` |
|     22227 | 6633 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6634 | `			}else{` |
|         - | 6635 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6636 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6637 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6638 | `					);` |
|         - | 6639 | `			}` |
|     11118 | 6640 | `		}` |
|         - | 6641 | `		/* Point to the next entry */` |
|     22237 | 6642 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6643 | `	}` |
|         - | 6644 | `	/* Return the freshly created array */` |
|        27 | 6645 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6646 | `	return PH7_OK;` |
|        18 | 6647 | `}` |
|         - | 6648 | `/*` |
|         - | 6649 | ` * number array_sum(array $array )` |
|         - | 6650 | ` *  Calculate the sum of values in an array.` |
|         - | 6651 | ` * Parameters` |
|         - | 6652 | ` *  $array: The input array.` |
|         - | 6653 | ` * Return` |
|         - | 6654 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6655 | ` */` |
|        24 | 6656 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6657 | `{` |
|         - | 6658 | `	ph7_hashmap_node *pEntry;` |
|         - | 6659 | `	ph7_value *pObj;` |
|        26 | 6660 | `	double dSum = 0;` |
|         - | 6661 | `	sxu32 n;` |
|        26 | 6662 | `	pEntry = pMap->pFirst;` |
|        92 | 6663 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6664 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6665 | `		if( pObj ){` |
|        68 | 6666 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6667 | `				dSum += pObj->rVal;` |
|        54 | 6668 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6669 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6670 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6671 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6672 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6673 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6674 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6675 | `						"Addition is not supported on type string");` |
|        14 | 6676 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6677 | `					double dv = 0;` |
|        13 | 6678 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6679 | `					dSum += dv;` |
|         8 | 6680 | `				}` |
|        12 | 6681 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6682 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6683 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6684 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6685 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6686 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6687 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6688 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6689 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6690 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6691 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6692 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6693 | `			}` |
|         - | 6694 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6695 | `		}` |
|         - | 6696 | `		/* Point to the next entry */` |
|        68 | 6697 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6698 | `	}` |
|         - | 6699 | `	/* Return sum */` |
|        26 | 6700 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6701 | `}` |
|       688 | 6702 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6703 | `{` |
|         - | 6704 | `	ph7_hashmap_node *pEntry;` |
|         - | 6705 | `	ph7_value *pObj;` |
|       690 | 6706 | `	sxi64 nSum = 0;` |
|         - | 6707 | `	sxu32 n;` |
|       690 | 6708 | `	pEntry = pMap->pFirst;` |
|      4702 | 6709 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6710 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6711 | `		if( pObj ){` |
|      4014 | 6712 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6713 | `				nSum += pObj->x.iVal;` |
|      2018 | 6714 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6715 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6716 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6717 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6718 | `						"Addition is not supported on type string");` |
|        10 | 6719 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6720 | `					sxi64 nv = 0;` |
|         8 | 6721 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6722 | `					nSum += nv;` |
|         5 | 6723 | `				}` |
|        17 | 6724 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6725 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6726 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6727 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6728 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6729 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6730 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6731 | `					"Addition is not supported on type %s",` |
|         2 | 6732 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6733 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6734 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6735 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6736 | `			}` |
|         - | 6737 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6738 | `		}` |
|         - | 6739 | `		/* Point to the next entry */` |
|      4014 | 6740 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6741 | `	}` |
|         - | 6742 | `	/* Return sum */` |
|       690 | 6743 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6744 | `}` |
|         - | 6745 | `/* number array_sum(array $array )` |
|         - | 6746 | ` * (See block-coment above)` |
|         - | 6747 | ` */` |
|       724 | 6748 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6749 | `{` |
|         - | 6750 | `	ph7_hashmap_node *pEntry;` |
|         - | 6751 | `	ph7_hashmap *pMap;` |
|         - | 6752 | `	ph7_value *pObj;` |
|       728 | 6753 | `	int useDouble = 0;` |
|         - | 6754 | `	sxu32 n;` |
|         - | 6755 | `	/* PHP requires exactly one argument */` |
|       728 | 6756 | `	if( nArg != 1 ){` |
|         4 | 6757 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6758 | `			"ArgumentCountError",` |
|         - | 6759 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6760 | `			nArg` |
|         - | 6761 | `			);` |
|         - | 6762 | `	}` |
|         - | 6763 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6764 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6765 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6766 | `		char zBuf[64];` |
|         8 | 6767 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6768 | `			"TypeError",` |
|         - | 6769 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6770 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6771 | `			);` |
|         - | 6772 | `	}` |
|       720 | 6773 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6774 | `	if( pMap->nEntry < 1 ){` |
|         - | 6775 | `		/* Nothing to compute,return 0 */` |
|         7 | 6776 | `		ph7_result_int(pCtx,0);` |
|         7 | 6777 | `		return PH7_OK;` |
|         - | 6778 | `	}` |
|         - | 6779 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6780 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6781 | `	 */` |
|       714 | 6782 | `	pEntry = pMap->pFirst;` |
|      4734 | 6783 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6784 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6785 | `		if( pObj ){` |
|      4046 | 6786 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6787 | `				useDouble = 1;` |
|        20 | 6788 | `				break;` |
|         - | 6789 | `			}` |
|      4028 | 6790 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6791 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6792 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6793 | `				sxu32 i;` |
|        32 | 6794 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6795 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6796 | `						useDouble = 1;` |
|         7 | 6797 | `						break;` |
|         - | 6798 | `					}` |
|         9 | 6799 | `				}` |
|        18 | 6800 | `				if( useDouble ){` |
|         7 | 6801 | `					break;` |
|         - | 6802 | `				}` |
|         5 | 6803 | `			}` |
|      2010 | 6804 | `		}` |
|      4022 | 6805 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6806 | `	}` |
|       714 | 6807 | `	if( useDouble ){` |
|        26 | 6808 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6809 | `	}else{` |
|       690 | 6810 | `		Int64Sum(pCtx,pMap);` |
|         - | 6811 | `	}` |
|       714 | 6812 | `	return PH7_OK;` |
|       366 | 6813 | `}` |
|         - | 6814 | `/*` |
|         - | 6815 | ` * number array_product(array $array )` |
|         - | 6816 | ` *  Calculate the product of values in an array.` |
|         - | 6817 | ` * Parameters` |
|         - | 6818 | ` *  $array: The input array.` |
|         - | 6819 | ` * Return` |
|         - | 6820 | ` *  Returns the product of values as an integer or float.` |
|         - | 6821 | ` */` |
|         2 | 6822 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6823 | `{` |
|         - | 6824 | `	ph7_hashmap_node *pEntry;` |
|         - | 6825 | `	ph7_value *pObj;` |
|         - | 6826 | `	double dProd;` |
|         - | 6827 | `	sxu32 n;` |
|         3 | 6828 | `	pEntry = pMap->pFirst;` |
|         3 | 6829 | `	dProd = 1;` |
|         7 | 6830 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6831 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6832 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6833 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6834 | `				dProd *= pObj->rVal;` |
|         4 | 6835 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6836 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6837 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6838 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6839 | `					double dv = 0;` |
|       ! 0 | 6840 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6841 | `					dProd *= dv;` |
|       ! 0 | 6842 | `				}` |
|       ! 0 | 6843 | `			}` |
|         2 | 6844 | `		}` |
|         - | 6845 | `		/* Point to the next entry */` |
|         5 | 6846 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6847 | `	}` |
|         - | 6848 | `	/* Return product */` |
|         3 | 6849 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6850 | `}` |
|         2 | 6851 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6852 | `{` |
|         - | 6853 | `	ph7_hashmap_node *pEntry;` |
|         - | 6854 | `	ph7_value *pObj;` |
|         - | 6855 | `	sxi64 nProd;` |
|         - | 6856 | `	sxu32 n;` |
|         3 | 6857 | `	pEntry = pMap->pFirst;` |
|         3 | 6858 | `	nProd = 1;` |
|         9 | 6859 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6860 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6861 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6862 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6863 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6864 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6865 | `				nProd *= pObj->x.iVal;` |
|         3 | 6866 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6867 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6868 | `					sxi64 nv = 0;` |
|       ! 0 | 6869 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6870 | `					nProd *= nv;` |
|       ! 0 | 6871 | `				}` |
|       ! 0 | 6872 | `			}` |
|         3 | 6873 | `		}` |
|         - | 6874 | `		/* Point to the next entry */` |
|         7 | 6875 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6876 | `	}` |
|         - | 6877 | `	/* Return product */` |
|         3 | 6878 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6879 | `}` |
|         - | 6880 | `/* number array_product(array $array )` |
|         - | 6881 | ` * (See block-block comment above)` |
|         - | 6882 | ` */` |
|        16 | 6883 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6884 | `{` |
|         - | 6885 | `	ph7_hashmap *pMap;` |
|         - | 6886 | `	ph7_value *pObj;` |
|        17 | 6887 | `	if( nArg < 1 ){` |
|         - | 6888 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6889 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6890 | `		return PH7_OK;` |
|         - | 6891 | `	}` |
|         - | 6892 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 6893 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6894 | `		char zBuf[64];` |
|        16 | 6895 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6896 | `			"TypeError",` |
|         - | 6897 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 6898 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6899 | `			);` |
|         - | 6900 | `	}` |
|         7 | 6901 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6902 | `	if( pMap->nEntry < 1 ){` |
|         - | 6903 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6904 | `		ph7_result_int(pCtx,1);` |
|         3 | 6905 | `		return PH7_OK;` |
|         - | 6906 | `	}` |
|         - | 6907 | `	/* If the first element is of type float,then perform floating` |
|         - | 6908 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6909 | `	 */` |
|         5 | 6910 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6911 | `	if( pObj == 0 ){` |
|       ! 0 | 6912 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6913 | `		return PH7_OK;` |
|         - | 6914 | `	}` |
|         5 | 6915 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6916 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6917 | `	}else{` |
|         3 | 6918 | `		Int64Prod(pCtx,pMap);` |
|         - | 6919 | `	}` |
|         5 | 6920 | `	return PH7_OK;` |
|         9 | 6921 | `}` |
|         - | 6922 | `/*` |
|         - | 6923 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6924 | ` *  Pick one or more random entries out of an array.` |
|         - | 6925 | ` * Parameters` |
|         - | 6926 | ` * $input` |
|         - | 6927 | ` *  The input array.` |
|         - | 6928 | ` * $num_req` |
|         - | 6929 | ` *  Specifies how many entries you want to pick.` |
|         - | 6930 | ` * Return` |
|         - | 6931 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6932 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6933 | ` *  NULL is returned on failure.` |
|         - | 6934 | ` */` |
|        36 | 6935 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6936 | `{` |
|         - | 6937 | `	ph7_hashmap_node *pNode;` |
|         - | 6938 | `	ph7_hashmap *pMap;` |
|        37 | 6939 | `	int nItem = 1;` |
|        37 | 6940 | `	if( nArg < 1 ){` |
|         - | 6941 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6942 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6943 | `		return PH7_OK;` |
|         - | 6944 | `	}` |
|         - | 6945 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 6946 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6947 | `		char zBuf[64];` |
|        10 | 6948 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6949 | `			"TypeError",` |
|         - | 6950 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6951 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6952 | `			);` |
|         - | 6953 | `	}` |
|         - | 6954 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6955 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 6956 | `	if( nArg > 1 ){` |
|        23 | 6957 | `		ph7_value *pNum = apArg[1];` |
|        22 | 6958 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 6959 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6960 | `			char zBuf[64];` |
|       ! 0 | 6961 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6962 | `				"TypeError",` |
|         - | 6963 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 6964 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6965 | `				);` |
|         - | 6966 | `		}` |
|        23 | 6967 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6968 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6969 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6970 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6971 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6972 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6973 | `			int len;` |
|         9 | 6974 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6975 | `			sxi64 iLong; double dReal;` |
|         9 | 6976 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6977 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6978 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6979 | `					"TypeError",` |
|         - | 6980 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6981 | `					);` |
|         - | 6982 | `			}` |
|         - | 6983 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6984 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6985 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6986 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6987 | `			}` |
|         3 | 6988 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6989 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6990 | `			nItem = (int)iLong;` |
|         2 | 6991 | `		}else{` |
|        15 | 6992 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6993 | `		}` |
|         8 | 6994 | `	}` |
|         - | 6995 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6996 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6997 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6998 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6999 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7000 | `			"ValueError",` |
|         - | 7001 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7002 | `			);` |
|         - | 7003 | `	}` |
|         - | 7004 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7005 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7006 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7007 | `			"ValueError",` |
|         - | 7008 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7009 | `			);` |
|         - | 7010 | `	}` |
|        13 | 7011 | `	if( nItem < 2 ){` |
|         - | 7012 | `		sxu32 nEntry;` |
|         - | 7013 | `		/* Select a random number */` |
|         9 | 7014 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7015 | `		/* Extract the desired entry.` |
|         - | 7016 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7017 | `		 */` |
|         9 | 7018 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         4 | 7019 | `			pNode = pMap->pLast;` |
|         4 | 7020 | `			nEntry = pMap->nEntry - nEntry;` |
|         4 | 7021 | `			if( nEntry > 1 ){` |
|       ! 0 | 7022 | `				for(;;){` |
|       ! 0 | 7023 | `					if( nEntry == 0 ){` |
|       ! 0 | 7024 | `						break;` |
|         - | 7025 | `					}` |
|         - | 7026 | `					/* Point to the previous entry */` |
|       ! 0 | 7027 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7028 | `					nEntry--;` |
|       ! 0 | 7029 | `				}` |
|       ! 0 | 7030 | `			}` |
|         2 | 7031 | `		}else{` |
|         6 | 7032 | `			pNode = pMap->pFirst;` |
|         6 | 7033 | `			for(;;){` |
|        10 | 7034 | `				if( nEntry == 0 ){` |
|         6 | 7035 | `					break;` |
|         - | 7036 | `				}` |
|         - | 7037 | `				/* Point to the next entry */` |
|         5 | 7038 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         5 | 7039 | `				nEntry--;` |
|         1 | 7040 | `			}` |
|         - | 7041 | `		}` |
|         9 | 7042 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7043 | `			/* Int key */` |
|         7 | 7044 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7045 | `		}else{` |
|         - | 7046 | `			/* Blob key */` |
|         3 | 7047 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7048 | `		}` |
|         5 | 7049 | `	}else{` |
|         - | 7050 | `		ph7_value sKey,*pArray;` |
|         - | 7051 | `		ph7_hashmap *pDest;` |
|         - | 7052 | `		/* Create a new array */` |
|         5 | 7053 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7054 | `		if( pArray == 0 ){` |
|       ! 0 | 7055 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7056 | `			return PH7_OK;` |
|         - | 7057 | `		}` |
|         - | 7058 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7059 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7060 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7061 | `		/* Copy the first n items */` |
|         5 | 7062 | `		pNode = pMap->pFirst;` |
|         5 | 7063 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7064 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7065 | `		}` |
|        15 | 7066 | `		while( nItem > 0){` |
|        11 | 7067 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7068 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7069 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7070 | `			/* Point to the next entry */` |
|        11 | 7071 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7072 | `			nItem--;` |
|         1 | 7073 | `		}` |
|         - | 7074 | `		/* Shuffle the array */` |
|         5 | 7075 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7076 | `		/* Rehash node */` |
|         5 | 7077 | `		HashmapSortRehash(pDest);` |
|         - | 7078 | `		/* Return the random array */` |
|         5 | 7079 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7080 | `	}` |
|        13 | 7081 | `	return PH7_OK;` |
|        19 | 7082 | `}` |
|         - | 7083 | `/*` |
|         - | 7084 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7085 | ` *  Split an array into chunks.` |
|         - | 7086 | ` * Parameters` |
|         - | 7087 | ` * $input` |
|         - | 7088 | ` *   The array to work on` |
|         - | 7089 | ` * $size` |
|         - | 7090 | ` *   The size of each chunk` |
|         - | 7091 | ` * $preserve_keys` |
|         - | 7092 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7093 | ` *   the chunk numerically.` |
|         - | 7094 | ` * Return` |
|         - | 7095 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7096 | ` *  zero, with each dimension containing size elements.` |
|         - | 7097 | ` */` |
|        36 | 7098 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7099 | `{` |
|         - | 7100 | `	ph7_value *pArray,*pChunk;` |
|         - | 7101 | `	ph7_hashmap_node *pEntry;` |
|         - | 7102 | `	ph7_hashmap *pMap;` |
|         - | 7103 | `	int bPreserve;` |
|         - | 7104 | `	sxu32 nChunk;` |
|         - | 7105 | `	sxu32 nSize;` |
|         - | 7106 | `	sxu32 n;` |
|         - | 7107 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7108 | `	if( nArg < 2 ){` |
|         - | 7109 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7110 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7111 | `			"ArgumentCountError",` |
|         - | 7112 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7113 | `			nArg` |
|         - | 7114 | `			);` |
|         - | 7115 | `	}` |
|        41 | 7116 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7118 | `			"TypeError",` |
|         - | 7119 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7120 | `			ph7_type_name(apArg[0])` |
|         - | 7121 | `			);` |
|         - | 7122 | `	}` |
|         - | 7123 | `	/* Create a new array */` |
|        38 | 7124 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7125 | `	if( pArray == 0 ){` |
|       ! 0 | 7126 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7127 | `		return PH7_OK;` |
|         - | 7128 | `	}` |
|         - | 7129 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7130 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7131 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7132 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7133 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7134 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7135 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7136 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7137 | `			"TypeError",` |
|         - | 7138 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7139 | `			ph7_type_name(apArg[1])` |
|         - | 7140 | `			);` |
|         - | 7141 | `	}` |
|         - | 7142 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7143 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7144 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7145 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7146 | `		int len;` |
|         3 | 7147 | `		sxu8 bReal = FALSE;` |
|         3 | 7148 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7149 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7150 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7151 | `				"TypeError",` |
|         - | 7152 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7153 | `				);` |
|         - | 7154 | `		}` |
|       ! 0 | 7155 | `		if( bReal ){` |
|         - | 7156 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7157 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7158 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7159 | `				zStr` |
|         - | 7160 | `				);` |
|       ! 0 | 7161 | `		}` |
|       ! 0 | 7162 | `	}` |
|         - | 7163 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7164 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7165 | `	 * later via ph7_value_to_int. */` |
|        35 | 7166 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7167 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7168 | `		sxi64 i = (sxi64)d;` |
|         3 | 7169 | `		if( d != (double)i ){` |
|         4 | 7170 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7171 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7172 | `				d` |
|         - | 7173 | `				);` |
|         1 | 7174 | `		}` |
|         1 | 7175 | `	}` |
|         - | 7176 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7177 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7178 | `	{` |
|        35 | 7179 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7180 | `		if( nSizeSigned < 1 ){` |
|         - | 7181 | `			/* size <= 0 -> ValueError */` |
|         6 | 7182 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7183 | `				"ValueError",` |
|         - | 7184 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7185 | `				);` |
|         - | 7186 | `		}` |
|        29 | 7187 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7188 | `	}` |
|        29 | 7189 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7190 | `		/* Return the whole array */` |
|         3 | 7191 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7192 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7193 | `		return PH7_OK;` |
|         - | 7194 | `	}` |
|        27 | 7195 | `	bPreserve = 0;` |
|        27 | 7196 | `	if( nArg > 2 ){` |
|         - | 7197 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7198 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7199 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7200 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7201 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7202 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7203 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7204 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7205 | `				"TypeError",` |
|         - | 7206 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7207 | `				ph7_type_name(apArg[2])` |
|         - | 7208 | `				);` |
|         - | 7209 | `		}` |
|        21 | 7210 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7211 | `	}` |
|         - | 7212 | `	/* Start processing */` |
|        27 | 7213 | `	pEntry = pMap->pFirst;` |
|        27 | 7214 | `	nChunk = 0;` |
|        27 | 7215 | `	pChunk = 0;` |
|        27 | 7216 | `	n = pMap->nEntry;` |
|        56 | 7217 | `	for( ;; ){` |
|       113 | 7218 | `		if( n < 1 ){` |
|         - | 7219 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7220 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7221 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7222 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7223 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7224 | `			 * exists. */` |
|        27 | 7225 | `			if( pChunk ){` |
|        27 | 7226 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7227 | `			}` |
|        27 | 7228 | `			break;` |
|         - | 7229 | `		}` |
|        87 | 7230 | `		if( nChunk < 1 ){` |
|        71 | 7231 | `			if( pChunk ){` |
|         - | 7232 | `				/* Put the first chunk */` |
|        45 | 7233 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7234 | `			}` |
|         - | 7235 | `			/* Create a new dimension */` |
|        71 | 7236 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7237 | `												   * will be automatically released as soon we return` |
|         - | 7238 | `												   * from this function */` |
|        71 | 7239 | `			if( pChunk == 0 ){` |
|       ! 0 | 7240 | `				break;` |
|         - | 7241 | `			}` |
|        71 | 7242 | `			nChunk = nSize;` |
|        35 | 7243 | `		}` |
|         - | 7244 | `		/* Insert the entry */` |
|        87 | 7245 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7246 | `		/* Point to the next entry */` |
|        87 | 7247 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7248 | `		nChunk--;` |
|        87 | 7249 | `		n--;` |
|         1 | 7250 | `	}` |
|         - | 7251 | `	/* Return the multidimensional array */` |
|        27 | 7252 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7253 | `	return PH7_OK;` |
|        23 | 7254 | `}` |
|         - | 7255 | `/*` |
|         - | 7256 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7257 | ` *  Pad array to the specified length with a value.` |
|         - | 7258 | ` * $input` |
|         - | 7259 | ` *   Initial array of values to pad.` |
|         - | 7260 | ` * $pad_size` |
|         - | 7261 | ` *   New size of the array.` |
|         - | 7262 | ` * $pad_value` |
|         - | 7263 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7264 | ` */` |
|         - | 7265 | `/*` |
|         - | 7266 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7267 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7268 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7269 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7270 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7271 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7272 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7273 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7274 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7275 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7276 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7277 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7278 | ` */` |
|        50 | 7279 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7280 | `	ph7_context *pCtx,` |
|         - | 7281 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7282 | `	int iArg,              /* 1-based argument position */` |
|         - | 7283 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7284 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7285 | `	)` |
|         1 | 7286 | `{` |
|        51 | 7287 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7288 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7289 | `			"ValueError",` |
|         - | 7290 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7291 | `			zFunc,iArg,zParam` |
|         - | 7292 | `			);` |
|         - | 7293 | `	}` |
|        37 | 7294 | `	return SXRET_OK;` |
|        26 | 7295 | `}` |
|        62 | 7296 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7297 | `{` |
|         - | 7298 | `	ph7_hashmap *pMap;` |
|         - | 7299 | `	ph7_value *pArray;` |
|         - | 7300 | `	sxi64 iLen,iAbs;` |
|         - | 7301 | `	int nEntry;` |
|         - | 7302 | `	sxi32 rc;` |
|        65 | 7303 | `	if( nArg != 3 ){` |
|         4 | 7304 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7305 | `			"ArgumentCountError",` |
|         - | 7306 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7307 | `			nArg` |
|         - | 7308 | `			);` |
|         - | 7309 | `	}` |
|        62 | 7310 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7311 | `		char zBuf[64];` |
|        11 | 7312 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7313 | `			"TypeError",` |
|         - | 7314 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7315 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7316 | `			);` |
|         - | 7317 | `	}` |
|         - | 7318 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7319 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7320 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7321 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7322 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7323 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7324 | `		char zBuf[64];` |
|       ! 0 | 7325 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7326 | `			"TypeError",` |
|         - | 7327 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7328 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7329 | `			);` |
|         - | 7330 | `	}` |
|        55 | 7331 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7332 | `		int nStr;` |
|        11 | 7333 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7334 | `		sxi64 iLong; double dReal;` |
|        11 | 7335 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7336 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7337 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7338 | `				"TypeError",` |
|         - | 7339 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7340 | `				);` |
|         - | 7341 | `		}` |
|         7 | 7342 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7343 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7344 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7345 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7346 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7347 | `					"TypeError",` |
|         - | 7348 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7349 | `					);` |
|         - | 7350 | `			}` |
|         3 | 7351 | `			iLen = (sxi64)dReal;` |
|         3 | 7352 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7353 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7354 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7355 | `					zStr` |
|         - | 7356 | `					);` |
|       ! 0 | 7357 | `			}` |
|         2 | 7358 | `		}else{` |
|         5 | 7359 | `			iLen = iLong;` |
|         - | 7360 | `		}` |
|         4 | 7361 | `	}else{` |
|        45 | 7362 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7363 | `	}` |
|         - | 7364 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7365 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7366 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7367 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7368 | `	 * overflow). */` |
|        51 | 7369 | `	iAbs = iLen;` |
|        51 | 7370 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7371 | `		iAbs = -iAbs;` |
|         7 | 7372 | `	}` |
|        51 | 7373 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7374 | `	if( rc != SXRET_OK ){` |
|        15 | 7375 | `		return rc;` |
|         - | 7376 | `	}` |
|        37 | 7377 | `	nEntry = (int)iLen;` |
|         - | 7378 | `	/* Create a new array */` |
|        37 | 7379 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7380 | `	if( pArray == 0 ){` |
|       ! 0 | 7381 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7382 | `	}` |
|        37 | 7383 | `	if( nEntry < 0 ){` |
|        11 | 7384 | `		nEntry = -nEntry;` |
|        11 | 7385 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7386 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7387 | `			/* Insert given items first */` |
|        25 | 7388 | `			while( nEntry > 0 ){` |
|        19 | 7389 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7390 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7391 | `				}` |
|        19 | 7392 | `				nEntry--;` |
|         1 | 7393 | `			}` |
|         - | 7394 | `			/* Merge the two arrays */` |
|         7 | 7395 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7396 | `		}else{` |
|         5 | 7397 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7398 | `		}` |
|        32 | 7399 | `	}else if( nEntry > 0 ){` |
|        25 | 7400 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7401 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7402 | `			/* Merge the two arrays first */` |
|        19 | 7403 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7404 | `			/* Insert given items */` |
|       275 | 7405 | `			while( nEntry > 0 ){` |
|       257 | 7406 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7407 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7408 | `				}` |
|       257 | 7409 | `				nEntry--;` |
|         1 | 7410 | `			}` |
|        10 | 7411 | `		}else{` |
|         7 | 7412 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7413 | `		}` |
|        13 | 7414 | `	}else{` |
|         - | 7415 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7416 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7417 | `	}` |
|         - | 7418 | `	/* Return the new array */` |
|        37 | 7419 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7420 | `	return PH7_OK;` |
|        34 | 7421 | `}` |
|         - | 7422 | `/*` |
|         - | 7423 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7424 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7425 | ` * Parameters` |
|         - | 7426 | ` * $array` |
|         - | 7427 | ` *   The array in which elements are replaced.` |
|         - | 7428 | ` * $array1` |
|         - | 7429 | ` *   The array from which elements will be extracted.` |
|         - | 7430 | ` * ....` |
|         - | 7431 | ` *  More arrays from which elements will be extracted.` |
|         - | 7432 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7433 | ` * Return` |
|         - | 7434 | ` *  Returns an array.` |
|         - | 7435 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7436 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7437 | ` */` |
|        20 | 7438 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7439 | `{` |
|         - | 7440 | `	ph7_hashmap *pMap;` |
|         - | 7441 | `	ph7_value *pArray;` |
|         - | 7442 | `	int i;` |
|        23 | 7443 | `	if( nArg < 1 ){` |
|       ! 0 | 7444 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7445 | `			"ArgumentCountError",` |
|         - | 7446 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7447 | `			);` |
|         - | 7448 | `	}` |
|        23 | 7449 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7450 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7451 | `			"TypeError",` |
|         - | 7452 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7453 | `			ph7_type_name(apArg[0])` |
|         - | 7454 | `			);` |
|         - | 7455 | `	}` |
|         - | 7456 | `	/* Create a new array */` |
|        20 | 7457 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7458 | `	if( pArray == 0 ){` |
|       ! 0 | 7459 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7460 | `		return PH7_OK;` |
|         - | 7461 | `	}` |
|         - | 7462 | `	/* Overwrite from the first array */` |
|        20 | 7463 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7464 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7465 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7466 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7467 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7468 | `			/* Type mismatch -> TypeError */` |
|         4 | 7469 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7470 | `				"TypeError",` |
|         - | 7471 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7472 | `				i + 1,` |
|         2 | 7473 | `				ph7_type_name(apArg[i])` |
|         - | 7474 | `				);` |
|         - | 7475 | `		}` |
|         - | 7476 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7477 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7478 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7479 | `	}` |
|         - | 7480 | `	/* Return the new array */` |
|        17 | 7481 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7482 | `	return PH7_OK;` |
|        13 | 7483 | `}` |
|         - | 7484 | `/*` |
|         - | 7485 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7486 | ` *  Filters elements of an array using a callback function.` |
|         - | 7487 | ` * Parameters` |
|         - | 7488 | ` *  $input` |
|         - | 7489 | ` *    The array to iterate over` |
|         - | 7490 | ` * $callback` |
|         - | 7491 | ` *    The callback function to use` |
|         - | 7492 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7493 | ` *    will be removed.` |
|         - | 7494 | ` * Return` |
|         - | 7495 | ` *  The filtered array.` |
|         - | 7496 | ` */` |
|        30 | 7497 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7498 | `{` |
|         - | 7499 | `	ph7_hashmap_node *pEntry;` |
|         - | 7500 | `	ph7_hashmap *pMap;` |
|         - | 7501 | `	ph7_value *pArray;` |
|         - | 7502 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7503 | `	ph7_value *pValue;` |
|         - | 7504 | `	sxi32 rc;` |
|         - | 7505 | `	int keep;` |
|         - | 7506 | `	sxu32 n;` |
|        32 | 7507 | `	if( nArg < 1 ){` |
|         - | 7508 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7509 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7510 | `		return PH7_OK;` |
|         - | 7511 | `	}` |
|         - | 7512 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7513 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7514 | `		char zBuf[64];` |
|        19 | 7515 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7516 | `			"TypeError",` |
|         - | 7517 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7518 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7519 | `			);` |
|         - | 7520 | `	}` |
|         - | 7521 | `	/* Create a new array */` |
|        20 | 7522 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7523 | `	if( pArray == 0 ){` |
|       ! 0 | 7524 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7525 | `		return PH7_OK;` |
|         - | 7526 | `	}` |
|         - | 7527 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7528 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7529 | `	pEntry = pMap->pFirst;` |
|        20 | 7530 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7531 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7532 | `	/* Perform the requested operation */` |
|        78 | 7533 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7534 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7535 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7536 | `		if( pValue == 0 ){` |
|         - | 7537 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7538 | `			keep = FALSE;` |
|        64 | 7539 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7540 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7541 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7542 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7543 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7544 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7545 | `					int len;` |
|         3 | 7546 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7547 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7548 | `						"TypeError",` |
|         - | 7549 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7550 | `						zName` |
|         - | 7551 | `						);` |
|       ! 0 | 7552 | `				}else{` |
|       ! 0 | 7553 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7554 | `						"TypeError",` |
|         - | 7555 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7556 | `						ph7_type_name(apArg[1])` |
|         - | 7557 | `						);` |
|         - | 7558 | `				}` |
|         - | 7559 | `			}` |
|        33 | 7560 | `			keep = FALSE;` |
|        33 | 7561 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7562 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7563 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7564 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7565 | `				return PH7_EXCEPTION;` |
|         - | 7566 | `			}` |
|        31 | 7567 | `			if( rc == SXRET_OK ){` |
|         - | 7568 | `				/* Perform a boolean cast */` |
|        31 | 7569 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7570 | `			}` |
|        31 | 7571 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7572 | `		}else{` |
|         - | 7573 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7574 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7575 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7576 | `			 */` |
|        29 | 7577 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7578 | `		}` |
|        59 | 7579 | `		if( keep ){` |
|         - | 7580 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7581 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7582 | `		}` |
|         - | 7583 | `		/* Point to the next entry */` |
|        59 | 7584 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7585 | `	}` |
|        15 | 7586 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7587 | `	return PH7_OK;` |
|        17 | 7588 | `}` |
|         - | 7589 | `/*` |
|         - | 7590 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7591 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7592 | ` * Parameters` |
|         - | 7593 | ` *  $callback` |
|         - | 7594 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7595 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7596 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7597 | ` *   are zipped together.` |
|         - | 7598 | ` *  $array` |
|         - | 7599 | ` *   The first array to run through the callback function.` |
|         - | 7600 | ` *  $arrays` |
|         - | 7601 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7602 | ` * Return` |
|         - | 7603 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7604 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7605 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7606 | ` *  padding shorter arrays with NULL.` |
|         - | 7607 | ` */` |
|        62 | 7608 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7609 | `{` |
|         - | 7610 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7611 | `	ph7_hashmap_node *pEntry;` |
|         - | 7612 | `	ph7_hashmap *pMap;` |
|         - | 7613 | `	ph7_vm *pVm;` |
|         - | 7614 | `	int bNullCallback;` |
|         - | 7615 | `	sxi32 rc;` |
|         - | 7616 | `	int i;` |
|         - | 7617 | `	sxu32 n;` |
|        66 | 7618 | `	if( nArg < 2 ){` |
|       ! 0 | 7619 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7620 | `			"ArgumentCountError",` |
|         - | 7621 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7622 | `			nArg` |
|         - | 7623 | `			);` |
|         - | 7624 | `	}` |
|        66 | 7625 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        66 | 7626 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         8 | 7627 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         6 | 7628 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         8 | 7629 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7630 | `				"TypeError",` |
|         - | 7631 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7632 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7633 | `				zFunc` |
|         - | 7634 | `				);` |
|         - | 7635 | `		}` |
|         3 | 7636 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7637 | `			"TypeError",` |
|         - | 7638 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7639 | `			"no array or string given"` |
|         - | 7640 | `			);` |
|         - | 7641 | `	}` |
|         - | 7642 | `	/* Every remaining argument must be an array */` |
|       125 | 7643 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        71 | 7644 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7645 | `			if( i == 1 ){` |
|         4 | 7646 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7647 | `					"TypeError",` |
|         - | 7648 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7649 | `					ph7_type_name(apArg[1])` |
|         - | 7650 | `					);` |
|         - | 7651 | `			}` |
|       ! 0 | 7652 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7653 | `				"TypeError",` |
|         - | 7654 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7655 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7656 | `				);` |
|         - | 7657 | `		}` |
|        36 | 7658 | `	}` |
|        57 | 7659 | `	pVm = pCtx->pVm;` |
|         - | 7660 | `	/* Create a new array */` |
|        57 | 7661 | `	pArray = ph7_context_new_array(pCtx);` |
|        57 | 7662 | `	if( pArray == 0 ){` |
|       ! 0 | 7663 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7664 | `		return PH7_OK;` |
|         - | 7665 | `	}` |
|        57 | 7666 | `	PH7_MemObjInit(pVm,&sResult);` |
|        57 | 7667 | `	PH7_MemObjInit(pVm,&sKey);` |
|        57 | 7668 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7669 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7670 | `	if( nArg == 2 ){` |
|         - | 7671 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        47 | 7672 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        47 | 7673 | `		pEntry = pMap->pFirst;` |
|       143 | 7674 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7675 | `			/* Extract the node value */` |
|       103 | 7676 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|       103 | 7677 | `			if( pValue ){` |
|         - | 7678 | `				/* Extract the node key */` |
|       103 | 7679 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       103 | 7680 | `				if( bNullCallback ){` |
|         - | 7681 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7682 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7683 | `				}else{` |
|         - | 7684 | `					/* Invoke the supplied callback */` |
|        93 | 7685 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        93 | 7686 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7687 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7688 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7689 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7690 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7691 | `						return PH7_EXCEPTION;` |
|         - | 7692 | `					}` |
|         - | 7693 | `					/* Insert the callback return value */` |
|        89 | 7694 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7695 | `				}` |
|        99 | 7696 | `				PH7_MemObjRelease(&sKey);` |
|        99 | 7697 | `				PH7_MemObjRelease(&sResult);` |
|        48 | 7698 | `			}` |
|         - | 7699 | `			/* Point to the next entry */` |
|        99 | 7700 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        51 | 7701 | `		}` |
|        23 | 7702 | `	}else{` |
|         - | 7703 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7704 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7705 | `		int nArrays = nArg - 1;` |
|         - | 7706 | `		ph7_hashmap_node **apCur;` |
|         - | 7707 | `		ph7_value **apCallArg;` |
|         - | 7708 | `		ph7_value sNull;` |
|        11 | 7709 | `		sxu32 nMax = 0;` |
|        11 | 7710 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7711 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7712 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7713 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7714 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7715 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7716 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7717 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7718 | `			return PH7_OK;` |
|         - | 7719 | `		}` |
|        11 | 7720 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7721 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7722 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7723 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7724 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7725 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7726 | `				nMax = pMap->nEntry;` |
|         6 | 7727 | `			}` |
|        12 | 7728 | `		}` |
|        35 | 7729 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7730 | `			ph7_value *pZip = 0;` |
|        25 | 7731 | `			if( bNullCallback ){` |
|         - | 7732 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7733 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7734 | `			}` |
|        79 | 7735 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7736 | `				ph7_value *pv = &sNull;` |
|        55 | 7737 | `				if( apCur[i] ){` |
|        53 | 7738 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7739 | `					if( pNodeVal ){` |
|        53 | 7740 | `						pv = pNodeVal;` |
|        26 | 7741 | `					}` |
|        53 | 7742 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7743 | `				}` |
|        55 | 7744 | `				if( bNullCallback ){` |
|         9 | 7745 | `					if( pZip ){` |
|         9 | 7746 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7747 | `					}` |
|         5 | 7748 | `				}else{` |
|        47 | 7749 | `					apCallArg[i] = pv;` |
|         - | 7750 | `				}` |
|        28 | 7751 | `			}` |
|        25 | 7752 | `			if( bNullCallback ){` |
|         5 | 7753 | `				if( pZip ){` |
|         5 | 7754 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7755 | `				}` |
|         3 | 7756 | `			}else{` |
|        21 | 7757 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7758 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7759 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7760 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7761 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7762 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7763 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7764 | `					return PH7_EXCEPTION;` |
|         - | 7765 | `				}` |
|        21 | 7766 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7767 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7768 | `			}` |
|        13 | 7769 | `		}` |
|        11 | 7770 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7771 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7772 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7773 | `	}` |
|        53 | 7774 | `	PH7_MemObjRelease(&sKey);` |
|        53 | 7775 | `	PH7_MemObjRelease(&sResult);` |
|        53 | 7776 | `	ph7_result_value(pCtx,pArray);` |
|        53 | 7777 | `	return PH7_OK;` |
|        35 | 7778 | `}` |
|         - | 7779 | `/*` |
|         - | 7780 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7781 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7782 | ` * Parameters` |
|         - | 7783 | ` *  $array` |
|         - | 7784 | ` *   The input array.` |
|         - | 7785 | ` *  $callback` |
|         - | 7786 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7787 | ` *  $initial` |
|         - | 7788 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7789 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7790 | ` * Return` |
|         - | 7791 | ` *  Returns the resulting value.` |
|         - | 7792 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7793 | ` */` |
|        30 | 7794 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7795 | `{` |
|         - | 7796 | `	ph7_hashmap_node *pEntry;` |
|         - | 7797 | `	ph7_hashmap *pMap;` |
|         - | 7798 | `	ph7_value *pValue;` |
|         - | 7799 | `	ph7_value sResult;` |
|         - | 7800 | `	sxi32 rc;` |
|         - | 7801 | `	sxu32 n;` |
|        35 | 7802 | `	if( nArg < 2 ){` |
|       ! 0 | 7803 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7804 | `			"ArgumentCountError",` |
|         - | 7805 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7806 | `			nArg` |
|         - | 7807 | `			);` |
|         - | 7808 | `	}` |
|        35 | 7809 | `	if( nArg > 3 ){` |
|         4 | 7810 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7811 | `			"ArgumentCountError",` |
|         - | 7812 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7813 | `			nArg` |
|         - | 7814 | `			);` |
|         - | 7815 | `	}` |
|        33 | 7816 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7817 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7818 | `			"TypeError",` |
|         - | 7819 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7820 | `			ph7_type_name(apArg[0])` |
|         - | 7821 | `			);` |
|         - | 7822 | `	}` |
|        31 | 7823 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7824 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7825 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7826 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7827 | `				"TypeError",` |
|         - | 7828 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7829 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7830 | `				zFunc` |
|         - | 7831 | `				);` |
|         - | 7832 | `		}` |
|         9 | 7833 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7834 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7835 | `				"TypeError",` |
|         - | 7836 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7837 | `				"array callback must have exactly two members"` |
|         - | 7838 | `				);` |
|         - | 7839 | `		}` |
|         6 | 7840 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7841 | `			"TypeError",` |
|         - | 7842 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7843 | `			"no array or string given"` |
|         - | 7844 | `			);` |
|         - | 7845 | `	}` |
|         - | 7846 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7847 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7848 | `	/* Assume a NULL initial value */` |
|        19 | 7849 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7850 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7851 | `	if( nArg > 2 ){` |
|         - | 7852 | `		/* Set the initial value */` |
|        13 | 7853 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7854 | `	}` |
|         - | 7855 | `	/* Perform the requested operation */` |
|        19 | 7856 | `	pEntry = pMap->pFirst;` |
|        55 | 7857 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7858 | `		/* Extract the node value */` |
|        39 | 7859 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7860 | `		/* Invoke the supplied callback */` |
|        39 | 7861 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7862 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7863 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7864 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7865 | `			return PH7_EXCEPTION;` |
|         - | 7866 | `		}` |
|         - | 7867 | `		/* Point to the next entry */` |
|        37 | 7868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7869 | `	}` |
|        17 | 7870 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7871 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7872 | `	return PH7_OK;` |
|        20 | 7873 | `}` |
|         - | 7874 | `/*` |
|         - | 7875 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7876 | ` *  Apply a user function to every member of an array.` |
|         - | 7877 | ` * Parameters` |
|         - | 7878 | ` *  $array` |
|         - | 7879 | ` *   The input array.` |
|         - | 7880 | ` *  $funcname` |
|         - | 7881 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7882 | ` *   the first, and the key/index second.` |
|         - | 7883 | ` * Note:` |
|         - | 7884 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7885 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7886 | ` *  be made in the original array itself.` |
|         - | 7887 | ` *  $userdata` |
|         - | 7888 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7889 | ` *   to the callback funcname.` |
|         - | 7890 | ` * Return` |
|         - | 7891 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7892 | ` */` |
|        36 | 7893 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7894 | `{` |
|         - | 7895 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7896 | `	ph7_hashmap_node *pEntry;` |
|         - | 7897 | `	ph7_hashmap *pMap;` |
|         - | 7898 | `	sxu32 n;` |
|        41 | 7899 | `	if( nArg < 2 ){` |
|       ! 0 | 7900 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7901 | `			"ArgumentCountError",` |
|         - | 7902 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7903 | `			nArg` |
|         - | 7904 | `			);` |
|         - | 7905 | `	}` |
|        41 | 7906 | `	if( nArg > 3 ){` |
|         4 | 7907 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7908 | `			"ArgumentCountError",` |
|         - | 7909 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7910 | `			nArg` |
|         - | 7911 | `			);` |
|         - | 7912 | `	}` |
|        39 | 7913 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7914 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7915 | `			"TypeError",` |
|         - | 7916 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7917 | `			ph7_type_name(apArg[0])` |
|         - | 7918 | `			);` |
|         - | 7919 | `	}` |
|        37 | 7920 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        17 | 7921 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         6 | 7922 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         8 | 7923 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7924 | `				"TypeError",` |
|         - | 7925 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7926 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7927 | `				zFunc` |
|         - | 7928 | `				);` |
|         - | 7929 | `		}` |
|        12 | 7930 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7931 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7932 | `				"TypeError",` |
|         - | 7933 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7934 | `				"array callback must have exactly two members"` |
|         - | 7935 | `				);` |
|         - | 7936 | `		}` |
|         6 | 7937 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7938 | `			"TypeError",` |
|         - | 7939 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7940 | `			"no array or string given"` |
|         - | 7941 | `			);` |
|         - | 7942 | `	}` |
|        21 | 7943 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7944 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7945 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7946 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7947 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7948 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7949 | `	/* Perform the desired operation */` |
|        21 | 7950 | `	pEntry = pMap->pFirst;` |
|        61 | 7951 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7952 | `		/* Extract the node value */` |
|        43 | 7953 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7954 | `		if( pValue ){` |
|         - | 7955 | `			sxi32 rcW;` |
|         - | 7956 | `			/* Extract the entry key */` |
|        43 | 7957 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7958 | `			/* Invoke the supplied callback */` |
|        43 | 7959 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7960 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7961 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7962 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7963 | `				return PH7_EXCEPTION;` |
|         - | 7964 | `			}` |
|        20 | 7965 | `		}` |
|         - | 7966 | `		/* Point to the next entry */` |
|        41 | 7967 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7968 | `	}` |
|         - | 7969 | `	/* All done, return TRUE */` |
|        19 | 7970 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7971 | `	return PH7_OK;` |
|        23 | 7972 | `}` |
|         - | 7973 | `/*` |
|         - | 7974 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7975 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7976 | ` */` |
|        22 | 7977 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7978 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7979 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7980 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7981 | `	int iNest             /* Nesting level */` |
|         - | 7982 | `	)` |
|         1 | 7983 | `{` |
|         - | 7984 | `	ph7_hashmap_node *pEntry;` |
|         - | 7985 | `	ph7_value *pValue,sKey;` |
|         - | 7986 | `	sxi32 rc;` |
|         - | 7987 | `	sxu32 n;` |
|         - | 7988 | `	/* Iterate through hashmap entries */` |
|        23 | 7989 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7990 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7991 | `	pEntry = pMap->pFirst;` |
|        59 | 7992 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7993 | `		/* Extract the node value */` |
|        37 | 7994 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7995 | `		if( pValue ){` |
|        37 | 7996 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7997 | `				if( iNest < 32 ){` |
|         - | 7998 | `					/* Recurse */` |
|        11 | 7999 | `					iNest++;` |
|        11 | 8000 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8001 | `					iNest--;` |
|        11 | 8002 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8003 | `						return PH7_EXCEPTION;` |
|         - | 8004 | `					}` |
|         5 | 8005 | `				}` |
|         6 | 8006 | `			}else{` |
|         - | 8007 | `				/* Extract the node key */` |
|        27 | 8008 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8009 | `				/* Invoke the supplied callback */` |
|        27 | 8010 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8011 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8012 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8013 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8014 | `					return PH7_EXCEPTION;` |
|         - | 8015 | `				}` |
|         - | 8016 | `			}` |
|        18 | 8017 | `		}` |
|         - | 8018 | `		/* Point to the next entry */` |
|        37 | 8019 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8020 | `	}` |
|        23 | 8021 | `	return PH7_OK;` |
|        12 | 8022 | `}` |
|         - | 8023 | `/*` |
|         - | 8024 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8025 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8026 | ` * Parameters` |
|         - | 8027 | ` *  $array` |
|         - | 8028 | ` *   The input array.` |
|         - | 8029 | ` *  $funcname` |
|         - | 8030 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8031 | ` *   the first, and the key/index second.` |
|         - | 8032 | ` * Note:` |
|         - | 8033 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8034 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8035 | ` *  be made in the original array itself.` |
|         - | 8036 | ` *  $userdata` |
|         - | 8037 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8038 | ` *   to the callback funcname.` |
|         - | 8039 | ` * Return` |
|         - | 8040 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8041 | ` */` |
|        26 | 8042 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8043 | `{` |
|         - | 8044 | `	ph7_hashmap *pMap;` |
|        31 | 8045 | `	if( nArg < 2 ){` |
|       ! 0 | 8046 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8047 | `			"ArgumentCountError",` |
|         - | 8048 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8049 | `			nArg` |
|         - | 8050 | `			);` |
|         - | 8051 | `	}` |
|        31 | 8052 | `	if( nArg > 3 ){` |
|         4 | 8053 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8054 | `			"ArgumentCountError",` |
|         - | 8055 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8056 | `			nArg` |
|         - | 8057 | `			);` |
|         - | 8058 | `	}` |
|        29 | 8059 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8060 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8061 | `			"TypeError",` |
|         - | 8062 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8063 | `			ph7_type_name(apArg[0])` |
|         - | 8064 | `			);` |
|         - | 8065 | `	}` |
|        27 | 8066 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8067 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8068 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8069 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8070 | `				"TypeError",` |
|         - | 8071 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8072 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8073 | `				zFunc` |
|         - | 8074 | `				);` |
|         - | 8075 | `		}` |
|        12 | 8076 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8077 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8078 | `				"TypeError",` |
|         - | 8079 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8080 | `				"array callback must have exactly two members"` |
|         - | 8081 | `				);` |
|         - | 8082 | `		}` |
|         6 | 8083 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8084 | `			"TypeError",` |
|         - | 8085 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8086 | `			"no array or string given"` |
|         - | 8087 | `			);` |
|         - | 8088 | `	}` |
|         - | 8089 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8090 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8091 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8092 | `	/* Perform the desired operation */` |
|        13 | 8093 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8094 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8095 | `		return PH7_EXCEPTION;` |
|         - | 8096 | `	}` |
|         - | 8097 | `	/* All done, return TRUE */` |
|        13 | 8098 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8099 | `	return PH7_OK;` |
|        18 | 8100 | `}` |
|         - | 8101 | `/*` |
|         - | 8102 | ` * bool array_is_list(array $array)` |
|         - | 8103 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8104 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8105 | ` * Return` |
|         - | 8106 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8107 | ` */` |
|         - | 8108 | `/*` |
|         - | 8109 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8110 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8111 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8112 | ` */` |
|       294 | 8113 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8114 | `{` |
|       295 | 8115 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       295 | 8116 | `	sxi64 iExpect = 0;` |
|         - | 8117 | `	sxu32 n;` |
|       663 | 8118 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       497 | 8119 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8120 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       129 | 8121 | `			return 0;` |
|         - | 8122 | `		}` |
|       369 | 8123 | `		++iExpect;` |
|       369 | 8124 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       185 | 8125 | `	}` |
|       167 | 8126 | `	return 1;` |
|       148 | 8127 | `}` |
|        12 | 8128 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8129 | `{` |
|        13 | 8130 | `	if( nArg < 1 ){` |
|       ! 0 | 8131 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8132 | `			"ArgumentCountError",` |
|         - | 8133 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8134 | `			);` |
|         - | 8135 | `	}` |
|        13 | 8136 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8137 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8138 | `			"TypeError",` |
|         - | 8139 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8140 | `			ph7_type_name(apArg[0])` |
|         - | 8141 | `			);` |
|         - | 8142 | `	}` |
|        13 | 8143 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8144 | `	return PH7_OK;` |
|         7 | 8145 | `}` |
|         - | 8146 | `/*` |
|         - | 8147 | ` * mixed array_first(array $array)` |
|         - | 8148 | ` * mixed array_last(array $array)` |
|         - | 8149 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8150 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8151 | ` *  untouched (unlike reset()/end()).` |
|         - | 8152 | ` */` |
|        18 | 8153 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8154 | `{` |
|         - | 8155 | `	ph7_hashmap *pMap;` |
|         - | 8156 | `	ph7_hashmap_node *pNode;` |
|         - | 8157 | `	ph7_value *pVal;` |
|        19 | 8158 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8159 | `	if( nArg < 1 ){` |
|       ! 0 | 8160 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8161 | `			"ArgumentCountError",` |
|         - | 8162 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8163 | `			zName` |
|         - | 8164 | `			);` |
|         - | 8165 | `	}` |
|        19 | 8166 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8167 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8168 | `			"TypeError",` |
|         - | 8169 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8170 | `			zName,` |
|         1 | 8171 | `			ph7_type_name(apArg[0])` |
|         - | 8172 | `			);` |
|         - | 8173 | `	}` |
|        17 | 8174 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8175 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8176 | `	if( pNode == 0 ){` |
|         - | 8177 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8178 | `		ph7_result_null(pCtx);` |
|         5 | 8179 | `		return PH7_OK;` |
|         - | 8180 | `	}` |
|        13 | 8181 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8182 | `	if( pVal ){` |
|        13 | 8183 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8184 | `	}else{` |
|       ! 0 | 8185 | `		ph7_result_null(pCtx);` |
|         - | 8186 | `	}` |
|        13 | 8187 | `	return PH7_OK;` |
|        10 | 8188 | `}` |
|         8 | 8189 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8190 | `{` |
|         9 | 8191 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8192 | `}` |
|        10 | 8193 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8194 | `{` |
|        11 | 8195 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8196 | `}` |
|         - | 8197 | `/*` |
|         - | 8198 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8199 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8200 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8201 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8202 | ` *  untouched.` |
|         - | 8203 | ` */` |
|        22 | 8204 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8205 | `{` |
|         - | 8206 | `	ph7_hashmap *pMap;` |
|         - | 8207 | `	ph7_hashmap_node *pNode;` |
|        23 | 8208 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8209 | `	if( nArg < 1 ){` |
|       ! 0 | 8210 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8211 | `			"ArgumentCountError",` |
|         - | 8212 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8213 | `			zName` |
|         - | 8214 | `			);` |
|         - | 8215 | `	}` |
|        23 | 8216 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8217 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8218 | `			"TypeError",` |
|         - | 8219 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8220 | `			zName,` |
|         1 | 8221 | `			ph7_type_name(apArg[0])` |
|         - | 8222 | `			);` |
|         - | 8223 | `	}` |
|        21 | 8224 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8225 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8226 | `	if( pNode == 0 ){` |
|         - | 8227 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8228 | `		ph7_result_null(pCtx);` |
|         5 | 8229 | `		return PH7_OK;` |
|         - | 8230 | `	}` |
|        17 | 8231 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8232 | `	return PH7_OK;` |
|        12 | 8233 | `}` |
|        10 | 8234 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8235 | `{` |
|        11 | 8236 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8237 | `}` |
|        12 | 8238 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8239 | `{` |
|        13 | 8240 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8241 | `}` |
|         - | 8242 | `/*` |
|         - | 8243 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8244 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8245 | ` * array_column() for both the column value and the index key.` |
|         - | 8246 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8247 | ` * container or the key is absent.` |
|         - | 8248 | ` */` |
|        32 | 8249 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8250 | `{` |
|        33 | 8251 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8252 | `		ph7_hashmap_node *pNode;` |
|        25 | 8253 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8254 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8255 | `		}` |
|        11 | 8256 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8257 | `		ph7_value sName;` |
|         - | 8258 | `		const char *zName;` |
|         - | 8259 | `		ph7_value *pAttr;` |
|         - | 8260 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8261 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8262 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8263 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8264 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8265 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8266 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8267 | `		return pAttr;` |
|         - | 8268 | `	}` |
|         5 | 8269 | `	return 0;` |
|        17 | 8270 | `}` |
|         - | 8271 | `/*` |
|         - | 8272 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8273 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8274 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8275 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8276 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8277 | ` *  Each row may be an array or an object.` |
|         - | 8278 | ` */` |
|        12 | 8279 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8280 | `{` |
|         - | 8281 | `	ph7_hashmap_node *pNode;` |
|         - | 8282 | `	ph7_hashmap *pMap;` |
|         - | 8283 | `	ph7_value *pArray;` |
|         - | 8284 | `	ph7_value *pRow;` |
|         - | 8285 | `	ph7_value *pCol;` |
|         - | 8286 | `	ph7_value *pIdx;` |
|         - | 8287 | `	int bWantCol;` |
|         - | 8288 | `	int bWantIdx;` |
|         - | 8289 | `	sxu32 n;` |
|        13 | 8290 | `	if( nArg < 2 ){` |
|       ! 0 | 8291 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8292 | `			"ArgumentCountError",` |
|         - | 8293 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8294 | `			nArg` |
|         - | 8295 | `			);` |
|         - | 8296 | `	}` |
|        13 | 8297 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8298 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8299 | `			"TypeError",` |
|         - | 8300 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8301 | `			ph7_type_name(apArg[0])` |
|         - | 8302 | `			);` |
|         - | 8303 | `	}` |
|        13 | 8304 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8305 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8306 | `	if( pArray == 0 ){` |
|       ! 0 | 8307 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8308 | `		return PH7_OK;` |
|         - | 8309 | `	}` |
|         - | 8310 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8311 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8312 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8313 | `	pNode = pMap->pFirst;` |
|        33 | 8314 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8315 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8316 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8317 | `		if( pRow == 0 ){` |
|       ! 0 | 8318 | `			continue;` |
|         - | 8319 | `		}` |
|        21 | 8320 | `		if( bWantCol ){` |
|        19 | 8321 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8322 | `			if( pCol == 0 ){` |
|         - | 8323 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8324 | `				continue;` |
|         - | 8325 | `			}` |
|         9 | 8326 | `		}else{` |
|         3 | 8327 | `			pCol = pRow;` |
|         - | 8328 | `		}` |
|        19 | 8329 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8330 | `		if( pIdx ){` |
|        13 | 8331 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8332 | `		}else{` |
|         7 | 8333 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8334 | `		}` |
|        10 | 8335 | `	}` |
|        13 | 8336 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8337 | `	return PH7_OK;` |
|         7 | 8338 | `}` |
|         - | 8339 | `/*` |
|         - | 8340 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8341 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8342 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8343 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8344 | ` */` |
|        28 | 8345 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8346 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8347 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8348 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8349 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8350 | `	)` |
|         1 | 8351 | `{` |
|         - | 8352 | `	ph7_hashmap_node *pEntry;` |
|         - | 8353 | `	ph7_hashmap *pMap;` |
|         - | 8354 | `	ph7_value *pValue;` |
|         - | 8355 | `	ph7_value *apCbArg[2];` |
|         - | 8356 | `	ph7_value sKey;` |
|         - | 8357 | `	ph7_value sResult;` |
|         - | 8358 | `	sxi32 rc;` |
|         - | 8359 | `	sxu32 n;` |
|        29 | 8360 | `	*ppMatch = 0;` |
|        29 | 8361 | `	if( nArg < 2 ){` |
|       ! 0 | 8362 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8363 | `			"ArgumentCountError",` |
|         - | 8364 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8365 | `			zName,nArg` |
|         - | 8366 | `			);` |
|         - | 8367 | `	}` |
|        29 | 8368 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8369 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8370 | `			"TypeError",` |
|         - | 8371 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8372 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8373 | `			);` |
|         - | 8374 | `	}` |
|        29 | 8375 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8376 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8377 | `			"TypeError",` |
|         - | 8378 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8379 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8380 | `			);` |
|         - | 8381 | `	}` |
|        29 | 8382 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8383 | `	pEntry = pMap->pFirst;` |
|        29 | 8384 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8385 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8386 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8387 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8388 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8389 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8390 | `		if( pValue ){` |
|         - | 8391 | `			/* The callback receives ($value, $key). */` |
|        59 | 8392 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8393 | `			apCbArg[0] = pValue;` |
|        59 | 8394 | `			apCbArg[1] = &sKey;` |
|        59 | 8395 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8396 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8397 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8398 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8399 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8400 | `				return PH7_EXCEPTION;` |
|         - | 8401 | `			}` |
|        59 | 8402 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8403 | `				*ppMatch = pEntry;` |
|        15 | 8404 | `				break;` |
|         - | 8405 | `			}` |
|        22 | 8406 | `		}` |
|        45 | 8407 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8408 | `	}` |
|        29 | 8409 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8410 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8411 | `	return PH7_OK;` |
|        15 | 8412 | `}` |
|         - | 8413 | `/*` |
|         - | 8414 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8415 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8416 | ` *  is truthy, or NULL if none match.` |
|         - | 8417 | ` */` |
|         6 | 8418 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8419 | `{` |
|         - | 8420 | `	ph7_hashmap_node *pMatch;` |
|         - | 8421 | `	ph7_value *pVal;` |
|         - | 8422 | `	sxi32 rc;` |
|         7 | 8423 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8424 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8425 | `		return rc;` |
|         - | 8426 | `	}` |
|         7 | 8427 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8428 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8429 | `	}else{` |
|         3 | 8430 | `		ph7_result_null(pCtx);` |
|         - | 8431 | `	}` |
|         7 | 8432 | `	return PH7_OK;` |
|         4 | 8433 | `}` |
|         - | 8434 | `/*` |
|         - | 8435 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8436 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8437 | ` *  is truthy, or NULL if none match.` |
|         - | 8438 | ` */` |
|         6 | 8439 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8440 | `{` |
|         - | 8441 | `	ph7_hashmap_node *pMatch;` |
|         - | 8442 | `	sxi32 rc;` |
|         7 | 8443 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8444 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8445 | `		return rc;` |
|         - | 8446 | `	}` |
|         7 | 8447 | `	if( pMatch == 0 ){` |
|         3 | 8448 | `		ph7_result_null(pCtx);` |
|         6 | 8449 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8450 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8451 | `	}else{` |
|         4 | 8452 | `		ph7_result_string(pCtx,` |
|         2 | 8453 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8454 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8455 | `	}` |
|         7 | 8456 | `	return PH7_OK;` |
|         4 | 8457 | `}` |
|         - | 8458 | `/*` |
|         - | 8459 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8460 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8461 | ` *  FALSE for an empty array.` |
|         - | 8462 | ` */` |
|         8 | 8463 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8464 | `{` |
|         - | 8465 | `	ph7_hashmap_node *pMatch;` |
|         - | 8466 | `	sxi32 rc;` |
|         9 | 8467 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8468 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8469 | `		return rc;` |
|         - | 8470 | `	}` |
|         9 | 8471 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8472 | `	return PH7_OK;` |
|         5 | 8473 | `}` |
|         - | 8474 | `/*` |
|         - | 8475 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8476 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8477 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8478 | ` */` |
|         8 | 8479 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8480 | `{` |
|         - | 8481 | `	ph7_hashmap_node *pMatch;` |
|         - | 8482 | `	sxi32 rc;` |
|         9 | 8483 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8484 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8485 | `		return rc;` |
|         - | 8486 | `	}` |
|         9 | 8487 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8488 | `	return PH7_OK;` |
|         5 | 8489 | `}` |
|         - | 8490 | `/*` |
|         - | 8491 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8492 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8493 | ` */` |
|         - | 8494 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8495 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8496 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8497 | `{` |
|        84 | 8498 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8499 | `	(void)pVm;` |
|        84 | 8500 | `	p->nCount++;` |
|        84 | 8501 | `	if( p->pArray ){` |
|         - | 8502 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8503 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8504 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8505 | `	}` |
|        84 | 8506 | `	return SXRET_OK;` |
|         4 | 8507 | `}` |
|         - | 8508 | `/*` |
|         - | 8509 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8510 | ` */` |
|        30 | 8511 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8512 | `{` |
|         - | 8513 | `	struct IterCollect sCol;` |
|         - | 8514 | `	ph7_value *pArray;` |
|         - | 8515 | `	sxi32 rc;` |
|        34 | 8516 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8517 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8518 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8519 | `	sCol.pArray = pArray;` |
|        34 | 8520 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8521 | `	sCol.nCount = 0;` |
|        34 | 8522 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8523 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8524 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8525 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8526 | `		sxu32 n;` |
|         9 | 8527 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8528 | `			ph7_value sKey, *pVal;` |
|         7 | 8529 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8530 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8531 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8532 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8533 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8534 | `			pEntry = pEntry->pPrev;` |
|         4 | 8535 | `		}` |
|         3 | 8536 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8537 | `		return PH7_OK;` |
|         - | 8538 | `	}` |
|        32 | 8539 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8540 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8541 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8542 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8543 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8544 | `			ph7_type_name(apArg[0]));` |
|         - | 8545 | `	}` |
|        30 | 8546 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8547 | `	return PH7_OK;` |
|        19 | 8548 | `}` |
|         - | 8549 | `/*` |
|         - | 8550 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8551 | ` */` |
|         8 | 8552 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8553 | `{` |
|         - | 8554 | `	struct IterCollect sCol;` |
|         - | 8555 | `	sxi32 rc;` |
|         9 | 8556 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8557 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8558 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8559 | `		return PH7_OK;` |
|         - | 8560 | `	}` |
|         7 | 8561 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8562 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8563 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8564 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8565 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8566 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8567 | `			ph7_type_name(apArg[0]));` |
|         - | 8568 | `	}` |
|         7 | 8569 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8570 | `	return PH7_OK;` |
|         5 | 8571 | `}` |
|         - | 8572 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8573 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8574 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8575 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8576 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8577 | `{` |
|        33 | 8578 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8579 | `	ph7_value sResult;` |
|         - | 8580 | `	SySet aArg;` |
|         - | 8581 | `	sxi32 rc;` |
|         - | 8582 | `	int bContinue;` |
|        16 | 8583 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8584 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8585 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8586 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8587 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8588 | `		sxu32 n;` |
|        17 | 8589 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8590 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8591 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8592 | `			pEntry = pEntry->pPrev;` |
|         5 | 8593 | `		}` |
|         4 | 8594 | `	}` |
|        33 | 8595 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8596 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8597 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8598 | `	SySetRelease(&aArg);` |
|        33 | 8599 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8600 | `	p->nCount++;` |
|        31 | 8601 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8602 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8603 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8604 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8605 | `}` |
|         - | 8606 | `/*` |
|         - | 8607 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8608 | ` */` |
|        12 | 8609 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8610 | `{` |
|         - | 8611 | `	struct IterApply sApp;` |
|         - | 8612 | `	sxi32 rc;` |
|        13 | 8613 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8614 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8615 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8616 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8617 | `	}` |
|        13 | 8618 | `	sApp.pCallback = apArg[1];` |
|        13 | 8619 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8620 | `	sApp.nCount = 0;` |
|        13 | 8621 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8622 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8623 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8624 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8625 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8626 | `			ph7_type_name(apArg[0]));` |
|         - | 8627 | `	}` |
|        11 | 8628 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8629 | `	return PH7_OK;` |
|         7 | 8630 | `}` |
|         - | 8631 | `/*` |
|         - | 8632 | ` * Table of hashmap functions.` |
|         - | 8633 | ` */` |
|         - | 8634 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8635 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8636 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8637 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8638 | `	{"count",             ph7_hashmap_count },` |
|         - | 8639 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8640 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8641 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8642 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8643 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8644 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8645 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8646 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8647 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8648 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8649 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8650 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8651 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8652 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8653 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8654 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8655 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8656 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8657 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8658 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8659 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8660 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8661 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8662 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8663 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8664 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8665 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8666 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8667 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8668 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8669 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8670 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8671 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8672 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8673 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8674 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8675 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8676 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8677 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8678 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8679 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8680 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8681 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8682 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8683 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8684 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8685 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8686 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8687 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8688 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8689 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8690 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8691 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8692 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8693 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8694 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8695 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8696 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8697 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8698 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8699 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8700 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8701 | `	{"current",           ph7_hashmap_current },` |
|         - | 8702 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8703 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8704 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8705 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8706 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8707 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8708 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8709 | `};` |
|         - | 8710 | `/*` |
|         - | 8711 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8712 | ` */` |
|      3356 | 8713 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8714 | `{` |
|         - | 8715 | `	sxu32 n;` |
|    251705 | 8716 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    248349 | 8717 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    124177 | 8718 | `	}` |
|      3361 | 8719 | `}` |
|         - | 8720 | `/*` |
|         - | 8721 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8722 | ` * the BLOB given as the first argument.` |
|         - | 8723 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8724 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8725 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8726 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8727 | ` */` |
|         - | 8728 | `/*` |
|         - | 8729 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8730 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8731 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8732 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8733 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8734 | ` */` |
|       120 | 8735 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8736 | `{` |
|       122 | 8737 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8738 | `	ph7_value *pObj;` |
|       122 | 8739 | `	sxu32 n = 0;` |
|         - | 8740 | `	int isRef;` |
|       122 | 8741 | `	sxi32 rc = SXRET_OK;` |
|         - | 8742 | `	int i;` |
|       195 | 8743 | `	for(;;){` |
|       392 | 8744 | `		if( n >= pMap->nEntry ){` |
|       122 | 8745 | `			break;` |
|         - | 8746 | `		}` |
|       272 | 8747 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8748 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8749 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       540 | 8750 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       270 | 8751 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       272 | 8752 | `		if( ShowType ){` |
|         - | 8753 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8754 | `			 * on the next line at the same indent (php). */` |
|       104 | 8755 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        70 | 8756 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        36 | 8757 | `			}` |
|        36 | 8758 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8759 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8760 | `			}else{` |
|        20 | 8761 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8762 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8763 | `			}` |
|        36 | 8764 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        36 | 8765 | `			if( pObj ){` |
|        36 | 8766 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        36 | 8767 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8768 | `					break;` |
|         - | 8769 | `				}` |
|        17 | 8770 | `			}` |
|        19 | 8771 | `		}else{` |
|         - | 8772 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8773 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8774 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8775 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8776 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8777 | `			}` |
|       238 | 8778 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8779 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8780 | `			}else{` |
|       170 | 8781 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8782 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8783 | `			}` |
|       236 | 8784 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8785 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8786 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8787 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8788 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8789 | `					break;` |
|         - | 8790 | `				}` |
|        13 | 8791 | `			}else{` |
|       214 | 8792 | `				if( pObj ){` |
|       214 | 8793 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8794 | `				}` |
|       214 | 8795 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8796 | `			}` |
|         - | 8797 | `		}` |
|         - | 8798 | `		/* Point to the next entry */` |
|       272 | 8799 | `		n++;` |
|       272 | 8800 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8801 | `	}` |
|       122 | 8802 | `	return rc;` |
|         2 | 8803 | `}` |
|       116 | 8804 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8805 | `{` |
|         - | 8806 | `	sxi32 rc;` |
|         - | 8807 | `	int i;` |
|       118 | 8808 | `	if( nDepth > 31 ){` |
|         - | 8809 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8810 | `		/* Nesting limit reached */` |
|       ! 0 | 8811 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8812 | `		return SXERR_LIMIT;` |
|         - | 8813 | `	}` |
|       118 | 8814 | `	if( ShowType ){` |
|         - | 8815 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8816 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8817 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8818 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8819 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8820 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8821 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8822 | `		}` |
|        14 | 8823 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8824 | `		return rc;` |
|         - | 8825 | `	}` |
|         - | 8826 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8827 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8828 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8829 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8830 | `	}` |
|       105 | 8831 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8832 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8833 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8834 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8835 | `	}` |
|       105 | 8836 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8837 | `	return rc;` |
|        60 | 8838 | `}` |
|         - | 8839 | `/*` |
|         - | 8840 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8841 | ` * retrieved entry.` |
|         - | 8842 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8843 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8844 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8845 | ` * a value different from PH7_OK.` |
|         - | 8846 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8847 | ` */` |
|     34108 | 8848 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8849 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8850 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8851 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8852 | `	)` |
|         5 | 8853 | `{` |
|         - | 8854 | `	ph7_hashmap_node *pEntry;` |
|         - | 8855 | `	ph7_value sKey,sValue;` |
|         - | 8856 | `	sxi32 rc;` |
|         - | 8857 | `	sxu32 n;` |
|         - | 8858 | `	/* Initialize walker parameter */` |
|     34113 | 8859 | `	rc = SXRET_OK;` |
|     34113 | 8860 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34113 | 8861 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34113 | 8862 | `	n = pMap->nEntry;` |
|     34113 | 8863 | `	pEntry = pMap->pFirst;` |
|         - | 8864 | `	/* Start the iteration process */` |
|     93173 | 8865 | `	for(;;){` |
|    186351 | 8866 | `		if( n < 1 ){` |
|     34113 | 8867 | `			break;` |
|         - | 8868 | `		}` |
|         - | 8869 | `		/* Extract a copy of the key and a copy the current value */` |
|    152243 | 8870 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    152243 | 8871 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8872 | `		/* Invoke the user callback */` |
|    152243 | 8873 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8874 | `		/* Release the copy of the key and the value */` |
|    152243 | 8875 | `		PH7_MemObjRelease(&sKey);` |
|    152243 | 8876 | `		PH7_MemObjRelease(&sValue);` |
|    152243 | 8877 | `		if( rc != PH7_OK ){` |
|         - | 8878 | `			/* Callback request an operation abort */` |
|       ! 0 | 8879 | `			return SXERR_ABORT;` |
|         - | 8880 | `		}` |
|         - | 8881 | `		/* Point to the next entry */` |
|    152243 | 8882 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    152243 | 8883 | `		n--;` |
|         5 | 8884 | `	}` |
|         - | 8885 | `	/* All done */` |
|     34113 | 8886 | `	return SXRET_OK;` |
|     17059 | 8887 | `}` |
|         - | 8888 |  |
