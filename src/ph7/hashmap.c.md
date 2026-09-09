# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3979/4459 lines (89.24%)

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
|   7492084 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7492089 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7492089 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    634178 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    634183 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    634183 |   35 | `	sxu32 nH = 5381;` |
|    634183 |   36 | `	zEnd = &zIn[nLen];` |
|    717137 |   37 | `	for(;;){` |
|   1434279 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1227663 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1099731 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    957763 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    634183 |   43 | `	return nH;` |
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
|   3188360 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3188365 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3188365 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3188365 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3188365 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3188365 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3188365 |  112 | `	pNode->nHash = nHash;` |
|   3188365 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3188365 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3188365 |  115 | `	return pNode;` |
|   1594185 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    262848 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    262853 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    262853 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    262853 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    262853 |  133 | `	pNode->pMap  = &(*pMap);` |
|    262853 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    262853 |  135 | `	pNode->nHash = nHash;` |
|    262853 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    262853 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    262853 |  138 | `	pNode->nValIdx = nValIdx;` |
|    262853 |  139 | `	return pNode;` |
|    131429 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3451208 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3451213 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2976237 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2976237 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1488116 |  150 | `	}` |
|   3451213 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3451213 |  153 | `	if( pMap->pFirst == 0 ){` |
|     90127 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     90127 |  156 | `		pMap->pCur = pNode;` |
|     45066 |  157 | `	}else{` |
|   3361091 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3451213 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3451213 |  174 | `	++pMap->nEntry;` |
|   3451213 |  175 | `}` |
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
|   3451208 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3451213 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     95395 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     95395 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     95395 |  245 | `		if( nNew < 1 ){` |
|     90127 |  246 | `			nNew = 16;` |
|     45061 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     95395 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     95395 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     95395 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     95395 |  260 | `		pMap->apBucket = apNew;` |
|     95395 |  261 | `		pMap->nSize = nNew;` |
|     95395 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     90127 |  264 | `			return SXRET_OK;` |
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
|   3361091 |  289 | `	return SXRET_OK;` |
|   1725609 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3188360 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3188365 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3188323 |  310 | `		if( pValue ){` |
|   3188317 |  311 | `			sSafeVal = *pValue;` |
|   3188317 |  312 | `			pValue = &sSafeVal;` |
|   1594156 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3188323 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3188323 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3188323 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3188317 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1594156 |  322 | `		}` |
|   3188323 |  323 | `		nIdx = pObj->nIdx;` |
|   1594164 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3188365 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3188365 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3188365 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3188365 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3188365 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3188365 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3188365 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3188365 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3188365 |  349 | `	return SXRET_OK;` |
|   1594185 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    262848 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    262853 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    216907 |  370 | `		if( pValue ){` |
|    216597 |  371 | `			sSafeVal = *pValue;` |
|    216597 |  372 | `			pValue = &sSafeVal;` |
|    108296 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    216907 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    216907 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    216907 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    216597 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    108296 |  382 | `		}` |
|    216907 |  383 | `		nIdx = pObj->nIdx;` |
|    108456 |  384 | `	}else{` |
|     45951 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    262853 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    262853 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    262853 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    262853 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     45951 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     22973 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    262853 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    262853 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    262853 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    262853 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    262853 |  409 | `	return SXRET_OK;` |
|    131429 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4290350 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4290355 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       769 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4289591 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4289591 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110566777 |  433 | `	for(;;){` |
| 221133559 |  434 | `		if( pNode == 0 ){` |
|   4284373 |  435 | `			break;` |
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
|   4284373 |  450 | `	return SXERR_NOTFOUND;` |
|   2145180 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    405934 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    405939 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     34609 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    371335 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    371335 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    305650 |  475 | `	for(;;){` |
|    611305 |  476 | `		if( pNode == 0 ){` |
|    306661 |  477 | `			break;` |
|         - |  478 | `		}` |
|    304644 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    303133 |  480 | `			&& pNode->nHash == nHash` |
|    183198 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     64779 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     64679 |  484 | `				if( ppNode ){` |
|     64651 |  485 | `					*ppNode = pNode;` |
|     32323 |  486 | `				}` |
|     64679 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    239975 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    306661 |  493 | `	return SXERR_NOTFOUND;` |
|    202972 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    406066 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    406071 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    406071 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    406071 |  504 | `	int isNeg = FALSE, nDigit;` |
|    406071 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    406049 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    406045 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    406045 |  516 | `	zDigit = zIn;` |
|    203454 |  517 | `	for(;;){` |
|    406913 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    406663 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    405795 |  523 | `			return FALSE;` |
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
|    203038 |  541 | `}` |
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
|   2142572 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2142577 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       813 |  596 | `		pMap->bIntKeySeen = 1;` |
|       813 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       813 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|       813 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2141769 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141519 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2141519 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1070757 |  609 | `	}` |
|   1071291 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   1045414 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1045419 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1045413 |  623 | `	return FALSE;` |
|    522712 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3404818 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3404823 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3404823 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3404823 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    216909 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    216909 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       229 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    325019 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    108338 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|    216195 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    216065 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    216065 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1593957 |  683 | `IntKey:` |
|   3188147 |  684 | `	if( pKey ){` |
|   2142767 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       259 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       129 |  688 | `		}` |
|   2142767 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   2142573 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2142571 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2142571 |  712 | `		if( rc == SXRET_OK ){` |
|   2142571 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1071283 |  714 | `		}` |
|   1071288 |  715 | `	}else{` |
|   1045385 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1045383 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1045377 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1045377 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1045375 |  726 | `			++pMap->iNextIdx;` |
|    522685 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3187943 |  730 | `	return rc;` |
|   1702414 |  731 | `}` |
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
|   1478513 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1478518 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1478518 |  832 | `	return pObj;` |
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
|     72290 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     72295 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     72295 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     72295 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     72295 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     72295 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     72295 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     72295 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     72295 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     72295 |  921 | `	return rc;` |
|     36145 |  922 | `}` |
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
|     11374 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5696 |  934 | `	}else{` |
|      2774 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
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
|     11684 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5850 |  949 | `	}` |
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
|     79109 |  983 | `	for(;;){` |
|    158224 |  984 | `		if( n < 1 ){` |
|       115 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    158110 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    158110 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    158110 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    158110 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    158110 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    158110 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    158110 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    158110 | 1001 | `			if( rc == 0 ){` |
|     33073 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     33073 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     62518 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    125042 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    125042 | 1011 | `		n--;` |
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
|    236620 | 1463 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1464 | `{` |
|    236625 | 1465 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1466 | `	ph7_hashmap *pNew;` |
|         - | 1467 | `	ph7_value *pBacking;` |
|         - | 1468 | `	sxu32 nValIdx;` |
|         - | 1469 | `	int bValueInPool;` |
|    236625 | 1470 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    236625 | 1471 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1472 | `		/* Sole owner, no separation needed */` |
|    233887 | 1473 | `		return pMap;` |
|         - | 1474 | `	}` |
|      2743 | 1475 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1476 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1477 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1478 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1479 | `		return pMap;` |
|         - | 1480 | `	}` |
|         - | 1481 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1482 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1483 | `	 * frame is popped. */` |
|      2617 | 1484 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2617 | 1485 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2612 | 1486 | `		if( pBacking && pBacking != pValue` |
|      2587 | 1487 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2567 | 1488 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1489 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2567 | 1490 | `			pMap->iRef--;` |
|      2567 | 1491 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1492 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2521 | 1493 | `				pMap->iRef++;` |
|      2521 | 1494 | `				return pMap;` |
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
|    118315 | 1557 | `}` |
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
|    144390 | 1649 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1650 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1651 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1652 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1653 | `	)` |
|         5 | 1654 | `{` |
|         - | 1655 | `	ph7_hashmap *pMap;` |
|         - | 1656 | `	/* Allocate a new instance */` |
|    144395 | 1657 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    144395 | 1658 | `	if( pMap == 0 ){` |
|       ! 0 | 1659 | `		return 0;` |
|         - | 1660 | `	}` |
|         - | 1661 | `	/* Zero the structure */` |
|    144395 | 1662 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1663 | `	/* Fill in the structure */` |
|    144395 | 1664 | `	pMap->pVm = &(*pVm);` |
|    144395 | 1665 | `	pMap->iRef = 1;` |
|         - | 1666 | `	/* Default hash functions */` |
|    144395 | 1667 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    144395 | 1668 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    144395 | 1669 | `	return pMap;` |
|     72200 | 1670 | `}` |
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
|    102462 | 1762 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1763 | `{` |
|         - | 1764 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    102467 | 1765 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1766 | `	sxu32 n;` |
|    102467 | 1767 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1768 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1769 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1770 | `		return SXRET_OK;` |
|         - | 1771 | `	}` |
|    102467 | 1772 | `	if( pMap->pActiveSteps ){` |
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
|    102467 | 1785 | `	n = 0;` |
|    102467 | 1786 | `	pEntry = pMap->pFirst;` |
|   1734447 | 1787 | `	for(;;){` |
|   3468899 | 1788 | `		if( n >= pMap->nEntry ){` |
|    102467 | 1789 | `			break;` |
|         - | 1790 | `		}` |
|   3366437 | 1791 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1792 | `		/* Remove the reference from the foreign table */` |
|   3366437 | 1793 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3366437 | 1794 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1795 | `			/* Restore the ph7_value to the free list */` |
|   3366377 | 1796 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1683186 | 1797 | `		}` |
|         - | 1798 | `		/* Release the node */` |
|   3366437 | 1799 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    189387 | 1800 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     94691 | 1801 | `		}` |
|   3366437 | 1802 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1803 | `		/* Point to the next entry */` |
|   3366437 | 1804 | `		pEntry = pNext;` |
|   3366437 | 1805 | `		n++;` |
|         5 | 1806 | `	}` |
|    102467 | 1807 | `	if( pMap->nEntry > 0 ){` |
|         - | 1808 | `		/* Release the hash bucket */` |
|     75191 | 1809 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     37593 | 1810 | `	}` |
|    102467 | 1811 | `	if( FreeDS ){` |
|         - | 1812 | `		/* Free the whole instance */` |
|    102441 | 1813 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     51223 | 1814 | `	}else{` |
|         - | 1815 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1816 | `		pMap->apBucket = 0;` |
|        28 | 1817 | `		pMap->iNextIdx = 0;` |
|        28 | 1818 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1819 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1820 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1821 | `	}` |
|    102467 | 1822 | `	return SXRET_OK;` |
|     51236 | 1823 | `}` |
|         - | 1824 | `/*` |
|         - | 1825 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1826 | ` * If the count reaches zero which mean no more variables` |
|         - | 1827 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1828 | ` */` |
|    842574 | 1829 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1830 | `{` |
|    842579 | 1831 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1832 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    842579 | 1833 | `	pMap->iRef--;` |
|    842579 | 1834 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    102421 | 1835 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     51208 | 1836 | `	}` |
|    842579 | 1837 | `}` |
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
|   2719826 | 1864 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2719831 | 1875 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2719831 | 1876 | `	return rc;` |
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
|    597136 | 1984 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1985 | `{` |
|    597141 | 1986 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    597141 | 1987 | `	if( pEntry ){` |
|    597141 | 1988 | `		if( bStore ){` |
|    237603 | 1989 | `			PH7_MemObjStore(pEntry,pValue);` |
|    118804 | 1990 | `		}else{` |
|    359543 | 1991 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1992 | `		}` |
|    298563 | 1993 | `	}else{` |
|       ! 0 | 1994 | `		PH7_MemObjRelease(pValue);` |
|         - | 1995 | `	}` |
|    597141 | 1996 | `}` |
|         - | 1997 | `/*` |
|         - | 1998 | ` * Extract a node key.` |
|         - | 1999 | ` */` |
|    158834 | 2000 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2001 | `{` |
|         - | 2002 | `	/* Fill with the current key */` |
|    158839 | 2003 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    153539 | 2004 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2005 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2006 | `		}` |
|    153539 | 2007 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    153539 | 2008 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     76772 | 2009 | `	}else{` |
|      5305 | 2010 | `		SyBlobReset(&pKey->sBlob);` |
|      5305 | 2011 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5305 | 2012 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2013 | `	}` |
|    158839 | 2014 | `}` |
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
|     36934 | 2065 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2066 | `{` |
|         - | 2067 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2068 | `    /* Prevent compiler warning */` |
|     36939 | 2069 | `	result.pNext = result.pPrev = 0;` |
|     36939 | 2070 | `	pTail = &result;` |
|    109475 | 2071 | `	while( pA && pB ){` |
|     72541 | 2072 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47903 | 2073 | `			pTail->pPrev = pA;` |
|     47903 | 2074 | `			pA->pNext = pTail;` |
|     47903 | 2075 | `			pTail = pA;` |
|     47903 | 2076 | `			pA = pA->pPrev;` |
|     23897 | 2077 | `		}else{` |
|     24643 | 2078 | `			pTail->pPrev = pB;` |
|     24643 | 2079 | `			pB->pNext = pTail;` |
|     24643 | 2080 | `			pTail = pB;` |
|     24643 | 2081 | `			pB = pB->pPrev;` |
|         - | 2082 | `		}` |
|         5 | 2083 | `	}` |
|     36939 | 2084 | `	if( pA ){` |
|     26308 | 2085 | `		pTail->pPrev = pA;` |
|     26308 | 2086 | `		pA->pNext = pTail;` |
|     23821 | 2087 | `	}else if( pB ){` |
|     10408 | 2088 | `		pTail->pPrev = pB;` |
|     10408 | 2089 | `		pB->pNext = pTail;` |
|      5173 | 2090 | `	}else{` |
|       233 | 2091 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2092 | `	}` |
|     36939 | 2093 | `	return result.pPrev;` |
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
|     14965 | 2114 | `	while( pIn ){` |
|     14187 | 2115 | `		p = pIn;` |
|     14187 | 2116 | `		pIn = p->pPrev;` |
|     14187 | 2117 | `		p->pPrev = 0;` |
|     27003 | 2118 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     27003 | 2119 | `			if( a[i]==0 ){` |
|     14187 | 2120 | `				a[i] = p;` |
|     14187 | 2121 | `				break;` |
|       ! 0 | 2122 | `			}else{` |
|     12821 | 2123 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12821 | 2124 | `				a[i] = 0;` |
|         - | 2125 | `			}` |
|      6413 | 2126 | `		}` |
|     14187 | 2127 | `		if( i==N_SORT_BUCKET-1 ){` |
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
|       144 | 2158 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|         1 | 2159 | `{` |
|         - | 2160 | `	sxi32 rc;` |
|       145 | 2161 | `	if( base == 1 ){` |
|         - | 2162 | `		/* SORT_NUMERIC */` |
|        47 | 2163 | `		PH7_MemObjToNumeric(pA);` |
|        47 | 2164 | `		PH7_MemObjToNumeric(pB);` |
|        47 | 2165 | `		rc = PH7_MemObjCmp(pA,pB,FALSE,0);` |
|        24 | 2166 | `	}else{` |
|         - | 2167 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|         - | 2168 | `		const char *zA,*zB;` |
|         - | 2169 | `		sxu32 nA,nB,nMin,i;` |
|        99 | 2170 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pA); }` |
|        99 | 2171 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pB); }` |
|        99 | 2172 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
|        99 | 2173 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
|        99 | 2174 | `		nA = SyBlobLength(&pA->sBlob);` |
|        99 | 2175 | `		nB = SyBlobLength(&pB->sBlob);` |
|        99 | 2176 | `		if( base == 6 ){` |
|        25 | 2177 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|        13 | 2178 | `		}else{` |
|         - | 2179 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|        75 | 2180 | `			nMin = nA < nB ? nA : nB;` |
|        75 | 2181 | `			rc = 0;` |
|        99 | 2182 | `			for( i = 0 ; i < nMin ; ++i ){` |
|        81 | 2183 | `				int ca = (unsigned char)zA[i];` |
|        81 | 2184 | `				int cb = (unsigned char)zB[i];` |
|        81 | 2185 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|        81 | 2186 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|        13 | 2187 | `			}` |
|        75 | 2188 | `			if( rc == 0 ){` |
|        19 | 2189 | `				if( nA < nB ) rc = -1;` |
|         7 | 2190 | `				else if( nA > nB ) rc = 1;` |
|         9 | 2191 | `			}` |
|         - | 2192 | `		}` |
|         - | 2193 | `	}` |
|       145 | 2194 | `	return rc;` |
|         1 | 2195 | `}` |
|         - | 2196 | `/*` |
|         - | 2197 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|         - | 2198 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|         - | 2199 | ` */` |
|       126 | 2200 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2201 | `{` |
|         - | 2202 | `	ph7_value sA,sB;` |
|       127 | 2203 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|       127 | 2204 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2205 | `	sxi32 rc;` |
|       127 | 2206 | `	if( base == 0 ){` |
|         - | 2207 | `		/* SORT_REGULAR */` |
|       ! 0 | 2208 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2209 | `	}` |
|       127 | 2210 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       127 | 2211 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       127 | 2212 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       127 | 2213 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       127 | 2214 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|       127 | 2215 | `	PH7_MemObjRelease(&sA);` |
|       127 | 2216 | `	PH7_MemObjRelease(&sB);` |
|       127 | 2217 | `	return rc;` |
|        64 | 2218 | `}` |
|     72224 | 2219 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2220 | `{` |
|     72229 | 2221 | `	if( pCmpData == 0 ){` |
|         - | 2222 | `		/* SORT_REGULAR fast path */` |
|     72141 | 2223 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2224 | `	}` |
|        89 | 2225 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     36112 | 2226 | `}` |
|         - | 2227 | `/*` |
|         - | 2228 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2229 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2230 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2231 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2232 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2233 | ` */` |
|         - | 2234 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2235 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2236 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        36 | 2237 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2238 | `{` |
|        38 | 2239 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        38 | 2240 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        38 | 2241 | `	if( rc == 0 ){` |
|       ! 0 | 2242 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2243 | `	}` |
|        38 | 2244 | `	return rc;` |
|         2 | 2245 | `}` |
|        58 | 2246 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2247 | `{` |
|         - | 2248 | `	sxi32 rc;` |
|        60 | 2249 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2250 | `		/* Perform a string comparison */` |
|        32 | 2251 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        20 | 2252 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        12 | 2253 | `	}else{` |
|         - | 2254 | `		SyString sStr;` |
|        39 | 2255 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2256 | `		int bNum = 1;` |
|        39 | 2257 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2258 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2259 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2260 | `				bNum = 0;` |
|         6 | 2261 | `			}else{` |
|       ! 0 | 2262 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2263 | `			}` |
|         6 | 2264 | `		}else{` |
|        29 | 2265 | `			iA = pA->xKey.iKey;` |
|         - | 2266 | `		}` |
|        39 | 2267 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2268 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2269 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2270 | `				bNum = 0;` |
|         4 | 2271 | `			}else{` |
|       ! 0 | 2272 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2273 | `			}` |
|         4 | 2274 | `		}else{` |
|        33 | 2275 | `			iB = pB->xKey.iKey;` |
|         - | 2276 | `		}` |
|        39 | 2277 | `		if( bNum ){` |
|        23 | 2278 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2279 | `		}else{` |
|         - | 2280 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2281 | `			char zNumA[24],zNumB[24];` |
|         - | 2282 | `			SyString sA,sB;` |
|        17 | 2283 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2284 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2285 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2286 | `			}else{` |
|        11 | 2287 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2288 | `			}` |
|        17 | 2289 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2290 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2291 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2292 | `			}else{` |
|         7 | 2293 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2294 | `			}` |
|        17 | 2295 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2296 | `		}` |
|         - | 2297 | `	}` |
|        60 | 2298 | `	return rc;` |
|         2 | 2299 | `}` |
|         - | 2300 | `/*` |
|         - | 2301 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|         - | 2302 | ` * -> string) for a flag-aware key comparison.` |
|         - | 2303 | ` */` |
|        36 | 2304 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|         1 | 2305 | `{` |
|        37 | 2306 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|        21 | 2307 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|        11 | 2308 | `	}else{` |
|        17 | 2309 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|        25 | 2310 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|         8 | 2311 | `			SyBlobLength(&pNode->xKey.sKey));` |
|         - | 2312 | `	}` |
|        37 | 2313 | `}` |
|         - | 2314 | `/*` |
|         - | 2315 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|         - | 2316 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|         - | 2317 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|         - | 2318 | ` */` |
|        18 | 2319 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2320 | `{` |
|         - | 2321 | `	ph7_value sA,sB;` |
|        19 | 2322 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|        19 | 2323 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2324 | `	sxi32 rc;` |
|        19 | 2325 | `	if( base == 0 ){` |
|       ! 0 | 2326 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2327 | `	}` |
|        19 | 2328 | `	HashmapNodeKeyToValue(pA,&sA);` |
|        19 | 2329 | `	HashmapNodeKeyToValue(pB,&sB);` |
|        19 | 2330 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|        19 | 2331 | `	PH7_MemObjRelease(&sA);` |
|        19 | 2332 | `	PH7_MemObjRelease(&sB);` |
|        19 | 2333 | `	return rc;` |
|        10 | 2334 | `}` |
|         - | 2335 | `/*` |
|         - | 2336 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2337 | ` * used-by: [ksort()]` |
|         - | 2338 | ` */` |
|        58 | 2339 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2340 | `{` |
|        60 | 2341 | `	if( pCmpData == 0 ){` |
|        46 | 2342 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2343 | `	}` |
|        15 | 2344 | `	return HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        31 | 2345 | `}` |
|         - | 2346 | `/*` |
|         - | 2347 | ` * Node comparison callback.` |
|         - | 2348 | ` * Used by: [rsort(),arsort()];` |
|         - | 2349 | ` */` |
|        96 | 2350 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2351 | `{` |
|        97 | 2352 | `	if( pCmpData == 0 ){` |
|         - | 2353 | `		/* SORT_REGULAR fast path, reversed */` |
|        59 | 2354 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2355 | `	}` |
|        39 | 2356 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        49 | 2357 | `}` |
|         - | 2358 | `/*` |
|         - | 2359 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2360 | ` * used-by: [usort(),uasort()]` |
|         - | 2361 | ` */` |
|       116 | 2362 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2363 | `{` |
|         - | 2364 | `	ph7_value sResult,*pCallback;` |
|         - | 2365 | `	ph7_value *pV1,*pV2;` |
|         - | 2366 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2367 | `	sxi32 rc;` |
|         - | 2368 | `	/* Point to the desired callback */` |
|       119 | 2369 | `	pCallback = (ph7_value *)pCmpData;` |
|       119 | 2370 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2371 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2372 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2373 | `		return 0;` |
|         - | 2374 | `	}` |
|         - | 2375 | `	/* initialize the result value */` |
|       113 | 2376 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2377 | `	/* Extract nodes values */` |
|       113 | 2378 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       113 | 2379 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       113 | 2380 | `	apArg[0] = pV1;` |
|       113 | 2381 | `	apArg[1] = pV2;` |
|         - | 2382 | `	/* Invoke the callback */` |
|       113 | 2383 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       113 | 2384 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2385 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2386 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2387 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2388 | `		rc = 0;` |
|       108 | 2389 | `	}else if( rc != SXRET_OK ){` |
|         - | 2390 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2391 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2392 | `	}else{` |
|         - | 2393 | `		/* Extract callback result */` |
|       104 | 2394 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2395 | `			/* Perform an int cast */` |
|       ! 0 | 2396 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2397 | `		}` |
|       104 | 2398 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2399 | `	}` |
|       113 | 2400 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2401 | `	/* Callback result */` |
|       113 | 2402 | `	return rc;` |
|        61 | 2403 | `}` |
|         - | 2404 | `/*` |
|         - | 2405 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2406 | ` * used-by: [krsort()]` |
|         - | 2407 | ` */` |
|        18 | 2408 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2409 | `{` |
|        19 | 2410 | `	if( pCmpData == 0 ){` |
|        15 | 2411 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         - | 2412 | `	}` |
|         5 | 2413 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        10 | 2414 | `}` |
|         - | 2415 | `/*` |
|         - | 2416 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2417 | ` * used-by: [uksort()]` |
|         - | 2418 | ` */` |
|         6 | 2419 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2420 | `{` |
|         - | 2421 | `	ph7_value sResult,*pCallback;` |
|         - | 2422 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2423 | `	ph7_value sK1,sK2;` |
|         - | 2424 | `	sxi32 rc;` |
|         - | 2425 | `	/* Point to the desired callback */` |
|         7 | 2426 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2427 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2428 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2429 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2430 | `		return 0;` |
|         - | 2431 | `	}` |
|         - | 2432 | `	/* initialize the result value */` |
|         7 | 2433 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2434 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2435 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2436 | `	/* Extract nodes keys */` |
|         7 | 2437 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2438 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2439 | `	apArg[0] = &sK1;` |
|         7 | 2440 | `	apArg[1] = &sK2;` |
|         - | 2441 | `	/* Mark keys as constants */` |
|         7 | 2442 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2443 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2444 | `	/* Invoke the callback */` |
|         7 | 2445 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2446 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2447 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2448 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2449 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2450 | `		rc = 0;` |
|         7 | 2451 | `	}else if( rc != SXRET_OK ){` |
|         - | 2452 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2453 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2454 | `	}else{` |
|         - | 2455 | `		/* Extract callback result */` |
|         7 | 2456 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2457 | `			/* Perform an int cast */` |
|       ! 0 | 2458 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2459 | `		}` |
|         7 | 2460 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2461 | `	}` |
|         7 | 2462 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2463 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2464 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2465 | `	/* Callback result */` |
|         7 | 2466 | `	return rc;` |
|         4 | 2467 | `}` |
|         - | 2468 | `/*` |
|         - | 2469 | ` * Node comparison callback: Random node comparison.` |
|         - | 2470 | ` * used-by: [shuffle()]` |
|         - | 2471 | ` */` |
|        18 | 2472 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2473 | `{` |
|         - | 2474 | `	sxu32 n;` |
|        10 | 2475 | `	SXUNUSED(pB); /* cc warning */` |
|        10 | 2476 | `	SXUNUSED(pCmpData);` |
|         - | 2477 | `	/* Grab a random number */` |
|        19 | 2478 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2479 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2480 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2481 | `	 */` |
|        19 | 2482 | `	return n&1 ? 1 : -1;` |
|         1 | 2483 | `}` |
|         - | 2484 | `/*` |
|         - | 2485 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2486 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2487 | ` */` |
|       698 | 2488 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2489 | `{` |
|         - | 2490 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2491 | `	sxu32 i;` |
|         - | 2492 | `	/* Rehash all entries */` |
|       703 | 2493 | `	pLast = p = pMap->pFirst;` |
|       703 | 2494 | `	pMap->iNextIdx = 0;` |
|       703 | 2495 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|       703 | 2496 | `	i = 0;` |
|      7306 | 2497 | `	for( ;; ){` |
|     14617 | 2498 | `		if( i >= pMap->nEntry ){` |
|       703 | 2499 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       703 | 2500 | `			break;` |
|         - | 2501 | `		}` |
|     13919 | 2502 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2503 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2504 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2505 | `			/* Change key type */` |
|         5 | 2506 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2507 | `		}` |
|     13919 | 2508 | `		HashmapRehashIntNode(p);` |
|         - | 2509 | `		/* Point to the next entry */` |
|     13919 | 2510 | `		i++;` |
|     13919 | 2511 | `		pLast = p;` |
|     13919 | 2512 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2513 | `	}` |
|       703 | 2514 | `}` |
|         - | 2515 | `/*` |
|         - | 2516 | ` * Array functions implementation.` |
|         - | 2517 | ` * Status:` |
|         - | 2518 | ` *  Stable.` |
|         - | 2519 | ` */` |
|         - | 2520 | `/*` |
|         - | 2521 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2522 | ` * Sort an array.` |
|         - | 2523 | ` * Parameters` |
|         - | 2524 | ` *  $array` |
|         - | 2525 | ` *   The input array.` |
|         - | 2526 | ` * $sort_flags` |
|         - | 2527 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2528 | ` *  Sorting type flags:` |
|         - | 2529 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2530 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2531 | ` *   SORT_STRING - compare items as strings` |
|         - | 2532 | ` * Return` |
|         - | 2533 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2534 | ` *` |
|         - | 2535 | ` */` |
|      1052 | 2536 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2537 | `{` |
|         - | 2538 | `	ph7_hashmap *pMap;` |
|         - | 2539 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1057 | 2540 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2541 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2542 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2543 | `		return PH7_OK;` |
|         - | 2544 | `	}` |
|         - | 2545 | `	/* Point to the internal representation of the input hashmap */` |
|      1057 | 2546 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1057 | 2547 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1057 | 2548 | `	if( pMap->nEntry > 1 ){` |
|       673 | 2549 | `		sxi32 iCmpFlags = 0;` |
|       673 | 2550 | `		if( nArg > 1 ){` |
|         - | 2551 | `			/* Extract comparison flags */` |
|        15 | 2552 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         7 | 2553 | `		}` |
|         - | 2554 | `		/* Do the merge sort */` |
|       673 | 2555 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2556 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       673 | 2557 | `		HashmapSortRehash(pMap);` |
|       334 | 2558 | `	}` |
|         - | 2559 | `	/* All done,return TRUE */` |
|      1057 | 2560 | `	ph7_result_bool(pCtx,1);` |
|      1057 | 2561 | `	return PH7_OK;` |
|       531 | 2562 | `}` |
|         - | 2563 | `/*` |
|         - | 2564 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2565 | ` *  Sort an array and maintain index association.` |
|         - | 2566 | ` * Parameters` |
|         - | 2567 | ` *  $array` |
|         - | 2568 | ` *   The input array.` |
|         - | 2569 | ` * $sort_flags` |
|         - | 2570 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2571 | ` *  Sorting type flags:` |
|         - | 2572 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2573 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2574 | ` *   SORT_STRING - compare items as strings` |
|         - | 2575 | ` * Return` |
|         - | 2576 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2577 | ` */` |
|        34 | 2578 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2579 | `{` |
|         - | 2580 | `	ph7_hashmap *pMap;` |
|         - | 2581 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        39 | 2582 | `	if( nArg < 1 ){` |
|       ! 0 | 2583 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2584 | `			"ArgumentCountError",` |
|         - | 2585 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2586 | `			);` |
|         - | 2587 | `	}` |
|         - | 2588 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        39 | 2589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2590 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2591 | `			"TypeError",` |
|         - | 2592 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2593 | `			ph7_type_name(apArg[0])` |
|         - | 2594 | `			);` |
|         - | 2595 | `	}` |
|         - | 2596 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 2597 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        27 | 2598 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        27 | 2599 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2600 | `		sxi32 iCmpFlags = 0;` |
|        23 | 2601 | `		if( nArg > 1 ){` |
|         - | 2602 | `			/* Extract comparison flags */` |
|         7 | 2603 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2604 | `		}` |
|         - | 2605 | `		/* Do the merge sort */` |
|        23 | 2606 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2607 | `		/* Fix the last link broken by the merge */` |
|        55 | 2608 | `		while(pMap->pLast->pPrev){` |
|        33 | 2609 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2610 | `		}` |
|        11 | 2611 | `	}` |
|         - | 2612 | `	/* All done,return TRUE */` |
|        27 | 2613 | `	ph7_result_bool(pCtx,1);` |
|        27 | 2614 | `	return PH7_OK;` |
|        22 | 2615 | `}` |
|         - | 2616 | `/*` |
|         - | 2617 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2618 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2619 | ` * Parameters` |
|         - | 2620 | ` *  $array` |
|         - | 2621 | ` *   The input array.` |
|         - | 2622 | ` * $sort_flags` |
|         - | 2623 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2624 | ` *  Sorting type flags:` |
|         - | 2625 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2626 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2627 | ` *   SORT_STRING - compare items as strings` |
|         - | 2628 | ` * Return` |
|         - | 2629 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2630 | ` */` |
|        32 | 2631 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2632 | `{` |
|         - | 2633 | `	ph7_hashmap *pMap;` |
|         - | 2634 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2635 | `	if( nArg < 1 ){` |
|       ! 0 | 2636 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2637 | `			"ArgumentCountError",` |
|         - | 2638 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2639 | `			);` |
|         - | 2640 | `	}` |
|         - | 2641 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2642 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2643 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2644 | `			"TypeError",` |
|         - | 2645 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2646 | `			ph7_type_name(apArg[0])` |
|         - | 2647 | `			);` |
|         - | 2648 | `	}` |
|         - | 2649 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2650 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2651 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2652 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2653 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2654 | `		if( nArg > 1 ){` |
|         - | 2655 | `			/* Extract comparison flags */` |
|         7 | 2656 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2657 | `		}` |
|         - | 2658 | `		/* Do the merge sort */` |
|        21 | 2659 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2660 | `		/* Fix the last link broken by the merge */` |
|        37 | 2661 | `		while(pMap->pLast->pPrev){` |
|        17 | 2662 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2663 | `		}` |
|        10 | 2664 | `	}` |
|         - | 2665 | `	/* All done,return TRUE */` |
|        25 | 2666 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2667 | `	return PH7_OK;` |
|        21 | 2668 | `}` |
|         - | 2669 | `/*` |
|         - | 2670 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2671 | ` *  Sort an array by key.` |
|         - | 2672 | ` * Parameters` |
|         - | 2673 | ` *  $array` |
|         - | 2674 | ` *   The input array.` |
|         - | 2675 | ` * $sort_flags` |
|         - | 2676 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2677 | ` *  Sorting type flags:` |
|         - | 2678 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2679 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2680 | ` *   SORT_STRING - compare items as strings` |
|         - | 2681 | ` * Return` |
|         - | 2682 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2683 | ` */` |
|        18 | 2684 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2685 | `{` |
|         - | 2686 | `	ph7_hashmap *pMap;` |
|         - | 2687 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 2688 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2689 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2690 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2691 | `		return PH7_OK;` |
|         - | 2692 | `	}` |
|         - | 2693 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 2694 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        20 | 2695 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 2696 | `	if( pMap->nEntry > 1 ){` |
|        20 | 2697 | `		sxi32 iCmpFlags = 0;` |
|        20 | 2698 | `		if( nArg > 1 ){` |
|         - | 2699 | `			/* Extract comparison flags */` |
|         5 | 2700 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2701 | `		}` |
|         - | 2702 | `		/* Do the merge sort */` |
|        20 | 2703 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2704 | `		/* Fix the last link broken by the merge */` |
|        52 | 2705 | `		while(pMap->pLast->pPrev){` |
|        33 | 2706 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2707 | `		}` |
|         9 | 2708 | `	}` |
|         - | 2709 | `	/* All done,return TRUE */` |
|        20 | 2710 | `	ph7_result_bool(pCtx,1);` |
|        20 | 2711 | `	return PH7_OK;` |
|        11 | 2712 | `}` |
|         - | 2713 | `/*` |
|         - | 2714 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2715 | ` *  Sort an array by key in reverse order.` |
|         - | 2716 | ` * Parameters` |
|         - | 2717 | ` *  $array` |
|         - | 2718 | ` *   The input array.` |
|         - | 2719 | ` * $sort_flags` |
|         - | 2720 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2721 | ` *  Sorting type flags:` |
|         - | 2722 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2723 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2724 | ` *   SORT_STRING - compare items as strings` |
|         - | 2725 | ` * Return` |
|         - | 2726 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2727 | ` */` |
|         6 | 2728 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2729 | `{` |
|         - | 2730 | `	ph7_hashmap *pMap;` |
|         - | 2731 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2732 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2733 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2734 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2735 | `		return PH7_OK;` |
|         - | 2736 | `	}` |
|         - | 2737 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2738 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2739 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2740 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2741 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2742 | `		if( nArg > 1 ){` |
|         - | 2743 | `			/* Extract comparison flags */` |
|         3 | 2744 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         1 | 2745 | `		}` |
|         - | 2746 | `		/* Do the merge sort */` |
|         7 | 2747 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2748 | `		/* Fix the last link broken by the merge */` |
|        23 | 2749 | `		while(pMap->pLast->pPrev){` |
|        17 | 2750 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2751 | `		}` |
|         3 | 2752 | `	}` |
|         - | 2753 | `	/* All done,return TRUE */` |
|         7 | 2754 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2755 | `	return PH7_OK;` |
|         4 | 2756 | `}` |
|         - | 2757 | `/*` |
|         - | 2758 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2759 | ` * Sort an array in reverse order.` |
|         - | 2760 | ` * Parameters` |
|         - | 2761 | ` *  $array` |
|         - | 2762 | ` *   The input array.` |
|         - | 2763 | ` * $sort_flags` |
|         - | 2764 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2765 | ` *  Sorting type flags:` |
|         - | 2766 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2767 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2768 | ` *   SORT_STRING - compare items as strings` |
|         - | 2769 | ` * Return` |
|         - | 2770 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2771 | ` */` |
|         6 | 2772 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2773 | `{` |
|         - | 2774 | `	ph7_hashmap *pMap;` |
|         - | 2775 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2776 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2777 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2778 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2779 | `		return PH7_OK;` |
|         - | 2780 | `	}` |
|         - | 2781 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2782 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2783 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2784 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2785 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2786 | `		if( nArg > 1 ){` |
|         - | 2787 | `			/* Extract comparison flags */` |
|         5 | 2788 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2789 | `		}` |
|         - | 2790 | `		/* Do the merge sort */` |
|         7 | 2791 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2792 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         7 | 2793 | `		HashmapSortRehash(pMap);` |
|         3 | 2794 | `	}` |
|         - | 2795 | `	/* All done,return TRUE */` |
|         7 | 2796 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2797 | `	return PH7_OK;` |
|         4 | 2798 | `}` |
|         - | 2799 | `/*` |
|         - | 2800 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2801 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2802 | ` * Parameters` |
|         - | 2803 | ` *  $array` |
|         - | 2804 | ` *   The input array.` |
|         - | 2805 | ` * $cmp_function` |
|         - | 2806 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2807 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2808 | ` *  to, or greater than the second.` |
|         - | 2809 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2810 | ` * Return` |
|         - | 2811 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2812 | ` */` |
|        22 | 2813 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2814 | `{` |
|         - | 2815 | `	ph7_hashmap *pMap;` |
|         - | 2816 | `	/* Make sure we are dealing with a valid hashmap */` |
|        25 | 2817 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2818 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2819 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2820 | `		return PH7_OK;` |
|         - | 2821 | `	}` |
|        25 | 2822 | `	if( nArg > 1 ){` |
|         - | 2823 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2824 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2825 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        25 | 2826 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        25 | 2827 | `		if( rcCb != PH7_OK ){` |
|         3 | 2828 | `			return rcCb;` |
|         - | 2829 | `		}` |
|        10 | 2830 | `	}` |
|         - | 2831 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2832 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2833 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2834 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2835 | `		ph7_value *pCallback = 0;` |
|         - | 2836 | `		ProcNodeCmp xCmp;` |
|        23 | 2837 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        23 | 2838 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2839 | `			/* Point to the desired callback */` |
|        23 | 2840 | `			pCallback = apArg[1];` |
|        13 | 2841 | `		}else{` |
|         - | 2842 | `			/* Use the default comparison function */` |
|       ! 0 | 2843 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2844 | `		}` |
|         - | 2845 | `		/* Do the merge sort */` |
|        23 | 2846 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        23 | 2847 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2848 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        23 | 2849 | `		HashmapSortRehash(pMap);` |
|        23 | 2850 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2851 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2852 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2853 | `			return PH7_EXCEPTION;` |
|         - | 2854 | `		}` |
|         6 | 2855 | `	}` |
|         - | 2856 | `	/* All done,return TRUE */` |
|        14 | 2857 | `	ph7_result_bool(pCtx,1);` |
|        14 | 2858 | `	return PH7_OK;` |
|        14 | 2859 | `}` |
|         - | 2860 | `/*` |
|         - | 2861 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2862 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2863 | ` *  and maintain index association.` |
|         - | 2864 | ` * Parameters` |
|         - | 2865 | ` *  $array` |
|         - | 2866 | ` *   The input array.` |
|         - | 2867 | ` * $cmp_function` |
|         - | 2868 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2869 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2870 | ` *  to, or greater than the second.` |
|         - | 2871 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2872 | ` * Return` |
|         - | 2873 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2874 | ` */` |
|        12 | 2875 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2876 | `{` |
|         - | 2877 | `	ph7_hashmap *pMap;` |
|         - | 2878 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 2879 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2880 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2881 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2882 | `		return PH7_OK;` |
|         - | 2883 | `	}` |
|        13 | 2884 | `	if( nArg > 1 ){` |
|         - | 2885 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2886 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2887 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        13 | 2888 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        13 | 2889 | `		if( rcCb != PH7_OK ){` |
|         3 | 2890 | `			return rcCb;` |
|         - | 2891 | `		}` |
|         5 | 2892 | `	}` |
|         - | 2893 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2894 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2895 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2896 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2897 | `		ph7_value *pCallback = 0;` |
|         - | 2898 | `		ProcNodeCmp xCmp;` |
|        11 | 2899 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2900 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2901 | `			/* Point to the desired callback */` |
|        11 | 2902 | `			pCallback = apArg[1];` |
|         6 | 2903 | `		}else{` |
|         - | 2904 | `			/* Use the default comparison function */` |
|       ! 0 | 2905 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2906 | `		}` |
|         - | 2907 | `		/* Do the merge sort */` |
|        11 | 2908 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2909 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2910 | `		/* Fix the last link broken by the merge */` |
|        23 | 2911 | `		while(pMap->pLast->pPrev){` |
|        13 | 2912 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2913 | `		}` |
|        11 | 2914 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2915 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2916 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2917 | `			return PH7_EXCEPTION;` |
|         - | 2918 | `		}` |
|         5 | 2919 | `	}` |
|         - | 2920 | `	/* All done,return TRUE */` |
|        11 | 2921 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2922 | `	return PH7_OK;` |
|         7 | 2923 | `}` |
|         - | 2924 | `/*` |
|         - | 2925 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2926 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2927 | ` *  function and maintain index association.` |
|         - | 2928 | ` * Parameters` |
|         - | 2929 | ` *  $array` |
|         - | 2930 | ` *   The input array.` |
|         - | 2931 | ` * $cmp_function` |
|         - | 2932 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2933 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2934 | ` *  to, or greater than the second.` |
|         - | 2935 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2936 | ` * Return` |
|         - | 2937 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2938 | ` */` |
|         4 | 2939 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2940 | `{` |
|         - | 2941 | `	ph7_hashmap *pMap;` |
|         - | 2942 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2943 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2944 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2945 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2946 | `		return PH7_OK;` |
|         - | 2947 | `	}` |
|         5 | 2948 | `	if( nArg > 1 ){` |
|         - | 2949 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2950 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2951 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|         5 | 2952 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|         5 | 2953 | `		if( rcCb != PH7_OK ){` |
|         3 | 2954 | `			return rcCb;` |
|         - | 2955 | `		}` |
|         1 | 2956 | `	}` |
|         - | 2957 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2958 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2959 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2960 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2961 | `		ph7_value *pCallback = 0;` |
|         - | 2962 | `		ProcNodeCmp xCmp;` |
|         3 | 2963 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2964 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2965 | `			/* Point to the desired callback */` |
|         3 | 2966 | `			pCallback = apArg[1];` |
|         2 | 2967 | `		}else{` |
|         - | 2968 | `			/* Use the default comparison function */` |
|       ! 0 | 2969 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2970 | `		}` |
|         - | 2971 | `		/* Do the merge sort */` |
|         3 | 2972 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2973 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2974 | `		/* Fix the last link broken by the merge */` |
|         3 | 2975 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2976 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2977 | `		}` |
|         3 | 2978 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2979 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2980 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2981 | `			return PH7_EXCEPTION;` |
|         - | 2982 | `		}` |
|         1 | 2983 | `	}` |
|         - | 2984 | `	/* All done,return TRUE */` |
|         3 | 2985 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2986 | `	return PH7_OK;` |
|         3 | 2987 | `}` |
|         - | 2988 | `/*` |
|         - | 2989 | ` * bool shuffle(array &$array)` |
|         - | 2990 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2991 | ` * Parameters` |
|         - | 2992 | ` *  $array` |
|         - | 2993 | ` *   The input array.` |
|         - | 2994 | ` * Return` |
|         - | 2995 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2996 | ` *` |
|         - | 2997 | ` */` |
|         2 | 2998 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2999 | `{` |
|         - | 3000 | `	ph7_hashmap *pMap;` |
|         - | 3001 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3002 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3003 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3004 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3005 | `		return PH7_OK;` |
|         - | 3006 | `	}` |
|         - | 3007 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3008 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3009 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3010 | `	if( pMap->nEntry > 1 ){` |
|         - | 3011 | `		/* Do the merge sort */` |
|         3 | 3012 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 3013 | `		/* Fix the last link broken by the merge */` |
|        10 | 3014 | `		while(pMap->pLast->pPrev){` |
|         8 | 3015 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 3016 | `		}` |
|         1 | 3017 | `	}` |
|         - | 3018 | `	/* All done,return TRUE */` |
|         3 | 3019 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3020 | `	return PH7_OK;` |
|         2 | 3021 | `}` |
|         - | 3022 | `/*` |
|         - | 3023 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 3024 | ` *   Count all elements in an array, or something in an object.` |
|         - | 3025 | ` * Parameters` |
|         - | 3026 | ` *  $var` |
|         - | 3027 | ` *   The array or the object.` |
|         - | 3028 | ` * $mode` |
|         - | 3029 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 3030 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 3031 | ` *  all the elements of a multidimensional array.` |
|         - | 3032 | ` * Return` |
|         - | 3033 | ` *  Returns the number of elements in the array.` |
|         - | 3034 | ` */` |
|      1976 | 3035 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3036 | `{` |
|      1981 | 3037 | `	int bRecursive = FALSE;` |
|      1981 | 3038 | `	int bCycleDetected = FALSE;` |
|         - | 3039 | `	sxi64 iCount;` |
|      1981 | 3040 | `	if( nArg < 1 ){` |
|       ! 0 | 3041 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3042 | `			"ArgumentCountError",` |
|         - | 3043 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 3044 | `			);` |
|         - | 3045 | `	}` |
|      1981 | 3046 | `	if( nArg > 2 ){` |
|         4 | 3047 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3048 | `			"ArgumentCountError",` |
|         - | 3049 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3050 | `			nArg` |
|         - | 3051 | `			);` |
|         - | 3052 | `	}` |
|         - | 3053 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3054 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3055 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1979 | 3056 | `	if( nArg > 1 ){` |
|        44 | 3057 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 3058 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 3059 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3060 | `				"ValueError",` |
|         - | 3061 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3062 | `				);` |
|         - | 3063 | `		}` |
|        34 | 3064 | `		bRecursive = iMode == 1;` |
|        16 | 3065 | `	}` |
|      1971 | 3066 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3067 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 3068 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 3069 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 3070 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 3071 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 3072 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3073 | `					"count",sizeof("count")-1);` |
|        59 | 3074 | `				if( pMeth ){` |
|         - | 3075 | `					ph7_value sResult;` |
|        59 | 3076 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 3077 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3078 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3079 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3080 | `					return PH7_OK;` |
|         - | 3081 | `				}` |
|       ! 0 | 3082 | `			}` |
|         1 | 3083 | `		}` |
|        22 | 3084 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3085 | `			"TypeError",` |
|         - | 3086 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3087 | `			ph7_type_name(apArg[0])` |
|         - | 3088 | `			);` |
|         - | 3089 | `	}` |
|         - | 3090 | `	/* Count */` |
|      1903 | 3091 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1903 | 3092 | `	if( bCycleDetected ){` |
|         3 | 3093 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3094 | `	}` |
|      1903 | 3095 | `	ph7_result_int64(pCtx,iCount);` |
|      1903 | 3096 | `	return PH7_OK;` |
|       993 | 3097 | `}` |
|         - | 3098 | `/*` |
|         - | 3099 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3100 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3101 | ` * Parameters` |
|         - | 3102 | ` * $key` |
|         - | 3103 | ` *   Value to check.` |
|         - | 3104 | ` * $search` |
|         - | 3105 | ` *  An array with keys to check.` |
|         - | 3106 | ` * Return` |
|         - | 3107 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3108 | ` */` |
|        94 | 3109 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3110 | `{` |
|         - | 3111 | `	sxi32 rc;` |
|        99 | 3112 | `	if( nArg != 2 ){` |
|         - | 3113 | `		/* PHP requires exactly two arguments */` |
|         4 | 3114 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3115 | `			"ArgumentCountError",` |
|         - | 3116 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3117 | `			nArg` |
|         - | 3118 | `			);` |
|         - | 3119 | `	}` |
|         - | 3120 | `	/* Make sure we are dealing with a valid hashmap */` |
|        97 | 3121 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3122 | `		/* Type mismatch -> TypeError */` |
|         8 | 3123 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3124 | `			"TypeError",` |
|         - | 3125 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3126 | `			ph7_type_name(apArg[1])` |
|         - | 3127 | `			);` |
|         - | 3128 | `	}` |
|         - | 3129 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        92 | 3130 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3131 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3132 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3133 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3134 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3135 | `			"use an empty string instead"` |
|         - | 3136 | `			);` |
|        91 | 3137 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3138 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3139 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3140 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3141 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3142 | `				,rVal` |
|         - | 3143 | `				);` |
|         1 | 3144 | `		}` |
|         1 | 3145 | `	}` |
|         - | 3146 | `	/* Perform the lookup */` |
|        92 | 3147 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3148 | `	/* lookup result */` |
|        92 | 3149 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        92 | 3150 | `	return PH7_OK;` |
|        52 | 3151 | `}` |
|         - | 3152 | `/*` |
|         - | 3153 | ` * value array_pop(array $array)` |
|         - | 3154 | ` *   POP the last inserted element from the array.` |
|         - | 3155 | ` * Parameter` |
|         - | 3156 | ` *  The array to get the value from.` |
|         - | 3157 | ` * Return` |
|         - | 3158 | ` *  Poped value or NULL on failure.` |
|         - | 3159 | ` */` |
|       102 | 3160 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3161 | `{` |
|         - | 3162 | `	ph7_hashmap *pMap;` |
|         - | 3163 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       106 | 3164 | `	if( nArg != 1 ){` |
|         4 | 3165 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3166 | `			"ArgumentCountError",` |
|         - | 3167 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3168 | `			nArg` |
|         - | 3169 | `			);` |
|         - | 3170 | `	}` |
|         - | 3171 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3172 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3173 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3174 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3175 | `			"Error",` |
|         - | 3176 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3177 | `			);` |
|         - | 3178 | `	}` |
|         - | 3179 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3180 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3181 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3182 | `			"TypeError",` |
|         - | 3183 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3184 | `			ph7_type_name(apArg[0])` |
|         - | 3185 | `			);` |
|         - | 3186 | `	}` |
|        95 | 3187 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3188 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3189 | `	if( pMap->nEntry < 1 ){` |
|         - | 3190 | `		/* Nothing to pop,return NULL */` |
|         3 | 3191 | `		ph7_result_null(pCtx);` |
|         2 | 3192 | `	}else{` |
|        93 | 3193 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3194 | `		ph7_value *pObj;` |
|        93 | 3195 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3196 | `		if( pObj ){` |
|         - | 3197 | `			/* Node value */` |
|        93 | 3198 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3199 | `			/* Unlink the node */` |
|        93 | 3200 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3201 | `		}else{` |
|       ! 0 | 3202 | `			ph7_result_null(pCtx);` |
|         - | 3203 | `		}` |
|         - | 3204 | `		/* Reset the cursor */` |
|        93 | 3205 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3206 | `	}` |
|        95 | 3207 | `	return PH7_OK;` |
|        55 | 3208 | `}` |
|         - | 3209 | `/*` |
|         - | 3210 | ` * int array_push($array,$var,...)` |
|         - | 3211 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3212 | ` * Parameters` |
|         - | 3213 | ` *  array` |
|         - | 3214 | ` *    The input array.` |
|         - | 3215 | ` *  var` |
|         - | 3216 | ` *   On or more value to push.` |
|         - | 3217 | ` * Return` |
|         - | 3218 | ` *  New array count (including old items).` |
|         - | 3219 | ` */` |
|        22 | 3220 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3221 | `{` |
|         - | 3222 | `	ph7_hashmap *pMap;` |
|         - | 3223 | `	sxi32 rc;` |
|         - | 3224 | `	int i;` |
|        26 | 3225 | `	if( nArg < 1 ){` |
|       ! 0 | 3226 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3227 | `			"ArgumentCountError",` |
|         - | 3228 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3229 | `			nArg` |
|         - | 3230 | `			);` |
|         - | 3231 | `	}` |
|         - | 3232 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3233 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3234 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3235 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3236 | `			"Error",` |
|         - | 3237 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3238 | `			);` |
|         - | 3239 | `	}` |
|         - | 3240 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3241 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3242 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3243 | `			"TypeError",` |
|         - | 3244 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3245 | `			ph7_type_name(apArg[0])` |
|         - | 3246 | `			);` |
|         - | 3247 | `	}` |
|         - | 3248 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3249 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3250 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3251 | `	/* Start pushing given values */` |
|        34 | 3252 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3253 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3254 | `		if( rc != SXRET_OK ){` |
|         3 | 3255 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3256 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3257 | `				return rc;` |
|         - | 3258 | `			}` |
|       ! 0 | 3259 | `			break;` |
|         - | 3260 | `		}` |
|         9 | 3261 | `	}` |
|         - | 3262 | `	/* Return the new count */` |
|        15 | 3263 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3264 | `	return PH7_OK;` |
|        15 | 3265 | `}` |
|         - | 3266 | `/*` |
|         - | 3267 | ` * value array_shift(array $array)` |
|         - | 3268 | ` *   Shift an element off the beginning of array.` |
|         - | 3269 | ` * Parameter` |
|         - | 3270 | ` *  The array to get the value from.` |
|         - | 3271 | ` * Return` |
|         - | 3272 | ` *  Shifted value or NULL on failure.` |
|         - | 3273 | ` */` |
|        44 | 3274 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3275 | `{` |
|         - | 3276 | `	ph7_hashmap *pMap;` |
|         - | 3277 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3278 | `	if( nArg != 1 ){` |
|         4 | 3279 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3280 | `			"ArgumentCountError",` |
|         - | 3281 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3282 | `			nArg` |
|         - | 3283 | `			);` |
|         - | 3284 | `	}` |
|         - | 3285 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3286 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3287 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3288 | `			"Error",` |
|         - | 3289 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3290 | `			);` |
|         - | 3291 | `	}` |
|         - | 3292 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3293 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3294 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3295 | `			"TypeError",` |
|         - | 3296 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3297 | `			ph7_type_name(apArg[0])` |
|         - | 3298 | `			);` |
|         - | 3299 | `	}` |
|         - | 3300 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3301 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3302 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3303 | `	if( pMap->nEntry < 1 ){` |
|         - | 3304 | `		/* Empty hashmap,return NULL */` |
|         3 | 3305 | `		ph7_result_null(pCtx);` |
|         2 | 3306 | `	}else{` |
|        39 | 3307 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3308 | `		ph7_value *pObj;` |
|         - | 3309 | `		sxu32 n;` |
|        39 | 3310 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3311 | `		if( pObj ){` |
|         - | 3312 | `			/* Node value */` |
|        39 | 3313 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3314 | `			/* Unlink the first node */` |
|        39 | 3315 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3316 | `		}else{` |
|       ! 0 | 3317 | `			ph7_result_null(pCtx);` |
|         - | 3318 | `		}` |
|         - | 3319 | `		/* Rehash all int keys */` |
|        39 | 3320 | `		n = pMap->nEntry;` |
|        39 | 3321 | `		pEntry = pMap->pFirst;` |
|        39 | 3322 | `		pMap->iNextIdx = 0;` |
|        39 | 3323 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3324 | `		for(;;){` |
|        99 | 3325 | `			if( n < 1 ){` |
|        39 | 3326 | `				break;` |
|         - | 3327 | `			}` |
|        65 | 3328 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3329 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3330 | `			}` |
|         - | 3331 | `			/* Point to the next entry */` |
|        65 | 3332 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3333 | `			n--;` |
|         5 | 3334 | `		}` |
|         - | 3335 | `		/* Reset the cursor */` |
|        39 | 3336 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3337 | `	}` |
|        41 | 3338 | `	return PH7_OK;` |
|        27 | 3339 | `}` |
|         - | 3340 | `/*` |
|         - | 3341 | ` * Extract the node cursor value.` |
|         - | 3342 | ` */` |
|      1094 | 3343 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3344 | `{` |
|      1095 | 3345 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3346 | `	ph7_value *pVal;` |
|      1095 | 3347 | `	if( pCur == 0 ){` |
|         - | 3348 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3349 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3350 | `		return PH7_OK;` |
|         - | 3351 | `	}` |
|      1057 | 3352 | `	if( iDirection != 0 ){` |
|       201 | 3353 | `		if( iDirection > 0 ){` |
|         - | 3354 | `			/* Point to the next entry */` |
|       199 | 3355 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3356 | `			pCur = pMap->pCur;` |
|       100 | 3357 | `		}else{` |
|         - | 3358 | `			/* Point to the previous entry */` |
|         3 | 3359 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3360 | `			pCur = pMap->pCur;` |
|         - | 3361 | `		}` |
|       201 | 3362 | `		if( pCur == 0 ){` |
|         - | 3363 | `			/* End of input reached,return FALSE */` |
|        83 | 3364 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3365 | `			return PH7_OK;` |
|         - | 3366 | `		}` |
|        59 | 3367 | `	}` |
|         - | 3368 | `	/* Point to the desired element */` |
|       975 | 3369 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3370 | `	if( pVal ){` |
|       975 | 3371 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3372 | `	}else{` |
|       ! 0 | 3373 | `		ph7_result_bool(pCtx,0);` |
|         - | 3374 | `	}` |
|       975 | 3375 | `	return PH7_OK;` |
|       548 | 3376 | `}` |
|         - | 3377 | `/*` |
|         - | 3378 | ` * value current(array $array)` |
|         - | 3379 | ` *  Return the current element in an array.` |
|         - | 3380 | ` * Parameter` |
|         - | 3381 | ` *  $input: The input array.` |
|         - | 3382 | ` * Return` |
|         - | 3383 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3384 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3385 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3386 | ` *  is empty, current() returns FALSE.` |
|         - | 3387 | ` */` |
|       302 | 3388 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3389 | `{` |
|       303 | 3390 | `	if( nArg < 1 ){` |
|         - | 3391 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3392 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3393 | `		return PH7_OK;` |
|         - | 3394 | `	}` |
|         - | 3395 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3396 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3397 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3398 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3399 | `		return PH7_OK;` |
|         - | 3400 | `	}` |
|       303 | 3401 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3402 | `	return PH7_OK;` |
|       152 | 3403 | `}` |
|         - | 3404 | `/*` |
|         - | 3405 | ` * value next(array $input)` |
|         - | 3406 | ` *  Advance the internal array pointer of an array.` |
|         - | 3407 | ` * Parameter` |
|         - | 3408 | ` *  $input: The input array.` |
|         - | 3409 | ` * Return` |
|         - | 3410 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3411 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3412 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3413 | ` */` |
|       198 | 3414 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3415 | `{` |
|       199 | 3416 | `	if( nArg < 1 ){` |
|         - | 3417 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3418 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3419 | `		return PH7_OK;` |
|         - | 3420 | `	}` |
|         - | 3421 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3422 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3423 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3424 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3425 | `		return PH7_OK;` |
|         - | 3426 | `	}` |
|       199 | 3427 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3428 | `	return PH7_OK;` |
|       100 | 3429 | `}` |
|         - | 3430 | `/*` |
|         - | 3431 | ` * value prev(array $input)` |
|         - | 3432 | ` *  Rewind the internal array pointer.` |
|         - | 3433 | ` * Parameter` |
|         - | 3434 | ` *  $input: The input array.` |
|         - | 3435 | ` * Return` |
|         - | 3436 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3437 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3438 | ` *  elements.` |
|         - | 3439 | ` */` |
|         2 | 3440 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3441 | `{` |
|         3 | 3442 | `	if( nArg < 1 ){` |
|         - | 3443 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3444 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3445 | `		return PH7_OK;` |
|         - | 3446 | `	}` |
|         - | 3447 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3448 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3449 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3450 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3451 | `		return PH7_OK;` |
|         - | 3452 | `	}` |
|         3 | 3453 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3454 | `	return PH7_OK;` |
|         2 | 3455 | `}` |
|         - | 3456 | `/*` |
|         - | 3457 | ` * value end(array $input)` |
|         - | 3458 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3459 | ` * Parameter` |
|         - | 3460 | ` *  $input: The input array.` |
|         - | 3461 | ` * Return` |
|         - | 3462 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3463 | ` */` |
|       348 | 3464 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3465 | `{` |
|         - | 3466 | `	ph7_hashmap *pMap;` |
|       349 | 3467 | `	if( nArg < 1 ){` |
|         - | 3468 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3469 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3470 | `		return PH7_OK;` |
|         - | 3471 | `	}` |
|         - | 3472 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3473 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3474 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3475 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3476 | `		return PH7_OK;` |
|         - | 3477 | `	}` |
|         - | 3478 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3479 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3480 | `	/* Point to the last node */` |
|       349 | 3481 | `	pMap->pCur = pMap->pLast;` |
|         - | 3482 | `	/* Return the last node value */` |
|       349 | 3483 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3484 | `	return PH7_OK;` |
|       175 | 3485 | `}` |
|         - | 3486 | `/*` |
|         - | 3487 | ` * value reset(array $array )` |
|         - | 3488 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3489 | ` * Parameter` |
|         - | 3490 | ` *  $input: The input array.` |
|         - | 3491 | ` * Return` |
|         - | 3492 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3493 | ` */` |
|       244 | 3494 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3495 | `{` |
|         - | 3496 | `	ph7_hashmap *pMap;` |
|       245 | 3497 | `	if( nArg < 1 ){` |
|         - | 3498 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3499 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3500 | `		return PH7_OK;` |
|         - | 3501 | `	}` |
|         - | 3502 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3503 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3504 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3505 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3506 | `		return PH7_OK;` |
|         - | 3507 | `	}` |
|         - | 3508 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3509 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3510 | `	/* Point to the first node */` |
|       245 | 3511 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3512 | `	/* Return the last node value if available */` |
|       245 | 3513 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3514 | `	return PH7_OK;` |
|       123 | 3515 | `}` |
|         - | 3516 | `/*` |
|         - | 3517 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3518 | ` * array_key_first() and array_key_last().` |
|         - | 3519 | ` */` |
|       672 | 3520 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3521 | `{` |
|       673 | 3522 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3523 | `		/* Key is integer */` |
|       283 | 3524 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3525 | `	}else{` |
|         - | 3526 | `		/* Key is blob */` |
|       586 | 3527 | `		ph7_result_string(pCtx,` |
|       390 | 3528 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3529 | `	}` |
|       673 | 3530 | `}` |
|         - | 3531 | `/*` |
|         - | 3532 | ` * value key(array $array)` |
|         - | 3533 | ` *   Fetch a key from an array` |
|         - | 3534 | ` * Parameter` |
|         - | 3535 | ` *  $input` |
|         - | 3536 | ` *   The input array.` |
|         - | 3537 | ` * Return` |
|         - | 3538 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3539 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3540 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3541 | ` *  is empty, key() returns NULL.` |
|         - | 3542 | ` */` |
|       776 | 3543 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3544 | `{` |
|         - | 3545 | `	ph7_hashmap_node *pCur;` |
|         - | 3546 | `	ph7_hashmap *pMap;` |
|       777 | 3547 | `	if( nArg < 1 ){` |
|         - | 3548 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3549 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3550 | `		return PH7_OK;` |
|         - | 3551 | `	}` |
|         - | 3552 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3553 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3554 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3555 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3556 | `		return PH7_OK;` |
|         - | 3557 | `	}` |
|       777 | 3558 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3559 | `	pCur = pMap->pCur;` |
|       777 | 3560 | `	if( pCur == 0 ){` |
|         - | 3561 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3562 | `		ph7_result_null(pCtx);` |
|       121 | 3563 | `		return PH7_OK;` |
|         - | 3564 | `	}` |
|       657 | 3565 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3566 | `	return PH7_OK;` |
|       389 | 3567 | `}` |
|         - | 3568 | `/*` |
|         - | 3569 | ` * array each(array $input)` |
|         - | 3570 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3571 | ` * Parameter` |
|         - | 3572 | ` *  $input` |
|         - | 3573 | ` *    The input array.` |
|         - | 3574 | ` * Return` |
|         - | 3575 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3576 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3577 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3578 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3579 | ` *  each() returns FALSE.` |
|         - | 3580 | ` */` |
|        22 | 3581 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3582 | `{` |
|         - | 3583 | `	ph7_hashmap_node *pCur;` |
|         - | 3584 | `	ph7_hashmap *pMap;` |
|         - | 3585 | `	ph7_value *pArray;` |
|         - | 3586 | `	ph7_value *pVal;` |
|         - | 3587 | `	ph7_value sKey;` |
|        23 | 3588 | `	if( nArg < 1 ){` |
|         - | 3589 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3590 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3591 | `		return PH7_OK;` |
|         - | 3592 | `	}` |
|         - | 3593 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3594 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3595 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3596 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3597 | `		return PH7_OK;` |
|         - | 3598 | `	}` |
|         - | 3599 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3600 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3601 | `	if( pMap->pCur == 0 ){` |
|         - | 3602 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3603 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3604 | `		return PH7_OK;` |
|         - | 3605 | `	}` |
|        15 | 3606 | `	pCur = pMap->pCur;` |
|         - | 3607 | `	/* Create a new array */` |
|        15 | 3608 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3609 | `	if( pArray == 0 ){` |
|       ! 0 | 3610 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3611 | `		return PH7_OK;` |
|         - | 3612 | `	}` |
|        15 | 3613 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3614 | `	/* Insert the current value */` |
|        15 | 3615 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3616 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3617 | `	/* Make the key */` |
|        15 | 3618 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3619 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3620 | `	}else{` |
|         9 | 3621 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3622 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3623 | `	}` |
|         - | 3624 | `	/* Insert the current key */` |
|        15 | 3625 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3626 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3627 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3628 | `	/* Advance the cursor */` |
|        15 | 3629 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3630 | `	/* Return the current entry */` |
|        15 | 3631 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3632 | `	return PH7_OK;` |
|        12 | 3633 | `}` |
|         - | 3634 | `/*` |
|         - | 3635 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3636 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3637 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3638 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3639 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3640 | ` */` |
|         - | 3641 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3642 | `/*` |
|         - | 3643 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3644 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3645 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3646 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3647 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3648 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3649 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3650 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3651 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3652 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3653 | ` */` |
|         - | 3654 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3655 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3656 | `/*` |
|         - | 3657 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3658 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3659 | ` */` |
|       ! 0 | 3660 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3661 | `{` |
|       ! 0 | 3662 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3663 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3664 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3665 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3666 | `		zBuf[n] = 0;` |
|       ! 0 | 3667 | `		return zBuf;` |
|         - | 3668 | `	}` |
|       ! 0 | 3669 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3670 | `}` |
|         - | 3671 | `/*` |
|         - | 3672 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3673 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3674 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3675 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3676 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3677 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3678 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3679 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3680 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3681 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3682 | ` */` |
|       156 | 3683 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3684 | `{` |
|       157 | 3685 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3686 | `	sxu64 uVal = 0;` |
|       157 | 3687 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3688 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3689 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3690 | `		bNeg = (z[0] == '-');` |
|         3 | 3691 | `		z++;` |
|         1 | 3692 | `	}` |
|       237 | 3693 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3694 | `		int d = z[0] - '0';` |
|         - | 3695 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3696 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3697 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3698 | `			bOverflow = 1;` |
|       ! 0 | 3699 | `		}else{` |
|        81 | 3700 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3701 | `		}` |
|        81 | 3702 | `		bDigit = 1;` |
|        81 | 3703 | `		z++;` |
|         1 | 3704 | `	}` |
|       157 | 3705 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3706 | `		bReal = 1;` |
|         3 | 3707 | `		z++;` |
|         5 | 3708 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3709 | `			bDigit = 1;` |
|         3 | 3710 | `			z++;` |
|         1 | 3711 | `		}` |
|         1 | 3712 | `	}` |
|         - | 3713 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3714 | `	if( !bDigit ){` |
|        61 | 3715 | `		return RANGE_IN_ERROR;` |
|         - | 3716 | `	}` |
|         - | 3717 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3718 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3719 | `		z++;` |
|         9 | 3720 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3721 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3722 | `			return RANGE_IN_ERROR;` |
|         - | 3723 | `		}` |
|         9 | 3724 | `		bReal = 1;` |
|        17 | 3725 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3726 | `	}` |
|         - | 3727 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3728 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3729 | `	if( z != zEnd ){` |
|        13 | 3730 | `		return RANGE_IN_ERROR;` |
|         - | 3731 | `	}` |
|        84 | 3732 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3733 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3734 | `		bReal = 1;` |
|        84 | 3735 | `	}` |
|        43 | 3736 | `	if( bReal ){` |
|        11 | 3737 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3738 | `		return RANGE_IN_DOUBLE;` |
|         - | 3739 | `	}` |
|         - | 3740 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3741 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3742 | `	return RANGE_IN_LONG;` |
|        58 | 3743 | `}` |
|         - | 3744 | `/*` |
|         - | 3745 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3746 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3747 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3748 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3749 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3750 | ` */` |
|       328 | 3751 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3752 | `{` |
|         - | 3753 | `	char zMsg[160];` |
|       329 | 3754 | `	*pRc = PH7_OK;` |
|       329 | 3755 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3756 | `		char zType[80];` |
|       ! 0 | 3757 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3758 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3759 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3760 | `		return FALSE;` |
|         - | 3761 | `	}` |
|       329 | 3762 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3763 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3764 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3765 | `			iArg,zName);` |
|         5 | 3766 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3767 | `		*pbNullCoerced = TRUE;` |
|         2 | 3768 | `	}` |
|       329 | 3769 | `	return TRUE;` |
|       165 | 3770 | `}` |
|         - | 3771 | `/*` |
|         - | 3772 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3773 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3774 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3775 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3776 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3777 | ` */` |
|        60 | 3778 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3779 | `{` |
|        61 | 3780 | `	*pRc = PH7_OK;` |
|        61 | 3781 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3782 | `		char zType[80];` |
|       ! 0 | 3783 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3784 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3785 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3786 | `		return RANGE_IN_ERROR;` |
|         - | 3787 | `	}` |
|        61 | 3788 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3789 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3790 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3791 | `		*pLong = 0;` |
|         3 | 3792 | `		return RANGE_IN_LONG;` |
|         - | 3793 | `	}` |
|        59 | 3794 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3795 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3796 | `		return RANGE_IN_DOUBLE;` |
|         - | 3797 | `	}` |
|        35 | 3798 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3799 | `		const char *zStr;` |
|         - | 3800 | `		int nLen;` |
|         - | 3801 | `		sxu8 iKind;` |
|         3 | 3802 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3803 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3804 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3805 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3806 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3807 | `		}` |
|         3 | 3808 | `		return iKind;` |
|         - | 3809 | `	}` |
|         - | 3810 | `	/* int / bool */` |
|        33 | 3811 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3812 | `	return RANGE_IN_LONG;` |
|        31 | 3813 | `}` |
|         - | 3814 | `/*` |
|         - | 3815 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3816 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3817 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3818 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3819 | ` */` |
|       296 | 3820 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3821 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3822 | `{` |
|         - | 3823 | `	char zMsg[160];` |
|         - | 3824 | `	double r;` |
|       297 | 3825 | `	*pRc = PH7_OK;` |
|       297 | 3826 | `	if( bNullCoerced ){` |
|         - | 3827 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3828 | `		*pLong = 0;` |
|         5 | 3829 | `		*pDouble = 0.0;` |
|         5 | 3830 | `		return RANGE_IN_LONG;` |
|         - | 3831 | `	}` |
|       293 | 3832 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3833 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3834 | `check_dval:` |
|        25 | 3835 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3836 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3837 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3838 | `			return RANGE_IN_ERROR;` |
|         - | 3839 | `		}` |
|        21 | 3840 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3841 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3842 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3843 | `			return RANGE_IN_ERROR;` |
|         - | 3844 | `		}` |
|        17 | 3845 | `		*pDouble = r;` |
|        17 | 3846 | `		return RANGE_IN_DOUBLE;` |
|         - | 3847 | `	}` |
|       273 | 3848 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3849 | `		const char *zStr;` |
|         - | 3850 | `		int nLen;` |
|         - | 3851 | `		sxu8 iKind;` |
|        81 | 3852 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3853 | `		if( nLen == 0 ){` |
|         7 | 3854 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3855 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3856 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3857 | `			*pLong = 0;` |
|         5 | 3858 | `			*pDouble = 0.0;` |
|        41 | 3859 | `			return RANGE_IN_LONG;` |
|         - | 3860 | `		}` |
|        77 | 3861 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3862 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3863 | `			r = *pDouble;` |
|         5 | 3864 | `			goto check_dval;` |
|         - | 3865 | `		}` |
|        73 | 3866 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3867 | `			*pDouble = (double)*pLong;` |
|        23 | 3868 | `			if( nLen == 1 ){` |
|         - | 3869 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3870 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3871 | `				return RANGE_IN_DIGIT;` |
|         - | 3872 | `			}` |
|        15 | 3873 | `			return RANGE_IN_LONG;` |
|         - | 3874 | `		}` |
|        51 | 3875 | `		if( nLen != 1 ){` |
|        10 | 3876 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3877 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3878 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3879 | `		}` |
|        51 | 3880 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3881 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3882 | `		*pLong = 0;` |
|        51 | 3883 | `		*pDouble = 0.0;` |
|        51 | 3884 | `		return RANGE_IN_STRING;` |
|         - | 3885 | `	}` |
|         - | 3886 | `	/* int / bool */` |
|       193 | 3887 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3888 | `	*pDouble = (double)*pLong;` |
|       193 | 3889 | `	return RANGE_IN_LONG;` |
|       149 | 3890 | `}` |
|         - | 3891 | `/*` |
|         - | 3892 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3893 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3894 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3895 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3896 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3897 | ` * exactly like php's two macros.` |
|         - | 3898 | ` */` |
|         6 | 3899 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3900 | `{` |
|        10 | 3901 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3902 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3903 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3904 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3905 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3906 | `}` |
|         6 | 3907 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3908 | `{` |
|         - | 3909 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3910 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3911 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3912 | `	const unsigned int nBuf = 1500;` |
|         7 | 3913 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3914 | `	if( zMsg == 0 ){` |
|       ! 0 | 3915 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3916 | `	}` |
|         7 | 3917 | `	snprintf(zMsg,nBuf,` |
|         - | 3918 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3919 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3920 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3921 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3922 | `}` |
|         - | 3923 | `/*` |
|         - | 3924 | ` * Set the element container to the next range element and append it to the` |
|         - | 3925 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3926 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3927 | ` * below stay one line per iteration.` |
|         - | 3928 | ` */` |
|      1680 | 3929 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3930 | `{` |
|      1681 | 3931 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3932 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3933 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3934 | `	}` |
|      1681 | 3935 | `	return PH7_OK;` |
|       841 | 3936 | `}` |
|        70 | 3937 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3938 | `{` |
|        71 | 3939 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3940 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3941 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3942 | `	}` |
|        71 | 3943 | `	return PH7_OK;` |
|        36 | 3944 | `}` |
|       168 | 3945 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3946 | `{` |
|       169 | 3947 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3948 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3949 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3950 | `	}` |
|       169 | 3951 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3952 | `	return PH7_OK;` |
|        85 | 3953 | `}` |
|         - | 3954 | `/*` |
|         - | 3955 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3956 | ` *  Create an array containing a range of elements.` |
|         - | 3957 | ` * Return` |
|         - | 3958 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3959 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3960 | ` */` |
|       166 | 3961 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3962 | `{` |
|         - | 3963 | `	ph7_value *pValue,*pArray;` |
|       167 | 3964 | `	sxi32 rc = PH7_OK;` |
|       167 | 3965 | `	int is_step_double = 0,is_step_negative = 0;` |
|       167 | 3966 | `	double step_double = 1.0;` |
|       167 | 3967 | `	sxi64 step = 1;` |
|         - | 3968 | `	sxu8 start_type,end_type;` |
|       167 | 3969 | `	sxi64 start_long = 0,end_long = 0;` |
|       167 | 3970 | `	double start_double = 0.0,end_double = 0.0;` |
|       167 | 3971 | `	unsigned char cStart = 0,cEnd = 0;` |
|       167 | 3972 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3973 | `	sxu32 i,size;` |
|         - | 3974 |  |
|         - | 3975 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       167 | 3976 | `	if( nArg > 3 ){` |
|         4 | 3977 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3978 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3979 | `	}` |
|       165 | 3980 | `	if( nArg < 2 ){` |
|         - | 3981 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3982 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3983 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3984 | `	}` |
|         - | 3985 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3986 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       165 | 3987 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 3988 | `		return rc;` |
|         - | 3989 | `	}` |
|       165 | 3990 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3991 | `		return rc;` |
|         - | 3992 | `	}` |
|       165 | 3993 | `	if( nArg > 2 ){` |
|        61 | 3994 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 3995 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 3996 | `			return rc;` |
|         - | 3997 | `		}` |
|        59 | 3998 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3999 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 4000 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4001 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 4002 | `			}` |
|        23 | 4003 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 4004 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4005 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 4006 | `			}` |
|         - | 4007 | `			/* We only want positive step values. */` |
|        21 | 4008 | `			if( step_double < 0.0 ){` |
|       ! 0 | 4009 | `				is_step_negative = 1;` |
|       ! 0 | 4010 | `				step_double *= -1;` |
|       ! 0 | 4011 | `			}` |
|         - | 4012 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 4013 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 4014 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 4015 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 4016 | `				step = (sxi64)step_double;` |
|        19 | 4017 | `				if( (double)step != step_double ){` |
|        17 | 4018 | `					is_step_double = 1;` |
|         8 | 4019 | `				}` |
|        10 | 4020 | `			}else{` |
|         - | 4021 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 4022 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 4023 | `				is_step_double = 1;` |
|         - | 4024 | `			}` |
|        11 | 4025 | `		}else{` |
|         - | 4026 | `			/* We only want positive step values. */` |
|        35 | 4027 | `			if( step < 0 ){` |
|        11 | 4028 | `				if( step == SMALLEST_INT64 ){` |
|         - | 4029 | `					/* -step would overflow */` |
|         4 | 4030 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 4031 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 4032 | `				}` |
|         9 | 4033 | `				is_step_negative = 1;` |
|         9 | 4034 | `				step = -step;` |
|         4 | 4035 | `			}` |
|        33 | 4036 | `			step_double = (double)step;` |
|         - | 4037 | `		}` |
|        53 | 4038 | `		if( step_double == 0.0 ){` |
|         7 | 4039 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4040 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 4041 | `		}` |
|        23 | 4042 | `	}` |
|       151 | 4043 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 4044 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 4045 | `		return rc;` |
|         - | 4046 | `	}` |
|       147 | 4047 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 4048 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 4049 | `		return rc;` |
|         - | 4050 | `	}` |
|         - | 4051 | `	/* Element container + result array */` |
|       143 | 4052 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 4053 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 4054 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4055 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4056 | `	}` |
|         - | 4057 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 4058 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4059 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4060 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4061 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4062 | `			 * and the range is numeric. */` |
|        15 | 4063 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4064 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4065 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4066 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4067 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4068 | `				}` |
|         7 | 4069 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4070 | `			}else{` |
|         9 | 4071 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4072 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4073 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4074 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4075 | `				}` |
|         9 | 4076 | `				start_type = RANGE_IN_LONG;` |
|         - | 4077 | `			}` |
|        15 | 4078 | `			goto handle_numeric_inputs;` |
|         - | 4079 | `		}` |
|        23 | 4080 | `		if( is_step_double ){` |
|         - | 4081 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4082 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4083 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4084 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4085 | `					" of characters, inputs converted to 0");` |
|         1 | 4086 | `			}` |
|         5 | 4087 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4088 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4089 | `			goto handle_numeric_inputs;` |
|         - | 4090 | `		}` |
|         - | 4091 | `		/* Generate an array of characters */` |
|        19 | 4092 | `		if( cStart > cEnd ){` |
|         - | 4093 | `			/* Decreasing char range */` |
|         - | 4094 | `			int iCur;` |
|         3 | 4095 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4096 | `				goto boundary_error;` |
|         - | 4097 | `			}` |
|        17 | 4098 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4099 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4100 | `					return rc;` |
|         - | 4101 | `				}` |
|         8 | 4102 | `			}` |
|        18 | 4103 | `		}else if( cEnd > cStart ){` |
|         - | 4104 | `			/* Increasing char range */` |
|         - | 4105 | `			int iCur;` |
|        15 | 4106 | `			if( is_step_negative ){` |
|         3 | 4107 | `				goto negative_step_error;` |
|         - | 4108 | `			}` |
|        13 | 4109 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4110 | `				goto boundary_error;` |
|         - | 4111 | `			}` |
|       163 | 4112 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4113 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4114 | `					return rc;` |
|         - | 4115 | `				}` |
|        77 | 4116 | `			}` |
|         6 | 4117 | `		}else{` |
|         3 | 4118 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4119 | `				return rc;` |
|         - | 4120 | `			}` |
|         - | 4121 | `		}` |
|        15 | 4122 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4123 | `		return PH7_OK;` |
|         - | 4124 | `	}` |
|        53 | 4125 | `handle_numeric_inputs:` |
|       133 | 4126 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4127 | `		/* Float range */` |
|         - | 4128 | `		double elem,calc;` |
|        25 | 4129 | `		if( start_double > end_double ){` |
|         - | 4130 | `			/* Decreasing float range */` |
|         7 | 4131 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4132 | `				goto boundary_error;` |
|         - | 4133 | `			}` |
|         7 | 4134 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4135 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4136 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4137 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4138 | `			}` |
|         5 | 4139 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4140 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4141 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4142 | `					return rc;` |
|         - | 4143 | `				}` |
|         8 | 4144 | `			}` |
|        21 | 4145 | `		}else if( end_double > start_double ){` |
|         - | 4146 | `			/* Increasing float range */` |
|        17 | 4147 | `			if( is_step_negative ){` |
|       ! 0 | 4148 | `				goto negative_step_error;` |
|         - | 4149 | `			}` |
|        17 | 4150 | `			if( end_double - start_double < step_double ){` |
|         3 | 4151 | `				goto boundary_error;` |
|         - | 4152 | `			}` |
|        15 | 4153 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4154 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4155 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4156 | `			}` |
|        11 | 4157 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4158 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4159 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4160 | `					return rc;` |
|         - | 4161 | `				}` |
|        28 | 4162 | `			}` |
|         6 | 4163 | `		}else{` |
|         3 | 4164 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4165 | `				return rc;` |
|         - | 4166 | `			}` |
|         - | 4167 | `		}` |
|         9 | 4168 | `	}else{` |
|         - | 4169 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4170 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4171 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4172 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4173 | `		sxu64 calc;` |
|       101 | 4174 | `		if( start_long > end_long ){` |
|         - | 4175 | `			/* Decreasing int range */` |
|        19 | 4176 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4177 | `				goto boundary_error;` |
|         - | 4178 | `			}` |
|        17 | 4179 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4180 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4181 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4182 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4183 | `			}` |
|        15 | 4184 | `			size = (sxu32)(calc + 1);` |
|       101 | 4185 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4186 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4187 | `					return rc;` |
|         - | 4188 | `				}` |
|        44 | 4189 | `			}` |
|        90 | 4190 | `		}else if( end_long > start_long ){` |
|         - | 4191 | `			/* Increasing int range */` |
|        77 | 4192 | `			if( is_step_negative ){` |
|         3 | 4193 | `				goto negative_step_error;` |
|         - | 4194 | `			}` |
|        75 | 4195 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4196 | `				goto boundary_error;` |
|         - | 4197 | `			}` |
|        73 | 4198 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4199 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4200 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4201 | `			}` |
|        69 | 4202 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4203 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4204 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4205 | `					return rc;` |
|         - | 4206 | `				}` |
|       795 | 4207 | `			}` |
|        35 | 4208 | `		}else{` |
|         7 | 4209 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4210 | `				return rc;` |
|         - | 4211 | `			}` |
|         - | 4212 | `		}` |
|         - | 4213 | `	}` |
|         - | 4214 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4215 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4216 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4217 | `	return PH7_OK;` |
|         2 | 4218 | `negative_step_error:` |
|         5 | 4219 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4220 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4221 | `boundary_error:` |
|         9 | 4222 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4223 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        84 | 4224 | `}` |
|         - | 4225 | `/*` |
|         - | 4226 | ` * array array_values(array $array)` |
|         - | 4227 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4228 | ` * Parameters` |
|         - | 4229 | ` *  $array` |
|         - | 4230 | ` *   The input array.` |
|         - | 4231 | ` * Return` |
|         - | 4232 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4233 | ` */` |
|        48 | 4234 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4235 | `{` |
|         - | 4236 | `	ph7_hashmap_node *pNode;` |
|         - | 4237 | `	ph7_hashmap *pMap;` |
|         - | 4238 | `	ph7_value *pArray;` |
|         - | 4239 | `	ph7_value *pObj;` |
|         - | 4240 | `	sxu32 n;` |
|        51 | 4241 | `	if( nArg != 1 ){` |
|         - | 4242 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4243 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4244 | `			"ArgumentCountError",` |
|         - | 4245 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4246 | `			nArg` |
|         - | 4247 | `			);` |
|         - | 4248 | `	}` |
|         - | 4249 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4250 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4251 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4252 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4253 | `			"TypeError",` |
|         - | 4254 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4255 | `			ph7_type_name(apArg[0])` |
|         - | 4256 | `			);` |
|         - | 4257 | `	}` |
|         - | 4258 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4259 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4260 | `	/* Create a new array */` |
|        46 | 4261 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4262 | `	if( pArray == 0 ){` |
|       ! 0 | 4263 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4264 | `		return PH7_OK;` |
|         - | 4265 | `	}` |
|         - | 4266 | `	/* Perform the requested operation */` |
|        46 | 4267 | `	pNode = pMap->pFirst;` |
|       144 | 4268 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4269 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4270 | `		if( pObj ){` |
|         - | 4271 | `			/* perform the insertion */` |
|       100 | 4272 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4273 | `		}` |
|         - | 4274 | `		/* Point to the next entry */` |
|       100 | 4275 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4276 | `	}` |
|         - | 4277 | `	/* return the new array */` |
|        46 | 4278 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4279 | `	return PH7_OK;` |
|        27 | 4280 | `}` |
|         - | 4281 | `/*` |
|         - | 4282 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4283 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4284 | ` * Parameters` |
|         - | 4285 | ` *  $input` |
|         - | 4286 | ` *   An array containing keys to return.` |
|         - | 4287 | ` * $search_value` |
|         - | 4288 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4289 | ` * $strict` |
|         - | 4290 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4291 | ` * Return` |
|         - | 4292 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4293 | ` */` |
|       166 | 4294 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4295 | `{` |
|         - | 4296 | `	ph7_hashmap_node *pNode;` |
|         - | 4297 | `	ph7_hashmap *pMap;` |
|         - | 4298 | `	ph7_value *pArray;` |
|         - | 4299 | `	ph7_value sObj;` |
|         - | 4300 | `	ph7_value sVal;` |
|         - | 4301 | `	SyString sKey;` |
|         - | 4302 | `	int bStrict;` |
|         - | 4303 | `	sxi32 rc;` |
|         - | 4304 | `	sxu32 n;` |
|       170 | 4305 | `	if( nArg < 1 ){` |
|         - | 4306 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4307 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4308 | `			"ArgumentCountError",` |
|         - | 4309 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4310 | `			);` |
|         - | 4311 | `	}` |
|         - | 4312 | `	/* Make sure we are dealing with a valid hashmap */` |
|       170 | 4313 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4314 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4315 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4316 | `			"TypeError",` |
|         - | 4317 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4318 | `			ph7_type_name(apArg[0])` |
|         - | 4319 | `			);` |
|         - | 4320 | `	}` |
|         - | 4321 | `	/* Point to the internal representation of the input hashmap */` |
|       167 | 4322 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4323 | `	/* Create a new array */` |
|       167 | 4324 | `	pArray = ph7_context_new_array(pCtx);` |
|       167 | 4325 | `	if( pArray == 0 ){` |
|       ! 0 | 4326 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4327 | `		return PH7_OK;` |
|         - | 4328 | `	}` |
|       167 | 4329 | `	bStrict = FALSE;` |
|       167 | 4330 | `	if( nArg > 2 ){` |
|         - | 4331 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4332 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4333 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4334 | `				"TypeError",` |
|         - | 4335 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4336 | `				ph7_type_name(apArg[2])` |
|         - | 4337 | `				);` |
|         - | 4338 | `		}` |
|         9 | 4339 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4340 | `	}` |
|         - | 4341 | `	/* Perform the requested operation */` |
|       167 | 4342 | `	pNode = pMap->pFirst;` |
|       167 | 4343 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1489 | 4344 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1325 | 4345 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       193 | 4346 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        98 | 4347 | `		}else{` |
|      1134 | 4348 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1134 | 4349 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4350 | `		}` |
|      1325 | 4351 | `		rc = 0;` |
|      1325 | 4352 | `		if( nArg > 1 ){` |
|        65 | 4353 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4354 | `			if( pValue ){` |
|         - | 4355 | `				ph7_value sNeedle;` |
|        65 | 4356 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4357 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4358 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4359 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4360 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4361 | `				 * corrupt every later comparison. */` |
|        65 | 4362 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4363 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4364 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4365 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4366 | `			}` |
|        32 | 4367 | `		}` |
|      1325 | 4368 | `		if( rc == 0 ){` |
|         - | 4369 | `			/* Perform the insertion */` |
|      1293 | 4370 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       645 | 4371 | `		}` |
|      1325 | 4372 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4373 | `		/* Point to the next entry */` |
|      1325 | 4374 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       664 | 4375 | `	}` |
|         - | 4376 | `	/* return the new array */` |
|       167 | 4377 | `	ph7_result_value(pCtx,pArray);` |
|       167 | 4378 | `	return PH7_OK;` |
|        87 | 4379 | `}` |
|         - | 4380 | `/*` |
|         - | 4381 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4382 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4383 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4384 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4385 | ` * Parameters` |
|         - | 4386 | ` *  $arr1` |
|         - | 4387 | ` *   First array` |
|         - | 4388 | ` *  $arr2` |
|         - | 4389 | ` *   Second array` |
|         - | 4390 | ` * Return` |
|         - | 4391 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4392 | ` * Note` |
|         - | 4393 | ` *  This function is a symisc eXtension.` |
|         - | 4394 | ` */` |
|         4 | 4395 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4396 | `{` |
|         - | 4397 | `	ph7_hashmap *p1,*p2;` |
|         - | 4398 | `	int rc;` |
|         5 | 4399 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4400 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4401 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4402 | `		return PH7_OK;` |
|         - | 4403 | `	}` |
|         - | 4404 | `	/* Point to the hashmaps */` |
|         5 | 4405 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4406 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4407 | `	rc = (p1 == p2);` |
|         - | 4408 | `	/* Same instance? */` |
|         5 | 4409 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4410 | `	return PH7_OK;` |
|         3 | 4411 | `}` |
|         - | 4412 | `/*` |
|         - | 4413 | ` * array array_merge(array ...$arrays)` |
|         - | 4414 | ` *  Merge one or more arrays.` |
|         - | 4415 | ` * Parameters` |
|         - | 4416 | ` *  ...$arrays` |
|         - | 4417 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4418 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4419 | ` * Return` |
|         - | 4420 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4421 | ` *  with no arguments.` |
|         - | 4422 | ` */` |
|      1080 | 4423 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4424 | `{` |
|         - | 4425 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4426 | `	ph7_value *pArray;` |
|         - | 4427 | `	int i;` |
|         - | 4428 | `	/* Create a new array */` |
|      1085 | 4429 | `	pArray = ph7_context_new_array(pCtx);` |
|      1085 | 4430 | `	if( pArray == 0 ){` |
|       ! 0 | 4431 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4432 | `		return PH7_OK;` |
|         - | 4433 | `	}` |
|         - | 4434 | `	/* Point to the internal representation of the hashmap */` |
|      1085 | 4435 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4436 | `	/* Start merging */` |
|      3235 | 4437 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4438 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2159 | 4439 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4440 | `			/* Type mismatch -> TypeError */` |
|         8 | 4441 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4442 | `				"TypeError",` |
|         - | 4443 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4444 | `				i + 1,` |
|         4 | 4445 | `				ph7_type_name(apArg[i])` |
|         - | 4446 | `				);` |
|       ! 0 | 4447 | `		}else{` |
|      2155 | 4448 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4449 | `			/* Merge the two hashmaps */` |
|      2155 | 4450 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4451 | `		}` |
|      1080 | 4452 | `	}` |
|         - | 4453 | `	/* Return the freshly created array */` |
|      1081 | 4454 | `	ph7_result_value(pCtx,pArray);` |
|      1081 | 4455 | `	return PH7_OK;` |
|       545 | 4456 | `}` |
|         - | 4457 | `/*` |
|         - | 4458 | ` * array array_copy(array $source)` |
|         - | 4459 | ` *  Make a blind copy of the target array.` |
|         - | 4460 | ` * Parameters` |
|         - | 4461 | ` *  $source` |
|         - | 4462 | ` *   Target array` |
|         - | 4463 | ` * Return` |
|         - | 4464 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4465 | ` * Note` |
|         - | 4466 | ` *  This function is a symisc eXtension.` |
|         - | 4467 | ` */` |
|        18 | 4468 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4469 | `{` |
|         - | 4470 | `	ph7_hashmap *pMap;` |
|         - | 4471 | `	ph7_value *pArray;` |
|        19 | 4472 | `	if( nArg < 1 ){` |
|         - | 4473 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4474 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4475 | `		return PH7_OK;` |
|         - | 4476 | `	}` |
|         - | 4477 | `	/* Create a new array */` |
|        19 | 4478 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4479 | `	if( pArray == 0 ){` |
|       ! 0 | 4480 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4481 | `		return PH7_OK;` |
|         - | 4482 | `	}` |
|         - | 4483 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4484 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4485 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4486 | `		/* Point to the internal representation of the source */` |
|        19 | 4487 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4488 | `		/* Perform the copy */` |
|        19 | 4489 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4490 | `	}else{` |
|         - | 4491 | `		/* Simple insertion */` |
|       ! 0 | 4492 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4493 | `	}` |
|         - | 4494 | `	/* Return the duplicated array */` |
|        19 | 4495 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4496 | `	return PH7_OK;` |
|        10 | 4497 | `}` |
|         - | 4498 | `/*` |
|         - | 4499 | ` * bool array_erase(array $source)` |
|         - | 4500 | ` *  Remove all elements from a given array.` |
|         - | 4501 | ` * Parameters` |
|         - | 4502 | ` *  $source` |
|         - | 4503 | ` *   Target array` |
|         - | 4504 | ` * Return` |
|         - | 4505 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4506 | ` * Note` |
|         - | 4507 | ` *  This function is a symisc eXtension.` |
|         - | 4508 | ` */` |
|        26 | 4509 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4510 | `{` |
|         - | 4511 | `	ph7_hashmap *pMap;` |
|        28 | 4512 | `	if( nArg < 1 ){` |
|         - | 4513 | `		/* Missing arguments */` |
|       ! 0 | 4514 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4515 | `		return PH7_OK;` |
|         - | 4516 | `	}` |
|         - | 4517 | `	/* Point to the target hashmap */` |
|        28 | 4518 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4519 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4520 | `	/* Erase */` |
|        28 | 4521 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4522 | `	return PH7_OK;` |
|        15 | 4523 | `}` |
|         - | 4524 | `/*` |
|         - | 4525 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4526 | ` *  Extract a slice of the array.` |
|         - | 4527 | ` * Parameters` |
|         - | 4528 | ` *  $array` |
|         - | 4529 | ` *    The input array.` |
|         - | 4530 | ` * $offset` |
|         - | 4531 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4532 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4533 | ` * $length (optional, nullable)` |
|         - | 4534 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4535 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4536 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4537 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4538 | ` * $preserve_keys (optional)` |
|         - | 4539 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4540 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4541 | ` * Return` |
|         - | 4542 | ` *   The new slice.` |
|         - | 4543 | ` */` |
|        66 | 4544 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4545 | `{` |
|         - | 4546 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4547 | `	ph7_hashmap_node *pCur;` |
|         - | 4548 | `	ph7_value *pArray;` |
|         - | 4549 | `	int iLength,iOfft;` |
|         - | 4550 | `	int bPreserve;` |
|         - | 4551 | `	sxi32 rc;` |
|        71 | 4552 | `	if( nArg < 2 ){` |
|       ! 0 | 4553 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4554 | `			"ArgumentCountError",` |
|         - | 4555 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4556 | `			nArg` |
|         - | 4557 | `			);` |
|         - | 4558 | `	}` |
|        71 | 4559 | `	if( nArg > 4 ){` |
|         4 | 4560 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4561 | `			"ArgumentCountError",` |
|         - | 4562 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4563 | `			nArg` |
|         - | 4564 | `			);` |
|         - | 4565 | `	}` |
|        69 | 4566 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4567 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4568 | `			"TypeError",` |
|         - | 4569 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4570 | `			ph7_type_name(apArg[0])` |
|         - | 4571 | `			);` |
|         - | 4572 | `	}` |
|         - | 4573 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        92 | 4574 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        95 | 4575 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4576 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4577 | `			"TypeError",` |
|         - | 4578 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4579 | `			ph7_type_name(apArg[1])` |
|         - | 4580 | `			);` |
|         - | 4581 | `	}` |
|         - | 4582 | `	/* Validate $length type if provided: nullable int */` |
|        65 | 4583 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        56 | 4584 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        56 | 4585 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4586 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4587 | `				"TypeError",` |
|         - | 4588 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4589 | `				ph7_type_name(apArg[2])` |
|         - | 4590 | `				);` |
|         - | 4591 | `		}` |
|        18 | 4592 | `	}` |
|         - | 4593 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        63 | 4594 | `	if( nArg > 3 ){` |
|         7 | 4595 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4596 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4597 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4598 | `				"TypeError",` |
|         - | 4599 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4600 | `				ph7_type_name(apArg[3])` |
|         - | 4601 | `				);` |
|         - | 4602 | `		}` |
|         2 | 4603 | `	}` |
|         - | 4604 | `	/* Point the internal representation of the target array */` |
|        63 | 4605 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        63 | 4606 | `	bPreserve = FALSE;` |
|         - | 4607 | `	/* Get the offset */` |
|         - | 4608 | `	{` |
|        63 | 4609 | `		sxi64 iTmp = 0;` |
|        63 | 4610 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        63 | 4611 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4612 | `			return rcArg;` |
|         - | 4613 | `		}` |
|        63 | 4614 | `		iOfft = (int)iTmp;` |
|         - | 4615 | `	}` |
|        63 | 4616 | `	if( iOfft < 0 ){` |
|         5 | 4617 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4618 | `		if( iOfft < 0 ){` |
|         3 | 4619 | `			iOfft = 0;` |
|         1 | 4620 | `		}` |
|         2 | 4621 | `	}` |
|        63 | 4622 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4623 | `		/* Offset past end of array, return empty array */` |
|         5 | 4624 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4625 | `		if( pArray == 0 ){` |
|       ! 0 | 4626 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4627 | `			return PH7_OK;` |
|         - | 4628 | `		}` |
|         5 | 4629 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4630 | `		return PH7_OK;` |
|         - | 4631 | `	}` |
|         - | 4632 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        59 | 4633 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        59 | 4634 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        37 | 4635 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        37 | 4636 | `		if( iLength < 0 ){` |
|         5 | 4637 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4638 | `		}` |
|        37 | 4639 | `		if( iLength < 0 ){` |
|         3 | 4640 | `			iLength = 0;` |
|         1 | 4641 | `		}` |
|        37 | 4642 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4643 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4644 | `		}` |
|        18 | 4645 | `	}` |
|        59 | 4646 | `	if( nArg > 3 ){` |
|         5 | 4647 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4648 | `	}` |
|         - | 4649 | `	/* Create a new array */` |
|        59 | 4650 | `	pArray = ph7_context_new_array(pCtx);` |
|        59 | 4651 | `	if( pArray == 0 ){` |
|       ! 0 | 4652 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4653 | `		return PH7_OK;` |
|         - | 4654 | `	}` |
|        59 | 4655 | `	if( iLength < 1 ){` |
|         - | 4656 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4657 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4658 | `		return PH7_OK;` |
|         - | 4659 | `	}` |
|         - | 4660 | `	/* Point to the desired entry */` |
|        55 | 4661 | `	pCur = pSrc->pFirst;` |
|        54 | 4662 | `	for(;;){` |
|       113 | 4663 | `		if( iOfft < 1 ){` |
|        55 | 4664 | `			break;` |
|         - | 4665 | `		}` |
|         - | 4666 | `		/* Point to the next entry */` |
|        63 | 4667 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        63 | 4668 | `		iOfft--;` |
|         5 | 4669 | `	}` |
|         - | 4670 | `	/* Point to the internal representation of the hashmap */` |
|        55 | 4671 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       106 | 4672 | `	for(;;){` |
|       217 | 4673 | `		if( iLength < 1 ){` |
|        55 | 4674 | `			break;` |
|         - | 4675 | `		}` |
|         - | 4676 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4677 | `		{` |
|       167 | 4678 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|       167 | 4679 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4680 | `		}` |
|       167 | 4681 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4682 | `			break;` |
|         - | 4683 | `		}` |
|         - | 4684 | `		/* Point to the next entry */` |
|       167 | 4685 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       167 | 4686 | `		iLength--;` |
|         5 | 4687 | `	}` |
|         - | 4688 | `	/* Return the freshly created array */` |
|        55 | 4689 | `	ph7_result_value(pCtx,pArray);` |
|        55 | 4690 | `	return PH7_OK;` |
|        38 | 4691 | `}` |
|         - | 4692 | `/*` |
|         - | 4693 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4694 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4695 | ` * beginning (becomes the new pFirst).` |
|         - | 4696 | ` */` |
|        38 | 4697 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4698 | `{` |
|         - | 4699 | `	ph7_hashmap_node *pNode;` |
|         - | 4700 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4701 | `	pNode = pMap->pLast;` |
|        39 | 4702 | `	if( pNode == 0 ){` |
|       ! 0 | 4703 | `		return;` |
|         - | 4704 | `	}` |
|        39 | 4705 | `	if( pNode->pNext == 0 ){` |
|         - | 4706 | `		/* Only node in the list, nothing to move */` |
|         5 | 4707 | `		return;` |
|         - | 4708 | `	}` |
|        35 | 4709 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4710 | `		/* Already in the correct position */` |
|         9 | 4711 | `		return;` |
|         - | 4712 | `	}` |
|         - | 4713 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4714 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4715 | `	pMap->pLast->pPrev = 0;` |
|         - | 4716 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4717 | `	if( pAfter == 0 ){` |
|         - | 4718 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4719 | `		pNode->pNext = 0;` |
|         3 | 4720 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4721 | `		if( pMap->pFirst ){` |
|         3 | 4722 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4723 | `		}` |
|         3 | 4724 | `		pMap->pFirst = pNode;` |
|         2 | 4725 | `	}else{` |
|        25 | 4726 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4727 | `		pNode->pPrev = pOldNext;` |
|        25 | 4728 | `		pNode->pNext = pAfter;` |
|        25 | 4729 | `		pAfter->pPrev = pNode;` |
|        25 | 4730 | `		if( pOldNext ){` |
|        25 | 4731 | `			pOldNext->pNext = pNode;` |
|        13 | 4732 | `		}else{` |
|       ! 0 | 4733 | `			pMap->pLast = pNode;` |
|         - | 4734 | `		}` |
|         - | 4735 | `	}` |
|        20 | 4736 | `}` |
|         - | 4737 | `/*` |
|         - | 4738 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4739 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4740 | ` * Parameters` |
|         - | 4741 | ` *  $array` |
|         - | 4742 | ` *    The input array.` |
|         - | 4743 | ` *  $offset` |
|         - | 4744 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4745 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4746 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4747 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4748 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4749 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4750 | ` *  $length (optional)` |
|         - | 4751 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4752 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4753 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4754 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4755 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4756 | ` *  $replacement (optional)` |
|         - | 4757 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4758 | ` *    with elements from this array.` |
|         - | 4759 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4760 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4761 | ` *    offset.` |
|         - | 4762 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4763 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4764 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4765 | ` * Return` |
|         - | 4766 | ` *   A new array consisting of the extracted elements.` |
|         - | 4767 | ` */` |
|        64 | 4768 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4769 | `{` |
|         - | 4770 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4771 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4772 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4773 | `	int iLength,iOfft,i;` |
|         - | 4774 | `	sxi32 rc;` |
|        66 | 4775 | `	if( nArg < 2 ){` |
|       ! 0 | 4776 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4777 | `			"ArgumentCountError",` |
|         - | 4778 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4779 | `			nArg` |
|         - | 4780 | `			);` |
|         - | 4781 | `	}` |
|        66 | 4782 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4783 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4784 | `			"TypeError",` |
|         - | 4785 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4786 | `			ph7_type_name(apArg[0])` |
|         - | 4787 | `			);` |
|         - | 4788 | `	}` |
|         - | 4789 | `	/* Point to the internal representation of the target array */` |
|        63 | 4790 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4791 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4792 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4793 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4794 | `	if( iOfft < 0 ){` |
|         9 | 4795 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4796 | `		if( iOfft < 0 ){` |
|         3 | 4797 | `			iOfft = 0;` |
|         2 | 4798 | `		}` |
|        59 | 4799 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4800 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4801 | `	}` |
|         - | 4802 | `	/* Get the length and clamp to valid range.` |
|         - | 4803 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4804 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4805 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4806 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4807 | `		if( iLength < 0 ){` |
|         7 | 4808 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4809 | `			if( iLength < 0 ){` |
|         3 | 4810 | `				iLength = 0;` |
|         1 | 4811 | `			}` |
|         3 | 4812 | `		}` |
|        45 | 4813 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4814 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4815 | `		}` |
|        22 | 4816 | `	}` |
|         - | 4817 | `	/* Create the result array for removed elements */` |
|        63 | 4818 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4819 | `	if( pArray == 0 ){` |
|       ! 0 | 4820 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4821 | `		return PH7_OK;` |
|         - | 4822 | `	}` |
|         - | 4823 | `	/* Get replacement array if provided */` |
|        63 | 4824 | `	pRep = 0;` |
|        63 | 4825 | `	if( nArg > 3 ){` |
|        27 | 4826 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4827 | `			/* Perform an array cast */` |
|         3 | 4828 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4829 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4830 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4831 | `			}` |
|         2 | 4832 | `		}else{` |
|        25 | 4833 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4834 | `		}` |
|        27 | 4835 | `		if( pRep ){` |
|         - | 4836 | `			/* Reset the loop cursor */` |
|        27 | 4837 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4838 | `		}` |
|        13 | 4839 | `	}` |
|         - | 4840 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4841 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4842 | `	/* Navigate to the offset position */` |
|        63 | 4843 | `	pCur = pSrc->pFirst;` |
|       131 | 4844 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4845 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4846 | `	}` |
|         - | 4847 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4848 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4849 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4850 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4851 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4852 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4853 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4854 | `		pPrev = pCur->pPrev;` |
|        79 | 4855 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4856 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4857 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4858 | `			break;` |
|         - | 4859 | `		}` |
|        79 | 4860 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4861 | `	}` |
|         - | 4862 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4863 | `	if( pRep ){` |
|         - | 4864 | `		ph7_value sSafeVal;` |
|        78 | 4865 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4866 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4867 | `			if( pRvalue ){` |
|         - | 4868 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4869 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4870 | `				 * since it points into that same pool. */` |
|        39 | 4871 | `				sSafeVal = *pRvalue;` |
|        39 | 4872 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4873 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4874 | `					pNewNode = pSrc->pLast;` |
|        39 | 4875 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4876 | `					pInsertAfter = pNewNode;` |
|        19 | 4877 | `				}` |
|        19 | 4878 | `			}` |
|         1 | 4879 | `		}` |
|        13 | 4880 | `	}` |
|         - | 4881 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4882 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4883 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4884 | `	 * and removals left gaps. */` |
|         - | 4885 | `	{` |
|        63 | 4886 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4887 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4888 | `		pSrc->iNextIdx = 0;` |
|       233 | 4889 | `		while( n > 0 ){` |
|       171 | 4890 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4891 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4892 | `			}` |
|       171 | 4893 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4894 | `			n--;` |
|         1 | 4895 | `		}` |
|        63 | 4896 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4897 | `	}` |
|         - | 4898 | `	/* Return the freshly created array */` |
|        63 | 4899 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4900 | `	return PH7_OK;` |
|        34 | 4901 | `}` |
|         - | 4902 | `/*` |
|         - | 4903 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4904 | ` *  Checks if a value exists in an array.` |
|         - | 4905 | ` * Parameters` |
|         - | 4906 | ` *  $needle` |
|         - | 4907 | ` *   The searched value.` |
|         - | 4908 | ` *   Note:` |
|         - | 4909 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4910 | ` * $haystack` |
|         - | 4911 | ` *  The target array.` |
|         - | 4912 | ` * $strict` |
|         - | 4913 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4914 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4915 | ` */` |
|     32990 | 4916 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4917 | `{` |
|         - | 4918 | `	ph7_value *pNeedle;` |
|         - | 4919 | `	int bStrict;` |
|         - | 4920 | `	int rc;` |
|     32995 | 4921 | `	if( nArg < 2 ){` |
|         - | 4922 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4923 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4924 | `		return PH7_OK;` |
|         - | 4925 | `	}` |
|     32995 | 4926 | `	pNeedle = apArg[0];` |
|     32995 | 4927 | `	bStrict = 0;` |
|     32995 | 4928 | `	if( nArg > 2 ){` |
|        53 | 4929 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4930 | `	}` |
|     32995 | 4931 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4932 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4933 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4934 | `		/* Set the comparison result */` |
|       ! 0 | 4935 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4936 | `		return PH7_OK;` |
|         - | 4937 | `	}` |
|         - | 4938 | `	/* Perform the lookup */` |
|     32995 | 4939 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4940 | `	/* Lookup result */` |
|     32995 | 4941 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32995 | 4942 | `	return PH7_OK;` |
|     16500 | 4943 | `}` |
|         - | 4944 | `/*` |
|         - | 4945 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4946 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4947 | ` * Parameters` |
|         - | 4948 | ` * $needle` |
|         - | 4949 | ` *   The searched value.` |
|         - | 4950 | ` * $haystack` |
|         - | 4951 | ` *   The array.` |
|         - | 4952 | ` * $strict` |
|         - | 4953 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4954 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4955 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4956 | ` * Return` |
|         - | 4957 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4958 | ` */` |
|        26 | 4959 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4960 | `{` |
|         - | 4961 | `	ph7_hashmap_node *pEntry;` |
|         - | 4962 | `	ph7_value *pVal,sNeedle;` |
|         - | 4963 | `	ph7_hashmap *pMap;` |
|         - | 4964 | `	ph7_value sVal;` |
|         - | 4965 | `	int bStrict;` |
|         - | 4966 | `	sxu32 n;` |
|         - | 4967 | `	int rc;` |
|        28 | 4968 | `	if( nArg < 2 ){` |
|         - | 4969 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4970 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4971 | `			"ArgumentCountError",` |
|         - | 4972 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 4973 | `			nArg` |
|         - | 4974 | `			);` |
|         - | 4975 | `	}` |
|        28 | 4976 | `	bStrict = FALSE;` |
|        28 | 4977 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4978 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4979 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4980 | `			"TypeError",` |
|         - | 4981 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4982 | `			ph7_type_name(apArg[1])` |
|         - | 4983 | `			);` |
|         - | 4984 | `	}` |
|        25 | 4985 | `	if( nArg > 2 ){` |
|         - | 4986 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        11 | 4987 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4988 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4989 | `				"TypeError",` |
|         - | 4990 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4991 | `				ph7_type_name(apArg[2])` |
|         - | 4992 | `				);` |
|         - | 4993 | `		}` |
|        11 | 4994 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4995 | `	}` |
|         - | 4996 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4997 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4998 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4999 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 5000 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 5001 | `	pEntry = pMap->pFirst;` |
|        25 | 5002 | `	n = pMap->nEntry;` |
|        28 | 5003 | `	for(;;){` |
|        57 | 5004 | `		if( !n ){` |
|         9 | 5005 | `			break;` |
|         - | 5006 | `		}` |
|         - | 5007 | `		/* Extract node value */` |
|        49 | 5008 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5009 | `		if( pVal ){` |
|         - | 5010 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 5011 | `			 * can change their type.` |
|         - | 5012 | `			 */` |
|        49 | 5013 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 5014 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 5015 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 5016 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 5017 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 5018 | `			if( rc == 0 ){` |
|         - | 5019 | `				/* Match found,return key */` |
|        17 | 5020 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 5021 | `					/* INT key */` |
|        11 | 5022 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 5023 | `				}else{` |
|         7 | 5024 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5025 | `					/* Blob key */` |
|         7 | 5026 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 5027 | `				}` |
|        17 | 5028 | `				return PH7_OK;` |
|         - | 5029 | `			}` |
|        16 | 5030 | `		}` |
|         - | 5031 | `		/* Point to the next entry */` |
|        33 | 5032 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5033 | `		n--;` |
|         1 | 5034 | `	}` |
|         - | 5035 | `	/* No such value,return FALSE */` |
|         9 | 5036 | `	ph7_result_bool(pCtx,0);` |
|         9 | 5037 | `	return PH7_OK;` |
|        15 | 5038 | `}` |
|         - | 5039 | `/*` |
|         - | 5040 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 5041 | ` *  Computes the difference of arrays.` |
|         - | 5042 | ` * Parameters` |
|         - | 5043 | ` *  $array1` |
|         - | 5044 | ` *    The array to compare from` |
|         - | 5045 | ` *  $array2` |
|         - | 5046 | ` *    An array to compare against` |
|         - | 5047 | ` *  $...` |
|         - | 5048 | ` *   More arrays to compare against` |
|         - | 5049 | ` * Return` |
|         - | 5050 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5051 | ` *  are not present in any of the other arrays.` |
|         - | 5052 | ` */` |
|        20 | 5053 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5054 | `{` |
|         - | 5055 | `	ph7_hashmap_node *pEntry;` |
|         - | 5056 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5057 | `	ph7_value *pArray;` |
|         - | 5058 | `	ph7_value *pVal;` |
|         - | 5059 | `	sxi32 rc;` |
|         - | 5060 | `	sxu32 n;` |
|         - | 5061 | `	int i;` |
|         - | 5062 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5063 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5064 | `	 * debugging difficult. */` |
|        23 | 5065 | `	if( nArg < 1 ){` |
|       ! 0 | 5066 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5067 | `			"ArgumentCountError",` |
|         - | 5068 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5069 | `			nArg` |
|         - | 5070 | `			);` |
|         - | 5071 | `	}` |
|        23 | 5072 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5073 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5074 | `			"TypeError",` |
|         - | 5075 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5076 | `			ph7_type_name(apArg[0])` |
|         - | 5077 | `			);` |
|         - | 5078 | `	}` |
|        36 | 5079 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5080 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5081 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5082 | `				"TypeError",` |
|         - | 5083 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5084 | `				i + 1,` |
|         2 | 5085 | `				ph7_type_name(apArg[i])` |
|         - | 5086 | `				);` |
|         - | 5087 | `		}` |
|         9 | 5088 | `	}` |
|        17 | 5089 | `	if( nArg == 1 ){` |
|         - | 5090 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5091 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5092 | `		return PH7_OK;` |
|         - | 5093 | `	}` |
|         - | 5094 | `	/* Create a new array */` |
|        15 | 5095 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5096 | `	if( pArray == 0 ){` |
|       ! 0 | 5097 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5098 | `		return PH7_OK;` |
|         - | 5099 | `	}` |
|         - | 5100 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5101 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5102 | `	/* Perform the diff */` |
|        15 | 5103 | `	pEntry = pSrc->pFirst;` |
|        15 | 5104 | `	n = pSrc->nEntry;` |
|        27 | 5105 | `	for(;;){` |
|        55 | 5106 | `		if( n < 1 ){` |
|        15 | 5107 | `			break;` |
|         - | 5108 | `		}` |
|         - | 5109 | `		/* Extract the node value */` |
|        41 | 5110 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5111 | `		if( pVal ){` |
|        69 | 5112 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5113 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5114 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5115 | `				/* Perform the lookup */` |
|        45 | 5116 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5117 | `				if( rc == SXRET_OK ){` |
|         - | 5118 | `					/* Value exist */` |
|        17 | 5119 | `					break;` |
|         - | 5120 | `				}` |
|        15 | 5121 | `			}` |
|        41 | 5122 | `			if( i >= nArg ){` |
|         - | 5123 | `				/* Perform the insertion */` |
|        25 | 5124 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5125 | `			}` |
|        20 | 5126 | `		}` |
|         - | 5127 | `		/* Point to the next entry */` |
|        41 | 5128 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5129 | `		n--;` |
|         1 | 5130 | `	}` |
|         - | 5131 | `	/* Return the freshly created array */` |
|        15 | 5132 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5133 | `	return PH7_OK;` |
|        13 | 5134 | `}` |
|         - | 5135 | `/*` |
|         - | 5136 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5137 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5138 | ` * Parameters` |
|         - | 5139 | ` *  $array1` |
|         - | 5140 | ` *    The array to compare from` |
|         - | 5141 | ` *  $array2` |
|         - | 5142 | ` *    An array to compare against` |
|         - | 5143 | ` *  $...` |
|         - | 5144 | ` *   More arrays to compare against.` |
|         - | 5145 | ` * $callback` |
|         - | 5146 | ` *  The callback comparison function.` |
|         - | 5147 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5148 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5149 | ` *  than the second.` |
|         - | 5150 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5151 | ` * Return` |
|         - | 5152 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5153 | ` *  are not present in any of the other arrays.` |
|         - | 5154 | ` */` |
|        20 | 5155 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5156 | `{` |
|         - | 5157 | `	ph7_hashmap_node *pEntry;` |
|         - | 5158 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5159 | `	ph7_value *pCallback;` |
|         - | 5160 | `	ph7_value *pArray;` |
|         - | 5161 | `	ph7_value *pVal;` |
|         - | 5162 | `	sxi32 rc;` |
|         - | 5163 | `	sxu32 n;` |
|         - | 5164 | `	int i;` |
|         - | 5165 |  |
|         - | 5166 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5167 | `	if( nArg < 2 ){` |
|       ! 0 | 5168 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5169 | `			"ArgumentCountError",` |
|         - | 5170 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5171 | `			nArg` |
|         - | 5172 | `			);` |
|         - | 5173 | `	}` |
|        25 | 5174 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5175 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5176 | `			"TypeError",` |
|         - | 5177 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5178 | `			ph7_type_name(apArg[0])` |
|         - | 5179 | `			);` |
|         - | 5180 | `	}` |
|         - | 5181 |  |
|        23 | 5182 | `	if( nArg == 2 ){` |
|         - | 5183 | `		/* Only the original array and the callback were provided. */` |
|         - | 5184 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5185 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5186 | `		 * validation order.` |
|         - | 5187 | `		 */` |
|         4 | 5188 | `	} else {` |
|         - | 5189 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5190 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5191 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5192 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5193 | `					"TypeError",` |
|         - | 5194 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5195 | `					i + 1,` |
|         6 | 5196 | `					ph7_type_name(apArg[i])` |
|         - | 5197 | `					);` |
|         - | 5198 | `			}` |
|         7 | 5199 | `		}` |
|         - | 5200 | `	}` |
|         - | 5201 |  |
|         - | 5202 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5203 | `	pCallback = apArg[nArg - 1];` |
|         - | 5204 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5205 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5206 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5207 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5208 | `				"TypeError",` |
|         - | 5209 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5210 | `				nArg` |
|         - | 5211 | `				);` |
|         - | 5212 | `		}` |
|         6 | 5213 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5214 | `			int len;` |
|         3 | 5215 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5216 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5217 | `				"TypeError",` |
|         - | 5218 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5219 | `				nArg,` |
|         1 | 5220 | `				zName` |
|         - | 5221 | `				);` |
|         - | 5222 | `		}` |
|         4 | 5223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5224 | `			"TypeError",` |
|         - | 5225 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5226 | `			nArg` |
|         - | 5227 | `			);` |
|         - | 5228 | `	}` |
|         - | 5229 |  |
|         7 | 5230 | `	if( nArg == 2 ){` |
|         - | 5231 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5232 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5233 | `		return PH7_OK;` |
|         - | 5234 | `	}` |
|         - | 5235 |  |
|         - | 5236 | `	/* Create a new array */` |
|         5 | 5237 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5238 | `	if( pArray == 0 ){` |
|       ! 0 | 5239 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5240 | `		return PH7_OK;` |
|         - | 5241 | `	}` |
|         - | 5242 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5243 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5244 | `	/* Perform the diff */` |
|         5 | 5245 | `	pEntry = pSrc->pFirst;` |
|         5 | 5246 | `	n = pSrc->nEntry;` |
|         5 | 5247 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5248 | `	for(;;){` |
|        11 | 5249 | `		if( n < 1 ){` |
|         3 | 5250 | `			break;` |
|         - | 5251 | `		}` |
|         - | 5252 | `		/* Extract the node value */` |
|         9 | 5253 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5254 | `		if( pVal ){` |
|        15 | 5255 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5256 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5257 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5258 | `				/* Perform the lookup */` |
|         9 | 5259 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5260 | `				if( rc == SXRET_OK ){` |
|         - | 5261 | `					/* Value exist */` |
|         3 | 5262 | `					break;` |
|         - | 5263 | `				}` |
|         4 | 5264 | `			}` |
|         9 | 5265 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5266 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5267 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5268 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5269 | `				return PH7_EXCEPTION;` |
|         - | 5270 | `			}` |
|         7 | 5271 | `			if( i >= (nArg - 1)){` |
|         - | 5272 | `				/* Perform the insertion */` |
|         5 | 5273 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5274 | `			}` |
|         3 | 5275 | `		}` |
|         - | 5276 | `		/* Point to the next entry */` |
|         7 | 5277 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5278 | `		n--;` |
|         1 | 5279 | `	}` |
|         - | 5280 | `	/* Return the freshly created array */` |
|         3 | 5281 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5282 | `	return PH7_OK;` |
|        15 | 5283 | `}` |
|         - | 5284 | `/*` |
|         - | 5285 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5286 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5287 | ` * Parameters` |
|         - | 5288 | ` *  $array1` |
|         - | 5289 | ` *    The array to compare from` |
|         - | 5290 | ` *  $array2` |
|         - | 5291 | ` *    An array to compare against` |
|         - | 5292 | ` *  $...` |
|         - | 5293 | ` *   More arrays to compare against` |
|         - | 5294 | ` * Return` |
|         - | 5295 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5296 | ` *  are not present in any of the other arrays.` |
|         - | 5297 | ` */` |
|        20 | 5298 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5299 | `{` |
|         - | 5300 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5301 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5302 | `	ph7_value *pArray;` |
|         - | 5303 | `	ph7_value *pVal;` |
|         - | 5304 | `	sxi32 rc;` |
|         - | 5305 | `	sxu32 n;` |
|         - | 5306 | `	int i;` |
|         - | 5307 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5308 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5309 | `	 * accompanying integration tests to pass. */` |
|        24 | 5310 | `	if( nArg < 1 ){` |
|       ! 0 | 5311 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5312 | `			"ArgumentCountError",` |
|         - | 5313 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5314 | `			nArg` |
|         - | 5315 | `			);` |
|         - | 5316 | `	}` |
|        24 | 5317 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5318 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5319 | `			"TypeError",` |
|         - | 5320 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5321 | `			ph7_type_name(apArg[0])` |
|         - | 5322 | `			);` |
|         - | 5323 | `	}` |
|        37 | 5324 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5325 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5326 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5327 | `				"TypeError",` |
|         - | 5328 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5329 | `				i + 1,` |
|         4 | 5330 | `				ph7_type_name(apArg[i])` |
|         - | 5331 | `				);` |
|         - | 5332 | `		}` |
|        10 | 5333 | `	}` |
|        15 | 5334 | `	if( nArg == 1 ){` |
|         - | 5335 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5336 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5337 | `		return PH7_OK;` |
|         - | 5338 | `	}` |
|         - | 5339 | `	/* Create a new array */` |
|        13 | 5340 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5341 | `	if( pArray == 0 ){` |
|       ! 0 | 5342 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5343 | `		return PH7_OK;` |
|         - | 5344 | `	}` |
|         - | 5345 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5346 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5347 | `	/* Perform the diff */` |
|        13 | 5348 | `	pEntry = pSrc->pFirst;` |
|        13 | 5349 | `	n = pSrc->nEntry;` |
|        13 | 5350 | `	pN1 = pN2 = 0;` |
|        34 | 5351 | `	for(;;){` |
|         - | 5352 | `		int keep;` |
|        41 | 5353 | `		if( n < 1 ){` |
|        13 | 5354 | `			break;` |
|         - | 5355 | `		}` |
|         - | 5356 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5357 | `		keep = 1;` |
|        47 | 5358 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5359 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5360 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5361 | `			/* Perform a key lookup first */` |
|        33 | 5362 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5363 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5364 | `			}else{` |
|        21 | 5365 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5366 | `			}` |
|        33 | 5367 | `			if( rc != SXRET_OK ){` |
|         - | 5368 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5369 | `				continue;` |
|         - | 5370 | `			}` |
|         - | 5371 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5372 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5373 | `			if( pVal ){` |
|         - | 5374 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5375 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5376 | `				if( pVal2 ){` |
|         - | 5377 | `					ph7_value sV1,sV2;` |
|         - | 5378 | `					sxi32 cmp;` |
|         - | 5379 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5380 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5381 | `					 * null element used to come back bool(false) in the` |
|         - | 5382 | `					 * caller's array). */` |
|        17 | 5383 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5384 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5385 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5386 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5387 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5388 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5389 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5390 | `					if( cmp == 0 ){` |
|         - | 5391 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5392 | `						keep = 0;` |
|        15 | 5393 | `						break;` |
|         - | 5394 | `					}` |
|         1 | 5395 | `				}` |
|         1 | 5396 | `			}` |
|         2 | 5397 | `		}` |
|        29 | 5398 | `		if( keep ){` |
|         - | 5399 | `			/* Perform the insertion */` |
|        15 | 5400 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5401 | `		}` |
|         - | 5402 | `		/* Point to the next entry */` |
|        29 | 5403 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5404 | `		n--;` |
|         1 | 5405 | `	}` |
|         - | 5406 | `	/* Return the freshly created array */` |
|        13 | 5407 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5408 | `	return PH7_OK;` |
|        14 | 5409 | `}` |
|         - | 5410 | `/*` |
|         - | 5411 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5412 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5413 | ` *  by a user supplied callback function.` |
|         - | 5414 | ` * Parameters` |
|         - | 5415 | ` *  $array1` |
|         - | 5416 | ` *    The array to compare from` |
|         - | 5417 | ` *  $array2` |
|         - | 5418 | ` *    An array to compare against` |
|         - | 5419 | ` *  $...` |
|         - | 5420 | ` *   More arrays to compare against.` |
|         - | 5421 | ` *  $key_compare_func` |
|         - | 5422 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5423 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5424 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5425 | ` * Return` |
|         - | 5426 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5427 | ` *  are not present in any of the other arrays.` |
|         - | 5428 | ` */` |
|        22 | 5429 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5430 | `{` |
|         - | 5431 | `	ph7_hashmap_node *pEntry;` |
|         - | 5432 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5433 | `	ph7_value *pCallback;` |
|         - | 5434 | `	ph7_value *pArray;` |
|         - | 5435 | `	sxi32 rc;` |
|         - | 5436 | `	sxu32 n;` |
|         - | 5437 | `	int i;` |
|         - | 5438 |  |
|         - | 5439 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5440 | `	if( nArg < 2 ){` |
|       ! 0 | 5441 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5442 | `			"ArgumentCountError",` |
|         - | 5443 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5444 | `			nArg` |
|         - | 5445 | `			);` |
|         - | 5446 | `	}` |
|        26 | 5447 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5448 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5449 | `			"TypeError",` |
|         - | 5450 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5451 | `			ph7_type_name(apArg[0])` |
|         - | 5452 | `			);` |
|         - | 5453 | `	}` |
|         - | 5454 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5455 | `	 * expected to be a callback. */` |
|        38 | 5456 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5457 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5458 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5459 | `				"TypeError",` |
|         - | 5460 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5461 | `				i + 1,` |
|         2 | 5462 | `				ph7_type_name(apArg[i])` |
|         - | 5463 | `				);` |
|         - | 5464 | `		}` |
|         9 | 5465 | `	}` |
|         - | 5466 | `	/* Point to the callback value */` |
|        22 | 5467 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5468 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5469 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5470 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5471 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5472 | `		 * string given" which we also reproduce. */` |
|         9 | 5473 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5474 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5475 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5476 | `				"TypeError",` |
|         - | 5477 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5478 | `				nArg` |
|         - | 5479 | `				);` |
|         - | 5480 | `		}` |
|         6 | 5481 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5482 | `			/* neither array nor string */` |
|         8 | 5483 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5484 | `				"TypeError",` |
|         - | 5485 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5486 | `				nArg` |
|         - | 5487 | `				);` |
|         - | 5488 | `		}` |
|         - | 5489 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5490 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5491 | `			"TypeError",` |
|         - | 5492 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5493 | `			nArg,` |
|       ! 0 | 5494 | `			ph7_type_name(pCallback)` |
|         - | 5495 | `			);` |
|         - | 5496 | `	}` |
|        13 | 5497 | `	if( nArg == 2 ){` |
|         - | 5498 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5499 | `		 * input array. */` |
|         3 | 5500 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5501 | `		return PH7_OK;` |
|         - | 5502 | `	}` |
|         - | 5503 | `	/* Create a new array */` |
|        11 | 5504 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5505 | `	if( pArray == 0 ){` |
|       ! 0 | 5506 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5507 | `		return PH7_OK;` |
|         - | 5508 | `	}` |
|         - | 5509 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5510 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5511 | `	/* Perform the diff */` |
|        11 | 5512 | `	pEntry = pSrc->pFirst;` |
|        11 | 5513 | `	n = pSrc->nEntry;` |
|        21 | 5514 | `	for(;;){` |
|         - | 5515 | `		int keep;` |
|        27 | 5516 | `		if( n < 1 ){` |
|         9 | 5517 | `			break;` |
|         - | 5518 | `		}` |
|        19 | 5519 | `		keep = 1;` |
|        31 | 5520 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5521 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5522 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5523 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5524 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5525 | `			while( pIt ){` |
|         - | 5526 | `				/* build temporary key values for callback */` |
|         - | 5527 | `				ph7_value key1, key2, result;` |
|         - | 5528 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5529 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5530 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5531 | `				}else{` |
|         - | 5532 | `					SyString sStr;` |
|        33 | 5533 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5534 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5535 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5536 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5537 | `				}` |
|        33 | 5538 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5539 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5540 | `				}else{` |
|         - | 5541 | `					SyString sStr;` |
|        33 | 5542 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5543 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5544 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5545 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5546 | `				}` |
|        33 | 5547 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5548 | `				/* call user callback with (key1, key2) */` |
|         - | 5549 | `				{` |
|         - | 5550 | `					ph7_value *apK[2];` |
|        33 | 5551 | `					apK[0] = &key1;` |
|        33 | 5552 | `					apK[1] = &key2;` |
|        33 | 5553 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5554 | `				}` |
|        33 | 5555 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5556 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5557 | `					 * array_uintersect (which signal back from` |
|         - | 5558 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5559 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5560 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5561 | `					PH7_MemObjRelease(&result);` |
|         3 | 5562 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5563 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5564 | `					return PH7_EXCEPTION;` |
|         - | 5565 | `				}` |
|        31 | 5566 | `				if( rc == SXRET_OK ){` |
|        31 | 5567 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5568 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5569 | `					}` |
|        31 | 5570 | `					if( result.x.iVal == 0 ){` |
|         - | 5571 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5572 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5573 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5574 | `						if( pVal1 && pVal2 ){` |
|         - | 5575 | `							ph7_value sV1,sV2;` |
|         - | 5576 | `							sxi32 cmp;` |
|         - | 5577 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5578 | `							 * place and these are LIVE array elements. */` |
|        13 | 5579 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5580 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5581 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5582 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5583 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5584 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5585 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5586 | `							if( cmp == 0 ){` |
|         9 | 5587 | `								keep = 0;` |
|         9 | 5588 | `								PH7_MemObjRelease(&result);` |
|         - | 5589 | `								/* release keys too before breaking */` |
|         9 | 5590 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5591 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5592 | `								break;` |
|         - | 5593 | `							}` |
|         2 | 5594 | `						}` |
|         2 | 5595 | `					}` |
|        11 | 5596 | `				}` |
|        23 | 5597 | `				PH7_MemObjRelease(&result);` |
|        23 | 5598 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5599 | `				PH7_MemObjRelease(&key2);` |
|         - | 5600 | `				/* move to next node */` |
|        23 | 5601 | `				pIt = pIt->pPrev;` |
|        23 | 5602 | `				if( keep == 0 ) break;` |
|         1 | 5603 | `			}` |
|        21 | 5604 | `			if( keep == 0 ) break;` |
|         7 | 5605 | `		}` |
|        17 | 5606 | `		if( keep ){` |
|         - | 5607 | `			/* Perform the insertion */` |
|         9 | 5608 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5609 | `		}` |
|         - | 5610 | `		/* Point to the next entry */` |
|        17 | 5611 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5612 | `		n--;` |
|         1 | 5613 | `	}` |
|         - | 5614 | `	/* Return the freshly created array */` |
|         9 | 5615 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5616 | `	return PH7_OK;` |
|        15 | 5617 | `}` |
|         - | 5618 | `/*` |
|         - | 5619 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5620 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5621 | ` * Parameters` |
|         - | 5622 | ` *  $array1` |
|         - | 5623 | ` *    The array to compare from` |
|         - | 5624 | ` *  $array2` |
|         - | 5625 | ` *    An array to compare against` |
|         - | 5626 | ` *  $...` |
|         - | 5627 | ` *   More arrays to compare against` |
|         - | 5628 | ` * Return` |
|         - | 5629 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5630 | ` *  in any of the other arrays.` |
|         - | 5631 | ` * Note that NULL is returned on failure.` |
|         - | 5632 | ` */` |
|        12 | 5633 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5634 | `{` |
|         - | 5635 | `	ph7_hashmap_node *pEntry;` |
|         - | 5636 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5637 | `	ph7_value *pArray;` |
|         - | 5638 | `	sxi32 rc;` |
|         - | 5639 | `	sxu32 n;` |
|         - | 5640 | `	int i;` |
|         - | 5641 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5642 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5643 | `	 * helpers. */` |
|        15 | 5644 | `	if( nArg < 1 ){` |
|       ! 0 | 5645 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5646 | `			"ArgumentCountError",` |
|         - | 5647 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5648 | `			nArg` |
|         - | 5649 | `			);` |
|         - | 5650 | `	}` |
|        15 | 5651 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5652 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5653 | `			"TypeError",` |
|         - | 5654 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5655 | `			ph7_type_name(apArg[0])` |
|         - | 5656 | `			);` |
|         - | 5657 | `	}` |
|        20 | 5658 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5659 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5660 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5661 | `				"TypeError",` |
|         - | 5662 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5663 | `				i + 1,` |
|         2 | 5664 | `				ph7_type_name(apArg[i])` |
|         - | 5665 | `				);` |
|         - | 5666 | `		}` |
|         5 | 5667 | `	}` |
|         9 | 5668 | `	if( nArg == 1 ){` |
|         - | 5669 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5670 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5671 | `		return PH7_OK;` |
|         - | 5672 | `	}` |
|         - | 5673 | `	/* Create a new array */` |
|         7 | 5674 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5675 | `	if( pArray == 0 ){` |
|       ! 0 | 5676 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5677 | `		return PH7_OK;` |
|         - | 5678 | `	}` |
|         - | 5679 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5680 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5681 | `	/* Perfrom the diff */` |
|         7 | 5682 | `	pEntry = pSrc->pFirst;` |
|         7 | 5683 | `	n = pSrc->nEntry;` |
|        12 | 5684 | `	for(;;){` |
|        25 | 5685 | `		if( n < 1 ){` |
|         7 | 5686 | `			break;` |
|         - | 5687 | `		}` |
|        31 | 5688 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5689 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5690 | `				/* ignore */` |
|       ! 0 | 5691 | `				continue;` |
|         - | 5692 | `			}` |
|        23 | 5693 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5694 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5695 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5696 | `				/* Blob lookup */` |
|        17 | 5697 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5698 | `			}else{` |
|         - | 5699 | `				/* Int lookup */` |
|         7 | 5700 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5701 | `			}` |
|        23 | 5702 | `			if( rc == SXRET_OK ){` |
|         - | 5703 | `				/* Key exists,break immediately */` |
|        11 | 5704 | `				break;` |
|         - | 5705 | `			}` |
|         7 | 5706 | `		}` |
|        19 | 5707 | `		if( i >= nArg ){` |
|         - | 5708 | `			/* Perform the insertion */` |
|         9 | 5709 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5710 | `		}` |
|         - | 5711 | `		/* Point to the next entry */` |
|        19 | 5712 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5713 | `		n--;` |
|         1 | 5714 | `	}` |
|         - | 5715 | `	/* Return the freshly created array */` |
|         7 | 5716 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5717 | `	return PH7_OK;` |
|         9 | 5718 | `}` |
|         - | 5719 | `/*` |
|         - | 5720 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5721 | ` *  Computes the intersection of arrays.` |
|         - | 5722 | ` * Parameters` |
|         - | 5723 | ` *  $array1` |
|         - | 5724 | ` *    The array to compare from` |
|         - | 5725 | ` *  $array2` |
|         - | 5726 | ` *    An array to compare against` |
|         - | 5727 | ` *  $...` |
|         - | 5728 | ` *   More arrays to compare against` |
|         - | 5729 | ` * Return` |
|         - | 5730 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5731 | ` *  in all of the parameters.` |
|         - | 5732 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5733 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5734 | ` */` |
|        20 | 5735 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5736 | `{` |
|         - | 5737 | `	ph7_hashmap_node *pEntry;` |
|         - | 5738 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5739 | `	ph7_value *pArray;` |
|         - | 5740 | `	ph7_value *pVal;` |
|         - | 5741 | `	sxi32 rc;` |
|         - | 5742 | `	sxu32 n;` |
|         - | 5743 | `	int i;` |
|        23 | 5744 | `	if( nArg < 1 ){` |
|       ! 0 | 5745 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5746 | `			"ArgumentCountError",` |
|         - | 5747 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5748 | `			nArg` |
|         - | 5749 | `			);` |
|         - | 5750 | `	}` |
|        23 | 5751 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5752 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5753 | `			"TypeError",` |
|         - | 5754 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5755 | `			ph7_type_name(apArg[0])` |
|         - | 5756 | `			);` |
|         - | 5757 | `	}` |
|        36 | 5758 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5759 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5760 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5761 | `				"TypeError",` |
|         - | 5762 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5763 | `				i + 1,` |
|         2 | 5764 | `				ph7_type_name(apArg[i])` |
|         - | 5765 | `				);` |
|         - | 5766 | `		}` |
|         9 | 5767 | `	}` |
|        17 | 5768 | `	if( nArg == 1 ){` |
|         - | 5769 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5770 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5771 | `		return PH7_OK;` |
|         - | 5772 | `	}` |
|         - | 5773 | `	/* Create a new array */` |
|        15 | 5774 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5775 | `	if( pArray == 0 ){` |
|       ! 0 | 5776 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5777 | `		return PH7_OK;` |
|         - | 5778 | `	}` |
|         - | 5779 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5780 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5781 | `	/* Perform the intersection */` |
|        15 | 5782 | `	pEntry = pSrc->pFirst;` |
|        15 | 5783 | `	n = pSrc->nEntry;` |
|        31 | 5784 | `	for(;;){` |
|        63 | 5785 | `		if( n < 1 ){` |
|        15 | 5786 | `			break;` |
|         - | 5787 | `		}` |
|         - | 5788 | `		/* Extract the node value */` |
|        49 | 5789 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5790 | `		if( pVal ){` |
|        79 | 5791 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5792 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5793 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5794 | `				/* Perform the lookup */` |
|        55 | 5795 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5796 | `				if( rc != SXRET_OK ){` |
|         - | 5797 | `					/* Value does not exist */` |
|        25 | 5798 | `					break;` |
|         - | 5799 | `				}` |
|        16 | 5800 | `			}` |
|        49 | 5801 | `			if( i >= nArg ){` |
|         - | 5802 | `				/* Perform the insertion */` |
|        25 | 5803 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5804 | `			}` |
|        24 | 5805 | `		}` |
|         - | 5806 | `		/* Point to the next entry */` |
|        49 | 5807 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5808 | `		n--;` |
|         1 | 5809 | `	}` |
|         - | 5810 | `	/* Return the freshly created array */` |
|        15 | 5811 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5812 | `	return PH7_OK;` |
|        13 | 5813 | `}` |
|         - | 5814 | `/*` |
|         - | 5815 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5816 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5817 | ` * Parameters` |
|         - | 5818 | ` *  $array1` |
|         - | 5819 | ` *    The array to compare from` |
|         - | 5820 | ` *  $array2` |
|         - | 5821 | ` *    An array to compare against` |
|         - | 5822 | ` *  $...` |
|         - | 5823 | ` *   More arrays to compare against` |
|         - | 5824 | ` * Return` |
|         - | 5825 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5826 | ` *  in all the arguments, with matching keys.` |
|         - | 5827 | ` */` |
|        20 | 5828 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5829 | `{` |
|         - | 5830 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5831 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5832 | `	ph7_value *pArray;` |
|         - | 5833 | `	ph7_value *pVal;` |
|         - | 5834 | `	sxi32 rc;` |
|         - | 5835 | `	sxu32 n;` |
|         - | 5836 | `	int i;` |
|        23 | 5837 | `	if( nArg < 1 ){` |
|       ! 0 | 5838 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5839 | `			"ArgumentCountError",` |
|         - | 5840 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5841 | `			nArg` |
|         - | 5842 | `			);` |
|         - | 5843 | `	}` |
|        23 | 5844 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5845 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5846 | `			"TypeError",` |
|         - | 5847 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5848 | `			ph7_type_name(apArg[0])` |
|         - | 5849 | `			);` |
|         - | 5850 | `	}` |
|        36 | 5851 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5852 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5853 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5854 | `				"TypeError",` |
|         - | 5855 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5856 | `				i + 1,` |
|         2 | 5857 | `				ph7_type_name(apArg[i])` |
|         - | 5858 | `				);` |
|         - | 5859 | `		}` |
|         9 | 5860 | `	}` |
|        17 | 5861 | `	if( nArg == 1 ){` |
|         - | 5862 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5863 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5864 | `		return PH7_OK;` |
|         - | 5865 | `	}` |
|         - | 5866 | `	/* Create a new array */` |
|        15 | 5867 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5868 | `	if( pArray == 0 ){` |
|       ! 0 | 5869 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5870 | `		return PH7_OK;` |
|         - | 5871 | `	}` |
|         - | 5872 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5873 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5874 | `	/* Perform the intersection */` |
|        15 | 5875 | `	pEntry = pSrc->pFirst;` |
|        15 | 5876 | `	n = pSrc->nEntry;` |
|        15 | 5877 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5878 | `	for(;;){` |
|        47 | 5879 | `		if( n < 1 ){` |
|        15 | 5880 | `			break;` |
|         - | 5881 | `		}` |
|         - | 5882 | `		/* Extract the node value */` |
|        33 | 5883 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5884 | `		if( pVal ){` |
|        53 | 5885 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5886 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5887 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5888 | `				/* Perform a key lookup first */` |
|        37 | 5889 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5890 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5891 | `				}else{` |
|        23 | 5892 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5893 | `				}` |
|        37 | 5894 | `				if( rc != SXRET_OK ){` |
|         - | 5895 | `					/* No such key,break immediately */` |
|         7 | 5896 | `					break;` |
|         - | 5897 | `				}` |
|         - | 5898 | `				/* Perform the lookup */` |
|        31 | 5899 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5900 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5901 | `					/* Value does not exist */` |
|         6 | 5902 | `					break;` |
|         - | 5903 | `				}` |
|        11 | 5904 | `			}` |
|        33 | 5905 | `			if( i >= nArg ){` |
|         - | 5906 | `				/* Perform the insertion */` |
|        17 | 5907 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5908 | `			}` |
|        16 | 5909 | `		}` |
|         - | 5910 | `		/* Point to the next entry */` |
|        33 | 5911 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5912 | `		n--;` |
|         1 | 5913 | `	}` |
|         - | 5914 | `	/* Return the freshly created array */` |
|        15 | 5915 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5916 | `	return PH7_OK;` |
|        13 | 5917 | `}` |
|         - | 5918 | `/*` |
|         - | 5919 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5920 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5921 | ` * Parameters` |
|         - | 5922 | ` *  $array1` |
|         - | 5923 | ` *    The array to compare from` |
|         - | 5924 | ` *  $...` |
|         - | 5925 | ` *   More arrays to compare against` |
|         - | 5926 | ` * Return` |
|         - | 5927 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5928 | ` *  have keys that are present in all arguments.` |
|         - | 5929 | ` * Note that NULL is returned on failure.` |
|         - | 5930 | ` */` |
|        20 | 5931 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5932 | `{` |
|         - | 5933 | `	ph7_hashmap_node *pEntry;` |
|         - | 5934 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5935 | `	ph7_value *pArray;` |
|         - | 5936 | `	sxi32 rc;` |
|         - | 5937 | `	sxu32 n;` |
|         - | 5938 | `	int i;` |
|        23 | 5939 | `	if( nArg < 1 ){` |
|       ! 0 | 5940 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5941 | `			"ArgumentCountError",` |
|         - | 5942 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5943 | `			nArg` |
|         - | 5944 | `			);` |
|         - | 5945 | `	}` |
|        23 | 5946 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5947 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5948 | `			"TypeError",` |
|         - | 5949 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5950 | `			ph7_type_name(apArg[0])` |
|         - | 5951 | `			);` |
|         - | 5952 | `	}` |
|        36 | 5953 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5954 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5955 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5956 | `				"TypeError",` |
|         - | 5957 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5958 | `				i + 1,` |
|         2 | 5959 | `				ph7_type_name(apArg[i])` |
|         - | 5960 | `				);` |
|         - | 5961 | `		}` |
|         9 | 5962 | `	}` |
|        17 | 5963 | `	if( nArg == 1 ){` |
|         - | 5964 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5965 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5966 | `		return PH7_OK;` |
|         - | 5967 | `	}` |
|         - | 5968 | `	/* Create a new array */` |
|        15 | 5969 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5970 | `	if( pArray == 0 ){` |
|       ! 0 | 5971 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5972 | `		return PH7_OK;` |
|         - | 5973 | `	}` |
|         - | 5974 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5975 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5976 | `	/* Perform the intersection */` |
|        15 | 5977 | `	pEntry = pSrc->pFirst;` |
|        15 | 5978 | `	n = pSrc->nEntry;` |
|        24 | 5979 | `	for(;;){` |
|        49 | 5980 | `		if( n < 1 ){` |
|        15 | 5981 | `			break;` |
|         - | 5982 | `		}` |
|        57 | 5983 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5984 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5985 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5986 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5987 | `				/* Blob lookup */` |
|        27 | 5988 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5989 | `			}else{` |
|         - | 5990 | `				/* Int key */` |
|        13 | 5991 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5992 | `			}` |
|        39 | 5993 | `			if( rc != SXRET_OK ){` |
|         - | 5994 | `				/* Key does not exist, break immediately */` |
|        17 | 5995 | `				break;` |
|         - | 5996 | `			}` |
|        12 | 5997 | `		}` |
|        35 | 5998 | `		if( i >= nArg ){` |
|         - | 5999 | `			/* Perform the insertion */` |
|        19 | 6000 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 6001 | `		}` |
|         - | 6002 | `		/* Point to the next entry */` |
|        35 | 6003 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6004 | `		n--;` |
|         1 | 6005 | `	}` |
|         - | 6006 | `	/* Return the freshly created array */` |
|        15 | 6007 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 6008 | `	return PH7_OK;` |
|        13 | 6009 | `}` |
|         - | 6010 | `/*` |
|         - | 6011 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 6012 | ` *  Computes the intersection of arrays.` |
|         - | 6013 | ` * Parameters` |
|         - | 6014 | ` *  $array1` |
|         - | 6015 | ` *    The array to compare from` |
|         - | 6016 | ` *  $array2` |
|         - | 6017 | ` *    An array to compare against` |
|         - | 6018 | ` *  $...` |
|         - | 6019 | ` *   More arrays to compare against` |
|         - | 6020 | ` * $callback` |
|         - | 6021 | ` *  The callback comparison function.` |
|         - | 6022 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 6023 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 6024 | ` *  than the second.` |
|         - | 6025 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 6026 | ` * Return` |
|         - | 6027 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 6028 | ` *  in all of the parameters. .` |
|         - | 6029 | ` * Note that NULL is returned on failure.` |
|         - | 6030 | ` */` |
|        24 | 6031 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6032 | `{` |
|         - | 6033 | `	ph7_hashmap_node *pEntry;` |
|         - | 6034 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 6035 | `	ph7_value *pCallback;` |
|         - | 6036 | `	ph7_value *pArray;` |
|         - | 6037 | `	ph7_value *pVal;` |
|         - | 6038 | `	sxi32 rc;` |
|         - | 6039 | `	sxu32 n;` |
|         - | 6040 | `	int i;` |
|         - | 6041 |  |
|         - | 6042 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 6043 | `	if( nArg < 2 ){` |
|       ! 0 | 6044 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6045 | `			"ArgumentCountError",` |
|         - | 6046 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 6047 | `			nArg` |
|         - | 6048 | `			);` |
|         - | 6049 | `	}` |
|        29 | 6050 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6051 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6052 | `			"TypeError",` |
|         - | 6053 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6054 | `			ph7_type_name(apArg[0])` |
|         - | 6055 | `			);` |
|         - | 6056 | `	}` |
|         - | 6057 |  |
|        27 | 6058 | `	if( nArg == 2 ){` |
|         - | 6059 | `		/* Only the original array and the callback were provided. */` |
|         - | 6060 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6061 | `		 * validation ordering. */` |
|         3 | 6062 | `	} else {` |
|         - | 6063 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6064 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6065 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6066 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6067 | `					"TypeError",` |
|         - | 6068 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6069 | `					i + 1,` |
|         2 | 6070 | `					ph7_type_name(apArg[i])` |
|         - | 6071 | `					);` |
|         - | 6072 | `			}` |
|        13 | 6073 | `		}` |
|         - | 6074 | `	}` |
|         - | 6075 |  |
|         - | 6076 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6077 | `	pCallback = apArg[nArg - 1];` |
|         - | 6078 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6079 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6080 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6081 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6082 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6083 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6084 | `			 */` |
|         9 | 6085 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6086 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6087 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6088 | `					"TypeError",` |
|         - | 6089 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6090 | `					nArg` |
|         - | 6091 | `					);` |
|         - | 6092 | `			}` |
|         - | 6093 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6094 | `			{` |
|         6 | 6095 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6096 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6097 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6098 | `					int nMethodLen;` |
|         6 | 6099 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6100 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6101 | `					if( pClass ){` |
|         - | 6102 | `						/* Class exists but method is missing. */` |
|         4 | 6103 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6104 | `							"TypeError",` |
|         - | 6105 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6106 | `							nArg,` |
|         1 | 6107 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6108 | `							zMethod` |
|         - | 6109 | `							);` |
|         - | 6110 | `					}` |
|         - | 6111 | `					/* Class not found */` |
|         - | 6112 | `					{` |
|         - | 6113 | `						int nName;` |
|         3 | 6114 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6115 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6116 | `							"TypeError",` |
|         - | 6117 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6118 | `							nArg,` |
|         1 | 6119 | `							zName` |
|         - | 6120 | `							);` |
|         - | 6121 | `					}` |
|         - | 6122 | `				}` |
|         - | 6123 | `			}` |
|         - | 6124 | `			/* Fallback message */` |
|       ! 0 | 6125 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6126 | `				"TypeError",` |
|         - | 6127 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6128 | `				nArg` |
|         - | 6129 | `				);` |
|         - | 6130 | `		}` |
|         6 | 6131 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6132 | `			int len;` |
|         3 | 6133 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6134 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6135 | `				"TypeError",` |
|         - | 6136 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6137 | `				nArg,` |
|         1 | 6138 | `				zName` |
|         - | 6139 | `				);` |
|         - | 6140 | `		}` |
|         4 | 6141 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6142 | `			"TypeError",` |
|         - | 6143 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6144 | `			nArg` |
|         - | 6145 | `			);` |
|         - | 6146 | `	}` |
|         - | 6147 |  |
|        11 | 6148 | `	if( nArg == 2 ){` |
|         - | 6149 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6150 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6151 | `		return PH7_OK;` |
|         - | 6152 | `	}` |
|         - | 6153 |  |
|         - | 6154 | `	/* Create a new array */` |
|         7 | 6155 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6156 | `	if( pArray == 0 ){` |
|       ! 0 | 6157 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6158 | `		return PH7_OK;` |
|         - | 6159 | `	}` |
|         - | 6160 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6161 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6162 | `	/* Perform the intersection */` |
|         7 | 6163 | `	pEntry = pSrc->pFirst;` |
|         7 | 6164 | `	n = pSrc->nEntry;` |
|         7 | 6165 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6166 | `	for(;;){` |
|        19 | 6167 | `		if( n < 1 ){` |
|         5 | 6168 | `			break;` |
|         - | 6169 | `		}` |
|         - | 6170 | `		/* Extract the node value */` |
|        15 | 6171 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6172 | `		if( pVal ){` |
|        23 | 6173 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6174 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6175 | `					/* ignore */` |
|       ! 0 | 6176 | `					continue;` |
|         - | 6177 | `				}` |
|         - | 6178 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6179 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6180 | `				/* Perform the lookup */` |
|        15 | 6181 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6182 | `				if( rc != SXRET_OK ){` |
|         - | 6183 | `					/* Value does not exist */` |
|         7 | 6184 | `					break;` |
|         - | 6185 | `				}` |
|         5 | 6186 | `			}` |
|        15 | 6187 | `			if( i >= (nArg-1) ){` |
|         - | 6188 | `				/* Perform the insertion */` |
|         9 | 6189 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6190 | `			}` |
|         7 | 6191 | `		}` |
|        15 | 6192 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6193 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6194 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6195 | `			return PH7_EXCEPTION;` |
|         - | 6196 | `		}` |
|         - | 6197 | `		/* Point to the next entry */` |
|        13 | 6198 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6199 | `		n--;` |
|         1 | 6200 | `	}` |
|         - | 6201 | `	/* Return the freshly created array */` |
|         5 | 6202 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6203 | `	return PH7_OK;` |
|        17 | 6204 | `}` |
|         - | 6205 | `/*` |
|         - | 6206 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6207 | ` *  Fill an array with values.` |
|         - | 6208 | ` * Parameters` |
|         - | 6209 | ` *  $start_index` |
|         - | 6210 | ` *    The first index of the returned array.` |
|         - | 6211 | ` *  $num` |
|         - | 6212 | ` *   Number of elements to insert.` |
|         - | 6213 | ` *  $value` |
|         - | 6214 | ` *    Value to use for filling.` |
|         - | 6215 | ` * Return` |
|         - | 6216 | ` *  The filled array or null on failure.` |
|         - | 6217 | ` */` |
|       240 | 6218 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6219 | `{` |
|         - | 6220 | `	ph7_value *pArray;` |
|         - | 6221 | `	int i,nEntry;` |
|         - | 6222 |  |
|         - | 6223 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6224 | `	if( nArg != 3 ){` |
|         - | 6225 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6226 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6227 | `			"ArgumentCountError",` |
|         - | 6228 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6229 | `			nArg` |
|         - | 6230 | `			);` |
|         - | 6231 | `	}` |
|         - | 6232 |  |
|         - | 6233 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6234 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6235 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6236 | `	 * and NULLs are rejected outright. */` |
|       357 | 6237 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6238 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6239 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6240 | `			"TypeError",` |
|         - | 6241 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6242 | `			ph7_type_name(apArg[0])` |
|         - | 6243 | `			);` |
|         - | 6244 | `	}` |
|       242 | 6245 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6246 | `		int len;` |
|         8 | 6247 | `		sxu8 bReal = FALSE;` |
|         8 | 6248 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6249 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6250 | `			/* Non‑numeric string is an error. */` |
|         3 | 6251 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6252 | `				"TypeError",` |
|         - | 6253 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6254 | `				);` |
|         - | 6255 | `		}` |
|         5 | 6256 | `		if( bReal ){` |
|         - | 6257 | `			/* float-string -> deprecation warning */` |
|         4 | 6258 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6259 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6260 | `				zStr` |
|         - | 6261 | `				);` |
|         1 | 6262 | `		}` |
|         2 | 6263 | `	}` |
|         - | 6264 |  |
|         - | 6265 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6266 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6267 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6268 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6269 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6270 | `			"TypeError",` |
|         - | 6271 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6272 | `			ph7_type_name(apArg[1])` |
|         - | 6273 | `			);` |
|         - | 6274 | `	}` |
|       239 | 6275 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6276 | `		int len;` |
|         3 | 6277 | `		sxu8 bReal = FALSE;` |
|         3 | 6278 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6279 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6280 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6281 | `				"TypeError",` |
|         - | 6282 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6283 | `				);` |
|         - | 6284 | `		}` |
|       ! 0 | 6285 | `	}` |
|         - | 6286 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6287 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6288 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6289 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6290 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6291 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6292 | `		if( d != (double)i64 ){` |
|         7 | 6293 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6294 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6295 | `				d` |
|         - | 6296 | `				);` |
|         2 | 6297 | `		}` |
|         2 | 6298 | `	}` |
|         - | 6299 |  |
|         - | 6300 | `	/* Total number of entries to insert */` |
|       236 | 6301 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6302 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6303 | `	if( nEntry < 0 ){` |
|         3 | 6304 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6305 | `			"ValueError",` |
|         - | 6306 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6307 | `			);` |
|         - | 6308 | `	}` |
|         - | 6309 |  |
|         - | 6310 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6311 | `	if( nEntry == 0 ){` |
|         7 | 6312 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6313 | `		return PH7_OK;` |
|         - | 6314 | `	}` |
|         - | 6315 |  |
|         - | 6316 | `	/* Create a new array */` |
|       227 | 6317 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6318 | `	if( pArray == 0 ){` |
|       ! 0 | 6319 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6320 | `	}` |
|         - | 6321 |  |
|         - | 6322 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6323 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6324 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6325 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6326 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6327 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6328 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6329 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6330 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6331 | `		}` |
|   1058803 | 6332 | `	}` |
|         - | 6333 | `	/* Return the filled array */` |
|       227 | 6334 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6335 | `	return PH7_OK;` |
|       124 | 6336 | `}` |
|         - | 6337 | `/*` |
|         - | 6338 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6339 | ` *  Fill an array with values, specifying keys.` |
|         - | 6340 | ` * Parameters` |
|         - | 6341 | ` *  $input` |
|         - | 6342 | ` *   Array of values that will be used as key.` |
|         - | 6343 | ` *  $value` |
|         - | 6344 | ` *    Value to use for filling.` |
|         - | 6345 | ` * Return` |
|         - | 6346 | ` *  The filled array.` |
|         - | 6347 | ` * Throws` |
|         - | 6348 | ` *  ValueError if $input is not an array.` |
|         - | 6349 | ` */` |
|        22 | 6350 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6351 | `{` |
|         - | 6352 | `	ph7_hashmap_node *pEntry;` |
|         - | 6353 | `	ph7_hashmap *pSrc;` |
|         - | 6354 | `	ph7_value *pArray;` |
|         - | 6355 | `	sxu32 n;` |
|         - | 6356 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6357 | `	if( nArg != 2 ){` |
|         4 | 6358 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6359 | `			"ArgumentCountError",` |
|         - | 6360 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6361 | `			nArg` |
|         - | 6362 | `			);` |
|         - | 6363 | `	}` |
|         - | 6364 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6365 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6366 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6367 | `			"TypeError",` |
|         - | 6368 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6369 | `			ph7_type_name(apArg[0])` |
|         - | 6370 | `			);` |
|         - | 6371 | `	}` |
|         - | 6372 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6373 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6374 | `	/* Create a new array */` |
|        17 | 6375 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6376 | `	if( pArray == 0 ){` |
|       ! 0 | 6377 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6378 | `		return PH7_OK;` |
|         - | 6379 | `	}` |
|         - | 6380 | `	/* Perform the requested operation */` |
|        17 | 6381 | `	pEntry = pSrc->pFirst;` |
|        45 | 6382 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6383 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6384 | `		/* Point to the next entry */` |
|        29 | 6385 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6386 | `	}` |
|         - | 6387 | `	/* Return the filled array */` |
|        17 | 6388 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6389 | `	return PH7_OK;` |
|        14 | 6390 | `}` |
|         - | 6391 | `/*` |
|         - | 6392 | ` * array array_combine(array $keys,array $values)` |
|         - | 6393 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6394 | ` * Parameters` |
|         - | 6395 | ` *  $keys` |
|         - | 6396 | ` *    Array of keys to be used.` |
|         - | 6397 | ` * $values` |
|         - | 6398 | ` *   Array of values to be used.` |
|         - | 6399 | ` * Return` |
|         - | 6400 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6401 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6402 | ` *  not an array.` |
|         - | 6403 | ` */` |
|        16 | 6404 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6405 | `{` |
|         - | 6406 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6407 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6408 | `	ph7_value *pArray;` |
|         - | 6409 | `	sxu32 n;` |
|         - | 6410 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6411 | `	if( nArg != 2 ){` |
|         - | 6412 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6413 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6414 | `			"ArgumentCountError",` |
|         - | 6415 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6416 | `			nArg` |
|         - | 6417 | `			);` |
|         - | 6418 | `	}` |
|         - | 6419 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6420 | `	 * argument index in the error message. */` |
|        20 | 6421 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6422 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6423 | `			"TypeError",` |
|         - | 6424 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6425 | `			ph7_type_name(apArg[0])` |
|         - | 6426 | `			);` |
|         - | 6427 | `	}` |
|        17 | 6428 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6429 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6430 | `			"TypeError",` |
|         - | 6431 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6432 | `			ph7_type_name(apArg[1])` |
|         - | 6433 | `			);` |
|         - | 6434 | `	}` |
|         - | 6435 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6436 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6437 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6438 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6439 | `		/* Length mismatch -> ValueError */` |
|         3 | 6440 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6441 | `			"ValueError",` |
|         - | 6442 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6443 | `			);` |
|         - | 6444 | `	}` |
|         - | 6445 | `	/* Create a new array */` |
|        11 | 6446 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6447 | `	if( pArray == 0 ){` |
|       ! 0 | 6448 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6449 | `		return PH7_OK;` |
|         - | 6450 | `	}` |
|         - | 6451 | `	/* Perform the requested operation */` |
|        11 | 6452 | `	pKe = pKey->pFirst;` |
|        11 | 6453 | `	pVe = pValue->pFirst;` |
|        33 | 6454 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6455 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6456 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6457 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6458 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6459 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6460 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6461 | `		 * original array must not be mutated. */` |
|        23 | 6462 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6463 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6464 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6465 | `			if( pTmpKey ){` |
|         5 | 6466 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6467 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6468 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6469 | `				pKeyCopy = pTmpKey;` |
|         2 | 6470 | `			}` |
|         2 | 6471 | `		}` |
|        23 | 6472 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6473 | `		/* Point to the next entry */` |
|        23 | 6474 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6475 | `		pVe = pVe->pPrev;` |
|        12 | 6476 | `	}` |
|         - | 6477 | `	/* Return the filled array */` |
|        11 | 6478 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6479 | `	return PH7_OK;` |
|        12 | 6480 | `}` |
|         - | 6481 | `/*` |
|         - | 6482 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6483 | ` *  Return an array with elements in reverse order.` |
|         - | 6484 | ` * Parameters` |
|         - | 6485 | ` *  $array` |
|         - | 6486 | ` *   The input array.` |
|         - | 6487 | ` *  $preserve_keys (optional)` |
|         - | 6488 | ` *   If set to TRUE keys are preserved.` |
|         - | 6489 | ` * Return` |
|         - | 6490 | ` *  The reversed array.` |
|         - | 6491 | ` */` |
|        18 | 6492 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6493 | `{` |
|         - | 6494 | `	ph7_hashmap_node *pEntry;` |
|         - | 6495 | `	ph7_hashmap *pSrc;` |
|         - | 6496 | `	ph7_value *pArray;` |
|         - | 6497 | `	int bPreserve;` |
|         - | 6498 | `	sxu32 n;` |
|        20 | 6499 | `	if( nArg < 1 ){` |
|       ! 0 | 6500 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6501 | `			"ArgumentCountError",` |
|         - | 6502 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6503 | `			nArg` |
|         - | 6504 | `			);` |
|         - | 6505 | `	}` |
|         - | 6506 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6508 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6509 | `			"TypeError",` |
|         - | 6510 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6511 | `			ph7_type_name(apArg[0])` |
|         - | 6512 | `			);` |
|         - | 6513 | `	}` |
|        17 | 6514 | `	bPreserve = FALSE;` |
|        17 | 6515 | `	if( nArg > 1 ){` |
|         7 | 6516 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6517 | `	}` |
|         - | 6518 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6519 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6520 | `	/* Create a new array */` |
|        17 | 6521 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6522 | `	if( pArray == 0 ){` |
|       ! 0 | 6523 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6524 | `		return PH7_OK;` |
|         - | 6525 | `	}` |
|         - | 6526 | `	/* Perform the requested operation */` |
|        17 | 6527 | `	pEntry = pSrc->pLast;` |
|        55 | 6528 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6529 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6530 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6531 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6532 | `		/* Point to the previous entry */` |
|        39 | 6533 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6534 | `	}` |
|        17 | 6535 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6536 | `	return PH7_OK;` |
|        11 | 6537 | `}` |
|         - | 6538 | `/*` |
|         - | 6539 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6540 | ` *  Removes duplicate values from an array.` |
|         - | 6541 | ` * Parameters` |
|         - | 6542 | ` *  $array` |
|         - | 6543 | ` *   The input array.` |
|         - | 6544 | ` *  $flags` |
|         - | 6545 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6546 | ` *   behavior using these values:` |
|         - | 6547 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6548 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6549 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6550 | ` * Return` |
|         - | 6551 | ` *  The filtered array.` |
|         - | 6552 | ` */` |
|        22 | 6553 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6554 | `{` |
|         - | 6555 | `	ph7_hashmap_node *pEntry;` |
|         - | 6556 | `	ph7_value *pNeedle;` |
|         - | 6557 | `	ph7_hashmap *pSrc;` |
|         - | 6558 | `	ph7_value *pArray;` |
|         - | 6559 | `	int bStrict;` |
|         - | 6560 | `	sxi32 rc;` |
|         - | 6561 | `	sxu32 n;` |
|        25 | 6562 | `	if( nArg < 1 ){` |
|         - | 6563 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6564 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6565 | `			"ArgumentCountError",` |
|         - | 6566 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6567 | `			);` |
|         - | 6568 | `	}` |
|        25 | 6569 | `	if( nArg > 2 ){` |
|         - | 6570 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6571 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6572 | `			"ArgumentCountError",` |
|         - | 6573 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6574 | `			nArg` |
|         - | 6575 | `			);` |
|         - | 6576 | `	}` |
|         - | 6577 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6578 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6579 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6580 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6581 | `			"TypeError",` |
|         - | 6582 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6583 | `			ph7_type_name(apArg[0])` |
|         - | 6584 | `			);` |
|         - | 6585 | `	}` |
|        19 | 6586 | `	bStrict = FALSE;` |
|         - | 6587 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6588 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6589 | `	/* Create a new array */` |
|        19 | 6590 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6591 | `	if( pArray == 0 ){` |
|       ! 0 | 6592 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6593 | `		return PH7_OK;` |
|         - | 6594 | `	}` |
|         - | 6595 | `	/* Perform the requested operation */` |
|        19 | 6596 | `	pEntry = pSrc->pFirst;` |
|        83 | 6597 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6598 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6599 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6600 | `		if( pNeedle ){` |
|        65 | 6601 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6602 | `		}` |
|        65 | 6603 | `		if( rc != SXRET_OK ){` |
|         - | 6604 | `			/* Perform the insertion */` |
|        37 | 6605 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6606 | `		}` |
|         - | 6607 | `		/* Point to the next entry */` |
|        65 | 6608 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6609 | `	}` |
|         - | 6610 | `	/* Return the freshly created array */` |
|        19 | 6611 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6612 | `	return PH7_OK;` |
|        14 | 6613 | `}` |
|         - | 6614 | `/*` |
|         - | 6615 | ` * array array_flip(array $input)` |
|         - | 6616 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6617 | ` * Parameter` |
|         - | 6618 | ` *  $input` |
|         - | 6619 | ` *   Input array.` |
|         - | 6620 | ` * Return` |
|         - | 6621 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6622 | ` */` |
|        30 | 6623 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6624 | `{` |
|         - | 6625 | `	ph7_hashmap_node *pEntry;` |
|         - | 6626 | `	ph7_hashmap *pSrc;` |
|         - | 6627 | `	ph7_value *pArray;` |
|         - | 6628 | `	ph7_value *pKey;` |
|         - | 6629 | `	ph7_value sVal;` |
|         - | 6630 | `	sxu32 n;` |
|         - | 6631 |  |
|         - | 6632 | `	/* PHP requires exactly one argument */` |
|        33 | 6633 | `	if( nArg != 1 ){` |
|         - | 6634 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6635 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6636 | `			"ArgumentCountError",` |
|         - | 6637 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6638 | `			nArg` |
|         - | 6639 | `			);` |
|         - | 6640 | `	}` |
|         - | 6641 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6642 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6643 | `		/* Type mismatch -> TypeError */` |
|         4 | 6644 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6645 | `			"TypeError",` |
|         - | 6646 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6647 | `			ph7_type_name(apArg[0])` |
|         - | 6648 | `			);` |
|         - | 6649 | `	}` |
|         - | 6650 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6651 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6652 | `	/* Create a new array */` |
|        27 | 6653 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6654 | `	if( pArray == 0 ){` |
|       ! 0 | 6655 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6656 | `		return PH7_OK;` |
|         - | 6657 | `	}` |
|         - | 6658 | `	/* Start processing */` |
|        27 | 6659 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6660 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6661 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6662 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6663 | `		if( pKey ){` |
|         - | 6664 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6665 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6666 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6667 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6668 | `					);` |
|     22236 | 6669 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6670 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6671 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6672 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6673 | `				}else{` |
|         - | 6674 | `					SyString sStr;` |
|      2227 | 6675 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6676 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6677 | `				}` |
|         - | 6678 | `				/* Perform the insertion */` |
|     22227 | 6679 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6680 | `				/* Safely release the value because each inserted entry` |
|         - | 6681 | `				 * has its own private copy of the value.` |
|         - | 6682 | `				 */` |
|     22227 | 6683 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6684 | `			}else{` |
|         - | 6685 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6686 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6687 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6688 | `					);` |
|         - | 6689 | `			}` |
|     11118 | 6690 | `		}` |
|         - | 6691 | `		/* Point to the next entry */` |
|     22237 | 6692 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6693 | `	}` |
|         - | 6694 | `	/* Return the freshly created array */` |
|        27 | 6695 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6696 | `	return PH7_OK;` |
|        18 | 6697 | `}` |
|         - | 6698 | `/*` |
|         - | 6699 | ` * number array_sum(array $array )` |
|         - | 6700 | ` *  Calculate the sum of values in an array.` |
|         - | 6701 | ` * Parameters` |
|         - | 6702 | ` *  $array: The input array.` |
|         - | 6703 | ` * Return` |
|         - | 6704 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6705 | ` */` |
|        24 | 6706 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6707 | `{` |
|         - | 6708 | `	ph7_hashmap_node *pEntry;` |
|         - | 6709 | `	ph7_value *pObj;` |
|        26 | 6710 | `	double dSum = 0;` |
|         - | 6711 | `	sxu32 n;` |
|        26 | 6712 | `	pEntry = pMap->pFirst;` |
|        92 | 6713 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6714 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6715 | `		if( pObj ){` |
|        68 | 6716 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6717 | `				dSum += pObj->rVal;` |
|        54 | 6718 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6719 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6720 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6721 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6722 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6723 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6724 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6725 | `						"Addition is not supported on type string");` |
|        14 | 6726 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6727 | `					double dv = 0;` |
|        13 | 6728 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6729 | `					dSum += dv;` |
|         8 | 6730 | `				}` |
|        12 | 6731 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6732 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6733 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6734 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6735 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6736 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6737 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6738 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6739 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6740 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6741 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6742 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6743 | `			}` |
|         - | 6744 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6745 | `		}` |
|         - | 6746 | `		/* Point to the next entry */` |
|        68 | 6747 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6748 | `	}` |
|         - | 6749 | `	/* Return sum */` |
|        26 | 6750 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6751 | `}` |
|       688 | 6752 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6753 | `{` |
|         - | 6754 | `	ph7_hashmap_node *pEntry;` |
|         - | 6755 | `	ph7_value *pObj;` |
|       690 | 6756 | `	sxi64 nSum = 0;` |
|         - | 6757 | `	sxu32 n;` |
|       690 | 6758 | `	pEntry = pMap->pFirst;` |
|      4702 | 6759 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6760 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6761 | `		if( pObj ){` |
|      4014 | 6762 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6763 | `				nSum += pObj->x.iVal;` |
|      2018 | 6764 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6765 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6766 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6767 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6768 | `						"Addition is not supported on type string");` |
|        10 | 6769 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6770 | `					sxi64 nv = 0;` |
|         8 | 6771 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6772 | `					nSum += nv;` |
|         5 | 6773 | `				}` |
|        17 | 6774 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6775 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6776 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6777 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6778 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6779 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6780 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6781 | `					"Addition is not supported on type %s",` |
|         2 | 6782 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6783 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6784 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6785 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6786 | `			}` |
|         - | 6787 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6788 | `		}` |
|         - | 6789 | `		/* Point to the next entry */` |
|      4014 | 6790 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6791 | `	}` |
|         - | 6792 | `	/* Return sum */` |
|       690 | 6793 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6794 | `}` |
|         - | 6795 | `/* number array_sum(array $array )` |
|         - | 6796 | ` * (See block-coment above)` |
|         - | 6797 | ` */` |
|       724 | 6798 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6799 | `{` |
|         - | 6800 | `	ph7_hashmap_node *pEntry;` |
|         - | 6801 | `	ph7_hashmap *pMap;` |
|         - | 6802 | `	ph7_value *pObj;` |
|       728 | 6803 | `	int useDouble = 0;` |
|         - | 6804 | `	sxu32 n;` |
|         - | 6805 | `	/* PHP requires exactly one argument */` |
|       728 | 6806 | `	if( nArg != 1 ){` |
|         4 | 6807 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6808 | `			"ArgumentCountError",` |
|         - | 6809 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6810 | `			nArg` |
|         - | 6811 | `			);` |
|         - | 6812 | `	}` |
|         - | 6813 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6814 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6815 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6816 | `		char zBuf[64];` |
|         8 | 6817 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6818 | `			"TypeError",` |
|         - | 6819 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6820 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6821 | `			);` |
|         - | 6822 | `	}` |
|       720 | 6823 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6824 | `	if( pMap->nEntry < 1 ){` |
|         - | 6825 | `		/* Nothing to compute,return 0 */` |
|         7 | 6826 | `		ph7_result_int(pCtx,0);` |
|         7 | 6827 | `		return PH7_OK;` |
|         - | 6828 | `	}` |
|         - | 6829 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6830 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6831 | `	 */` |
|       714 | 6832 | `	pEntry = pMap->pFirst;` |
|      4734 | 6833 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6834 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6835 | `		if( pObj ){` |
|      4046 | 6836 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6837 | `				useDouble = 1;` |
|        20 | 6838 | `				break;` |
|         - | 6839 | `			}` |
|      4028 | 6840 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6841 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6842 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6843 | `				sxu32 i;` |
|        32 | 6844 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6845 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6846 | `						useDouble = 1;` |
|         7 | 6847 | `						break;` |
|         - | 6848 | `					}` |
|         9 | 6849 | `				}` |
|        18 | 6850 | `				if( useDouble ){` |
|         7 | 6851 | `					break;` |
|         - | 6852 | `				}` |
|         5 | 6853 | `			}` |
|      2010 | 6854 | `		}` |
|      4022 | 6855 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6856 | `	}` |
|       714 | 6857 | `	if( useDouble ){` |
|        26 | 6858 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6859 | `	}else{` |
|       690 | 6860 | `		Int64Sum(pCtx,pMap);` |
|         - | 6861 | `	}` |
|       714 | 6862 | `	return PH7_OK;` |
|       366 | 6863 | `}` |
|         - | 6864 | `/*` |
|         - | 6865 | ` * number array_product(array $array )` |
|         - | 6866 | ` *  Calculate the product of values in an array.` |
|         - | 6867 | ` * Parameters` |
|         - | 6868 | ` *  $array: The input array.` |
|         - | 6869 | ` * Return` |
|         - | 6870 | ` *  Returns the product of values as an integer or float.` |
|         - | 6871 | ` */` |
|         2 | 6872 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6873 | `{` |
|         - | 6874 | `	ph7_hashmap_node *pEntry;` |
|         - | 6875 | `	ph7_value *pObj;` |
|         - | 6876 | `	double dProd;` |
|         - | 6877 | `	sxu32 n;` |
|         3 | 6878 | `	pEntry = pMap->pFirst;` |
|         3 | 6879 | `	dProd = 1;` |
|         7 | 6880 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6881 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6882 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6883 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6884 | `				dProd *= pObj->rVal;` |
|         4 | 6885 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6886 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6887 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6888 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6889 | `					double dv = 0;` |
|       ! 0 | 6890 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6891 | `					dProd *= dv;` |
|       ! 0 | 6892 | `				}` |
|       ! 0 | 6893 | `			}` |
|         2 | 6894 | `		}` |
|         - | 6895 | `		/* Point to the next entry */` |
|         5 | 6896 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6897 | `	}` |
|         - | 6898 | `	/* Return product */` |
|         3 | 6899 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6900 | `}` |
|         2 | 6901 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6902 | `{` |
|         - | 6903 | `	ph7_hashmap_node *pEntry;` |
|         - | 6904 | `	ph7_value *pObj;` |
|         - | 6905 | `	sxi64 nProd;` |
|         - | 6906 | `	sxu32 n;` |
|         3 | 6907 | `	pEntry = pMap->pFirst;` |
|         3 | 6908 | `	nProd = 1;` |
|         9 | 6909 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6910 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6911 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6912 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6913 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6914 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6915 | `				nProd *= pObj->x.iVal;` |
|         3 | 6916 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6917 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6918 | `					sxi64 nv = 0;` |
|       ! 0 | 6919 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6920 | `					nProd *= nv;` |
|       ! 0 | 6921 | `				}` |
|       ! 0 | 6922 | `			}` |
|         3 | 6923 | `		}` |
|         - | 6924 | `		/* Point to the next entry */` |
|         7 | 6925 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6926 | `	}` |
|         - | 6927 | `	/* Return product */` |
|         3 | 6928 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6929 | `}` |
|         - | 6930 | `/* number array_product(array $array )` |
|         - | 6931 | ` * (See block-block comment above)` |
|         - | 6932 | ` */` |
|        16 | 6933 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6934 | `{` |
|         - | 6935 | `	ph7_hashmap *pMap;` |
|         - | 6936 | `	ph7_value *pObj;` |
|        17 | 6937 | `	if( nArg < 1 ){` |
|         - | 6938 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6939 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6940 | `		return PH7_OK;` |
|         - | 6941 | `	}` |
|         - | 6942 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 6943 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6944 | `		char zBuf[64];` |
|        16 | 6945 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6946 | `			"TypeError",` |
|         - | 6947 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 6948 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6949 | `			);` |
|         - | 6950 | `	}` |
|         7 | 6951 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6952 | `	if( pMap->nEntry < 1 ){` |
|         - | 6953 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6954 | `		ph7_result_int(pCtx,1);` |
|         3 | 6955 | `		return PH7_OK;` |
|         - | 6956 | `	}` |
|         - | 6957 | `	/* If the first element is of type float,then perform floating` |
|         - | 6958 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6959 | `	 */` |
|         5 | 6960 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6961 | `	if( pObj == 0 ){` |
|       ! 0 | 6962 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6963 | `		return PH7_OK;` |
|         - | 6964 | `	}` |
|         5 | 6965 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6966 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6967 | `	}else{` |
|         3 | 6968 | `		Int64Prod(pCtx,pMap);` |
|         - | 6969 | `	}` |
|         5 | 6970 | `	return PH7_OK;` |
|         9 | 6971 | `}` |
|         - | 6972 | `/*` |
|         - | 6973 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6974 | ` *  Pick one or more random entries out of an array.` |
|         - | 6975 | ` * Parameters` |
|         - | 6976 | ` * $input` |
|         - | 6977 | ` *  The input array.` |
|         - | 6978 | ` * $num_req` |
|         - | 6979 | ` *  Specifies how many entries you want to pick.` |
|         - | 6980 | ` * Return` |
|         - | 6981 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6982 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6983 | ` *  NULL is returned on failure.` |
|         - | 6984 | ` */` |
|        36 | 6985 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6986 | `{` |
|         - | 6987 | `	ph7_hashmap_node *pNode;` |
|         - | 6988 | `	ph7_hashmap *pMap;` |
|        37 | 6989 | `	int nItem = 1;` |
|        37 | 6990 | `	if( nArg < 1 ){` |
|         - | 6991 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6992 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6993 | `		return PH7_OK;` |
|         - | 6994 | `	}` |
|         - | 6995 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 6996 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6997 | `		char zBuf[64];` |
|        10 | 6998 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6999 | `			"TypeError",` |
|         - | 7000 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7001 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7002 | `			);` |
|         - | 7003 | `	}` |
|         - | 7004 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 7005 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 7006 | `	if( nArg > 1 ){` |
|        23 | 7007 | `		ph7_value *pNum = apArg[1];` |
|        22 | 7008 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 7009 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 7010 | `			char zBuf[64];` |
|       ! 0 | 7011 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7012 | `				"TypeError",` |
|         - | 7013 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 7014 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 7015 | `				);` |
|         - | 7016 | `		}` |
|        23 | 7017 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 7018 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 7019 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 7020 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 7021 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 7022 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 7023 | `			int len;` |
|         9 | 7024 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 7025 | `			sxi64 iLong; double dReal;` |
|         9 | 7026 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 7027 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 7028 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7029 | `					"TypeError",` |
|         - | 7030 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 7031 | `					);` |
|         - | 7032 | `			}` |
|         - | 7033 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 7034 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 7035 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 7036 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 7037 | `			}` |
|         3 | 7038 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 7039 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 7040 | `			nItem = (int)iLong;` |
|         2 | 7041 | `		}else{` |
|        15 | 7042 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 7043 | `		}` |
|         8 | 7044 | `	}` |
|         - | 7045 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 7046 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7047 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 7048 | `	if( pMap->nEntry < 1 ){` |
|         5 | 7049 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7050 | `			"ValueError",` |
|         - | 7051 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7052 | `			);` |
|         - | 7053 | `	}` |
|         - | 7054 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7055 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7056 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7057 | `			"ValueError",` |
|         - | 7058 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7059 | `			);` |
|         - | 7060 | `	}` |
|        13 | 7061 | `	if( nItem < 2 ){` |
|         - | 7062 | `		sxu32 nEntry;` |
|         - | 7063 | `		/* Select a random number */` |
|         9 | 7064 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7065 | `		/* Extract the desired entry.` |
|         - | 7066 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7067 | `		 */` |
|         9 | 7068 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         1 | 7069 | `			pNode = pMap->pLast;` |
|         1 | 7070 | `			nEntry = pMap->nEntry - nEntry;` |
|         1 | 7071 | `			if( nEntry > 1 ){` |
|       ! 0 | 7072 | `				for(;;){` |
|       ! 0 | 7073 | `					if( nEntry == 0 ){` |
|       ! 0 | 7074 | `						break;` |
|         - | 7075 | `					}` |
|         - | 7076 | `					/* Point to the previous entry */` |
|       ! 0 | 7077 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7078 | `					nEntry--;` |
|       ! 0 | 7079 | `				}` |
|       ! 0 | 7080 | `			}` |
|         1 | 7081 | `		}else{` |
|         8 | 7082 | `			pNode = pMap->pFirst;` |
|         3 | 7083 | `			for(;;){` |
|         9 | 7084 | `				if( nEntry == 0 ){` |
|         8 | 7085 | `					break;` |
|         - | 7086 | `				}` |
|         - | 7087 | `				/* Point to the next entry */` |
|         2 | 7088 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         2 | 7089 | `				nEntry--;` |
|         1 | 7090 | `			}` |
|         - | 7091 | `		}` |
|         9 | 7092 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7093 | `			/* Int key */` |
|         7 | 7094 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7095 | `		}else{` |
|         - | 7096 | `			/* Blob key */` |
|         3 | 7097 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7098 | `		}` |
|         5 | 7099 | `	}else{` |
|         - | 7100 | `		ph7_value sKey,*pArray;` |
|         - | 7101 | `		ph7_hashmap *pDest;` |
|         - | 7102 | `		/* Create a new array */` |
|         5 | 7103 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7104 | `		if( pArray == 0 ){` |
|       ! 0 | 7105 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7106 | `			return PH7_OK;` |
|         - | 7107 | `		}` |
|         - | 7108 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7109 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7110 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7111 | `		/* Copy the first n items */` |
|         5 | 7112 | `		pNode = pMap->pFirst;` |
|         5 | 7113 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7114 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7115 | `		}` |
|        15 | 7116 | `		while( nItem > 0){` |
|        11 | 7117 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7118 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7119 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7120 | `			/* Point to the next entry */` |
|        11 | 7121 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7122 | `			nItem--;` |
|         1 | 7123 | `		}` |
|         - | 7124 | `		/* Shuffle the array */` |
|         5 | 7125 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7126 | `		/* Rehash node */` |
|         5 | 7127 | `		HashmapSortRehash(pDest);` |
|         - | 7128 | `		/* Return the random array */` |
|         5 | 7129 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7130 | `	}` |
|        13 | 7131 | `	return PH7_OK;` |
|        19 | 7132 | `}` |
|         - | 7133 | `/*` |
|         - | 7134 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7135 | ` *  Split an array into chunks.` |
|         - | 7136 | ` * Parameters` |
|         - | 7137 | ` * $input` |
|         - | 7138 | ` *   The array to work on` |
|         - | 7139 | ` * $size` |
|         - | 7140 | ` *   The size of each chunk` |
|         - | 7141 | ` * $preserve_keys` |
|         - | 7142 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7143 | ` *   the chunk numerically.` |
|         - | 7144 | ` * Return` |
|         - | 7145 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7146 | ` *  zero, with each dimension containing size elements.` |
|         - | 7147 | ` */` |
|        36 | 7148 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7149 | `{` |
|         - | 7150 | `	ph7_value *pArray,*pChunk;` |
|         - | 7151 | `	ph7_hashmap_node *pEntry;` |
|         - | 7152 | `	ph7_hashmap *pMap;` |
|         - | 7153 | `	int bPreserve;` |
|         - | 7154 | `	sxu32 nChunk;` |
|         - | 7155 | `	sxu32 nSize;` |
|         - | 7156 | `	sxu32 n;` |
|         - | 7157 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7158 | `	if( nArg < 2 ){` |
|         - | 7159 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7160 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7161 | `			"ArgumentCountError",` |
|         - | 7162 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7163 | `			nArg` |
|         - | 7164 | `			);` |
|         - | 7165 | `	}` |
|        41 | 7166 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7167 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7168 | `			"TypeError",` |
|         - | 7169 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7170 | `			ph7_type_name(apArg[0])` |
|         - | 7171 | `			);` |
|         - | 7172 | `	}` |
|         - | 7173 | `	/* Create a new array */` |
|        38 | 7174 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7175 | `	if( pArray == 0 ){` |
|       ! 0 | 7176 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7177 | `		return PH7_OK;` |
|         - | 7178 | `	}` |
|         - | 7179 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7180 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7181 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7182 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7183 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7184 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7185 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7186 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7187 | `			"TypeError",` |
|         - | 7188 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7189 | `			ph7_type_name(apArg[1])` |
|         - | 7190 | `			);` |
|         - | 7191 | `	}` |
|         - | 7192 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7193 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7194 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7195 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7196 | `		int len;` |
|         3 | 7197 | `		sxu8 bReal = FALSE;` |
|         3 | 7198 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7199 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7200 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7201 | `				"TypeError",` |
|         - | 7202 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7203 | `				);` |
|         - | 7204 | `		}` |
|       ! 0 | 7205 | `		if( bReal ){` |
|         - | 7206 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7207 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7208 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7209 | `				zStr` |
|         - | 7210 | `				);` |
|       ! 0 | 7211 | `		}` |
|       ! 0 | 7212 | `	}` |
|         - | 7213 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7214 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7215 | `	 * later via ph7_value_to_int. */` |
|        35 | 7216 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7217 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7218 | `		sxi64 i = (sxi64)d;` |
|         3 | 7219 | `		if( d != (double)i ){` |
|         4 | 7220 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7221 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7222 | `				d` |
|         - | 7223 | `				);` |
|         1 | 7224 | `		}` |
|         1 | 7225 | `	}` |
|         - | 7226 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7227 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7228 | `	{` |
|        35 | 7229 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7230 | `		if( nSizeSigned < 1 ){` |
|         - | 7231 | `			/* size <= 0 -> ValueError */` |
|         6 | 7232 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7233 | `				"ValueError",` |
|         - | 7234 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7235 | `				);` |
|         - | 7236 | `		}` |
|        29 | 7237 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7238 | `	}` |
|        29 | 7239 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7240 | `		/* Return the whole array */` |
|         3 | 7241 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7242 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7243 | `		return PH7_OK;` |
|         - | 7244 | `	}` |
|        27 | 7245 | `	bPreserve = 0;` |
|        27 | 7246 | `	if( nArg > 2 ){` |
|         - | 7247 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7248 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7249 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7250 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7251 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7252 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7253 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7254 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7255 | `				"TypeError",` |
|         - | 7256 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7257 | `				ph7_type_name(apArg[2])` |
|         - | 7258 | `				);` |
|         - | 7259 | `		}` |
|        21 | 7260 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7261 | `	}` |
|         - | 7262 | `	/* Start processing */` |
|        27 | 7263 | `	pEntry = pMap->pFirst;` |
|        27 | 7264 | `	nChunk = 0;` |
|        27 | 7265 | `	pChunk = 0;` |
|        27 | 7266 | `	n = pMap->nEntry;` |
|        56 | 7267 | `	for( ;; ){` |
|       113 | 7268 | `		if( n < 1 ){` |
|         - | 7269 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7270 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7271 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7272 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7273 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7274 | `			 * exists. */` |
|        27 | 7275 | `			if( pChunk ){` |
|        27 | 7276 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7277 | `			}` |
|        27 | 7278 | `			break;` |
|         - | 7279 | `		}` |
|        87 | 7280 | `		if( nChunk < 1 ){` |
|        71 | 7281 | `			if( pChunk ){` |
|         - | 7282 | `				/* Put the first chunk */` |
|        45 | 7283 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7284 | `			}` |
|         - | 7285 | `			/* Create a new dimension */` |
|        71 | 7286 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7287 | `												   * will be automatically released as soon we return` |
|         - | 7288 | `												   * from this function */` |
|        71 | 7289 | `			if( pChunk == 0 ){` |
|       ! 0 | 7290 | `				break;` |
|         - | 7291 | `			}` |
|        71 | 7292 | `			nChunk = nSize;` |
|        35 | 7293 | `		}` |
|         - | 7294 | `		/* Insert the entry */` |
|        87 | 7295 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7296 | `		/* Point to the next entry */` |
|        87 | 7297 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7298 | `		nChunk--;` |
|        87 | 7299 | `		n--;` |
|         1 | 7300 | `	}` |
|         - | 7301 | `	/* Return the multidimensional array */` |
|        27 | 7302 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7303 | `	return PH7_OK;` |
|        23 | 7304 | `}` |
|         - | 7305 | `/*` |
|         - | 7306 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7307 | ` *  Pad array to the specified length with a value.` |
|         - | 7308 | ` * $input` |
|         - | 7309 | ` *   Initial array of values to pad.` |
|         - | 7310 | ` * $pad_size` |
|         - | 7311 | ` *   New size of the array.` |
|         - | 7312 | ` * $pad_value` |
|         - | 7313 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7314 | ` */` |
|         - | 7315 | `/*` |
|         - | 7316 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7317 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7318 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7319 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7320 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7321 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7322 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7323 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7324 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7325 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7326 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7327 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7328 | ` */` |
|        50 | 7329 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7330 | `	ph7_context *pCtx,` |
|         - | 7331 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7332 | `	int iArg,              /* 1-based argument position */` |
|         - | 7333 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7334 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7335 | `	)` |
|         1 | 7336 | `{` |
|        51 | 7337 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7338 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7339 | `			"ValueError",` |
|         - | 7340 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7341 | `			zFunc,iArg,zParam` |
|         - | 7342 | `			);` |
|         - | 7343 | `	}` |
|        37 | 7344 | `	return SXRET_OK;` |
|        26 | 7345 | `}` |
|        62 | 7346 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7347 | `{` |
|         - | 7348 | `	ph7_hashmap *pMap;` |
|         - | 7349 | `	ph7_value *pArray;` |
|         - | 7350 | `	sxi64 iLen,iAbs;` |
|         - | 7351 | `	int nEntry;` |
|         - | 7352 | `	sxi32 rc;` |
|        65 | 7353 | `	if( nArg != 3 ){` |
|         4 | 7354 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7355 | `			"ArgumentCountError",` |
|         - | 7356 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7357 | `			nArg` |
|         - | 7358 | `			);` |
|         - | 7359 | `	}` |
|        62 | 7360 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7361 | `		char zBuf[64];` |
|        11 | 7362 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7363 | `			"TypeError",` |
|         - | 7364 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7365 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7366 | `			);` |
|         - | 7367 | `	}` |
|         - | 7368 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7369 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7370 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7371 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7372 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7373 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7374 | `		char zBuf[64];` |
|       ! 0 | 7375 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7376 | `			"TypeError",` |
|         - | 7377 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7378 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7379 | `			);` |
|         - | 7380 | `	}` |
|        55 | 7381 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7382 | `		int nStr;` |
|        11 | 7383 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7384 | `		sxi64 iLong; double dReal;` |
|        11 | 7385 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7386 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7387 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7388 | `				"TypeError",` |
|         - | 7389 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7390 | `				);` |
|         - | 7391 | `		}` |
|         7 | 7392 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7393 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7394 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7395 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7396 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7397 | `					"TypeError",` |
|         - | 7398 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7399 | `					);` |
|         - | 7400 | `			}` |
|         3 | 7401 | `			iLen = (sxi64)dReal;` |
|         3 | 7402 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7403 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7404 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7405 | `					zStr` |
|         - | 7406 | `					);` |
|       ! 0 | 7407 | `			}` |
|         2 | 7408 | `		}else{` |
|         5 | 7409 | `			iLen = iLong;` |
|         - | 7410 | `		}` |
|         4 | 7411 | `	}else{` |
|        45 | 7412 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7413 | `	}` |
|         - | 7414 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7415 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7416 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7417 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7418 | `	 * overflow). */` |
|        51 | 7419 | `	iAbs = iLen;` |
|        51 | 7420 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7421 | `		iAbs = -iAbs;` |
|         7 | 7422 | `	}` |
|        51 | 7423 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7424 | `	if( rc != SXRET_OK ){` |
|        15 | 7425 | `		return rc;` |
|         - | 7426 | `	}` |
|        37 | 7427 | `	nEntry = (int)iLen;` |
|         - | 7428 | `	/* Create a new array */` |
|        37 | 7429 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7430 | `	if( pArray == 0 ){` |
|       ! 0 | 7431 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7432 | `	}` |
|        37 | 7433 | `	if( nEntry < 0 ){` |
|        11 | 7434 | `		nEntry = -nEntry;` |
|        11 | 7435 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7436 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7437 | `			/* Insert given items first */` |
|        25 | 7438 | `			while( nEntry > 0 ){` |
|        19 | 7439 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7440 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7441 | `				}` |
|        19 | 7442 | `				nEntry--;` |
|         1 | 7443 | `			}` |
|         - | 7444 | `			/* Merge the two arrays */` |
|         7 | 7445 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7446 | `		}else{` |
|         5 | 7447 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7448 | `		}` |
|        32 | 7449 | `	}else if( nEntry > 0 ){` |
|        25 | 7450 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7451 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7452 | `			/* Merge the two arrays first */` |
|        19 | 7453 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7454 | `			/* Insert given items */` |
|       275 | 7455 | `			while( nEntry > 0 ){` |
|       257 | 7456 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7457 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7458 | `				}` |
|       257 | 7459 | `				nEntry--;` |
|         1 | 7460 | `			}` |
|        10 | 7461 | `		}else{` |
|         7 | 7462 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7463 | `		}` |
|        13 | 7464 | `	}else{` |
|         - | 7465 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7466 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7467 | `	}` |
|         - | 7468 | `	/* Return the new array */` |
|        37 | 7469 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7470 | `	return PH7_OK;` |
|        34 | 7471 | `}` |
|         - | 7472 | `/*` |
|         - | 7473 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7474 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7475 | ` * Parameters` |
|         - | 7476 | ` * $array` |
|         - | 7477 | ` *   The array in which elements are replaced.` |
|         - | 7478 | ` * $array1` |
|         - | 7479 | ` *   The array from which elements will be extracted.` |
|         - | 7480 | ` * ....` |
|         - | 7481 | ` *  More arrays from which elements will be extracted.` |
|         - | 7482 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7483 | ` * Return` |
|         - | 7484 | ` *  Returns an array.` |
|         - | 7485 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7486 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7487 | ` */` |
|        20 | 7488 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7489 | `{` |
|         - | 7490 | `	ph7_hashmap *pMap;` |
|         - | 7491 | `	ph7_value *pArray;` |
|         - | 7492 | `	int i;` |
|        23 | 7493 | `	if( nArg < 1 ){` |
|       ! 0 | 7494 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7495 | `			"ArgumentCountError",` |
|         - | 7496 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7497 | `			);` |
|         - | 7498 | `	}` |
|        23 | 7499 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7500 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7501 | `			"TypeError",` |
|         - | 7502 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7503 | `			ph7_type_name(apArg[0])` |
|         - | 7504 | `			);` |
|         - | 7505 | `	}` |
|         - | 7506 | `	/* Create a new array */` |
|        20 | 7507 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7508 | `	if( pArray == 0 ){` |
|       ! 0 | 7509 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7510 | `		return PH7_OK;` |
|         - | 7511 | `	}` |
|         - | 7512 | `	/* Overwrite from the first array */` |
|        20 | 7513 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7514 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7515 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7516 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7517 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7518 | `			/* Type mismatch -> TypeError */` |
|         4 | 7519 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7520 | `				"TypeError",` |
|         - | 7521 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7522 | `				i + 1,` |
|         2 | 7523 | `				ph7_type_name(apArg[i])` |
|         - | 7524 | `				);` |
|         - | 7525 | `		}` |
|         - | 7526 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7527 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7528 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7529 | `	}` |
|         - | 7530 | `	/* Return the new array */` |
|        17 | 7531 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7532 | `	return PH7_OK;` |
|        13 | 7533 | `}` |
|         - | 7534 | `/*` |
|         - | 7535 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7536 | ` *  Filters elements of an array using a callback function.` |
|         - | 7537 | ` * Parameters` |
|         - | 7538 | ` *  $input` |
|         - | 7539 | ` *    The array to iterate over` |
|         - | 7540 | ` * $callback` |
|         - | 7541 | ` *    The callback function to use` |
|         - | 7542 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7543 | ` *    will be removed.` |
|         - | 7544 | ` * Return` |
|         - | 7545 | ` *  The filtered array.` |
|         - | 7546 | ` */` |
|        30 | 7547 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7548 | `{` |
|         - | 7549 | `	ph7_hashmap_node *pEntry;` |
|         - | 7550 | `	ph7_hashmap *pMap;` |
|         - | 7551 | `	ph7_value *pArray;` |
|         - | 7552 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7553 | `	ph7_value *pValue;` |
|         - | 7554 | `	sxi32 rc;` |
|         - | 7555 | `	int keep;` |
|         - | 7556 | `	sxu32 n;` |
|        32 | 7557 | `	if( nArg < 1 ){` |
|         - | 7558 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7559 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7560 | `		return PH7_OK;` |
|         - | 7561 | `	}` |
|         - | 7562 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7563 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7564 | `		char zBuf[64];` |
|        19 | 7565 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7566 | `			"TypeError",` |
|         - | 7567 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7568 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7569 | `			);` |
|         - | 7570 | `	}` |
|         - | 7571 | `	/* Create a new array */` |
|        20 | 7572 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7573 | `	if( pArray == 0 ){` |
|       ! 0 | 7574 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7575 | `		return PH7_OK;` |
|         - | 7576 | `	}` |
|         - | 7577 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7578 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7579 | `	pEntry = pMap->pFirst;` |
|        20 | 7580 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7581 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7582 | `	/* Perform the requested operation */` |
|        78 | 7583 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7584 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7585 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7586 | `		if( pValue == 0 ){` |
|         - | 7587 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7588 | `			keep = FALSE;` |
|        64 | 7589 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7590 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7591 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7592 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7593 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7594 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7595 | `					int len;` |
|         3 | 7596 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7597 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7598 | `						"TypeError",` |
|         - | 7599 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7600 | `						zName` |
|         - | 7601 | `						);` |
|       ! 0 | 7602 | `				}else{` |
|       ! 0 | 7603 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7604 | `						"TypeError",` |
|         - | 7605 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7606 | `						ph7_type_name(apArg[1])` |
|         - | 7607 | `						);` |
|         - | 7608 | `				}` |
|         - | 7609 | `			}` |
|        33 | 7610 | `			keep = FALSE;` |
|        33 | 7611 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7612 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7613 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7614 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7615 | `				return PH7_EXCEPTION;` |
|         - | 7616 | `			}` |
|        31 | 7617 | `			if( rc == SXRET_OK ){` |
|         - | 7618 | `				/* Perform a boolean cast */` |
|        31 | 7619 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7620 | `			}` |
|        31 | 7621 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7622 | `		}else{` |
|         - | 7623 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7624 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7625 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7626 | `			 */` |
|        29 | 7627 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7628 | `		}` |
|        59 | 7629 | `		if( keep ){` |
|         - | 7630 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7631 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7632 | `		}` |
|         - | 7633 | `		/* Point to the next entry */` |
|        59 | 7634 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7635 | `	}` |
|        15 | 7636 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7637 | `	return PH7_OK;` |
|        17 | 7638 | `}` |
|         - | 7639 | `/*` |
|         - | 7640 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7641 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7642 | ` * Parameters` |
|         - | 7643 | ` *  $callback` |
|         - | 7644 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7645 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7646 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7647 | ` *   are zipped together.` |
|         - | 7648 | ` *  $array` |
|         - | 7649 | ` *   The first array to run through the callback function.` |
|         - | 7650 | ` *  $arrays` |
|         - | 7651 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7652 | ` * Return` |
|         - | 7653 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7654 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7655 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7656 | ` *  padding shorter arrays with NULL.` |
|         - | 7657 | ` */` |
|        62 | 7658 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7659 | `{` |
|         - | 7660 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7661 | `	ph7_hashmap_node *pEntry;` |
|         - | 7662 | `	ph7_hashmap *pMap;` |
|         - | 7663 | `	ph7_vm *pVm;` |
|         - | 7664 | `	int bNullCallback;` |
|         - | 7665 | `	sxi32 rc;` |
|         - | 7666 | `	int i;` |
|         - | 7667 | `	sxu32 n;` |
|        66 | 7668 | `	if( nArg < 2 ){` |
|       ! 0 | 7669 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7670 | `			"ArgumentCountError",` |
|         - | 7671 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7672 | `			nArg` |
|         - | 7673 | `			);` |
|         - | 7674 | `	}` |
|        66 | 7675 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        66 | 7676 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         8 | 7677 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         6 | 7678 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         8 | 7679 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7680 | `				"TypeError",` |
|         - | 7681 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7682 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7683 | `				zFunc` |
|         - | 7684 | `				);` |
|         - | 7685 | `		}` |
|         3 | 7686 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7687 | `			"TypeError",` |
|         - | 7688 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7689 | `			"no array or string given"` |
|         - | 7690 | `			);` |
|         - | 7691 | `	}` |
|         - | 7692 | `	/* Every remaining argument must be an array */` |
|       125 | 7693 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        71 | 7694 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7695 | `			if( i == 1 ){` |
|         4 | 7696 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7697 | `					"TypeError",` |
|         - | 7698 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7699 | `					ph7_type_name(apArg[1])` |
|         - | 7700 | `					);` |
|         - | 7701 | `			}` |
|       ! 0 | 7702 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7703 | `				"TypeError",` |
|         - | 7704 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7705 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7706 | `				);` |
|         - | 7707 | `		}` |
|        36 | 7708 | `	}` |
|        57 | 7709 | `	pVm = pCtx->pVm;` |
|         - | 7710 | `	/* Create a new array */` |
|        57 | 7711 | `	pArray = ph7_context_new_array(pCtx);` |
|        57 | 7712 | `	if( pArray == 0 ){` |
|       ! 0 | 7713 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7714 | `		return PH7_OK;` |
|         - | 7715 | `	}` |
|        57 | 7716 | `	PH7_MemObjInit(pVm,&sResult);` |
|        57 | 7717 | `	PH7_MemObjInit(pVm,&sKey);` |
|        57 | 7718 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7719 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7720 | `	if( nArg == 2 ){` |
|         - | 7721 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        47 | 7722 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        47 | 7723 | `		pEntry = pMap->pFirst;` |
|       143 | 7724 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7725 | `			/* Extract the node value */` |
|       103 | 7726 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|       103 | 7727 | `			if( pValue ){` |
|         - | 7728 | `				/* Extract the node key */` |
|       103 | 7729 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       103 | 7730 | `				if( bNullCallback ){` |
|         - | 7731 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7732 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7733 | `				}else{` |
|         - | 7734 | `					/* Invoke the supplied callback */` |
|        93 | 7735 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        93 | 7736 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7737 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7738 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7739 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7740 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7741 | `						return PH7_EXCEPTION;` |
|         - | 7742 | `					}` |
|         - | 7743 | `					/* Insert the callback return value */` |
|        89 | 7744 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7745 | `				}` |
|        99 | 7746 | `				PH7_MemObjRelease(&sKey);` |
|        99 | 7747 | `				PH7_MemObjRelease(&sResult);` |
|        48 | 7748 | `			}` |
|         - | 7749 | `			/* Point to the next entry */` |
|        99 | 7750 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        51 | 7751 | `		}` |
|        23 | 7752 | `	}else{` |
|         - | 7753 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7754 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7755 | `		int nArrays = nArg - 1;` |
|         - | 7756 | `		ph7_hashmap_node **apCur;` |
|         - | 7757 | `		ph7_value **apCallArg;` |
|         - | 7758 | `		ph7_value sNull;` |
|        11 | 7759 | `		sxu32 nMax = 0;` |
|        11 | 7760 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7761 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7762 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7763 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7764 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7765 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7766 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7767 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7768 | `			return PH7_OK;` |
|         - | 7769 | `		}` |
|        11 | 7770 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7771 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7772 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7773 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7774 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7775 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7776 | `				nMax = pMap->nEntry;` |
|         6 | 7777 | `			}` |
|        12 | 7778 | `		}` |
|        35 | 7779 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7780 | `			ph7_value *pZip = 0;` |
|        25 | 7781 | `			if( bNullCallback ){` |
|         - | 7782 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7783 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7784 | `			}` |
|        79 | 7785 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7786 | `				ph7_value *pv = &sNull;` |
|        55 | 7787 | `				if( apCur[i] ){` |
|        53 | 7788 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7789 | `					if( pNodeVal ){` |
|        53 | 7790 | `						pv = pNodeVal;` |
|        26 | 7791 | `					}` |
|        53 | 7792 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7793 | `				}` |
|        55 | 7794 | `				if( bNullCallback ){` |
|         9 | 7795 | `					if( pZip ){` |
|         9 | 7796 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7797 | `					}` |
|         5 | 7798 | `				}else{` |
|        47 | 7799 | `					apCallArg[i] = pv;` |
|         - | 7800 | `				}` |
|        28 | 7801 | `			}` |
|        25 | 7802 | `			if( bNullCallback ){` |
|         5 | 7803 | `				if( pZip ){` |
|         5 | 7804 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7805 | `				}` |
|         3 | 7806 | `			}else{` |
|        21 | 7807 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7808 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7809 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7810 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7811 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7812 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7813 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7814 | `					return PH7_EXCEPTION;` |
|         - | 7815 | `				}` |
|        21 | 7816 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7817 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7818 | `			}` |
|        13 | 7819 | `		}` |
|        11 | 7820 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7821 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7822 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7823 | `	}` |
|        53 | 7824 | `	PH7_MemObjRelease(&sKey);` |
|        53 | 7825 | `	PH7_MemObjRelease(&sResult);` |
|        53 | 7826 | `	ph7_result_value(pCtx,pArray);` |
|        53 | 7827 | `	return PH7_OK;` |
|        35 | 7828 | `}` |
|         - | 7829 | `/*` |
|         - | 7830 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7831 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7832 | ` * Parameters` |
|         - | 7833 | ` *  $array` |
|         - | 7834 | ` *   The input array.` |
|         - | 7835 | ` *  $callback` |
|         - | 7836 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7837 | ` *  $initial` |
|         - | 7838 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7839 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7840 | ` * Return` |
|         - | 7841 | ` *  Returns the resulting value.` |
|         - | 7842 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7843 | ` */` |
|        30 | 7844 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7845 | `{` |
|         - | 7846 | `	ph7_hashmap_node *pEntry;` |
|         - | 7847 | `	ph7_hashmap *pMap;` |
|         - | 7848 | `	ph7_value *pValue;` |
|         - | 7849 | `	ph7_value sResult;` |
|         - | 7850 | `	sxi32 rc;` |
|         - | 7851 | `	sxu32 n;` |
|        35 | 7852 | `	if( nArg < 2 ){` |
|       ! 0 | 7853 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7854 | `			"ArgumentCountError",` |
|         - | 7855 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7856 | `			nArg` |
|         - | 7857 | `			);` |
|         - | 7858 | `	}` |
|        35 | 7859 | `	if( nArg > 3 ){` |
|         4 | 7860 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7861 | `			"ArgumentCountError",` |
|         - | 7862 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7863 | `			nArg` |
|         - | 7864 | `			);` |
|         - | 7865 | `	}` |
|        33 | 7866 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7867 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7868 | `			"TypeError",` |
|         - | 7869 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7870 | `			ph7_type_name(apArg[0])` |
|         - | 7871 | `			);` |
|         - | 7872 | `	}` |
|        31 | 7873 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7874 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7875 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7876 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7877 | `				"TypeError",` |
|         - | 7878 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7879 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7880 | `				zFunc` |
|         - | 7881 | `				);` |
|         - | 7882 | `		}` |
|         9 | 7883 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7884 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7885 | `				"TypeError",` |
|         - | 7886 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7887 | `				"array callback must have exactly two members"` |
|         - | 7888 | `				);` |
|         - | 7889 | `		}` |
|         6 | 7890 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7891 | `			"TypeError",` |
|         - | 7892 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7893 | `			"no array or string given"` |
|         - | 7894 | `			);` |
|         - | 7895 | `	}` |
|         - | 7896 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7897 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7898 | `	/* Assume a NULL initial value */` |
|        19 | 7899 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7900 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7901 | `	if( nArg > 2 ){` |
|         - | 7902 | `		/* Set the initial value */` |
|        13 | 7903 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7904 | `	}` |
|         - | 7905 | `	/* Perform the requested operation */` |
|        19 | 7906 | `	pEntry = pMap->pFirst;` |
|        55 | 7907 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7908 | `		/* Extract the node value */` |
|        39 | 7909 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7910 | `		/* Invoke the supplied callback */` |
|        39 | 7911 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7912 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7913 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7914 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7915 | `			return PH7_EXCEPTION;` |
|         - | 7916 | `		}` |
|         - | 7917 | `		/* Point to the next entry */` |
|        37 | 7918 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7919 | `	}` |
|        17 | 7920 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7921 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7922 | `	return PH7_OK;` |
|        20 | 7923 | `}` |
|         - | 7924 | `/*` |
|         - | 7925 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7926 | ` *  Apply a user function to every member of an array.` |
|         - | 7927 | ` * Parameters` |
|         - | 7928 | ` *  $array` |
|         - | 7929 | ` *   The input array.` |
|         - | 7930 | ` *  $funcname` |
|         - | 7931 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7932 | ` *   the first, and the key/index second.` |
|         - | 7933 | ` * Note:` |
|         - | 7934 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7935 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7936 | ` *  be made in the original array itself.` |
|         - | 7937 | ` *  $userdata` |
|         - | 7938 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7939 | ` *   to the callback funcname.` |
|         - | 7940 | ` * Return` |
|         - | 7941 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7942 | ` */` |
|        36 | 7943 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7944 | `{` |
|         - | 7945 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7946 | `	ph7_hashmap_node *pEntry;` |
|         - | 7947 | `	ph7_hashmap *pMap;` |
|         - | 7948 | `	sxu32 n;` |
|        41 | 7949 | `	if( nArg < 2 ){` |
|       ! 0 | 7950 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7951 | `			"ArgumentCountError",` |
|         - | 7952 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7953 | `			nArg` |
|         - | 7954 | `			);` |
|         - | 7955 | `	}` |
|        41 | 7956 | `	if( nArg > 3 ){` |
|         4 | 7957 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7958 | `			"ArgumentCountError",` |
|         - | 7959 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7960 | `			nArg` |
|         - | 7961 | `			);` |
|         - | 7962 | `	}` |
|        39 | 7963 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7964 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7965 | `			"TypeError",` |
|         - | 7966 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7967 | `			ph7_type_name(apArg[0])` |
|         - | 7968 | `			);` |
|         - | 7969 | `	}` |
|        37 | 7970 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        17 | 7971 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         6 | 7972 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         8 | 7973 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7974 | `				"TypeError",` |
|         - | 7975 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7976 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7977 | `				zFunc` |
|         - | 7978 | `				);` |
|         - | 7979 | `		}` |
|        12 | 7980 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7981 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7982 | `				"TypeError",` |
|         - | 7983 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7984 | `				"array callback must have exactly two members"` |
|         - | 7985 | `				);` |
|         - | 7986 | `		}` |
|         6 | 7987 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7988 | `			"TypeError",` |
|         - | 7989 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7990 | `			"no array or string given"` |
|         - | 7991 | `			);` |
|         - | 7992 | `	}` |
|        21 | 7993 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7994 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7995 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7996 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7997 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7998 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7999 | `	/* Perform the desired operation */` |
|        21 | 8000 | `	pEntry = pMap->pFirst;` |
|        61 | 8001 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8002 | `		/* Extract the node value */` |
|        43 | 8003 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 8004 | `		if( pValue ){` |
|         - | 8005 | `			sxi32 rcW;` |
|         - | 8006 | `			/* Extract the entry key */` |
|        43 | 8007 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8008 | `			/* Invoke the supplied callback */` |
|        43 | 8009 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 8010 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 8011 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 8012 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 8013 | `				return PH7_EXCEPTION;` |
|         - | 8014 | `			}` |
|        20 | 8015 | `		}` |
|         - | 8016 | `		/* Point to the next entry */` |
|        41 | 8017 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 8018 | `	}` |
|         - | 8019 | `	/* All done, return TRUE */` |
|        19 | 8020 | `	ph7_result_bool(pCtx,1);` |
|        19 | 8021 | `	return PH7_OK;` |
|        23 | 8022 | `}` |
|         - | 8023 | `/*` |
|         - | 8024 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 8025 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 8026 | ` */` |
|        22 | 8027 | `static sxi32 HashmapWalkRecursive(` |
|         - | 8028 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 8029 | `	ph7_value *pCallback, /* User callback */` |
|         - | 8030 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 8031 | `	int iNest             /* Nesting level */` |
|         - | 8032 | `	)` |
|         1 | 8033 | `{` |
|         - | 8034 | `	ph7_hashmap_node *pEntry;` |
|         - | 8035 | `	ph7_value *pValue,sKey;` |
|         - | 8036 | `	sxi32 rc;` |
|         - | 8037 | `	sxu32 n;` |
|         - | 8038 | `	/* Iterate through hashmap entries */` |
|        23 | 8039 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 8040 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 8041 | `	pEntry = pMap->pFirst;` |
|        59 | 8042 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8043 | `		/* Extract the node value */` |
|        37 | 8044 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 8045 | `		if( pValue ){` |
|        37 | 8046 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 8047 | `				if( iNest < 32 ){` |
|         - | 8048 | `					/* Recurse */` |
|        11 | 8049 | `					iNest++;` |
|        11 | 8050 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8051 | `					iNest--;` |
|        11 | 8052 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8053 | `						return PH7_EXCEPTION;` |
|         - | 8054 | `					}` |
|         5 | 8055 | `				}` |
|         6 | 8056 | `			}else{` |
|         - | 8057 | `				/* Extract the node key */` |
|        27 | 8058 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8059 | `				/* Invoke the supplied callback */` |
|        27 | 8060 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8061 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8062 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8063 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8064 | `					return PH7_EXCEPTION;` |
|         - | 8065 | `				}` |
|         - | 8066 | `			}` |
|        18 | 8067 | `		}` |
|         - | 8068 | `		/* Point to the next entry */` |
|        37 | 8069 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8070 | `	}` |
|        23 | 8071 | `	return PH7_OK;` |
|        12 | 8072 | `}` |
|         - | 8073 | `/*` |
|         - | 8074 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8075 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8076 | ` * Parameters` |
|         - | 8077 | ` *  $array` |
|         - | 8078 | ` *   The input array.` |
|         - | 8079 | ` *  $funcname` |
|         - | 8080 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8081 | ` *   the first, and the key/index second.` |
|         - | 8082 | ` * Note:` |
|         - | 8083 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8084 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8085 | ` *  be made in the original array itself.` |
|         - | 8086 | ` *  $userdata` |
|         - | 8087 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8088 | ` *   to the callback funcname.` |
|         - | 8089 | ` * Return` |
|         - | 8090 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8091 | ` */` |
|        26 | 8092 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8093 | `{` |
|         - | 8094 | `	ph7_hashmap *pMap;` |
|        31 | 8095 | `	if( nArg < 2 ){` |
|       ! 0 | 8096 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8097 | `			"ArgumentCountError",` |
|         - | 8098 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8099 | `			nArg` |
|         - | 8100 | `			);` |
|         - | 8101 | `	}` |
|        31 | 8102 | `	if( nArg > 3 ){` |
|         4 | 8103 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8104 | `			"ArgumentCountError",` |
|         - | 8105 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8106 | `			nArg` |
|         - | 8107 | `			);` |
|         - | 8108 | `	}` |
|        29 | 8109 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8110 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8111 | `			"TypeError",` |
|         - | 8112 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8113 | `			ph7_type_name(apArg[0])` |
|         - | 8114 | `			);` |
|         - | 8115 | `	}` |
|        27 | 8116 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8117 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8118 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8119 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8120 | `				"TypeError",` |
|         - | 8121 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8122 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8123 | `				zFunc` |
|         - | 8124 | `				);` |
|         - | 8125 | `		}` |
|        12 | 8126 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8127 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8128 | `				"TypeError",` |
|         - | 8129 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8130 | `				"array callback must have exactly two members"` |
|         - | 8131 | `				);` |
|         - | 8132 | `		}` |
|         6 | 8133 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8134 | `			"TypeError",` |
|         - | 8135 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8136 | `			"no array or string given"` |
|         - | 8137 | `			);` |
|         - | 8138 | `	}` |
|         - | 8139 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8140 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8141 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8142 | `	/* Perform the desired operation */` |
|        13 | 8143 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8144 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8145 | `		return PH7_EXCEPTION;` |
|         - | 8146 | `	}` |
|         - | 8147 | `	/* All done, return TRUE */` |
|        13 | 8148 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8149 | `	return PH7_OK;` |
|        18 | 8150 | `}` |
|         - | 8151 | `/*` |
|         - | 8152 | ` * bool array_is_list(array $array)` |
|         - | 8153 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8154 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8155 | ` * Return` |
|         - | 8156 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8157 | ` */` |
|         - | 8158 | `/*` |
|         - | 8159 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8160 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8161 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8162 | ` */` |
|       300 | 8163 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8164 | `{` |
|       301 | 8165 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       301 | 8166 | `	sxi64 iExpect = 0;` |
|         - | 8167 | `	sxu32 n;` |
|       689 | 8168 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       517 | 8169 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8170 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       129 | 8171 | `			return 0;` |
|         - | 8172 | `		}` |
|       389 | 8173 | `		++iExpect;` |
|       389 | 8174 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       195 | 8175 | `	}` |
|       173 | 8176 | `	return 1;` |
|       151 | 8177 | `}` |
|        12 | 8178 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8179 | `{` |
|        13 | 8180 | `	if( nArg < 1 ){` |
|       ! 0 | 8181 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8182 | `			"ArgumentCountError",` |
|         - | 8183 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8184 | `			);` |
|         - | 8185 | `	}` |
|        13 | 8186 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8187 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8188 | `			"TypeError",` |
|         - | 8189 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8190 | `			ph7_type_name(apArg[0])` |
|         - | 8191 | `			);` |
|         - | 8192 | `	}` |
|        13 | 8193 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8194 | `	return PH7_OK;` |
|         7 | 8195 | `}` |
|         - | 8196 | `/*` |
|         - | 8197 | ` * mixed array_first(array $array)` |
|         - | 8198 | ` * mixed array_last(array $array)` |
|         - | 8199 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8200 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8201 | ` *  untouched (unlike reset()/end()).` |
|         - | 8202 | ` */` |
|        18 | 8203 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8204 | `{` |
|         - | 8205 | `	ph7_hashmap *pMap;` |
|         - | 8206 | `	ph7_hashmap_node *pNode;` |
|         - | 8207 | `	ph7_value *pVal;` |
|        19 | 8208 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8209 | `	if( nArg < 1 ){` |
|       ! 0 | 8210 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8211 | `			"ArgumentCountError",` |
|         - | 8212 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8213 | `			zName` |
|         - | 8214 | `			);` |
|         - | 8215 | `	}` |
|        19 | 8216 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8217 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8218 | `			"TypeError",` |
|         - | 8219 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8220 | `			zName,` |
|         1 | 8221 | `			ph7_type_name(apArg[0])` |
|         - | 8222 | `			);` |
|         - | 8223 | `	}` |
|        17 | 8224 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8225 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8226 | `	if( pNode == 0 ){` |
|         - | 8227 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8228 | `		ph7_result_null(pCtx);` |
|         5 | 8229 | `		return PH7_OK;` |
|         - | 8230 | `	}` |
|        13 | 8231 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8232 | `	if( pVal ){` |
|        13 | 8233 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8234 | `	}else{` |
|       ! 0 | 8235 | `		ph7_result_null(pCtx);` |
|         - | 8236 | `	}` |
|        13 | 8237 | `	return PH7_OK;` |
|        10 | 8238 | `}` |
|         8 | 8239 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8240 | `{` |
|         9 | 8241 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8242 | `}` |
|        10 | 8243 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8244 | `{` |
|        11 | 8245 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8246 | `}` |
|         - | 8247 | `/*` |
|         - | 8248 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8249 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8250 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8251 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8252 | ` *  untouched.` |
|         - | 8253 | ` */` |
|        22 | 8254 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8255 | `{` |
|         - | 8256 | `	ph7_hashmap *pMap;` |
|         - | 8257 | `	ph7_hashmap_node *pNode;` |
|        23 | 8258 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8259 | `	if( nArg < 1 ){` |
|       ! 0 | 8260 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8261 | `			"ArgumentCountError",` |
|         - | 8262 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8263 | `			zName` |
|         - | 8264 | `			);` |
|         - | 8265 | `	}` |
|        23 | 8266 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8267 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8268 | `			"TypeError",` |
|         - | 8269 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8270 | `			zName,` |
|         1 | 8271 | `			ph7_type_name(apArg[0])` |
|         - | 8272 | `			);` |
|         - | 8273 | `	}` |
|        21 | 8274 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8275 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8276 | `	if( pNode == 0 ){` |
|         - | 8277 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8278 | `		ph7_result_null(pCtx);` |
|         5 | 8279 | `		return PH7_OK;` |
|         - | 8280 | `	}` |
|        17 | 8281 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8282 | `	return PH7_OK;` |
|        12 | 8283 | `}` |
|        10 | 8284 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8285 | `{` |
|        11 | 8286 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8287 | `}` |
|        12 | 8288 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8289 | `{` |
|        13 | 8290 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8291 | `}` |
|         - | 8292 | `/*` |
|         - | 8293 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8294 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8295 | ` * array_column() for both the column value and the index key.` |
|         - | 8296 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8297 | ` * container or the key is absent.` |
|         - | 8298 | ` */` |
|        32 | 8299 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8300 | `{` |
|        33 | 8301 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8302 | `		ph7_hashmap_node *pNode;` |
|        25 | 8303 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8304 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8305 | `		}` |
|        11 | 8306 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8307 | `		ph7_value sName;` |
|         - | 8308 | `		const char *zName;` |
|         - | 8309 | `		ph7_value *pAttr;` |
|         - | 8310 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8311 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8312 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8313 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8314 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8315 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8316 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8317 | `		return pAttr;` |
|         - | 8318 | `	}` |
|         5 | 8319 | `	return 0;` |
|        17 | 8320 | `}` |
|         - | 8321 | `/*` |
|         - | 8322 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8323 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8324 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8325 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8326 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8327 | ` *  Each row may be an array or an object.` |
|         - | 8328 | ` */` |
|        12 | 8329 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8330 | `{` |
|         - | 8331 | `	ph7_hashmap_node *pNode;` |
|         - | 8332 | `	ph7_hashmap *pMap;` |
|         - | 8333 | `	ph7_value *pArray;` |
|         - | 8334 | `	ph7_value *pRow;` |
|         - | 8335 | `	ph7_value *pCol;` |
|         - | 8336 | `	ph7_value *pIdx;` |
|         - | 8337 | `	int bWantCol;` |
|         - | 8338 | `	int bWantIdx;` |
|         - | 8339 | `	sxu32 n;` |
|        13 | 8340 | `	if( nArg < 2 ){` |
|       ! 0 | 8341 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8342 | `			"ArgumentCountError",` |
|         - | 8343 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8344 | `			nArg` |
|         - | 8345 | `			);` |
|         - | 8346 | `	}` |
|        13 | 8347 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8348 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8349 | `			"TypeError",` |
|         - | 8350 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8351 | `			ph7_type_name(apArg[0])` |
|         - | 8352 | `			);` |
|         - | 8353 | `	}` |
|        13 | 8354 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8355 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8356 | `	if( pArray == 0 ){` |
|       ! 0 | 8357 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8358 | `		return PH7_OK;` |
|         - | 8359 | `	}` |
|         - | 8360 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8361 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8362 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8363 | `	pNode = pMap->pFirst;` |
|        33 | 8364 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8365 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8366 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8367 | `		if( pRow == 0 ){` |
|       ! 0 | 8368 | `			continue;` |
|         - | 8369 | `		}` |
|        21 | 8370 | `		if( bWantCol ){` |
|        19 | 8371 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8372 | `			if( pCol == 0 ){` |
|         - | 8373 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8374 | `				continue;` |
|         - | 8375 | `			}` |
|         9 | 8376 | `		}else{` |
|         3 | 8377 | `			pCol = pRow;` |
|         - | 8378 | `		}` |
|        19 | 8379 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8380 | `		if( pIdx ){` |
|        13 | 8381 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8382 | `		}else{` |
|         7 | 8383 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8384 | `		}` |
|        10 | 8385 | `	}` |
|        13 | 8386 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8387 | `	return PH7_OK;` |
|         7 | 8388 | `}` |
|         - | 8389 | `/*` |
|         - | 8390 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8391 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8392 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8393 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8394 | ` */` |
|        28 | 8395 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8396 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8397 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8398 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8399 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8400 | `	)` |
|         1 | 8401 | `{` |
|         - | 8402 | `	ph7_hashmap_node *pEntry;` |
|         - | 8403 | `	ph7_hashmap *pMap;` |
|         - | 8404 | `	ph7_value *pValue;` |
|         - | 8405 | `	ph7_value *apCbArg[2];` |
|         - | 8406 | `	ph7_value sKey;` |
|         - | 8407 | `	ph7_value sResult;` |
|         - | 8408 | `	sxi32 rc;` |
|         - | 8409 | `	sxu32 n;` |
|        29 | 8410 | `	*ppMatch = 0;` |
|        29 | 8411 | `	if( nArg < 2 ){` |
|       ! 0 | 8412 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8413 | `			"ArgumentCountError",` |
|         - | 8414 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8415 | `			zName,nArg` |
|         - | 8416 | `			);` |
|         - | 8417 | `	}` |
|        29 | 8418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8419 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8420 | `			"TypeError",` |
|         - | 8421 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8422 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8423 | `			);` |
|         - | 8424 | `	}` |
|        29 | 8425 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8426 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8427 | `			"TypeError",` |
|         - | 8428 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8429 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8430 | `			);` |
|         - | 8431 | `	}` |
|        29 | 8432 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8433 | `	pEntry = pMap->pFirst;` |
|        29 | 8434 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8435 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8436 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8437 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8438 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8439 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8440 | `		if( pValue ){` |
|         - | 8441 | `			/* The callback receives ($value, $key). */` |
|        59 | 8442 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8443 | `			apCbArg[0] = pValue;` |
|        59 | 8444 | `			apCbArg[1] = &sKey;` |
|        59 | 8445 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8446 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8447 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8448 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8449 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8450 | `				return PH7_EXCEPTION;` |
|         - | 8451 | `			}` |
|        59 | 8452 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8453 | `				*ppMatch = pEntry;` |
|        15 | 8454 | `				break;` |
|         - | 8455 | `			}` |
|        22 | 8456 | `		}` |
|        45 | 8457 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8458 | `	}` |
|        29 | 8459 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8460 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8461 | `	return PH7_OK;` |
|        15 | 8462 | `}` |
|         - | 8463 | `/*` |
|         - | 8464 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8465 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8466 | ` *  is truthy, or NULL if none match.` |
|         - | 8467 | ` */` |
|         6 | 8468 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8469 | `{` |
|         - | 8470 | `	ph7_hashmap_node *pMatch;` |
|         - | 8471 | `	ph7_value *pVal;` |
|         - | 8472 | `	sxi32 rc;` |
|         7 | 8473 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8474 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8475 | `		return rc;` |
|         - | 8476 | `	}` |
|         7 | 8477 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8478 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8479 | `	}else{` |
|         3 | 8480 | `		ph7_result_null(pCtx);` |
|         - | 8481 | `	}` |
|         7 | 8482 | `	return PH7_OK;` |
|         4 | 8483 | `}` |
|         - | 8484 | `/*` |
|         - | 8485 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8486 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8487 | ` *  is truthy, or NULL if none match.` |
|         - | 8488 | ` */` |
|         6 | 8489 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8490 | `{` |
|         - | 8491 | `	ph7_hashmap_node *pMatch;` |
|         - | 8492 | `	sxi32 rc;` |
|         7 | 8493 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8494 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8495 | `		return rc;` |
|         - | 8496 | `	}` |
|         7 | 8497 | `	if( pMatch == 0 ){` |
|         3 | 8498 | `		ph7_result_null(pCtx);` |
|         6 | 8499 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8500 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8501 | `	}else{` |
|         4 | 8502 | `		ph7_result_string(pCtx,` |
|         2 | 8503 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8504 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8505 | `	}` |
|         7 | 8506 | `	return PH7_OK;` |
|         4 | 8507 | `}` |
|         - | 8508 | `/*` |
|         - | 8509 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8510 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8511 | ` *  FALSE for an empty array.` |
|         - | 8512 | ` */` |
|         8 | 8513 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8514 | `{` |
|         - | 8515 | `	ph7_hashmap_node *pMatch;` |
|         - | 8516 | `	sxi32 rc;` |
|         9 | 8517 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8518 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8519 | `		return rc;` |
|         - | 8520 | `	}` |
|         9 | 8521 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8522 | `	return PH7_OK;` |
|         5 | 8523 | `}` |
|         - | 8524 | `/*` |
|         - | 8525 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8526 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8527 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8528 | ` */` |
|         8 | 8529 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8530 | `{` |
|         - | 8531 | `	ph7_hashmap_node *pMatch;` |
|         - | 8532 | `	sxi32 rc;` |
|         9 | 8533 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8534 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8535 | `		return rc;` |
|         - | 8536 | `	}` |
|         9 | 8537 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8538 | `	return PH7_OK;` |
|         5 | 8539 | `}` |
|         - | 8540 | `/*` |
|         - | 8541 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8542 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8543 | ` */` |
|         - | 8544 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8545 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8546 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8547 | `{` |
|        84 | 8548 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8549 | `	(void)pVm;` |
|        84 | 8550 | `	p->nCount++;` |
|        84 | 8551 | `	if( p->pArray ){` |
|         - | 8552 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8553 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8554 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8555 | `	}` |
|        84 | 8556 | `	return SXRET_OK;` |
|         4 | 8557 | `}` |
|         - | 8558 | `/*` |
|         - | 8559 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8560 | ` */` |
|        30 | 8561 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8562 | `{` |
|         - | 8563 | `	struct IterCollect sCol;` |
|         - | 8564 | `	ph7_value *pArray;` |
|         - | 8565 | `	sxi32 rc;` |
|        34 | 8566 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8567 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8568 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8569 | `	sCol.pArray = pArray;` |
|        34 | 8570 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8571 | `	sCol.nCount = 0;` |
|        34 | 8572 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8573 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8574 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8575 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8576 | `		sxu32 n;` |
|         9 | 8577 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8578 | `			ph7_value sKey, *pVal;` |
|         7 | 8579 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8580 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8581 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8582 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8583 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8584 | `			pEntry = pEntry->pPrev;` |
|         4 | 8585 | `		}` |
|         3 | 8586 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8587 | `		return PH7_OK;` |
|         - | 8588 | `	}` |
|        32 | 8589 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8590 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8591 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8592 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8593 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8594 | `			ph7_type_name(apArg[0]));` |
|         - | 8595 | `	}` |
|        30 | 8596 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8597 | `	return PH7_OK;` |
|        19 | 8598 | `}` |
|         - | 8599 | `/*` |
|         - | 8600 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8601 | ` */` |
|         8 | 8602 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8603 | `{` |
|         - | 8604 | `	struct IterCollect sCol;` |
|         - | 8605 | `	sxi32 rc;` |
|         9 | 8606 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8607 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8608 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8609 | `		return PH7_OK;` |
|         - | 8610 | `	}` |
|         7 | 8611 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8612 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8613 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8614 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8615 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8616 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8617 | `			ph7_type_name(apArg[0]));` |
|         - | 8618 | `	}` |
|         7 | 8619 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8620 | `	return PH7_OK;` |
|         5 | 8621 | `}` |
|         - | 8622 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8623 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8624 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8625 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8626 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8627 | `{` |
|        33 | 8628 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8629 | `	ph7_value sResult;` |
|         - | 8630 | `	SySet aArg;` |
|         - | 8631 | `	sxi32 rc;` |
|         - | 8632 | `	int bContinue;` |
|        16 | 8633 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8634 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8635 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8636 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8637 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8638 | `		sxu32 n;` |
|        17 | 8639 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8640 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8641 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8642 | `			pEntry = pEntry->pPrev;` |
|         5 | 8643 | `		}` |
|         4 | 8644 | `	}` |
|        33 | 8645 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8646 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8647 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8648 | `	SySetRelease(&aArg);` |
|        33 | 8649 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8650 | `	p->nCount++;` |
|        31 | 8651 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8652 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8653 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8654 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8655 | `}` |
|         - | 8656 | `/*` |
|         - | 8657 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8658 | ` */` |
|        12 | 8659 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8660 | `{` |
|         - | 8661 | `	struct IterApply sApp;` |
|         - | 8662 | `	sxi32 rc;` |
|        13 | 8663 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8664 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8665 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8666 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8667 | `	}` |
|        13 | 8668 | `	sApp.pCallback = apArg[1];` |
|        13 | 8669 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8670 | `	sApp.nCount = 0;` |
|        13 | 8671 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8672 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8673 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8674 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8675 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8676 | `			ph7_type_name(apArg[0]));` |
|         - | 8677 | `	}` |
|        11 | 8678 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8679 | `	return PH7_OK;` |
|         7 | 8680 | `}` |
|         - | 8681 | `/*` |
|         - | 8682 | ` * Table of hashmap functions.` |
|         - | 8683 | ` */` |
|         - | 8684 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8685 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8686 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8687 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8688 | `	{"count",             ph7_hashmap_count },` |
|         - | 8689 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8690 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8691 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8692 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8693 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8694 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8695 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8696 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8697 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8698 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8699 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8700 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8701 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8702 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8703 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8704 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8705 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8706 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8707 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8708 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8709 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8710 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8711 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8712 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8713 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8714 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8715 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8716 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8717 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8718 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8719 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8720 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8721 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8722 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8723 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8724 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8725 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8726 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8727 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8728 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8729 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8730 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8731 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8732 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8733 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8734 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8735 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8736 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8737 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8738 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8739 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8740 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8741 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8742 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8743 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8744 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8745 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8746 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8747 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8748 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8749 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8750 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8751 | `	{"current",           ph7_hashmap_current },` |
|         - | 8752 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8753 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8754 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8755 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8756 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8757 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8758 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8759 | `};` |
|         - | 8760 | `/*` |
|         - | 8761 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8762 | ` */` |
|      3356 | 8763 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8764 | `{` |
|         - | 8765 | `	sxu32 n;` |
|    251705 | 8766 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    248349 | 8767 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    124177 | 8768 | `	}` |
|      3361 | 8769 | `}` |
|         - | 8770 | `/*` |
|         - | 8771 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8772 | ` * the BLOB given as the first argument.` |
|         - | 8773 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8774 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8775 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8776 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8777 | ` */` |
|         - | 8778 | `/*` |
|         - | 8779 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8780 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8781 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8782 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8783 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8784 | ` */` |
|       120 | 8785 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8786 | `{` |
|       122 | 8787 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8788 | `	ph7_value *pObj;` |
|       122 | 8789 | `	sxu32 n = 0;` |
|         - | 8790 | `	int isRef;` |
|       122 | 8791 | `	sxi32 rc = SXRET_OK;` |
|         - | 8792 | `	int i;` |
|       195 | 8793 | `	for(;;){` |
|       392 | 8794 | `		if( n >= pMap->nEntry ){` |
|       122 | 8795 | `			break;` |
|         - | 8796 | `		}` |
|       272 | 8797 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8798 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8799 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       540 | 8800 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       270 | 8801 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       272 | 8802 | `		if( ShowType ){` |
|         - | 8803 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8804 | `			 * on the next line at the same indent (php). */` |
|       104 | 8805 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        70 | 8806 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        36 | 8807 | `			}` |
|        36 | 8808 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8809 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8810 | `			}else{` |
|        20 | 8811 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8812 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8813 | `			}` |
|        36 | 8814 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        36 | 8815 | `			if( pObj ){` |
|        36 | 8816 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        36 | 8817 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8818 | `					break;` |
|         - | 8819 | `				}` |
|        17 | 8820 | `			}` |
|        19 | 8821 | `		}else{` |
|         - | 8822 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8823 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8824 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8825 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8826 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8827 | `			}` |
|       238 | 8828 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8829 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8830 | `			}else{` |
|       170 | 8831 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8832 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8833 | `			}` |
|       236 | 8834 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8835 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8836 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8837 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8838 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8839 | `					break;` |
|         - | 8840 | `				}` |
|        13 | 8841 | `			}else{` |
|       214 | 8842 | `				if( pObj ){` |
|       214 | 8843 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8844 | `				}` |
|       214 | 8845 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8846 | `			}` |
|         - | 8847 | `		}` |
|         - | 8848 | `		/* Point to the next entry */` |
|       272 | 8849 | `		n++;` |
|       272 | 8850 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8851 | `	}` |
|       122 | 8852 | `	return rc;` |
|         2 | 8853 | `}` |
|       116 | 8854 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8855 | `{` |
|         - | 8856 | `	sxi32 rc;` |
|         - | 8857 | `	int i;` |
|       118 | 8858 | `	if( nDepth > 31 ){` |
|         - | 8859 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8860 | `		/* Nesting limit reached */` |
|       ! 0 | 8861 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8862 | `		return SXERR_LIMIT;` |
|         - | 8863 | `	}` |
|       118 | 8864 | `	if( ShowType ){` |
|         - | 8865 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8866 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8867 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8868 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8869 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8870 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8871 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8872 | `		}` |
|        14 | 8873 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8874 | `		return rc;` |
|         - | 8875 | `	}` |
|         - | 8876 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8877 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8878 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8879 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8880 | `	}` |
|       105 | 8881 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8882 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8883 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8884 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8885 | `	}` |
|       105 | 8886 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8887 | `	return rc;` |
|        60 | 8888 | `}` |
|         - | 8889 | `/*` |
|         - | 8890 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8891 | ` * retrieved entry.` |
|         - | 8892 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8893 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8894 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8895 | ` * a value different from PH7_OK.` |
|         - | 8896 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8897 | ` */` |
|     34114 | 8898 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8899 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8900 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8901 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8902 | `	)` |
|         5 | 8903 | `{` |
|         - | 8904 | `	ph7_hashmap_node *pEntry;` |
|         - | 8905 | `	ph7_value sKey,sValue;` |
|         - | 8906 | `	sxi32 rc;` |
|         - | 8907 | `	sxu32 n;` |
|         - | 8908 | `	/* Initialize walker parameter */` |
|     34119 | 8909 | `	rc = SXRET_OK;` |
|     34119 | 8910 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34119 | 8911 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34119 | 8912 | `	n = pMap->nEntry;` |
|     34119 | 8913 | `	pEntry = pMap->pFirst;` |
|         - | 8914 | `	/* Start the iteration process */` |
|     93195 | 8915 | `	for(;;){` |
|    186395 | 8916 | `		if( n < 1 ){` |
|     34119 | 8917 | `			break;` |
|         - | 8918 | `		}` |
|         - | 8919 | `		/* Extract a copy of the key and a copy the current value */` |
|    152281 | 8920 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    152281 | 8921 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8922 | `		/* Invoke the user callback */` |
|    152281 | 8923 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8924 | `		/* Release the copy of the key and the value */` |
|    152281 | 8925 | `		PH7_MemObjRelease(&sKey);` |
|    152281 | 8926 | `		PH7_MemObjRelease(&sValue);` |
|    152281 | 8927 | `		if( rc != PH7_OK ){` |
|         - | 8928 | `			/* Callback request an operation abort */` |
|       ! 0 | 8929 | `			return SXERR_ABORT;` |
|         - | 8930 | `		}` |
|         - | 8931 | `		/* Point to the next entry */` |
|    152281 | 8932 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    152281 | 8933 | `		n--;` |
|         5 | 8934 | `	}` |
|         - | 8935 | `	/* All done */` |
|     34119 | 8936 | `	return SXRET_OK;` |
|     17062 | 8937 | `}` |
|         - | 8938 |  |
