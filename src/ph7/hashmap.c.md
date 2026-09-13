# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4021/4505 lines (89.26%)

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
|   7956040 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7956045 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7956045 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    519565 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    519570 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    519570 |   35 | `	sxu32 nH = 5381;` |
|    519570 |   36 | `	zEnd = &zIn[nLen];` |
|    596425 |   37 | `	for(;;){` |
|   1192856 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1018934 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    917705 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    793591 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    519570 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      2284 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      2289 |   54 | `	sxi64 iCount = 0;` |
|      2289 |   55 | `	if( !bRecursive ){` |
|      2115 |   56 | `		iCount = pMap->nEntry;` |
|      1060 |   57 | `	}else{` |
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
|      2289 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3647734 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3647739 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3647739 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3647739 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3647739 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3647739 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3647739 |  112 | `	pNode->nHash = nHash;` |
|   3647739 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3647739 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3647739 |  115 | `	return pNode;` |
|   1823872 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    199761 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    199766 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    199766 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    199766 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    199766 |  133 | `	pNode->pMap  = &(*pMap);` |
|    199766 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    199766 |  135 | `	pNode->nHash = nHash;` |
|    199766 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    199766 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    199766 |  138 | `	pNode->nValIdx = nValIdx;` |
|    199766 |  139 | `	return pNode;` |
|     99885 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3847495 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3847500 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   3389003 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   3389003 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1694499 |  150 | `	}` |
|   3847500 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3847500 |  153 | `	if( pMap->pFirst == 0 ){` |
|     88460 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     88460 |  156 | `		pMap->pCur = pNode;` |
|     44232 |  157 | `	}else{` |
|   3759045 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3847500 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3847500 |  174 | `	++pMap->nEntry;` |
|   3847500 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7910 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7915 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7915 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7915 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7425 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3715 |  187 | `	}else{` |
|       492 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7915 |  190 | `	if( pNode->pNextCollide ){` |
|      5327 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2662 |  192 | `	}` |
|      7915 |  193 | `	if( pMap->pFirst == pNode ){` |
|       173 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        84 |  195 | `	}` |
|      7915 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       209 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       102 |  199 | `	}` |
|      7915 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7915 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7915 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       215 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       215 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       215 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       105 |  218 | `		}` |
|       105 |  219 | `	}` |
|      7915 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7670 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3833 |  222 | `	}` |
|      7915 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7915 |  224 | `	pMap->nEntry--;` |
|      7915 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       101 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       101 |  228 | `		pMap->apBucket = 0;` |
|       101 |  229 | `		pMap->nSize = 0;` |
|       101 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        48 |  231 | `	}` |
|      7915 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3847495 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3847500 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     93956 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     93956 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     93956 |  245 | `		if( nNew < 1 ){` |
|     88460 |  246 | `			nNew = 16;` |
|     44227 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     93956 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     93956 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     93956 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     93956 |  260 | `		pMap->apBucket = apNew;` |
|     93956 |  261 | `		pMap->nSize = nNew;` |
|     93956 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     88460 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5501 |  267 | `		pEntry = pMap->pFirst;` |
|      5501 |  268 | `		n = 0;` |
|   2545308 |  269 | `		for( ;; ){` |
|   5090621 |  270 | `			if( n >= pMap->nEntry ){` |
|      5501 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   5085125 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   5085125 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   5085125 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   4412605 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   4412605 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2206300 |  280 | `			}` |
|   5085125 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   5085125 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   5085125 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5501 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2748 |  288 | `	}` |
|   3759045 |  289 | `	return SXRET_OK;` |
|   1923752 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3647734 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3647739 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3647697 |  310 | `		if( pValue ){` |
|   3647691 |  311 | `			sSafeVal = *pValue;` |
|   3647691 |  312 | `			pValue = &sSafeVal;` |
|   1823843 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3647697 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3647697 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3647697 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3647691 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1823843 |  322 | `		}` |
|   3647697 |  323 | `		nIdx = pObj->nIdx;` |
|   1823851 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3647739 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3647739 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3647739 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3647739 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3647739 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3647739 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3647739 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3647739 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3647739 |  349 | `	return SXRET_OK;` |
|   1823872 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    199761 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    199766 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    152824 |  370 | `		if( pValue ){` |
|    152514 |  371 | `			sSafeVal = *pValue;` |
|    152514 |  372 | `			pValue = &sSafeVal;` |
|     76254 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    152824 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    152824 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    152824 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    152514 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|     76254 |  382 | `		}` |
|    152824 |  383 | `		nIdx = pObj->nIdx;` |
|     76414 |  384 | `	}else{` |
|     46947 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    199766 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    199766 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    199766 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    199766 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     46947 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23471 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    199766 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    199766 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    199766 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    199766 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    199766 |  409 | `	return SXRET_OK;` |
|     99885 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4292096 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4292101 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       829 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4291277 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4291277 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110567612 |  433 | `	for(;;){` |
| 221135229 |  434 | `		if( pNode == 0 ){` |
|   4284647 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216850582 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216847566 |  438 | `			&& pNode->nHash == nHash` |
| 108425595 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      6635 |  441 | `				if( ppNode ){` |
|      6613 |  442 | `					*ppNode = pNode;` |
|      3304 |  443 | `				}` |
|      6635 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216843954 |  447 | `		pNode = pNode->pNextCollide;` |
|         2 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4284647 |  450 | `	return SXERR_NOTFOUND;` |
|   2146053 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    348207 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    348212 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     28408 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    319809 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    319809 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    259256 |  475 | `	for(;;){` |
|    518517 |  476 | `		if( pNode == 0 ){` |
|    252025 |  477 | `			break;` |
|         - |  478 | `		}` |
|    266492 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    264978 |  480 | `			&& pNode->nHash == nHash` |
|    165675 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     67891 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     67789 |  484 | `				if( ppNode ){` |
|     67761 |  485 | `					*ppNode = pNode;` |
|     33878 |  486 | `				}` |
|     67789 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    198713 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    252025 |  493 | `	return SXERR_NOTFOUND;` |
|    174108 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    348403 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    348408 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    348408 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    348408 |  504 | `	int isNeg = FALSE, nDigit;` |
|    348408 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    348386 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    348382 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    348382 |  516 | `	zDigit = zIn;` |
|    174655 |  517 | `	for(;;){` |
|    349316 |  518 | `		if( zIn >= zEnd ){` |
|       315 |  519 | `			break;` |
|         - |  520 | `		}` |
|    349002 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    348068 |  523 | `			return FALSE;` |
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
|    174206 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    155174 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    155179 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    155179 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    148647 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    148647 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    148571 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    148571 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|        38 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      6613 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        89 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        44 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      6613 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     77587 |  575 | `result:` |
|    155179 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     73609 |  578 | `		if( ppNode ){` |
|     73557 |  579 | `			*ppNode = pNode;` |
|     36776 |  580 | `		}` |
|     73609 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     81575 |  584 | `	return SXERR_NOTFOUND;` |
|     77592 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  588 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  589 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  590 | ` * via HashmapAppendIndexBusy.` |
|         - |  591 | ` */` |
|   2142732 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2142737 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       873 |  596 | `		pMap->bIntKeySeen = 1;` |
|       873 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       873 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|       873 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2141869 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141617 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2141617 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1070806 |  609 | `	}` |
|   1071371 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   1501164 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1501169 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1501163 |  623 | `	return FALSE;` |
|    750587 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3796631 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3796636 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3796636 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3796636 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    152812 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    152812 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       231 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    228870 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     76288 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  654 | `				/* Overwrite the old value */` |
|         - |  655 | `				ph7_value *pElem;` |
|       492 |  656 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       492 |  657 | `				if( pElem ){` |
|       492 |  658 | `					if( pVal ){` |
|       492 |  659 | `						PH7_MemObjStore(pVal,pElem);` |
|       248 |  660 | `					}else{` |
|         - |  661 | `						/* Nullify the entry */` |
|       ! 0 |  662 | `						PH7_MemObjToNull(pElem);` |
|         - |  663 | `					}` |
|       244 |  664 | `				}` |
|       492 |  665 | `				return SXRET_OK;` |
|         - |  666 | `		}` |
|    152094 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    151958 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    151958 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1821912 |  683 | `IntKey:` |
|   3644059 |  684 | `	if( pKey ){` |
|   2142929 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       261 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       130 |  688 | `		}` |
|   2142929 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   2142733 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2142731 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2142731 |  712 | `		if( rc == SXRET_OK ){` |
|   2142731 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1071363 |  714 | `		}` |
|   1071368 |  715 | `	}else{` |
|   1501135 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1501133 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1501127 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1501127 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1501125 |  726 | `			++pMap->iNextIdx;` |
|    750560 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3643853 |  730 | `	return rc;` |
|   1898320 |  731 | `}` |
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
|     46994 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     46999 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     46999 |  766 | `	sxi32 rc = SXRET_OK;` |
|     46999 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46959 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     46959 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     70433 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23476 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     46947 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46947 |  790 | `		return rc;` |
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
|     23502 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1554845 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1554850 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1554850 |  832 | `	return pObj;` |
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
|     79362 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     79367 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     79367 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     79367 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     79367 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     79367 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     79367 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     79367 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     79367 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     79367 |  921 | `	return rc;` |
|     39572 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     17034 |  927 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     17039 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     17039 |  932 | `	if( pEntry->pPrevCollide ){` |
|     12071 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5931 |  934 | `	}else{` |
|      4973 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     17039 |  937 | `	if( pEntry->pNextCollide ){` |
|      1063 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       544 |  939 | `	}` |
|     17039 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     17039 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     17039 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     17039 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     17039 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17039 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     12400 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      6098 |  949 | `	}` |
|     17039 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17039 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     17039 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     17039 |  956 | `		pMap->iNextIdx++;` |
|      8517 |  957 | `	}` |
|     17039 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     33870 |  966 | `static int HashmapFindValue(` |
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
|     33875 |  979 | `	pEntry = pMap->pFirst;` |
|     33875 |  980 | `	n = pMap->nEntry;` |
|     33875 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33875 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     80773 |  983 | `	for(;;){` |
|    161548 |  984 | `		if( n < 1 ){` |
|        79 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    161470 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    161470 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    161470 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    161470 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    161470 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    161470 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    161470 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    161470 | 1001 | `			if( rc == 0 ){` |
|     33797 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     33797 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     63838 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    127678 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    127678 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|        79 | 1014 | `	return SXERR_NOTFOUND;` |
|     16940 | 1015 | `}` |
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
|    725916 | 1237 | `static sxi32 HashmapDuplicateNode(` |
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
|    725916 | 1248 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    725918 | 1249 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
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
|    725913 | 1274 | `	sSafeVal = *pVal;` |
|         - | 1275 |  |
|    725913 | 1276 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1277 | `		/* Blob key insertion */` |
|      4049 | 1278 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4049 | 1279 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4049 | 1280 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4049 | 1281 | `		PH7_MemObjRelease(&sKey);` |
|      2027 | 1282 | `	}else{` |
|         - | 1283 | `		/* Int key */` |
|    721869 | 1284 | `		if( iAction == 0 ){ /* Merge */` |
|    718183 | 1285 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    362780 | 1286 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1287 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1288 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1289 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1290 | `		}else{ /* Dup */` |
|      3661 | 1291 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1292 | `		}` |
|         - | 1293 | `	}` |
|    725913 | 1294 | `	return rc;` |
|    362963 | 1295 | `}` |
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
|      2910 | 1308 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1309 | `{` |
|         - | 1310 | `	ph7_hashmap_node *pEntry;` |
|         - | 1311 | `	ph7_value *pVal;` |
|         - | 1312 | `	sxi32 rc;` |
|         - | 1313 | `	sxu32 n;` |
|      2915 | 1314 | `	if( pSrc == pDest ){` |
|         - | 1315 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1316 | `		 * Unlike the zend engine.` |
|         - | 1317 | `		 */` |
|       ! 0 | 1318 | `		return SXRET_OK;` |
|         - | 1319 | `	}` |
|         - | 1320 | `	/* Point to the first inserted entry in the source */` |
|      2915 | 1321 | `	pEntry = pSrc->pFirst;` |
|         - | 1322 | `	/* Perform the merge */` |
|    721153 | 1323 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1324 | `		/* Extract the node value */` |
|    718243 | 1325 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    718243 | 1326 | `		if( pVal ){` |
|         - | 1327 | `			/* Make a local copy of the value.` |
|         - | 1328 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1329 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1330 | `			 * to the old pool.` |
|         - | 1331 | `			 */` |
|    718243 | 1332 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    359124 | 1333 | `		}else{` |
|       ! 0 | 1334 | `			rc = SXRET_OK;` |
|         - | 1335 | `		}` |
|    718243 | 1336 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1337 | `			return rc;` |
|         - | 1338 | `		}` |
|         - | 1339 | `		/* Point to the next entry */` |
|    718243 | 1340 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    359124 | 1341 | `	}` |
|      2915 | 1342 | `	return SXRET_OK;` |
|      1460 | 1343 | `}` |
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
|    248350 | 1499 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1500 | `{` |
|    248355 | 1501 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1502 | `	ph7_hashmap *pNew;` |
|         - | 1503 | `	ph7_value *pBacking;` |
|         - | 1504 | `	sxu32 nValIdx;` |
|         - | 1505 | `	int bValueInPool;` |
|    248355 | 1506 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    248355 | 1507 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1508 | `		/* Sole owner, no separation needed */` |
|    245551 | 1509 | `		return pMap;` |
|         - | 1510 | `	}` |
|      2809 | 1511 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1512 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1513 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1514 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       131 | 1515 | `		return pMap;` |
|         - | 1516 | `	}` |
|         - | 1517 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1518 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1519 | `	 * frame is popped. */` |
|      2679 | 1520 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2679 | 1521 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2674 | 1522 | `		if( pBacking && pBacking != pValue` |
|      2649 | 1523 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2629 | 1524 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1525 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2629 | 1526 | `			pMap->iRef--;` |
|      2629 | 1527 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1528 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2579 | 1529 | `				pMap->iRef++;` |
|      2579 | 1530 | `				return pMap;` |
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
|    124180 | 1593 | `}` |
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
|        43 | 1647 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
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
|        43 | 1677 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 1678 | `	}` |
|      3827 | 1679 | `	return SXRET_OK;` |
|      1916 | 1680 | `}` |
|         - | 1681 | `/*` |
|         - | 1682 | ` * Allocate a new hashmap.` |
|         - | 1683 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1684 | ` */` |
|    143124 | 1685 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1686 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1687 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1688 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1689 | `	)` |
|         5 | 1690 | `{` |
|         - | 1691 | `	ph7_hashmap *pMap;` |
|         - | 1692 | `	/* Allocate a new instance */` |
|    143129 | 1693 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    143129 | 1694 | `	if( pMap == 0 ){` |
|       ! 0 | 1695 | `		return 0;` |
|         - | 1696 | `	}` |
|         - | 1697 | `	/* Zero the structure */` |
|    143129 | 1698 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1699 | `	/* Fill in the structure */` |
|    143129 | 1700 | `	pMap->pVm = &(*pVm);` |
|    143129 | 1701 | `	pMap->iRef = 1;` |
|         - | 1702 | `	/* Default hash functions */` |
|    143129 | 1703 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    143129 | 1704 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    143129 | 1705 | `	return pMap;` |
|     71567 | 1706 | `}` |
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
|     95110 | 1798 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1799 | `{` |
|         - | 1800 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     95115 | 1801 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1802 | `	sxu32 n;` |
|     95115 | 1803 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1804 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1805 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1806 | `		return SXRET_OK;` |
|         - | 1807 | `	}` |
|     95115 | 1808 | `	if( pMap->pActiveSteps ){` |
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
|     95115 | 1821 | `	n = 0;` |
|     95115 | 1822 | `	pEntry = pMap->pFirst;` |
|   1916512 | 1823 | `	for(;;){` |
|   3833030 | 1824 | `		if( n >= pMap->nEntry ){` |
|     95115 | 1825 | `			break;` |
|         - | 1826 | `		}` |
|   3737920 | 1827 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1828 | `		/* Remove the reference from the foreign table */` |
|   3737920 | 1829 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3737920 | 1830 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1831 | `			/* Restore the ph7_value to the free list */` |
|   3737860 | 1832 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1868927 | 1833 | `		}` |
|         - | 1834 | `		/* Release the node */` |
|   3737920 | 1835 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    109638 | 1836 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     54816 | 1837 | `		}` |
|   3737920 | 1838 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1839 | `		/* Point to the next entry */` |
|   3737920 | 1840 | `		pEntry = pNext;` |
|   3737920 | 1841 | `		n++;` |
|         5 | 1842 | `	}` |
|     95115 | 1843 | `	if( pMap->nEntry > 0 ){` |
|         - | 1844 | `		/* Release the hash bucket */` |
|     68870 | 1845 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     34432 | 1846 | `	}` |
|     95115 | 1847 | `	if( FreeDS ){` |
|         - | 1848 | `		/* Free the whole instance */` |
|     95089 | 1849 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     47547 | 1850 | `	}else{` |
|         - | 1851 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1852 | `		pMap->apBucket = 0;` |
|        28 | 1853 | `		pMap->iNextIdx = 0;` |
|        28 | 1854 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1855 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1856 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1857 | `	}` |
|     95115 | 1858 | `	return SXRET_OK;` |
|     47560 | 1859 | `}` |
|         - | 1860 | `/*` |
|         - | 1861 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1862 | ` * If the count reaches zero which mean no more variables` |
|         - | 1863 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1864 | ` */` |
|    884510 | 1865 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1866 | `{` |
|    884515 | 1867 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1868 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    884515 | 1869 | `	pMap->iRef--;` |
|    884515 | 1870 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     95069 | 1871 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     47532 | 1872 | `	}` |
|    884515 | 1873 | `}` |
|         - | 1874 | `/*` |
|         - | 1875 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1876 | ` * Write a pointer to the target node on success.` |
|         - | 1877 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1878 | ` */` |
|    155394 | 1879 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1880 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1881 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1882 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1883 | `	)` |
|         5 | 1884 | `{` |
|         - | 1885 | `	sxi32 rc;` |
|    155399 | 1886 | `	if( pMap->nEntry < 1 ){` |
|         - | 1887 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1888 | `		 */` |
|       225 | 1889 | `		return SXERR_NOTFOUND;` |
|         - | 1890 | `	}` |
|    155179 | 1891 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    155179 | 1892 | `	return rc;` |
|     77702 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1896 | ` * hashmap.` |
|         - | 1897 | ` * If a node with the given key already exists in the database` |
|         - | 1898 | ` * then this function overwrite the old value.` |
|         - | 1899 | ` */` |
|   3078119 | 1900 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   3078124 | 1911 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   3078124 | 1912 | `	return rc;` |
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
|     46984 | 1951 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1952 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1953 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1954 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1955 | `	)` |
|         5 | 1956 | `{` |
|         - | 1957 | `	sxi32 rc;` |
|     46989 | 1958 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1959 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1960 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1961 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1962 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1963 | `		return PH7_ABORT;` |
|         - | 1964 | `	}` |
|     46989 | 1965 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     46989 | 1966 | `	return rc;` |
|     23497 | 1967 | `}` |
|         - | 1968 | `/*` |
|         - | 1969 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1970 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1971 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1972 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1973 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1974 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1975 | ` */` |
|     23492 | 1976 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1977 | `{` |
|     23497 | 1978 | `	pStep->pCursor = pMap->pFirst;` |
|     23497 | 1979 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     23497 | 1980 | `	pMap->pActiveSteps = pStep;` |
|     23497 | 1981 | `}` |
|         - | 1982 | `/*` |
|         - | 1983 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1984 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1985 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1986 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1987 | ` */` |
|     23316 | 1988 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1989 | `{` |
|     23321 | 1990 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     23321 | 1991 | `	while( *ppLink ){` |
|     23321 | 1992 | `		if( *ppLink == pStep ){` |
|     23321 | 1993 | `			*ppLink = pStep->pNextActive;` |
|     23321 | 1994 | `			pStep->pNextActive = 0;` |
|     23321 | 1995 | `			return;` |
|         - | 1996 | `		}` |
|       ! 0 | 1997 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1998 | `	}` |
|     11663 | 1999 | `}` |
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
|    628670 | 2020 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2021 | `{` |
|    628675 | 2022 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    628675 | 2023 | `	if( pEntry ){` |
|    628675 | 2024 | `		if( bStore ){` |
|    242127 | 2025 | `			PH7_MemObjStore(pEntry,pValue);` |
|    121066 | 2026 | `		}else{` |
|    386553 | 2027 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2028 | `		}` |
|    314112 | 2029 | `	}else{` |
|       ! 0 | 2030 | `		PH7_MemObjRelease(pValue);` |
|         - | 2031 | `	}` |
|    628675 | 2032 | `}` |
|         - | 2033 | `/*` |
|         - | 2034 | ` * Extract a node key.` |
|         - | 2035 | ` */` |
|    167716 | 2036 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2037 | `{` |
|         - | 2038 | `	/* Fill with the current key */` |
|    167721 | 2039 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    161955 | 2040 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2041 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2042 | `		}` |
|    161955 | 2043 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    161955 | 2044 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     80980 | 2045 | `	}else{` |
|      5771 | 2046 | `		SyBlobReset(&pKey->sBlob);` |
|      5771 | 2047 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5771 | 2048 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2049 | `	}` |
|    167721 | 2050 | `}` |
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
|     52228 | 2101 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2102 | `{` |
|         - | 2103 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2104 | `    /* Prevent compiler warning */` |
|     52233 | 2105 | `	result.pNext = result.pPrev = 0;` |
|     52233 | 2106 | `	pTail = &result;` |
|    131832 | 2107 | `	while( pA && pB ){` |
|     79604 | 2108 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     52129 | 2109 | `			pTail->pPrev = pA;` |
|     52129 | 2110 | `			pA->pNext = pTail;` |
|     52129 | 2111 | `			pTail = pA;` |
|     52129 | 2112 | `			pA = pA->pPrev;` |
|     26191 | 2113 | `		}else{` |
|     27480 | 2114 | `			pTail->pPrev = pB;` |
|     27480 | 2115 | `			pB->pNext = pTail;` |
|     27480 | 2116 | `			pTail = pB;` |
|     27480 | 2117 | `			pB = pB->pPrev;` |
|         - | 2118 | `		}` |
|         5 | 2119 | `	}` |
|     52233 | 2120 | `	if( pA ){` |
|     39584 | 2121 | `		pTail->pPrev = pA;` |
|     39584 | 2122 | `		pA->pNext = pTail;` |
|     32350 | 2123 | `	}else if( pB ){` |
|     12356 | 2124 | `		pTail->pPrev = pB;` |
|     12356 | 2125 | `		pB->pNext = pTail;` |
|      6274 | 2126 | `	}else{` |
|       303 | 2127 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2128 | `	}` |
|     52233 | 2129 | `	return result.pPrev;` |
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
|      1210 | 2143 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2144 | `{` |
|         - | 2145 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2146 | `	sxu32 i;` |
|      1215 | 2147 | `	SyZero(a,sizeof(a));` |
|         - | 2148 | `	/* Point to the first inserted entry */` |
|      1215 | 2149 | `	pIn = pMap->pFirst;` |
|     18301 | 2150 | `	while( pIn ){` |
|     17091 | 2151 | `		p = pIn;` |
|     17091 | 2152 | `		pIn = p->pPrev;` |
|     17091 | 2153 | `		p->pPrev = 0;` |
|     31809 | 2154 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     31809 | 2155 | `			if( a[i]==0 ){` |
|     17091 | 2156 | `				a[i] = p;` |
|     17091 | 2157 | `				break;` |
|       ! 0 | 2158 | `			}else{` |
|     14723 | 2159 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     14723 | 2160 | `				a[i] = 0;` |
|         - | 2161 | `			}` |
|      7364 | 2162 | `		}` |
|     17091 | 2163 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2164 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2165 | `			 * But that is impossible.` |
|         - | 2166 | `			 */` |
|       ! 0 | 2167 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2168 | `		}` |
|         5 | 2169 | `	}` |
|      1215 | 2170 | `	p = a[0];` |
|     38725 | 2171 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     37515 | 2172 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     18760 | 2173 | `	}` |
|      1215 | 2174 | `	p->pNext = 0;` |
|         - | 2175 | `	/* Reflect the change */` |
|      1215 | 2176 | `	pMap->pFirst = p;` |
|         - | 2177 | `	/* Reset the loop cursor */` |
|      1215 | 2178 | `	pMap->pCur = pMap->pFirst;` |
|      1215 | 2179 | `	return SXRET_OK;` |
|         5 | 2180 | `}` |
|         - | 2181 | `/* SPDX-SnippetEnd */` |
|         - | 2182 | `/*` |
|         - | 2183 | ` * Node comparison callback.` |
|         - | 2184 | ` * used-by: [sort(),asort(),...]` |
|         - | 2185 | ` */` |
|         - | 2186 | `/*` |
|         - | 2187 | ` * Compare two scalar values under an EXPLICIT php sort base type (never 0 —` |
|         - | 2188 | ` * SORT_REGULAR is handled by the callers, which differ for keys vs values):` |
|         - | 2189 | ` *   SORT_NUMERIC 1 · SORT_STRING 2 · SORT_LOCALE_STRING 5 · SORT_NATURAL 6,` |
|         - | 2190 | ` * with bFold applying SORT_FLAG_CASE. PHL has no locale tables, so` |
|         - | 2191 | ` * SORT_LOCALE_STRING behaves like SORT_STRING. Mutates both operands (numeric or` |
|         - | 2192 | ` * string cast); the caller owns and releases them.` |
|         - | 2193 | ` */` |
|       250 | 2194 | `static sxi32 HashmapScalarFlagCmp(ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|         1 | 2195 | `{` |
|         - | 2196 | `	sxi32 rc;` |
|       251 | 2197 | `	if( base == 1 ){` |
|         - | 2198 | `		/* SORT_NUMERIC */` |
|        59 | 2199 | `		PH7_MemObjToNumeric(pA);` |
|        59 | 2200 | `		PH7_MemObjToNumeric(pB);` |
|        59 | 2201 | `		rc = PH7_MemObjCmp(pA,pB,FALSE,0);` |
|        30 | 2202 | `	}else{` |
|         - | 2203 | `		/* SORT_STRING (2) / SORT_LOCALE_STRING (5) / SORT_NATURAL (6) */` |
|         - | 2204 | `		const char *zA,*zB;` |
|         - | 2205 | `		sxu32 nA,nB,nMin,i;` |
|       193 | 2206 | `		if( (pA->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pA); }` |
|       193 | 2207 | `		if( (pB->iFlags & MEMOBJ_STRING) == 0 ){ PH7_MemObjToString(pB); }` |
|       193 | 2208 | `		zA = (const char *)SyBlobData(&pA->sBlob);` |
|       193 | 2209 | `		zB = (const char *)SyBlobData(&pB->sBlob);` |
|       193 | 2210 | `		nA = SyBlobLength(&pA->sBlob);` |
|       193 | 2211 | `		nB = SyBlobLength(&pB->sBlob);` |
|       193 | 2212 | `		if( base == 6 ){` |
|        29 | 2213 | `			rc = PH7_StrNatCmp(zA,(int)nA,zB,(int)nB,bFold);` |
|        15 | 2214 | `		}else{` |
|         - | 2215 | `			/* Lexicographic comparison (binary-safe), case-folded on request. */` |
|       165 | 2216 | `			nMin = nA < nB ? nA : nB;` |
|       165 | 2217 | `			rc = 0;` |
|       241 | 2218 | `			for( i = 0 ; i < nMin ; ++i ){` |
|       187 | 2219 | `				int ca = (unsigned char)zA[i];` |
|       187 | 2220 | `				int cb = (unsigned char)zB[i];` |
|       187 | 2221 | `				if( bFold ){ ca = SyToLower(ca); cb = SyToLower(cb); }` |
|       187 | 2222 | `				if( ca != cb ){ rc = ca < cb ? -1 : 1; break; }` |
|        39 | 2223 | `			}` |
|       165 | 2224 | `			if( rc == 0 ){` |
|        55 | 2225 | `				if( nA < nB ) rc = -1;` |
|        43 | 2226 | `				else if( nA > nB ) rc = 1;` |
|        27 | 2227 | `			}` |
|         - | 2228 | `		}` |
|         - | 2229 | `	}` |
|       251 | 2230 | `	return rc;` |
|         1 | 2231 | `}` |
|         - | 2232 | `/*` |
|         - | 2233 | ` * Are two live values equal under an array_unique() sort_flags? A non-mutating` |
|         - | 2234 | ` * wrapper (works on private copies): base 0 = SORT_REGULAR loose comparison,` |
|         - | 2235 | ` * explicit flags route through HashmapScalarFlagCmp. Used by array_unique.` |
|         - | 2236 | ` */` |
|       116 | 2237 | `static int HashmapValueFlagEqual(ph7_vm *pVm,ph7_value *pA,ph7_value *pB,int base,int bFold)` |
|         1 | 2238 | `{` |
|         - | 2239 | `	ph7_value sA,sB;` |
|         - | 2240 | `	sxi32 rc;` |
|       117 | 2241 | `	PH7_MemObjInit(pVm,&sA);` |
|       117 | 2242 | `	PH7_MemObjInit(pVm,&sB);` |
|       117 | 2243 | `	PH7_MemObjStore(pA,&sA);` |
|       117 | 2244 | `	PH7_MemObjStore(pB,&sB);` |
|       117 | 2245 | `	if( base == 0 ){` |
|        11 | 2246 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0); /* SORT_REGULAR: loose comparison */` |
|         6 | 2247 | `	}else{` |
|       107 | 2248 | `		rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|         - | 2249 | `	}` |
|       117 | 2250 | `	PH7_MemObjRelease(&sA);` |
|       117 | 2251 | `	PH7_MemObjRelease(&sB);` |
|       117 | 2252 | `	return rc == 0;` |
|         1 | 2253 | `}` |
|         - | 2254 | `/*` |
|         - | 2255 | ` * Compare two node VALUES under php's sort_flags (base 0 = SORT_REGULAR uses the` |
|         - | 2256 | ` * standard value comparison; explicit flags route through HashmapScalarFlagCmp).` |
|         - | 2257 | ` */` |
|       126 | 2258 | `static sxi32 HashmapFlagValueCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2259 | `{` |
|         - | 2260 | `	ph7_value sA,sB;` |
|       127 | 2261 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|       127 | 2262 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2263 | `	sxi32 rc;` |
|       127 | 2264 | `	if( base == 0 ){` |
|         - | 2265 | `		/* SORT_REGULAR */` |
|       ! 0 | 2266 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2267 | `	}` |
|       127 | 2268 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       127 | 2269 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       127 | 2270 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       127 | 2271 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       127 | 2272 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|       127 | 2273 | `	PH7_MemObjRelease(&sA);` |
|       127 | 2274 | `	PH7_MemObjRelease(&sB);` |
|       127 | 2275 | `	return rc;` |
|        64 | 2276 | `}` |
|     79278 | 2277 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2278 | `{` |
|     79283 | 2279 | `	if( pCmpData == 0 ){` |
|         - | 2280 | `		/* SORT_REGULAR fast path */` |
|     79195 | 2281 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2282 | `	}` |
|        89 | 2283 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     39530 | 2284 | `}` |
|         - | 2285 | `/*` |
|         - | 2286 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2287 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2288 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2289 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2290 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2291 | ` */` |
|         - | 2292 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2293 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2294 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        44 | 2295 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2296 | `{` |
|        46 | 2297 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        46 | 2298 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        46 | 2299 | `	if( rc == 0 ){` |
|       ! 0 | 2300 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2301 | `	}` |
|        46 | 2302 | `	return rc;` |
|         2 | 2303 | `}` |
|        66 | 2304 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2305 | `{` |
|         - | 2306 | `	sxi32 rc;` |
|        68 | 2307 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2308 | `		/* Perform a string comparison */` |
|        44 | 2309 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        28 | 2310 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        16 | 2311 | `	}else{` |
|         - | 2312 | `		SyString sStr;` |
|        39 | 2313 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2314 | `		int bNum = 1;` |
|        39 | 2315 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2316 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2317 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2318 | `				bNum = 0;` |
|         6 | 2319 | `			}else{` |
|       ! 0 | 2320 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2321 | `			}` |
|         6 | 2322 | `		}else{` |
|        29 | 2323 | `			iA = pA->xKey.iKey;` |
|         - | 2324 | `		}` |
|        39 | 2325 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2326 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2327 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2328 | `				bNum = 0;` |
|         4 | 2329 | `			}else{` |
|       ! 0 | 2330 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2331 | `			}` |
|         4 | 2332 | `		}else{` |
|        33 | 2333 | `			iB = pB->xKey.iKey;` |
|         - | 2334 | `		}` |
|        39 | 2335 | `		if( bNum ){` |
|        23 | 2336 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2337 | `		}else{` |
|         - | 2338 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2339 | `			char zNumA[24],zNumB[24];` |
|         - | 2340 | `			SyString sA,sB;` |
|        17 | 2341 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2342 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2343 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2344 | `			}else{` |
|        11 | 2345 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2346 | `			}` |
|        17 | 2347 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2348 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2349 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2350 | `			}else{` |
|         7 | 2351 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2352 | `			}` |
|        17 | 2353 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2354 | `		}` |
|         - | 2355 | `	}` |
|        68 | 2356 | `	return rc;` |
|         2 | 2357 | `}` |
|         - | 2358 | `/*` |
|         - | 2359 | ` * Materialise a node's KEY as a scalar ph7_value (int key -> integer, string key` |
|         - | 2360 | ` * -> string) for a flag-aware key comparison.` |
|         - | 2361 | ` */` |
|        36 | 2362 | `static void HashmapNodeKeyToValue(ph7_hashmap_node *pNode,ph7_value *pOut)` |
|         1 | 2363 | `{` |
|        37 | 2364 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|        21 | 2365 | `		PH7_MemObjInitFromInt(pNode->pMap->pVm,pOut,pNode->xKey.iKey);` |
|        11 | 2366 | `	}else{` |
|        17 | 2367 | `		PH7_MemObjInitFromString(pNode->pMap->pVm,pOut,0);` |
|        25 | 2368 | `		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|         8 | 2369 | `			SyBlobLength(&pNode->xKey.sKey));` |
|         - | 2370 | `	}` |
|        37 | 2371 | `}` |
|         - | 2372 | `/*` |
|         - | 2373 | ` * Compare two node KEYS under php's sort_flags. base 0 = SORT_REGULAR keeps the` |
|         - | 2374 | ` * php-8 mixed int/string key semantics (HashmapKeyNodeCmp); explicit flags route` |
|         - | 2375 | ` * the materialised keys through HashmapScalarFlagCmp.` |
|         - | 2376 | ` */` |
|        18 | 2377 | `static sxi32 HashmapFlagKeyCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB,sxi32 iFlags)` |
|         1 | 2378 | `{` |
|         - | 2379 | `	ph7_value sA,sB;` |
|        19 | 2380 | `	int base = iFlags & ~8;      /* strip SORT_FLAG_CASE */` |
|        19 | 2381 | `	int bFold = (iFlags & 8) != 0;` |
|         - | 2382 | `	sxi32 rc;` |
|        19 | 2383 | `	if( base == 0 ){` |
|       ! 0 | 2384 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2385 | `	}` |
|        19 | 2386 | `	HashmapNodeKeyToValue(pA,&sA);` |
|        19 | 2387 | `	HashmapNodeKeyToValue(pB,&sB);` |
|        19 | 2388 | `	rc = HashmapScalarFlagCmp(&sA,&sB,base,bFold);` |
|        19 | 2389 | `	PH7_MemObjRelease(&sA);` |
|        19 | 2390 | `	PH7_MemObjRelease(&sB);` |
|        19 | 2391 | `	return rc;` |
|        10 | 2392 | `}` |
|         - | 2393 | `/*` |
|         - | 2394 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2395 | ` * used-by: [ksort()]` |
|         - | 2396 | ` */` |
|        66 | 2397 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2398 | `{` |
|        68 | 2399 | `	if( pCmpData == 0 ){` |
|        54 | 2400 | `		return HashmapKeyNodeCmp(pA,pB);` |
|         - | 2401 | `	}` |
|        15 | 2402 | `	return HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        35 | 2403 | `}` |
|         - | 2404 | `/*` |
|         - | 2405 | ` * Node comparison callback.` |
|         - | 2406 | ` * Used by: [rsort(),arsort()];` |
|         - | 2407 | ` */` |
|        96 | 2408 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2409 | `{` |
|        97 | 2410 | `	if( pCmpData == 0 ){` |
|         - | 2411 | `		/* SORT_REGULAR fast path, reversed */` |
|        59 | 2412 | `		return -HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2413 | `	}` |
|        39 | 2414 | `	return -HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        49 | 2415 | `}` |
|         - | 2416 | `/*` |
|         - | 2417 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2418 | ` * used-by: [usort(),uasort()]` |
|         - | 2419 | ` */` |
|       116 | 2420 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2421 | `{` |
|         - | 2422 | `	ph7_value sResult,*pCallback;` |
|         - | 2423 | `	ph7_value *pV1,*pV2;` |
|         - | 2424 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2425 | `	sxi32 rc;` |
|         - | 2426 | `	/* Point to the desired callback */` |
|       119 | 2427 | `	pCallback = (ph7_value *)pCmpData;` |
|       119 | 2428 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2429 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2430 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2431 | `		return 0;` |
|         - | 2432 | `	}` |
|         - | 2433 | `	/* initialize the result value */` |
|       113 | 2434 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2435 | `	/* Extract nodes values */` |
|       113 | 2436 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       113 | 2437 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       113 | 2438 | `	apArg[0] = pV1;` |
|       113 | 2439 | `	apArg[1] = pV2;` |
|         - | 2440 | `	/* Invoke the callback */` |
|       113 | 2441 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       113 | 2442 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2443 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2444 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2445 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2446 | `		rc = 0;` |
|       108 | 2447 | `	}else if( rc != SXRET_OK ){` |
|         - | 2448 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2449 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2450 | `	}else{` |
|         - | 2451 | `		/* Extract callback result */` |
|       104 | 2452 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2453 | `			/* Perform an int cast */` |
|       ! 0 | 2454 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2455 | `		}` |
|       104 | 2456 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2457 | `	}` |
|       113 | 2458 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2459 | `	/* Callback result */` |
|       113 | 2460 | `	return rc;` |
|        61 | 2461 | `}` |
|         - | 2462 | `/*` |
|         - | 2463 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2464 | ` * used-by: [krsort()]` |
|         - | 2465 | ` */` |
|        18 | 2466 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2467 | `{` |
|        19 | 2468 | `	if( pCmpData == 0 ){` |
|        15 | 2469 | `		return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         - | 2470 | `	}` |
|         5 | 2471 | `	return -HashmapFlagKeyCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|        10 | 2472 | `}` |
|         - | 2473 | `/*` |
|         - | 2474 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2475 | ` * used-by: [uksort()]` |
|         - | 2476 | ` */` |
|         6 | 2477 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2478 | `{` |
|         - | 2479 | `	ph7_value sResult,*pCallback;` |
|         - | 2480 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2481 | `	ph7_value sK1,sK2;` |
|         - | 2482 | `	sxi32 rc;` |
|         - | 2483 | `	/* Point to the desired callback */` |
|         7 | 2484 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2485 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2486 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2487 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2488 | `		return 0;` |
|         - | 2489 | `	}` |
|         - | 2490 | `	/* initialize the result value */` |
|         7 | 2491 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2492 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2493 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2494 | `	/* Extract nodes keys */` |
|         7 | 2495 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2496 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2497 | `	apArg[0] = &sK1;` |
|         7 | 2498 | `	apArg[1] = &sK2;` |
|         - | 2499 | `	/* Mark keys as constants */` |
|         7 | 2500 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2501 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2502 | `	/* Invoke the callback */` |
|         7 | 2503 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2504 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2505 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2506 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2507 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2508 | `		rc = 0;` |
|         7 | 2509 | `	}else if( rc != SXRET_OK ){` |
|         - | 2510 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2511 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2512 | `	}else{` |
|         - | 2513 | `		/* Extract callback result */` |
|         7 | 2514 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2515 | `			/* Perform an int cast */` |
|       ! 0 | 2516 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2517 | `		}` |
|         7 | 2518 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2519 | `	}` |
|         7 | 2520 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2521 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2522 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2523 | `	/* Callback result */` |
|         7 | 2524 | `	return rc;` |
|         4 | 2525 | `}` |
|         - | 2526 | `/*` |
|         - | 2527 | ` * Node comparison callback: Random node comparison.` |
|         - | 2528 | ` * used-by: [shuffle()]` |
|         - | 2529 | ` */` |
|        19 | 2530 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2531 | `{` |
|         - | 2532 | `	sxu32 n;` |
|         9 | 2533 | `	SXUNUSED(pB); /* cc warning */` |
|         9 | 2534 | `	SXUNUSED(pCmpData);` |
|         - | 2535 | `	/* Grab a random number */` |
|        20 | 2536 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2537 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2538 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2539 | `	 */` |
|        20 | 2540 | `	return n&1 ? 1 : -1;` |
|         1 | 2541 | `}` |
|         - | 2542 | `/*` |
|         - | 2543 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2544 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2545 | ` */` |
|      1128 | 2546 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2547 | `{` |
|         - | 2548 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2549 | `	sxu32 i;` |
|         - | 2550 | `	/* Rehash all entries */` |
|      1133 | 2551 | `	pLast = p = pMap->pFirst;` |
|      1133 | 2552 | `	pMap->iNextIdx = 0;` |
|      1133 | 2553 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      1133 | 2554 | `	i = 0;` |
|      8969 | 2555 | `	for( ;; ){` |
|     17943 | 2556 | `		if( i >= pMap->nEntry ){` |
|      1133 | 2557 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|      1133 | 2558 | `			break;` |
|         - | 2559 | `		}` |
|     16815 | 2560 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2561 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2562 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2563 | `			/* Change key type */` |
|         5 | 2564 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2565 | `		}` |
|     16815 | 2566 | `		HashmapRehashIntNode(p);` |
|         - | 2567 | `		/* Point to the next entry */` |
|     16815 | 2568 | `		i++;` |
|     16815 | 2569 | `		pLast = p;` |
|     16815 | 2570 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2571 | `	}` |
|      1133 | 2572 | `}` |
|         - | 2573 | `/*` |
|         - | 2574 | ` * Array functions implementation.` |
|         - | 2575 | ` * Status:` |
|         - | 2576 | ` *  Stable.` |
|         - | 2577 | ` */` |
|         - | 2578 | `/*` |
|         - | 2579 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2580 | ` * Sort an array.` |
|         - | 2581 | ` * Parameters` |
|         - | 2582 | ` *  $array` |
|         - | 2583 | ` *   The input array.` |
|         - | 2584 | ` * $sort_flags` |
|         - | 2585 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2586 | ` *  Sorting type flags:` |
|         - | 2587 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2588 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2589 | ` *   SORT_STRING - compare items as strings` |
|         - | 2590 | ` * Return` |
|         - | 2591 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2592 | ` *` |
|         - | 2593 | ` */` |
|      1100 | 2594 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2595 | `{` |
|         - | 2596 | `	ph7_hashmap *pMap;` |
|         - | 2597 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1105 | 2598 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2599 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2600 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2601 | `		return PH7_OK;` |
|         - | 2602 | `	}` |
|         - | 2603 | `	/* Point to the internal representation of the input hashmap */` |
|      1105 | 2604 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1105 | 2605 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1105 | 2606 | `	if( pMap->nEntry > 1 ){` |
|      1103 | 2607 | `		sxi32 iCmpFlags = 0;` |
|      1103 | 2608 | `		if( nArg > 1 ){` |
|         - | 2609 | `			/* Extract comparison flags */` |
|        15 | 2610 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         7 | 2611 | `		}` |
|         - | 2612 | `		/* Do the merge sort */` |
|      1103 | 2613 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2614 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      1103 | 2615 | `		HashmapSortRehash(pMap);` |
|       549 | 2616 | `	}` |
|         - | 2617 | `	/* All done,return TRUE */` |
|      1105 | 2618 | `	ph7_result_bool(pCtx,1);` |
|      1105 | 2619 | `	return PH7_OK;` |
|       555 | 2620 | `}` |
|         - | 2621 | `/*` |
|         - | 2622 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2623 | ` *  Sort an array and maintain index association.` |
|         - | 2624 | ` * Parameters` |
|         - | 2625 | ` *  $array` |
|         - | 2626 | ` *   The input array.` |
|         - | 2627 | ` * $sort_flags` |
|         - | 2628 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2629 | ` *  Sorting type flags:` |
|         - | 2630 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2631 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2632 | ` *   SORT_STRING - compare items as strings` |
|         - | 2633 | ` * Return` |
|         - | 2634 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2635 | ` */` |
|        34 | 2636 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2637 | `{` |
|         - | 2638 | `	ph7_hashmap *pMap;` |
|         - | 2639 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        39 | 2640 | `	if( nArg < 1 ){` |
|       ! 0 | 2641 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2642 | `			"ArgumentCountError",` |
|         - | 2643 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2644 | `			);` |
|         - | 2645 | `	}` |
|         - | 2646 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        39 | 2647 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2648 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2649 | `			"TypeError",` |
|         - | 2650 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2651 | `			ph7_type_name(apArg[0])` |
|         - | 2652 | `			);` |
|         - | 2653 | `	}` |
|         - | 2654 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 2655 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        27 | 2656 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        27 | 2657 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2658 | `		sxi32 iCmpFlags = 0;` |
|        23 | 2659 | `		if( nArg > 1 ){` |
|         - | 2660 | `			/* Extract comparison flags */` |
|         7 | 2661 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2662 | `		}` |
|         - | 2663 | `		/* Do the merge sort */` |
|        23 | 2664 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2665 | `		/* Fix the last link broken by the merge */` |
|        55 | 2666 | `		while(pMap->pLast->pPrev){` |
|        33 | 2667 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2668 | `		}` |
|        11 | 2669 | `	}` |
|         - | 2670 | `	/* All done,return TRUE */` |
|        27 | 2671 | `	ph7_result_bool(pCtx,1);` |
|        27 | 2672 | `	return PH7_OK;` |
|        22 | 2673 | `}` |
|         - | 2674 | `/*` |
|         - | 2675 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2676 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2677 | ` * Parameters` |
|         - | 2678 | ` *  $array` |
|         - | 2679 | ` *   The input array.` |
|         - | 2680 | ` * $sort_flags` |
|         - | 2681 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2682 | ` *  Sorting type flags:` |
|         - | 2683 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2684 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2685 | ` *   SORT_STRING - compare items as strings` |
|         - | 2686 | ` * Return` |
|         - | 2687 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2688 | ` */` |
|        32 | 2689 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2690 | `{` |
|         - | 2691 | `	ph7_hashmap *pMap;` |
|         - | 2692 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2693 | `	if( nArg < 1 ){` |
|       ! 0 | 2694 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2695 | `			"ArgumentCountError",` |
|         - | 2696 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2697 | `			);` |
|         - | 2698 | `	}` |
|         - | 2699 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2700 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2701 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2702 | `			"TypeError",` |
|         - | 2703 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2704 | `			ph7_type_name(apArg[0])` |
|         - | 2705 | `			);` |
|         - | 2706 | `	}` |
|         - | 2707 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2708 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2709 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2710 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2711 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2712 | `		if( nArg > 1 ){` |
|         - | 2713 | `			/* Extract comparison flags */` |
|         7 | 2714 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2715 | `		}` |
|         - | 2716 | `		/* Do the merge sort */` |
|        21 | 2717 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2718 | `		/* Fix the last link broken by the merge */` |
|        37 | 2719 | `		while(pMap->pLast->pPrev){` |
|        17 | 2720 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2721 | `		}` |
|        10 | 2722 | `	}` |
|         - | 2723 | `	/* All done,return TRUE */` |
|        25 | 2724 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2725 | `	return PH7_OK;` |
|        21 | 2726 | `}` |
|         - | 2727 | `/*` |
|         - | 2728 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2729 | ` *  Sort an array by key.` |
|         - | 2730 | ` * Parameters` |
|         - | 2731 | ` *  $array` |
|         - | 2732 | ` *   The input array.` |
|         - | 2733 | ` * $sort_flags` |
|         - | 2734 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2735 | ` *  Sorting type flags:` |
|         - | 2736 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2737 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2738 | ` *   SORT_STRING - compare items as strings` |
|         - | 2739 | ` * Return` |
|         - | 2740 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2741 | ` */` |
|        20 | 2742 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2743 | `{` |
|         - | 2744 | `	ph7_hashmap *pMap;` |
|         - | 2745 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 2746 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2747 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2748 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2749 | `		return PH7_OK;` |
|         - | 2750 | `	}` |
|         - | 2751 | `	/* Point to the internal representation of the input hashmap */` |
|        22 | 2752 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        22 | 2753 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        22 | 2754 | `	if( pMap->nEntry > 1 ){` |
|        22 | 2755 | `		sxi32 iCmpFlags = 0;` |
|        22 | 2756 | `		if( nArg > 1 ){` |
|         - | 2757 | `			/* Extract comparison flags */` |
|         5 | 2758 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2759 | `		}` |
|         - | 2760 | `		/* Do the merge sort */` |
|        22 | 2761 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2762 | `		/* Fix the last link broken by the merge */` |
|        56 | 2763 | `		while(pMap->pLast->pPrev){` |
|        35 | 2764 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2765 | `		}` |
|        10 | 2766 | `	}` |
|         - | 2767 | `	/* All done,return TRUE */` |
|        22 | 2768 | `	ph7_result_bool(pCtx,1);` |
|        22 | 2769 | `	return PH7_OK;` |
|        12 | 2770 | `}` |
|         - | 2771 | `/*` |
|         - | 2772 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2773 | ` *  Sort an array by key in reverse order.` |
|         - | 2774 | ` * Parameters` |
|         - | 2775 | ` *  $array` |
|         - | 2776 | ` *   The input array.` |
|         - | 2777 | ` * $sort_flags` |
|         - | 2778 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2779 | ` *  Sorting type flags:` |
|         - | 2780 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2781 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2782 | ` *   SORT_STRING - compare items as strings` |
|         - | 2783 | ` * Return` |
|         - | 2784 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2785 | ` */` |
|         6 | 2786 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2787 | `{` |
|         - | 2788 | `	ph7_hashmap *pMap;` |
|         - | 2789 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2790 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2791 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2792 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2793 | `		return PH7_OK;` |
|         - | 2794 | `	}` |
|         - | 2795 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2796 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2797 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2798 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2799 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2800 | `		if( nArg > 1 ){` |
|         - | 2801 | `			/* Extract comparison flags */` |
|         3 | 2802 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         1 | 2803 | `		}` |
|         - | 2804 | `		/* Do the merge sort */` |
|         7 | 2805 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2806 | `		/* Fix the last link broken by the merge */` |
|        23 | 2807 | `		while(pMap->pLast->pPrev){` |
|        17 | 2808 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2809 | `		}` |
|         3 | 2810 | `	}` |
|         - | 2811 | `	/* All done,return TRUE */` |
|         7 | 2812 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2813 | `	return PH7_OK;` |
|         4 | 2814 | `}` |
|         - | 2815 | `/*` |
|         - | 2816 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2817 | ` * Sort an array in reverse order.` |
|         - | 2818 | ` * Parameters` |
|         - | 2819 | ` *  $array` |
|         - | 2820 | ` *   The input array.` |
|         - | 2821 | ` * $sort_flags` |
|         - | 2822 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2823 | ` *  Sorting type flags:` |
|         - | 2824 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2825 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2826 | ` *   SORT_STRING - compare items as strings` |
|         - | 2827 | ` * Return` |
|         - | 2828 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2829 | ` */` |
|         6 | 2830 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2831 | `{` |
|         - | 2832 | `	ph7_hashmap *pMap;` |
|         - | 2833 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2834 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2835 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2836 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2837 | `		return PH7_OK;` |
|         - | 2838 | `	}` |
|         - | 2839 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2840 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2841 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2842 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2843 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2844 | `		if( nArg > 1 ){` |
|         - | 2845 | `			/* Extract comparison flags */` |
|         5 | 2846 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2847 | `		}` |
|         - | 2848 | `		/* Do the merge sort */` |
|         7 | 2849 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2850 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         7 | 2851 | `		HashmapSortRehash(pMap);` |
|         3 | 2852 | `	}` |
|         - | 2853 | `	/* All done,return TRUE */` |
|         7 | 2854 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2855 | `	return PH7_OK;` |
|         4 | 2856 | `}` |
|         - | 2857 | `/*` |
|         - | 2858 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2859 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2860 | ` * Parameters` |
|         - | 2861 | ` *  $array` |
|         - | 2862 | ` *   The input array.` |
|         - | 2863 | ` * $cmp_function` |
|         - | 2864 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2865 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2866 | ` *  to, or greater than the second.` |
|         - | 2867 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2868 | ` * Return` |
|         - | 2869 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2870 | ` */` |
|        22 | 2871 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2872 | `{` |
|         - | 2873 | `	ph7_hashmap *pMap;` |
|         - | 2874 | `	/* Make sure we are dealing with a valid hashmap */` |
|        25 | 2875 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2876 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2877 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2878 | `		return PH7_OK;` |
|         - | 2879 | `	}` |
|        25 | 2880 | `	if( nArg > 1 ){` |
|         - | 2881 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2882 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2883 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        25 | 2884 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        25 | 2885 | `		if( rcCb != PH7_OK ){` |
|         3 | 2886 | `			return rcCb;` |
|         - | 2887 | `		}` |
|        10 | 2888 | `	}` |
|         - | 2889 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2890 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2891 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2892 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2893 | `		ph7_value *pCallback = 0;` |
|         - | 2894 | `		ProcNodeCmp xCmp;` |
|        23 | 2895 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        23 | 2896 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2897 | `			/* Point to the desired callback */` |
|        23 | 2898 | `			pCallback = apArg[1];` |
|        13 | 2899 | `		}else{` |
|         - | 2900 | `			/* Use the default comparison function */` |
|       ! 0 | 2901 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2902 | `		}` |
|         - | 2903 | `		/* Do the merge sort */` |
|        23 | 2904 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        23 | 2905 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2906 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        23 | 2907 | `		HashmapSortRehash(pMap);` |
|        23 | 2908 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2909 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2910 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2911 | `			return PH7_EXCEPTION;` |
|         - | 2912 | `		}` |
|         6 | 2913 | `	}` |
|         - | 2914 | `	/* All done,return TRUE */` |
|        14 | 2915 | `	ph7_result_bool(pCtx,1);` |
|        14 | 2916 | `	return PH7_OK;` |
|        14 | 2917 | `}` |
|         - | 2918 | `/*` |
|         - | 2919 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2920 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2921 | ` *  and maintain index association.` |
|         - | 2922 | ` * Parameters` |
|         - | 2923 | ` *  $array` |
|         - | 2924 | ` *   The input array.` |
|         - | 2925 | ` * $cmp_function` |
|         - | 2926 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2927 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2928 | ` *  to, or greater than the second.` |
|         - | 2929 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2930 | ` * Return` |
|         - | 2931 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2932 | ` */` |
|        12 | 2933 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2934 | `{` |
|         - | 2935 | `	ph7_hashmap *pMap;` |
|         - | 2936 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 2937 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2938 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2939 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2940 | `		return PH7_OK;` |
|         - | 2941 | `	}` |
|        13 | 2942 | `	if( nArg > 1 ){` |
|         - | 2943 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2944 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2945 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        13 | 2946 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        13 | 2947 | `		if( rcCb != PH7_OK ){` |
|         3 | 2948 | `			return rcCb;` |
|         - | 2949 | `		}` |
|         5 | 2950 | `	}` |
|         - | 2951 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2952 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2953 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2954 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2955 | `		ph7_value *pCallback = 0;` |
|         - | 2956 | `		ProcNodeCmp xCmp;` |
|        11 | 2957 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2958 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2959 | `			/* Point to the desired callback */` |
|        11 | 2960 | `			pCallback = apArg[1];` |
|         6 | 2961 | `		}else{` |
|         - | 2962 | `			/* Use the default comparison function */` |
|       ! 0 | 2963 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2964 | `		}` |
|         - | 2965 | `		/* Do the merge sort */` |
|        11 | 2966 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2967 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2968 | `		/* Fix the last link broken by the merge */` |
|        23 | 2969 | `		while(pMap->pLast->pPrev){` |
|        13 | 2970 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2971 | `		}` |
|        11 | 2972 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2973 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2974 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2975 | `			return PH7_EXCEPTION;` |
|         - | 2976 | `		}` |
|         5 | 2977 | `	}` |
|         - | 2978 | `	/* All done,return TRUE */` |
|        11 | 2979 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2980 | `	return PH7_OK;` |
|         7 | 2981 | `}` |
|         - | 2982 | `/*` |
|         - | 2983 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2984 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2985 | ` *  function and maintain index association.` |
|         - | 2986 | ` * Parameters` |
|         - | 2987 | ` *  $array` |
|         - | 2988 | ` *   The input array.` |
|         - | 2989 | ` * $cmp_function` |
|         - | 2990 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2991 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2992 | ` *  to, or greater than the second.` |
|         - | 2993 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2994 | ` * Return` |
|         - | 2995 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2996 | ` */` |
|         4 | 2997 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2998 | `{` |
|         - | 2999 | `	ph7_hashmap *pMap;` |
|         - | 3000 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3001 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3002 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3003 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3004 | `		return PH7_OK;` |
|         - | 3005 | `	}` |
|         5 | 3006 | `	if( nArg > 1 ){` |
|         - | 3007 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 3008 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 3009 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|         5 | 3010 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|         5 | 3011 | `		if( rcCb != PH7_OK ){` |
|         3 | 3012 | `			return rcCb;` |
|         - | 3013 | `		}` |
|         1 | 3014 | `	}` |
|         - | 3015 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3016 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3017 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3018 | `	if( pMap->nEntry > 1 ){` |
|         3 | 3019 | `		ph7_value *pCallback = 0;` |
|         - | 3020 | `		ProcNodeCmp xCmp;` |
|         3 | 3021 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 3022 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 3023 | `			/* Point to the desired callback */` |
|         3 | 3024 | `			pCallback = apArg[1];` |
|         2 | 3025 | `		}else{` |
|         - | 3026 | `			/* Use the default comparison function */` |
|       ! 0 | 3027 | `			xCmp = HashmapCmpCallback2;` |
|         - | 3028 | `		}` |
|         - | 3029 | `		/* Do the merge sort */` |
|         3 | 3030 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 3031 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 3032 | `		/* Fix the last link broken by the merge */` |
|         3 | 3033 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 3034 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 3035 | `		}` |
|         3 | 3036 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 3037 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 3038 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 3039 | `			return PH7_EXCEPTION;` |
|         - | 3040 | `		}` |
|         1 | 3041 | `	}` |
|         - | 3042 | `	/* All done,return TRUE */` |
|         3 | 3043 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3044 | `	return PH7_OK;` |
|         3 | 3045 | `}` |
|         - | 3046 | `/*` |
|         - | 3047 | ` * bool shuffle(array &$array)` |
|         - | 3048 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 3049 | ` * Parameters` |
|         - | 3050 | ` *  $array` |
|         - | 3051 | ` *   The input array.` |
|         - | 3052 | ` * Return` |
|         - | 3053 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3054 | ` *` |
|         - | 3055 | ` */` |
|         2 | 3056 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3057 | `{` |
|         - | 3058 | `	ph7_hashmap *pMap;` |
|         - | 3059 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3060 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3061 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3062 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3063 | `		return PH7_OK;` |
|         - | 3064 | `	}` |
|         - | 3065 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3066 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3067 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3068 | `	if( pMap->nEntry > 1 ){` |
|         - | 3069 | `		/* Do the merge sort */` |
|         3 | 3070 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 3071 | `		/* Fix the last link broken by the merge */` |
|        11 | 3072 | `		while(pMap->pLast->pPrev){` |
|         9 | 3073 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 3074 | `		}` |
|         1 | 3075 | `	}` |
|         - | 3076 | `	/* All done,return TRUE */` |
|         3 | 3077 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3078 | `	return PH7_OK;` |
|         2 | 3079 | `}` |
|         - | 3080 | `/*` |
|         - | 3081 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 3082 | ` *   Count all elements in an array, or something in an object.` |
|         - | 3083 | ` * Parameters` |
|         - | 3084 | ` *  $var` |
|         - | 3085 | ` *   The array or the object.` |
|         - | 3086 | ` * $mode` |
|         - | 3087 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 3088 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 3089 | ` *  all the elements of a multidimensional array.` |
|         - | 3090 | ` * Return` |
|         - | 3091 | ` *  Returns the number of elements in the array.` |
|         - | 3092 | ` */` |
|      2216 | 3093 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3094 | `{` |
|      2221 | 3095 | `	int bRecursive = FALSE;` |
|      2221 | 3096 | `	int bCycleDetected = FALSE;` |
|         - | 3097 | `	sxi64 iCount;` |
|      2221 | 3098 | `	if( nArg < 1 ){` |
|       ! 0 | 3099 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3100 | `			"ArgumentCountError",` |
|         - | 3101 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 3102 | `			);` |
|         - | 3103 | `	}` |
|      2221 | 3104 | `	if( nArg > 2 ){` |
|         4 | 3105 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3106 | `			"ArgumentCountError",` |
|         - | 3107 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3108 | `			nArg` |
|         - | 3109 | `			);` |
|         - | 3110 | `	}` |
|         - | 3111 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3112 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3113 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      2219 | 3114 | `	if( nArg > 1 ){` |
|        45 | 3115 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 3116 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 3117 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3118 | `				"ValueError",` |
|         - | 3119 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3120 | `				);` |
|         - | 3121 | `		}` |
|        34 | 3122 | `		bRecursive = iMode == 1;` |
|        16 | 3123 | `	}` |
|      2211 | 3124 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3125 | `		/* Countable object: dispatch to ->count() */` |
|        75 | 3126 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        63 | 3127 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        63 | 3128 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        63 | 3129 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        60 | 3130 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3131 | `					"count",sizeof("count")-1);` |
|        60 | 3132 | `				if( pMeth ){` |
|         - | 3133 | `					ph7_value sResult;` |
|        60 | 3134 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        60 | 3135 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        60 | 3136 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        60 | 3137 | `					PH7_MemObjRelease(&sResult);` |
|        60 | 3138 | `					return PH7_OK;` |
|         - | 3139 | `				}` |
|       ! 0 | 3140 | `			}` |
|         1 | 3141 | `		}` |
|        22 | 3142 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3143 | `			"TypeError",` |
|         - | 3144 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3145 | `			ph7_type_name(apArg[0])` |
|         - | 3146 | `			);` |
|         - | 3147 | `	}` |
|         - | 3148 | `	/* Count */` |
|      2141 | 3149 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      2141 | 3150 | `	if( bCycleDetected ){` |
|         3 | 3151 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3152 | `	}` |
|      2141 | 3153 | `	ph7_result_int64(pCtx,iCount);` |
|      2141 | 3154 | `	return PH7_OK;` |
|      1113 | 3155 | `}` |
|         - | 3156 | `/*` |
|         - | 3157 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3158 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3159 | ` * Parameters` |
|         - | 3160 | ` * $key` |
|         - | 3161 | ` *   Value to check.` |
|         - | 3162 | ` * $search` |
|         - | 3163 | ` *  An array with keys to check.` |
|         - | 3164 | ` * Return` |
|         - | 3165 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3166 | ` */` |
|        94 | 3167 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3168 | `{` |
|         - | 3169 | `	sxi32 rc;` |
|        98 | 3170 | `	if( nArg != 2 ){` |
|         - | 3171 | `		/* PHP requires exactly two arguments */` |
|         4 | 3172 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3173 | `			"ArgumentCountError",` |
|         - | 3174 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3175 | `			nArg` |
|         - | 3176 | `			);` |
|         - | 3177 | `	}` |
|         - | 3178 | `	/* Make sure we are dealing with a valid hashmap */` |
|        96 | 3179 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3180 | `		/* Type mismatch -> TypeError */` |
|         8 | 3181 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3182 | `			"TypeError",` |
|         - | 3183 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3184 | `			ph7_type_name(apArg[1])` |
|         - | 3185 | `			);` |
|         - | 3186 | `	}` |
|         - | 3187 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        92 | 3188 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3189 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3190 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3191 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3192 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3193 | `			"use an empty string instead"` |
|         - | 3194 | `			);` |
|        91 | 3195 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3196 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3197 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3198 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3199 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3200 | `				,rVal` |
|         - | 3201 | `				);` |
|         1 | 3202 | `		}` |
|         1 | 3203 | `	}` |
|         - | 3204 | `	/* Perform the lookup */` |
|        92 | 3205 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3206 | `	/* lookup result */` |
|        92 | 3207 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        92 | 3208 | `	return PH7_OK;` |
|        51 | 3209 | `}` |
|         - | 3210 | `/*` |
|         - | 3211 | ` * value array_pop(array $array)` |
|         - | 3212 | ` *   POP the last inserted element from the array.` |
|         - | 3213 | ` * Parameter` |
|         - | 3214 | ` *  The array to get the value from.` |
|         - | 3215 | ` * Return` |
|         - | 3216 | ` *  Poped value or NULL on failure.` |
|         - | 3217 | ` */` |
|       108 | 3218 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3219 | `{` |
|         - | 3220 | `	ph7_hashmap *pMap;` |
|         - | 3221 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       112 | 3222 | `	if( nArg != 1 ){` |
|         4 | 3223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3224 | `			"ArgumentCountError",` |
|         - | 3225 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3226 | `			nArg` |
|         - | 3227 | `			);` |
|         - | 3228 | `	}` |
|         - | 3229 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3230 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       110 | 3231 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3232 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3233 | `			"Error",` |
|         - | 3234 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3235 | `			);` |
|         - | 3236 | `	}` |
|         - | 3237 | `	/* Make sure we are dealing with a valid hashmap */` |
|       104 | 3238 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3239 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3240 | `			"TypeError",` |
|         - | 3241 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3242 | `			ph7_type_name(apArg[0])` |
|         - | 3243 | `			);` |
|         - | 3244 | `	}` |
|       101 | 3245 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       101 | 3246 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       101 | 3247 | `	if( pMap->nEntry < 1 ){` |
|         - | 3248 | `		/* Nothing to pop,return NULL */` |
|         3 | 3249 | `		ph7_result_null(pCtx);` |
|         2 | 3250 | `	}else{` |
|        99 | 3251 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3252 | `		ph7_value *pObj;` |
|        99 | 3253 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        99 | 3254 | `		if( pObj ){` |
|         - | 3255 | `			/* Node value */` |
|        99 | 3256 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3257 | `			/* Unlink the node */` |
|        99 | 3258 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        50 | 3259 | `		}else{` |
|       ! 0 | 3260 | `			ph7_result_null(pCtx);` |
|         - | 3261 | `		}` |
|         - | 3262 | `		/* Reset the cursor */` |
|        99 | 3263 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3264 | `	}` |
|       101 | 3265 | `	return PH7_OK;` |
|        58 | 3266 | `}` |
|         - | 3267 | `/*` |
|         - | 3268 | ` * int array_push($array,$var,...)` |
|         - | 3269 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3270 | ` * Parameters` |
|         - | 3271 | ` *  array` |
|         - | 3272 | ` *    The input array.` |
|         - | 3273 | ` *  var` |
|         - | 3274 | ` *   On or more value to push.` |
|         - | 3275 | ` * Return` |
|         - | 3276 | ` *  New array count (including old items).` |
|         - | 3277 | ` */` |
|        22 | 3278 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3279 | `{` |
|         - | 3280 | `	ph7_hashmap *pMap;` |
|         - | 3281 | `	sxi32 rc;` |
|         - | 3282 | `	int i;` |
|        27 | 3283 | `	if( nArg < 1 ){` |
|       ! 0 | 3284 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3285 | `			"ArgumentCountError",` |
|         - | 3286 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3287 | `			nArg` |
|         - | 3288 | `			);` |
|         - | 3289 | `	}` |
|         - | 3290 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3291 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        27 | 3292 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3293 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3294 | `			"Error",` |
|         - | 3295 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3296 | `			);` |
|         - | 3297 | `	}` |
|         - | 3298 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3299 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3300 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3301 | `			"TypeError",` |
|         - | 3302 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3303 | `			ph7_type_name(apArg[0])` |
|         - | 3304 | `			);` |
|         - | 3305 | `	}` |
|         - | 3306 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3307 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3308 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3309 | `	/* Start pushing given values */` |
|        34 | 3310 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3311 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3312 | `		if( rc != SXRET_OK ){` |
|         3 | 3313 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3314 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3315 | `				return rc;` |
|         - | 3316 | `			}` |
|       ! 0 | 3317 | `			break;` |
|         - | 3318 | `		}` |
|         9 | 3319 | `	}` |
|         - | 3320 | `	/* Return the new count */` |
|        15 | 3321 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3322 | `	return PH7_OK;` |
|        16 | 3323 | `}` |
|         - | 3324 | `/*` |
|         - | 3325 | ` * value array_shift(array $array)` |
|         - | 3326 | ` *   Shift an element off the beginning of array.` |
|         - | 3327 | ` * Parameter` |
|         - | 3328 | ` *  The array to get the value from.` |
|         - | 3329 | ` * Return` |
|         - | 3330 | ` *  Shifted value or NULL on failure.` |
|         - | 3331 | ` */` |
|        44 | 3332 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3333 | `{` |
|         - | 3334 | `	ph7_hashmap *pMap;` |
|         - | 3335 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3336 | `	if( nArg != 1 ){` |
|         4 | 3337 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3338 | `			"ArgumentCountError",` |
|         - | 3339 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3340 | `			nArg` |
|         - | 3341 | `			);` |
|         - | 3342 | `	}` |
|         - | 3343 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3344 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3345 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3346 | `			"Error",` |
|         - | 3347 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3348 | `			);` |
|         - | 3349 | `	}` |
|         - | 3350 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3351 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3352 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3353 | `			"TypeError",` |
|         - | 3354 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3355 | `			ph7_type_name(apArg[0])` |
|         - | 3356 | `			);` |
|         - | 3357 | `	}` |
|         - | 3358 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3359 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3360 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3361 | `	if( pMap->nEntry < 1 ){` |
|         - | 3362 | `		/* Empty hashmap,return NULL */` |
|         3 | 3363 | `		ph7_result_null(pCtx);` |
|         2 | 3364 | `	}else{` |
|        39 | 3365 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3366 | `		ph7_value *pObj;` |
|         - | 3367 | `		sxu32 n;` |
|        39 | 3368 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3369 | `		if( pObj ){` |
|         - | 3370 | `			/* Node value */` |
|        39 | 3371 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3372 | `			/* Unlink the first node */` |
|        39 | 3373 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3374 | `		}else{` |
|       ! 0 | 3375 | `			ph7_result_null(pCtx);` |
|         - | 3376 | `		}` |
|         - | 3377 | `		/* Rehash all int keys */` |
|        39 | 3378 | `		n = pMap->nEntry;` |
|        39 | 3379 | `		pEntry = pMap->pFirst;` |
|        39 | 3380 | `		pMap->iNextIdx = 0;` |
|        39 | 3381 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3382 | `		for(;;){` |
|        99 | 3383 | `			if( n < 1 ){` |
|        39 | 3384 | `				break;` |
|         - | 3385 | `			}` |
|        65 | 3386 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3387 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3388 | `			}` |
|         - | 3389 | `			/* Point to the next entry */` |
|        65 | 3390 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3391 | `			n--;` |
|         5 | 3392 | `		}` |
|         - | 3393 | `		/* Reset the cursor */` |
|        39 | 3394 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3395 | `	}` |
|        41 | 3396 | `	return PH7_OK;` |
|        27 | 3397 | `}` |
|         - | 3398 | `/*` |
|         - | 3399 | ` * Extract the node cursor value.` |
|         - | 3400 | ` */` |
|      1232 | 3401 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3402 | `{` |
|      1233 | 3403 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3404 | `	ph7_value *pVal;` |
|      1233 | 3405 | `	if( pCur == 0 ){` |
|         - | 3406 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3407 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3408 | `		return PH7_OK;` |
|         - | 3409 | `	}` |
|      1195 | 3410 | `	if( iDirection != 0 ){` |
|       227 | 3411 | `		if( iDirection > 0 ){` |
|         - | 3412 | `			/* Point to the next entry */` |
|       225 | 3413 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       225 | 3414 | `			pCur = pMap->pCur;` |
|       113 | 3415 | `		}else{` |
|         - | 3416 | `			/* Point to the previous entry */` |
|         3 | 3417 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3418 | `			pCur = pMap->pCur;` |
|         - | 3419 | `		}` |
|       227 | 3420 | `		if( pCur == 0 ){` |
|         - | 3421 | `			/* End of input reached,return FALSE */` |
|        91 | 3422 | `			ph7_result_bool(pCtx,0);` |
|        91 | 3423 | `			return PH7_OK;` |
|         - | 3424 | `		}` |
|        68 | 3425 | `	}` |
|         - | 3426 | `	/* Point to the desired element */` |
|      1105 | 3427 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      1105 | 3428 | `	if( pVal ){` |
|      1105 | 3429 | `		ph7_result_value(pCtx,pVal);` |
|       553 | 3430 | `	}else{` |
|       ! 0 | 3431 | `		ph7_result_bool(pCtx,0);` |
|         - | 3432 | `	}` |
|      1105 | 3433 | `	return PH7_OK;` |
|       617 | 3434 | `}` |
|         - | 3435 | `/*` |
|         - | 3436 | ` * value current(array $array)` |
|         - | 3437 | ` *  Return the current element in an array.` |
|         - | 3438 | ` * Parameter` |
|         - | 3439 | ` *  $input: The input array.` |
|         - | 3440 | ` * Return` |
|         - | 3441 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3442 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3443 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3444 | ` *  is empty, current() returns FALSE.` |
|         - | 3445 | ` */` |
|       356 | 3446 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3447 | `{` |
|       357 | 3448 | `	if( nArg < 1 ){` |
|         - | 3449 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3450 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3451 | `		return PH7_OK;` |
|         - | 3452 | `	}` |
|         - | 3453 | `	/* Make sure we are dealing with a valid hashmap */` |
|       357 | 3454 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3455 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3456 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3457 | `		return PH7_OK;` |
|         - | 3458 | `	}` |
|       357 | 3459 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       357 | 3460 | `	return PH7_OK;` |
|       179 | 3461 | `}` |
|         - | 3462 | `/*` |
|         - | 3463 | ` * value next(array $input)` |
|         - | 3464 | ` *  Advance the internal array pointer of an array.` |
|         - | 3465 | ` * Parameter` |
|         - | 3466 | ` *  $input: The input array.` |
|         - | 3467 | ` * Return` |
|         - | 3468 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3469 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3470 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3471 | ` */` |
|       224 | 3472 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3473 | `{` |
|       225 | 3474 | `	if( nArg < 1 ){` |
|         - | 3475 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3476 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3477 | `		return PH7_OK;` |
|         - | 3478 | `	}` |
|         - | 3479 | `	/* Make sure we are dealing with a valid hashmap */` |
|       225 | 3480 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3481 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3482 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3483 | `		return PH7_OK;` |
|         - | 3484 | `	}` |
|       225 | 3485 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       225 | 3486 | `	return PH7_OK;` |
|       113 | 3487 | `}` |
|         - | 3488 | `/*` |
|         - | 3489 | ` * value prev(array $input)` |
|         - | 3490 | ` *  Rewind the internal array pointer.` |
|         - | 3491 | ` * Parameter` |
|         - | 3492 | ` *  $input: The input array.` |
|         - | 3493 | ` * Return` |
|         - | 3494 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3495 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3496 | ` *  elements.` |
|         - | 3497 | ` */` |
|         2 | 3498 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3499 | `{` |
|         3 | 3500 | `	if( nArg < 1 ){` |
|         - | 3501 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3502 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3503 | `		return PH7_OK;` |
|         - | 3504 | `	}` |
|         - | 3505 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3506 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3507 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3508 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3509 | `		return PH7_OK;` |
|         - | 3510 | `	}` |
|         3 | 3511 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3512 | `	return PH7_OK;` |
|         2 | 3513 | `}` |
|         - | 3514 | `/*` |
|         - | 3515 | ` * value end(array $input)` |
|         - | 3516 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3517 | ` * Parameter` |
|         - | 3518 | ` *  $input: The input array.` |
|         - | 3519 | ` * Return` |
|         - | 3520 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3521 | ` */` |
|       390 | 3522 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3523 | `{` |
|         - | 3524 | `	ph7_hashmap *pMap;` |
|       391 | 3525 | `	if( nArg < 1 ){` |
|         - | 3526 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3527 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3528 | `		return PH7_OK;` |
|         - | 3529 | `	}` |
|         - | 3530 | `	/* Make sure we are dealing with a valid hashmap */` |
|       391 | 3531 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3532 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3533 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3534 | `		return PH7_OK;` |
|         - | 3535 | `	}` |
|         - | 3536 | `	/* Point to the internal representation of the input hashmap */` |
|       391 | 3537 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3538 | `	/* Point to the last node */` |
|       391 | 3539 | `	pMap->pCur = pMap->pLast;` |
|         - | 3540 | `	/* Return the last node value */` |
|       391 | 3541 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       391 | 3542 | `	return PH7_OK;` |
|       196 | 3543 | `}` |
|         - | 3544 | `/*` |
|         - | 3545 | ` * value reset(array $array )` |
|         - | 3546 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3547 | ` * Parameter` |
|         - | 3548 | ` *  $input: The input array.` |
|         - | 3549 | ` * Return` |
|         - | 3550 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3551 | ` */` |
|       260 | 3552 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3553 | `{` |
|         - | 3554 | `	ph7_hashmap *pMap;` |
|       261 | 3555 | `	if( nArg < 1 ){` |
|         - | 3556 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3557 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3558 | `		return PH7_OK;` |
|         - | 3559 | `	}` |
|         - | 3560 | `	/* Make sure we are dealing with a valid hashmap */` |
|       261 | 3561 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3562 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3563 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3564 | `		return PH7_OK;` |
|         - | 3565 | `	}` |
|         - | 3566 | `	/* Point to the internal representation of the input hashmap */` |
|       261 | 3567 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3568 | `	/* Point to the first node */` |
|       261 | 3569 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3570 | `	/* Return the last node value if available */` |
|       261 | 3571 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       261 | 3572 | `	return PH7_OK;` |
|       131 | 3573 | `}` |
|         - | 3574 | `/*` |
|         - | 3575 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3576 | ` * array_key_first() and array_key_last().` |
|         - | 3577 | ` */` |
|       772 | 3578 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3579 | `{` |
|       773 | 3580 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3581 | `		/* Key is integer */` |
|       383 | 3582 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       192 | 3583 | `	}else{` |
|         - | 3584 | `		/* Key is blob */` |
|       586 | 3585 | `		ph7_result_string(pCtx,` |
|       390 | 3586 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3587 | `	}` |
|       773 | 3588 | `}` |
|         - | 3589 | `/*` |
|         - | 3590 | ` * value key(array $array)` |
|         - | 3591 | ` *   Fetch a key from an array` |
|         - | 3592 | ` * Parameter` |
|         - | 3593 | ` *  $input` |
|         - | 3594 | ` *   The input array.` |
|         - | 3595 | ` * Return` |
|         - | 3596 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3597 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3598 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3599 | ` *  is empty, key() returns NULL.` |
|         - | 3600 | ` */` |
|       892 | 3601 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3602 | `{` |
|         - | 3603 | `	ph7_hashmap_node *pCur;` |
|         - | 3604 | `	ph7_hashmap *pMap;` |
|       893 | 3605 | `	if( nArg < 1 ){` |
|         - | 3606 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3607 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3608 | `		return PH7_OK;` |
|         - | 3609 | `	}` |
|         - | 3610 | `	/* Make sure we are dealing with a valid hashmap */` |
|       893 | 3611 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3612 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3613 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3614 | `		return PH7_OK;` |
|         - | 3615 | `	}` |
|       893 | 3616 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       893 | 3617 | `	pCur = pMap->pCur;` |
|       893 | 3618 | `	if( pCur == 0 ){` |
|         - | 3619 | `		/* Cursor does not point to anything,return NULL */` |
|       137 | 3620 | `		ph7_result_null(pCtx);` |
|       137 | 3621 | `		return PH7_OK;` |
|         - | 3622 | `	}` |
|       757 | 3623 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       757 | 3624 | `	return PH7_OK;` |
|       447 | 3625 | `}` |
|         - | 3626 | `/*` |
|         - | 3627 | ` * array each(array $input)` |
|         - | 3628 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3629 | ` * Parameter` |
|         - | 3630 | ` *  $input` |
|         - | 3631 | ` *    The input array.` |
|         - | 3632 | ` * Return` |
|         - | 3633 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3634 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3635 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3636 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3637 | ` *  each() returns FALSE.` |
|         - | 3638 | ` */` |
|        22 | 3639 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3640 | `{` |
|         - | 3641 | `	ph7_hashmap_node *pCur;` |
|         - | 3642 | `	ph7_hashmap *pMap;` |
|         - | 3643 | `	ph7_value *pArray;` |
|         - | 3644 | `	ph7_value *pVal;` |
|         - | 3645 | `	ph7_value sKey;` |
|        23 | 3646 | `	if( nArg < 1 ){` |
|         - | 3647 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3648 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3649 | `		return PH7_OK;` |
|         - | 3650 | `	}` |
|         - | 3651 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3652 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3653 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3654 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3655 | `		return PH7_OK;` |
|         - | 3656 | `	}` |
|         - | 3657 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3658 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3659 | `	if( pMap->pCur == 0 ){` |
|         - | 3660 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3661 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3662 | `		return PH7_OK;` |
|         - | 3663 | `	}` |
|        15 | 3664 | `	pCur = pMap->pCur;` |
|         - | 3665 | `	/* Create a new array */` |
|        15 | 3666 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3667 | `	if( pArray == 0 ){` |
|       ! 0 | 3668 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3669 | `		return PH7_OK;` |
|         - | 3670 | `	}` |
|        15 | 3671 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3672 | `	/* Insert the current value */` |
|        15 | 3673 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3674 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3675 | `	/* Make the key */` |
|        15 | 3676 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3677 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3678 | `	}else{` |
|         9 | 3679 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3680 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3681 | `	}` |
|         - | 3682 | `	/* Insert the current key */` |
|        15 | 3683 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3684 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3685 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3686 | `	/* Advance the cursor */` |
|        15 | 3687 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3688 | `	/* Return the current entry */` |
|        15 | 3689 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3690 | `	return PH7_OK;` |
|        12 | 3691 | `}` |
|         - | 3692 | `/*` |
|         - | 3693 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3694 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3695 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3696 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3697 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3698 | ` */` |
|         - | 3699 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3700 | `/*` |
|         - | 3701 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3702 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3703 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3704 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3705 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3706 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3707 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3708 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3709 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3710 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3711 | ` */` |
|         - | 3712 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3713 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3714 | `/*` |
|         - | 3715 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3716 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3717 | ` */` |
|       ! 0 | 3718 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3719 | `{` |
|       ! 0 | 3720 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3721 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3722 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3723 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3724 | `		zBuf[n] = 0;` |
|       ! 0 | 3725 | `		return zBuf;` |
|         - | 3726 | `	}` |
|       ! 0 | 3727 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3728 | `}` |
|         - | 3729 | `/*` |
|         - | 3730 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3731 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3732 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3733 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3734 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3735 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3736 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3737 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3738 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3739 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3740 | ` */` |
|       156 | 3741 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3742 | `{` |
|       157 | 3743 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3744 | `	sxu64 uVal = 0;` |
|       157 | 3745 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3746 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3747 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3748 | `		bNeg = (z[0] == '-');` |
|         3 | 3749 | `		z++;` |
|         1 | 3750 | `	}` |
|       237 | 3751 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3752 | `		int d = z[0] - '0';` |
|         - | 3753 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3754 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3755 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3756 | `			bOverflow = 1;` |
|       ! 0 | 3757 | `		}else{` |
|        81 | 3758 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3759 | `		}` |
|        81 | 3760 | `		bDigit = 1;` |
|        81 | 3761 | `		z++;` |
|         1 | 3762 | `	}` |
|       157 | 3763 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3764 | `		bReal = 1;` |
|         3 | 3765 | `		z++;` |
|         5 | 3766 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3767 | `			bDigit = 1;` |
|         3 | 3768 | `			z++;` |
|         1 | 3769 | `		}` |
|         1 | 3770 | `	}` |
|         - | 3771 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3772 | `	if( !bDigit ){` |
|        61 | 3773 | `		return RANGE_IN_ERROR;` |
|         - | 3774 | `	}` |
|         - | 3775 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3776 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3777 | `		z++;` |
|         9 | 3778 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3779 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3780 | `			return RANGE_IN_ERROR;` |
|         - | 3781 | `		}` |
|         9 | 3782 | `		bReal = 1;` |
|        17 | 3783 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3784 | `	}` |
|         - | 3785 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3786 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3787 | `	if( z != zEnd ){` |
|        13 | 3788 | `		return RANGE_IN_ERROR;` |
|         - | 3789 | `	}` |
|        84 | 3790 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3791 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3792 | `		bReal = 1;` |
|        84 | 3793 | `	}` |
|        43 | 3794 | `	if( bReal ){` |
|        11 | 3795 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3796 | `		return RANGE_IN_DOUBLE;` |
|         - | 3797 | `	}` |
|         - | 3798 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3799 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3800 | `	return RANGE_IN_LONG;` |
|        58 | 3801 | `}` |
|         - | 3802 | `/*` |
|         - | 3803 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3804 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3805 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3806 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3807 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3808 | ` */` |
|       332 | 3809 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3810 | `{` |
|         - | 3811 | `	char zMsg[160];` |
|       333 | 3812 | `	*pRc = PH7_OK;` |
|       333 | 3813 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3814 | `		char zType[80];` |
|       ! 0 | 3815 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3816 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3817 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3818 | `		return FALSE;` |
|         - | 3819 | `	}` |
|       333 | 3820 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3821 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3822 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3823 | `			iArg,zName);` |
|         5 | 3824 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3825 | `		*pbNullCoerced = TRUE;` |
|         2 | 3826 | `	}` |
|       333 | 3827 | `	return TRUE;` |
|       167 | 3828 | `}` |
|         - | 3829 | `/*` |
|         - | 3830 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3831 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3832 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3833 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3834 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3835 | ` */` |
|        60 | 3836 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3837 | `{` |
|        61 | 3838 | `	*pRc = PH7_OK;` |
|        61 | 3839 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3840 | `		char zType[80];` |
|       ! 0 | 3841 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3842 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3843 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3844 | `		return RANGE_IN_ERROR;` |
|         - | 3845 | `	}` |
|        61 | 3846 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3847 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3848 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3849 | `		*pLong = 0;` |
|         3 | 3850 | `		return RANGE_IN_LONG;` |
|         - | 3851 | `	}` |
|        59 | 3852 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3853 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3854 | `		return RANGE_IN_DOUBLE;` |
|         - | 3855 | `	}` |
|        35 | 3856 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3857 | `		const char *zStr;` |
|         - | 3858 | `		int nLen;` |
|         - | 3859 | `		sxu8 iKind;` |
|         3 | 3860 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3861 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3862 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3863 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3864 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3865 | `		}` |
|         3 | 3866 | `		return iKind;` |
|         - | 3867 | `	}` |
|         - | 3868 | `	/* int / bool */` |
|        33 | 3869 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3870 | `	return RANGE_IN_LONG;` |
|        31 | 3871 | `}` |
|         - | 3872 | `/*` |
|         - | 3873 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3874 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3875 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3876 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3877 | ` */` |
|       300 | 3878 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3879 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3880 | `{` |
|         - | 3881 | `	char zMsg[160];` |
|         - | 3882 | `	double r;` |
|       301 | 3883 | `	*pRc = PH7_OK;` |
|       301 | 3884 | `	if( bNullCoerced ){` |
|         - | 3885 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3886 | `		*pLong = 0;` |
|         5 | 3887 | `		*pDouble = 0.0;` |
|         5 | 3888 | `		return RANGE_IN_LONG;` |
|         - | 3889 | `	}` |
|       297 | 3890 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3891 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3892 | `check_dval:` |
|        25 | 3893 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3894 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3895 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3896 | `			return RANGE_IN_ERROR;` |
|         - | 3897 | `		}` |
|        21 | 3898 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3899 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3900 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3901 | `			return RANGE_IN_ERROR;` |
|         - | 3902 | `		}` |
|        17 | 3903 | `		*pDouble = r;` |
|        17 | 3904 | `		return RANGE_IN_DOUBLE;` |
|         - | 3905 | `	}` |
|       277 | 3906 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3907 | `		const char *zStr;` |
|         - | 3908 | `		int nLen;` |
|         - | 3909 | `		sxu8 iKind;` |
|        81 | 3910 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3911 | `		if( nLen == 0 ){` |
|         7 | 3912 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3913 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3914 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3915 | `			*pLong = 0;` |
|         5 | 3916 | `			*pDouble = 0.0;` |
|        41 | 3917 | `			return RANGE_IN_LONG;` |
|         - | 3918 | `		}` |
|        77 | 3919 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3920 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3921 | `			r = *pDouble;` |
|         5 | 3922 | `			goto check_dval;` |
|         - | 3923 | `		}` |
|        73 | 3924 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3925 | `			*pDouble = (double)*pLong;` |
|        23 | 3926 | `			if( nLen == 1 ){` |
|         - | 3927 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3928 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3929 | `				return RANGE_IN_DIGIT;` |
|         - | 3930 | `			}` |
|        15 | 3931 | `			return RANGE_IN_LONG;` |
|         - | 3932 | `		}` |
|        51 | 3933 | `		if( nLen != 1 ){` |
|        10 | 3934 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3935 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3936 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3937 | `		}` |
|        51 | 3938 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3939 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3940 | `		*pLong = 0;` |
|        51 | 3941 | `		*pDouble = 0.0;` |
|        51 | 3942 | `		return RANGE_IN_STRING;` |
|         - | 3943 | `	}` |
|         - | 3944 | `	/* int / bool */` |
|       197 | 3945 | `	*pLong = ph7_value_to_int64(pIn);` |
|       197 | 3946 | `	*pDouble = (double)*pLong;` |
|       197 | 3947 | `	return RANGE_IN_LONG;` |
|       151 | 3948 | `}` |
|         - | 3949 | `/*` |
|         - | 3950 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3951 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3952 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3953 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3954 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3955 | ` * exactly like php's two macros.` |
|         - | 3956 | ` */` |
|         6 | 3957 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3958 | `{` |
|        10 | 3959 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3960 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3961 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3962 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3963 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3964 | `}` |
|         6 | 3965 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3966 | `{` |
|         - | 3967 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3968 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3969 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3970 | `	const unsigned int nBuf = 1500;` |
|         7 | 3971 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3972 | `	if( zMsg == 0 ){` |
|       ! 0 | 3973 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3974 | `	}` |
|         7 | 3975 | `	snprintf(zMsg,nBuf,` |
|         - | 3976 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3977 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3978 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3979 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3980 | `}` |
|         - | 3981 | `/*` |
|         - | 3982 | ` * Set the element container to the next range element and append it to the` |
|         - | 3983 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3984 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3985 | ` * below stay one line per iteration.` |
|         - | 3986 | ` */` |
|    401680 | 3987 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3988 | `{` |
|    401681 | 3989 | `	ph7_value_int64(pValue,iVal);` |
|    401681 | 3990 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3991 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3992 | `	}` |
|    401681 | 3993 | `	return PH7_OK;` |
|    200841 | 3994 | `}` |
|        70 | 3995 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3996 | `{` |
|        71 | 3997 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3998 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3999 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4000 | `	}` |
|        71 | 4001 | `	return PH7_OK;` |
|        36 | 4002 | `}` |
|       168 | 4003 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 4004 | `{` |
|       169 | 4005 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 4006 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 4007 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4008 | `	}` |
|       169 | 4009 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 4010 | `	return PH7_OK;` |
|        85 | 4011 | `}` |
|         - | 4012 | `/*` |
|         - | 4013 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 4014 | ` *  Create an array containing a range of elements.` |
|         - | 4015 | ` * Return` |
|         - | 4016 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 4017 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 4018 | ` */` |
|       168 | 4019 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4020 | `{` |
|         - | 4021 | `	ph7_value *pValue,*pArray;` |
|       169 | 4022 | `	sxi32 rc = PH7_OK;` |
|       169 | 4023 | `	int is_step_double = 0,is_step_negative = 0;` |
|       169 | 4024 | `	double step_double = 1.0;` |
|       169 | 4025 | `	sxi64 step = 1;` |
|         - | 4026 | `	sxu8 start_type,end_type;` |
|       169 | 4027 | `	sxi64 start_long = 0,end_long = 0;` |
|       169 | 4028 | `	double start_double = 0.0,end_double = 0.0;` |
|       169 | 4029 | `	unsigned char cStart = 0,cEnd = 0;` |
|       169 | 4030 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 4031 | `	sxu32 i,size;` |
|         - | 4032 |  |
|         - | 4033 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       169 | 4034 | `	if( nArg > 3 ){` |
|         4 | 4035 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 4036 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 4037 | `	}` |
|       167 | 4038 | `	if( nArg < 2 ){` |
|         - | 4039 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 4040 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 4041 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 4042 | `	}` |
|         - | 4043 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 4044 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       167 | 4045 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 4046 | `		return rc;` |
|         - | 4047 | `	}` |
|       167 | 4048 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 4049 | `		return rc;` |
|         - | 4050 | `	}` |
|       167 | 4051 | `	if( nArg > 2 ){` |
|        61 | 4052 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 4053 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 4054 | `			return rc;` |
|         - | 4055 | `		}` |
|        59 | 4056 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 4057 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 4058 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4059 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 4060 | `			}` |
|        23 | 4061 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 4062 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4063 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 4064 | `			}` |
|         - | 4065 | `			/* We only want positive step values. */` |
|        21 | 4066 | `			if( step_double < 0.0 ){` |
|       ! 0 | 4067 | `				is_step_negative = 1;` |
|       ! 0 | 4068 | `				step_double *= -1;` |
|       ! 0 | 4069 | `			}` |
|         - | 4070 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 4071 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 4072 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 4073 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 4074 | `				step = (sxi64)step_double;` |
|        19 | 4075 | `				if( (double)step != step_double ){` |
|        17 | 4076 | `					is_step_double = 1;` |
|         8 | 4077 | `				}` |
|        10 | 4078 | `			}else{` |
|         - | 4079 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 4080 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 4081 | `				is_step_double = 1;` |
|         - | 4082 | `			}` |
|        11 | 4083 | `		}else{` |
|         - | 4084 | `			/* We only want positive step values. */` |
|        35 | 4085 | `			if( step < 0 ){` |
|        11 | 4086 | `				if( step == SMALLEST_INT64 ){` |
|         - | 4087 | `					/* -step would overflow */` |
|         4 | 4088 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 4089 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 4090 | `				}` |
|         9 | 4091 | `				is_step_negative = 1;` |
|         9 | 4092 | `				step = -step;` |
|         4 | 4093 | `			}` |
|        33 | 4094 | `			step_double = (double)step;` |
|         - | 4095 | `		}` |
|        53 | 4096 | `		if( step_double == 0.0 ){` |
|         7 | 4097 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4098 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 4099 | `		}` |
|        23 | 4100 | `	}` |
|       153 | 4101 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       153 | 4102 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 4103 | `		return rc;` |
|         - | 4104 | `	}` |
|       149 | 4105 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       149 | 4106 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 4107 | `		return rc;` |
|         - | 4108 | `	}` |
|         - | 4109 | `	/* Element container + result array */` |
|       145 | 4110 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       145 | 4111 | `	pArray = ph7_context_new_array(pCtx);` |
|       145 | 4112 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4113 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4114 | `	}` |
|         - | 4115 | `	/* If the range is given as strings, generate an array of characters. */` |
|       145 | 4116 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4117 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4118 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4119 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4120 | `			 * and the range is numeric. */` |
|        15 | 4121 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4122 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4123 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4124 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4125 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4126 | `				}` |
|         7 | 4127 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4128 | `			}else{` |
|         9 | 4129 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4130 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4131 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4132 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4133 | `				}` |
|         9 | 4134 | `				start_type = RANGE_IN_LONG;` |
|         - | 4135 | `			}` |
|        15 | 4136 | `			goto handle_numeric_inputs;` |
|         - | 4137 | `		}` |
|        23 | 4138 | `		if( is_step_double ){` |
|         - | 4139 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4140 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4141 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4142 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4143 | `					" of characters, inputs converted to 0");` |
|         1 | 4144 | `			}` |
|         5 | 4145 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4146 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4147 | `			goto handle_numeric_inputs;` |
|         - | 4148 | `		}` |
|         - | 4149 | `		/* Generate an array of characters */` |
|        19 | 4150 | `		if( cStart > cEnd ){` |
|         - | 4151 | `			/* Decreasing char range */` |
|         - | 4152 | `			int iCur;` |
|         3 | 4153 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4154 | `				goto boundary_error;` |
|         - | 4155 | `			}` |
|        17 | 4156 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4157 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4158 | `					return rc;` |
|         - | 4159 | `				}` |
|         8 | 4160 | `			}` |
|        18 | 4161 | `		}else if( cEnd > cStart ){` |
|         - | 4162 | `			/* Increasing char range */` |
|         - | 4163 | `			int iCur;` |
|        15 | 4164 | `			if( is_step_negative ){` |
|         3 | 4165 | `				goto negative_step_error;` |
|         - | 4166 | `			}` |
|        13 | 4167 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4168 | `				goto boundary_error;` |
|         - | 4169 | `			}` |
|       163 | 4170 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4171 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4172 | `					return rc;` |
|         - | 4173 | `				}` |
|        77 | 4174 | `			}` |
|         6 | 4175 | `		}else{` |
|         3 | 4176 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4177 | `				return rc;` |
|         - | 4178 | `			}` |
|         - | 4179 | `		}` |
|        15 | 4180 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4181 | `		return PH7_OK;` |
|         - | 4182 | `	}` |
|        54 | 4183 | `handle_numeric_inputs:` |
|       135 | 4184 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4185 | `		/* Float range */` |
|         - | 4186 | `		double elem,calc;` |
|        25 | 4187 | `		if( start_double > end_double ){` |
|         - | 4188 | `			/* Decreasing float range */` |
|         7 | 4189 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4190 | `				goto boundary_error;` |
|         - | 4191 | `			}` |
|         7 | 4192 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4193 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4194 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4195 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4196 | `			}` |
|         5 | 4197 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4198 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4199 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4200 | `					return rc;` |
|         - | 4201 | `				}` |
|         8 | 4202 | `			}` |
|        21 | 4203 | `		}else if( end_double > start_double ){` |
|         - | 4204 | `			/* Increasing float range */` |
|        17 | 4205 | `			if( is_step_negative ){` |
|       ! 0 | 4206 | `				goto negative_step_error;` |
|         - | 4207 | `			}` |
|        17 | 4208 | `			if( end_double - start_double < step_double ){` |
|         3 | 4209 | `				goto boundary_error;` |
|         - | 4210 | `			}` |
|        15 | 4211 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4212 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4213 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4214 | `			}` |
|        11 | 4215 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4216 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4217 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4218 | `					return rc;` |
|         - | 4219 | `				}` |
|        28 | 4220 | `			}` |
|         6 | 4221 | `		}else{` |
|         3 | 4222 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4223 | `				return rc;` |
|         - | 4224 | `			}` |
|         - | 4225 | `		}` |
|         9 | 4226 | `	}else{` |
|         - | 4227 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4228 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4229 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       103 | 4230 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4231 | `		sxu64 calc;` |
|       103 | 4232 | `		if( start_long > end_long ){` |
|         - | 4233 | `			/* Decreasing int range */` |
|        19 | 4234 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4235 | `				goto boundary_error;` |
|         - | 4236 | `			}` |
|        17 | 4237 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4238 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4239 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4240 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4241 | `			}` |
|        15 | 4242 | `			size = (sxu32)(calc + 1);` |
|       101 | 4243 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4244 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4245 | `					return rc;` |
|         - | 4246 | `				}` |
|        44 | 4247 | `			}` |
|        92 | 4248 | `		}else if( end_long > start_long ){` |
|         - | 4249 | `			/* Increasing int range */` |
|        79 | 4250 | `			if( is_step_negative ){` |
|         3 | 4251 | `				goto negative_step_error;` |
|         - | 4252 | `			}` |
|        77 | 4253 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4254 | `				goto boundary_error;` |
|         - | 4255 | `			}` |
|        75 | 4256 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        75 | 4257 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4258 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4259 | `			}` |
|        71 | 4260 | `			size = (sxu32)(calc + 1);` |
|    401659 | 4261 | `			for( i = 0 ; i < size ; ++i ){` |
|    401589 | 4262 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4263 | `					return rc;` |
|         - | 4264 | `				}` |
|    200795 | 4265 | `			}` |
|        36 | 4266 | `		}else{` |
|         7 | 4267 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4268 | `				return rc;` |
|         - | 4269 | `			}` |
|         - | 4270 | `		}` |
|         - | 4271 | `	}` |
|         - | 4272 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4273 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       107 | 4274 | `	ph7_result_value(pCtx,pArray);` |
|       107 | 4275 | `	return PH7_OK;` |
|         2 | 4276 | `negative_step_error:` |
|         5 | 4277 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4278 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4279 | `boundary_error:` |
|         9 | 4280 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4281 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        85 | 4282 | `}` |
|         - | 4283 | `/*` |
|         - | 4284 | ` * array array_values(array $array)` |
|         - | 4285 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4286 | ` * Parameters` |
|         - | 4287 | ` *  $array` |
|         - | 4288 | ` *   The input array.` |
|         - | 4289 | ` * Return` |
|         - | 4290 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4291 | ` */` |
|        48 | 4292 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4293 | `{` |
|         - | 4294 | `	ph7_hashmap_node *pNode;` |
|         - | 4295 | `	ph7_hashmap *pMap;` |
|         - | 4296 | `	ph7_value *pArray;` |
|         - | 4297 | `	ph7_value *pObj;` |
|         - | 4298 | `	sxu32 n;` |
|        51 | 4299 | `	if( nArg != 1 ){` |
|         - | 4300 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4301 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4302 | `			"ArgumentCountError",` |
|         - | 4303 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4304 | `			nArg` |
|         - | 4305 | `			);` |
|         - | 4306 | `	}` |
|         - | 4307 | `	/* Make sure we are dealing with a valid hashmap */` |
|        48 | 4308 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4309 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4310 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4311 | `			"TypeError",` |
|         - | 4312 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4313 | `			ph7_type_name(apArg[0])` |
|         - | 4314 | `			);` |
|         - | 4315 | `	}` |
|         - | 4316 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4317 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4318 | `	/* Create a new array */` |
|        46 | 4319 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4320 | `	if( pArray == 0 ){` |
|       ! 0 | 4321 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4322 | `		return PH7_OK;` |
|         - | 4323 | `	}` |
|         - | 4324 | `	/* Perform the requested operation */` |
|        46 | 4325 | `	pNode = pMap->pFirst;` |
|       144 | 4326 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4327 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4328 | `		if( pObj ){` |
|         - | 4329 | `			/* perform the insertion */` |
|       100 | 4330 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4331 | `		}` |
|         - | 4332 | `		/* Point to the next entry */` |
|       100 | 4333 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4334 | `	}` |
|         - | 4335 | `	/* return the new array */` |
|        46 | 4336 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4337 | `	return PH7_OK;` |
|        27 | 4338 | `}` |
|         - | 4339 | `/*` |
|         - | 4340 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4341 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4342 | ` * Parameters` |
|         - | 4343 | ` *  $input` |
|         - | 4344 | ` *   An array containing keys to return.` |
|         - | 4345 | ` * $search_value` |
|         - | 4346 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4347 | ` * $strict` |
|         - | 4348 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4349 | ` * Return` |
|         - | 4350 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4351 | ` */` |
|       172 | 4352 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4353 | `{` |
|         - | 4354 | `	ph7_hashmap_node *pNode;` |
|         - | 4355 | `	ph7_hashmap *pMap;` |
|         - | 4356 | `	ph7_value *pArray;` |
|         - | 4357 | `	ph7_value sObj;` |
|         - | 4358 | `	ph7_value sVal;` |
|         - | 4359 | `	SyString sKey;` |
|         - | 4360 | `	int bStrict;` |
|         - | 4361 | `	sxi32 rc;` |
|         - | 4362 | `	sxu32 n;` |
|       176 | 4363 | `	if( nArg < 1 ){` |
|         - | 4364 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4365 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4366 | `			"ArgumentCountError",` |
|         - | 4367 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4368 | `			);` |
|         - | 4369 | `	}` |
|         - | 4370 | `	/* Make sure we are dealing with a valid hashmap */` |
|       176 | 4371 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4372 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4373 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4374 | `			"TypeError",` |
|         - | 4375 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4376 | `			ph7_type_name(apArg[0])` |
|         - | 4377 | `			);` |
|         - | 4378 | `	}` |
|         - | 4379 | `	/* Point to the internal representation of the input hashmap */` |
|       173 | 4380 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4381 | `	/* Create a new array */` |
|       173 | 4382 | `	pArray = ph7_context_new_array(pCtx);` |
|       173 | 4383 | `	if( pArray == 0 ){` |
|       ! 0 | 4384 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4385 | `		return PH7_OK;` |
|         - | 4386 | `	}` |
|       173 | 4387 | `	bStrict = FALSE;` |
|       173 | 4388 | `	if( nArg > 2 ){` |
|         - | 4389 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4390 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4391 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4392 | `				"TypeError",` |
|         - | 4393 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4394 | `				ph7_type_name(apArg[2])` |
|         - | 4395 | `				);` |
|         - | 4396 | `		}` |
|         9 | 4397 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4398 | `	}` |
|         - | 4399 | `	/* Perform the requested operation */` |
|       173 | 4400 | `	pNode = pMap->pFirst;` |
|       173 | 4401 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1523 | 4402 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1353 | 4403 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       199 | 4404 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|       101 | 4405 | `		}else{` |
|      1156 | 4406 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1156 | 4407 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4408 | `		}` |
|      1353 | 4409 | `		rc = 0;` |
|      1353 | 4410 | `		if( nArg > 1 ){` |
|        65 | 4411 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4412 | `			if( pValue ){` |
|         - | 4413 | `				ph7_value sNeedle;` |
|        65 | 4414 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4415 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4416 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4417 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4418 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4419 | `				 * corrupt every later comparison. */` |
|        65 | 4420 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4421 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4422 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4423 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4424 | `			}` |
|        32 | 4425 | `		}` |
|      1353 | 4426 | `		if( rc == 0 ){` |
|         - | 4427 | `			/* Perform the insertion */` |
|      1321 | 4428 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       659 | 4429 | `		}` |
|      1353 | 4430 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4431 | `		/* Point to the next entry */` |
|      1353 | 4432 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       678 | 4433 | `	}` |
|         - | 4434 | `	/* return the new array */` |
|       173 | 4435 | `	ph7_result_value(pCtx,pArray);` |
|       173 | 4436 | `	return PH7_OK;` |
|        90 | 4437 | `}` |
|         - | 4438 | `/*` |
|         - | 4439 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4440 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4441 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4442 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4443 | ` * Parameters` |
|         - | 4444 | ` *  $arr1` |
|         - | 4445 | ` *   First array` |
|         - | 4446 | ` *  $arr2` |
|         - | 4447 | ` *   Second array` |
|         - | 4448 | ` * Return` |
|         - | 4449 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4450 | ` * Note` |
|         - | 4451 | ` *  This function is a symisc eXtension.` |
|         - | 4452 | ` */` |
|         4 | 4453 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4454 | `{` |
|         - | 4455 | `	ph7_hashmap *p1,*p2;` |
|         - | 4456 | `	int rc;` |
|         5 | 4457 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4458 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4459 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4460 | `		return PH7_OK;` |
|         - | 4461 | `	}` |
|         - | 4462 | `	/* Point to the hashmaps */` |
|         5 | 4463 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4464 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4465 | `	rc = (p1 == p2);` |
|         - | 4466 | `	/* Same instance? */` |
|         5 | 4467 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4468 | `	return PH7_OK;` |
|         3 | 4469 | `}` |
|         - | 4470 | `/*` |
|         - | 4471 | ` * array array_merge(array ...$arrays)` |
|         - | 4472 | ` *  Merge one or more arrays.` |
|         - | 4473 | ` * Parameters` |
|         - | 4474 | ` *  ...$arrays` |
|         - | 4475 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4476 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4477 | ` * Return` |
|         - | 4478 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4479 | ` *  with no arguments.` |
|         - | 4480 | ` */` |
|      1126 | 4481 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4482 | `{` |
|         - | 4483 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4484 | `	ph7_value *pArray;` |
|         - | 4485 | `	int i;` |
|         - | 4486 | `	/* Create a new array */` |
|      1131 | 4487 | `	pArray = ph7_context_new_array(pCtx);` |
|      1131 | 4488 | `	if( pArray == 0 ){` |
|       ! 0 | 4489 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4490 | `		return PH7_OK;` |
|         - | 4491 | `	}` |
|         - | 4492 | `	/* Point to the internal representation of the hashmap */` |
|      1131 | 4493 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4494 | `	/* Start merging */` |
|      3359 | 4495 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4496 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2237 | 4497 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4498 | `			/* Type mismatch -> TypeError */` |
|         8 | 4499 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4500 | `				"TypeError",` |
|         - | 4501 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4502 | `				i + 1,` |
|         4 | 4503 | `				ph7_type_name(apArg[i])` |
|         - | 4504 | `				);` |
|       ! 0 | 4505 | `		}else{` |
|      2233 | 4506 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4507 | `			/* Merge the two hashmaps */` |
|      2233 | 4508 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4509 | `		}` |
|      1119 | 4510 | `	}` |
|         - | 4511 | `	/* Return the freshly created array */` |
|      1127 | 4512 | `	ph7_result_value(pCtx,pArray);` |
|      1127 | 4513 | `	return PH7_OK;` |
|       568 | 4514 | `}` |
|         - | 4515 | `/*` |
|         - | 4516 | ` * array array_copy(array $source)` |
|         - | 4517 | ` *  Make a blind copy of the target array.` |
|         - | 4518 | ` * Parameters` |
|         - | 4519 | ` *  $source` |
|         - | 4520 | ` *   Target array` |
|         - | 4521 | ` * Return` |
|         - | 4522 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4523 | ` * Note` |
|         - | 4524 | ` *  This function is a symisc eXtension.` |
|         - | 4525 | ` */` |
|        18 | 4526 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4527 | `{` |
|         - | 4528 | `	ph7_hashmap *pMap;` |
|         - | 4529 | `	ph7_value *pArray;` |
|        19 | 4530 | `	if( nArg < 1 ){` |
|         - | 4531 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4532 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4533 | `		return PH7_OK;` |
|         - | 4534 | `	}` |
|         - | 4535 | `	/* Create a new array */` |
|        19 | 4536 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4537 | `	if( pArray == 0 ){` |
|       ! 0 | 4538 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4539 | `		return PH7_OK;` |
|         - | 4540 | `	}` |
|         - | 4541 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4542 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4543 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4544 | `		/* Point to the internal representation of the source */` |
|        19 | 4545 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4546 | `		/* Perform the copy */` |
|        19 | 4547 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4548 | `	}else{` |
|         - | 4549 | `		/* Simple insertion */` |
|       ! 0 | 4550 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4551 | `	}` |
|         - | 4552 | `	/* Return the duplicated array */` |
|        19 | 4553 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4554 | `	return PH7_OK;` |
|        10 | 4555 | `}` |
|         - | 4556 | `/*` |
|         - | 4557 | ` * bool array_erase(array $source)` |
|         - | 4558 | ` *  Remove all elements from a given array.` |
|         - | 4559 | ` * Parameters` |
|         - | 4560 | ` *  $source` |
|         - | 4561 | ` *   Target array` |
|         - | 4562 | ` * Return` |
|         - | 4563 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4564 | ` * Note` |
|         - | 4565 | ` *  This function is a symisc eXtension.` |
|         - | 4566 | ` */` |
|        26 | 4567 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4568 | `{` |
|         - | 4569 | `	ph7_hashmap *pMap;` |
|        28 | 4570 | `	if( nArg < 1 ){` |
|         - | 4571 | `		/* Missing arguments */` |
|       ! 0 | 4572 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4573 | `		return PH7_OK;` |
|         - | 4574 | `	}` |
|         - | 4575 | `	/* Point to the target hashmap */` |
|        28 | 4576 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4577 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4578 | `	/* Erase */` |
|        28 | 4579 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4580 | `	return PH7_OK;` |
|        15 | 4581 | `}` |
|         - | 4582 | `/*` |
|         - | 4583 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4584 | ` *  Extract a slice of the array.` |
|         - | 4585 | ` * Parameters` |
|         - | 4586 | ` *  $array` |
|         - | 4587 | ` *    The input array.` |
|         - | 4588 | ` * $offset` |
|         - | 4589 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4590 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4591 | ` * $length (optional, nullable)` |
|         - | 4592 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4593 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4594 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4595 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4596 | ` * $preserve_keys (optional)` |
|         - | 4597 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4598 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4599 | ` * Return` |
|         - | 4600 | ` *   The new slice.` |
|         - | 4601 | ` */` |
|        66 | 4602 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4603 | `{` |
|         - | 4604 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4605 | `	ph7_hashmap_node *pCur;` |
|         - | 4606 | `	ph7_value *pArray;` |
|         - | 4607 | `	int iLength,iOfft;` |
|         - | 4608 | `	int bPreserve;` |
|         - | 4609 | `	sxi32 rc;` |
|        71 | 4610 | `	if( nArg < 2 ){` |
|       ! 0 | 4611 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4612 | `			"ArgumentCountError",` |
|         - | 4613 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4614 | `			nArg` |
|         - | 4615 | `			);` |
|         - | 4616 | `	}` |
|        71 | 4617 | `	if( nArg > 4 ){` |
|         4 | 4618 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4619 | `			"ArgumentCountError",` |
|         - | 4620 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4621 | `			nArg` |
|         - | 4622 | `			);` |
|         - | 4623 | `	}` |
|        69 | 4624 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4625 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4626 | `			"TypeError",` |
|         - | 4627 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4628 | `			ph7_type_name(apArg[0])` |
|         - | 4629 | `			);` |
|         - | 4630 | `	}` |
|         - | 4631 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        92 | 4632 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        95 | 4633 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4634 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4635 | `			"TypeError",` |
|         - | 4636 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4637 | `			ph7_type_name(apArg[1])` |
|         - | 4638 | `			);` |
|         - | 4639 | `	}` |
|         - | 4640 | `	/* Validate $length type if provided: nullable int */` |
|        65 | 4641 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        56 | 4642 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        56 | 4643 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4644 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4645 | `				"TypeError",` |
|         - | 4646 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4647 | `				ph7_type_name(apArg[2])` |
|         - | 4648 | `				);` |
|         - | 4649 | `		}` |
|        18 | 4650 | `	}` |
|         - | 4651 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        63 | 4652 | `	if( nArg > 3 ){` |
|         7 | 4653 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4654 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4655 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4656 | `				"TypeError",` |
|         - | 4657 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4658 | `				ph7_type_name(apArg[3])` |
|         - | 4659 | `				);` |
|         - | 4660 | `		}` |
|         2 | 4661 | `	}` |
|         - | 4662 | `	/* Point the internal representation of the target array */` |
|        63 | 4663 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        63 | 4664 | `	bPreserve = FALSE;` |
|         - | 4665 | `	/* Get the offset */` |
|         - | 4666 | `	{` |
|        63 | 4667 | `		sxi64 iTmp = 0;` |
|        63 | 4668 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        63 | 4669 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4670 | `			return rcArg;` |
|         - | 4671 | `		}` |
|        63 | 4672 | `		iOfft = (int)iTmp;` |
|         - | 4673 | `	}` |
|        63 | 4674 | `	if( iOfft < 0 ){` |
|         5 | 4675 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4676 | `		if( iOfft < 0 ){` |
|         3 | 4677 | `			iOfft = 0;` |
|         1 | 4678 | `		}` |
|         2 | 4679 | `	}` |
|        63 | 4680 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4681 | `		/* Offset past end of array, return empty array */` |
|         5 | 4682 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4683 | `		if( pArray == 0 ){` |
|       ! 0 | 4684 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4685 | `			return PH7_OK;` |
|         - | 4686 | `		}` |
|         5 | 4687 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4688 | `		return PH7_OK;` |
|         - | 4689 | `	}` |
|         - | 4690 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        59 | 4691 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        59 | 4692 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        37 | 4693 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        37 | 4694 | `		if( iLength < 0 ){` |
|         5 | 4695 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4696 | `		}` |
|        37 | 4697 | `		if( iLength < 0 ){` |
|         3 | 4698 | `			iLength = 0;` |
|         1 | 4699 | `		}` |
|        37 | 4700 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4701 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4702 | `		}` |
|        18 | 4703 | `	}` |
|        59 | 4704 | `	if( nArg > 3 ){` |
|         5 | 4705 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4706 | `	}` |
|         - | 4707 | `	/* Create a new array */` |
|        59 | 4708 | `	pArray = ph7_context_new_array(pCtx);` |
|        59 | 4709 | `	if( pArray == 0 ){` |
|       ! 0 | 4710 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4711 | `		return PH7_OK;` |
|         - | 4712 | `	}` |
|        59 | 4713 | `	if( iLength < 1 ){` |
|         - | 4714 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4715 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4716 | `		return PH7_OK;` |
|         - | 4717 | `	}` |
|         - | 4718 | `	/* Point to the desired entry */` |
|        55 | 4719 | `	pCur = pSrc->pFirst;` |
|        54 | 4720 | `	for(;;){` |
|       113 | 4721 | `		if( iOfft < 1 ){` |
|        55 | 4722 | `			break;` |
|         - | 4723 | `		}` |
|         - | 4724 | `		/* Point to the next entry */` |
|        63 | 4725 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        63 | 4726 | `		iOfft--;` |
|         5 | 4727 | `	}` |
|         - | 4728 | `	/* Point to the internal representation of the hashmap */` |
|        55 | 4729 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       106 | 4730 | `	for(;;){` |
|       217 | 4731 | `		if( iLength < 1 ){` |
|        55 | 4732 | `			break;` |
|         - | 4733 | `		}` |
|         - | 4734 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4735 | `		{` |
|       167 | 4736 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|       167 | 4737 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4738 | `		}` |
|       167 | 4739 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4740 | `			break;` |
|         - | 4741 | `		}` |
|         - | 4742 | `		/* Point to the next entry */` |
|       167 | 4743 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       167 | 4744 | `		iLength--;` |
|         5 | 4745 | `	}` |
|         - | 4746 | `	/* Return the freshly created array */` |
|        55 | 4747 | `	ph7_result_value(pCtx,pArray);` |
|        55 | 4748 | `	return PH7_OK;` |
|        38 | 4749 | `}` |
|         - | 4750 | `/*` |
|         - | 4751 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4752 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4753 | ` * beginning (becomes the new pFirst).` |
|         - | 4754 | ` */` |
|        38 | 4755 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4756 | `{` |
|         - | 4757 | `	ph7_hashmap_node *pNode;` |
|         - | 4758 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4759 | `	pNode = pMap->pLast;` |
|        39 | 4760 | `	if( pNode == 0 ){` |
|       ! 0 | 4761 | `		return;` |
|         - | 4762 | `	}` |
|        39 | 4763 | `	if( pNode->pNext == 0 ){` |
|         - | 4764 | `		/* Only node in the list, nothing to move */` |
|         5 | 4765 | `		return;` |
|         - | 4766 | `	}` |
|        35 | 4767 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4768 | `		/* Already in the correct position */` |
|         9 | 4769 | `		return;` |
|         - | 4770 | `	}` |
|         - | 4771 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4772 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4773 | `	pMap->pLast->pPrev = 0;` |
|         - | 4774 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4775 | `	if( pAfter == 0 ){` |
|         - | 4776 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4777 | `		pNode->pNext = 0;` |
|         3 | 4778 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4779 | `		if( pMap->pFirst ){` |
|         3 | 4780 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4781 | `		}` |
|         3 | 4782 | `		pMap->pFirst = pNode;` |
|         2 | 4783 | `	}else{` |
|        25 | 4784 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4785 | `		pNode->pPrev = pOldNext;` |
|        25 | 4786 | `		pNode->pNext = pAfter;` |
|        25 | 4787 | `		pAfter->pPrev = pNode;` |
|        25 | 4788 | `		if( pOldNext ){` |
|        25 | 4789 | `			pOldNext->pNext = pNode;` |
|        13 | 4790 | `		}else{` |
|       ! 0 | 4791 | `			pMap->pLast = pNode;` |
|         - | 4792 | `		}` |
|         - | 4793 | `	}` |
|        20 | 4794 | `}` |
|         - | 4795 | `/*` |
|         - | 4796 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4797 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4798 | ` * Parameters` |
|         - | 4799 | ` *  $array` |
|         - | 4800 | ` *    The input array.` |
|         - | 4801 | ` *  $offset` |
|         - | 4802 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4803 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4804 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4805 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4806 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4807 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4808 | ` *  $length (optional)` |
|         - | 4809 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4810 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4811 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4812 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4813 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4814 | ` *  $replacement (optional)` |
|         - | 4815 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4816 | ` *    with elements from this array.` |
|         - | 4817 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4818 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4819 | ` *    offset.` |
|         - | 4820 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4821 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4822 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4823 | ` * Return` |
|         - | 4824 | ` *   A new array consisting of the extracted elements.` |
|         - | 4825 | ` */` |
|        64 | 4826 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4827 | `{` |
|         - | 4828 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4829 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4830 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4831 | `	int iLength,iOfft,i;` |
|         - | 4832 | `	sxi32 rc;` |
|        66 | 4833 | `	if( nArg < 2 ){` |
|       ! 0 | 4834 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4835 | `			"ArgumentCountError",` |
|         - | 4836 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4837 | `			nArg` |
|         - | 4838 | `			);` |
|         - | 4839 | `	}` |
|        66 | 4840 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4841 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4842 | `			"TypeError",` |
|         - | 4843 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4844 | `			ph7_type_name(apArg[0])` |
|         - | 4845 | `			);` |
|         - | 4846 | `	}` |
|         - | 4847 | `	/* Point to the internal representation of the target array */` |
|        63 | 4848 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4849 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4850 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4851 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4852 | `	if( iOfft < 0 ){` |
|         9 | 4853 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4854 | `		if( iOfft < 0 ){` |
|         3 | 4855 | `			iOfft = 0;` |
|         2 | 4856 | `		}` |
|        59 | 4857 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4858 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4859 | `	}` |
|         - | 4860 | `	/* Get the length and clamp to valid range.` |
|         - | 4861 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4862 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4863 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4864 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4865 | `		if( iLength < 0 ){` |
|         7 | 4866 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4867 | `			if( iLength < 0 ){` |
|         3 | 4868 | `				iLength = 0;` |
|         1 | 4869 | `			}` |
|         3 | 4870 | `		}` |
|        45 | 4871 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4872 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4873 | `		}` |
|        22 | 4874 | `	}` |
|         - | 4875 | `	/* Create the result array for removed elements */` |
|        63 | 4876 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4877 | `	if( pArray == 0 ){` |
|       ! 0 | 4878 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4879 | `		return PH7_OK;` |
|         - | 4880 | `	}` |
|         - | 4881 | `	/* Get replacement array if provided */` |
|        63 | 4882 | `	pRep = 0;` |
|        63 | 4883 | `	if( nArg > 3 ){` |
|        27 | 4884 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4885 | `			/* Perform an array cast */` |
|         3 | 4886 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4887 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4888 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4889 | `			}` |
|         2 | 4890 | `		}else{` |
|        25 | 4891 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4892 | `		}` |
|        27 | 4893 | `		if( pRep ){` |
|         - | 4894 | `			/* Reset the loop cursor */` |
|        27 | 4895 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4896 | `		}` |
|        13 | 4897 | `	}` |
|         - | 4898 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4899 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4900 | `	/* Navigate to the offset position */` |
|        63 | 4901 | `	pCur = pSrc->pFirst;` |
|       131 | 4902 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4903 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4904 | `	}` |
|         - | 4905 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4906 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4907 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4908 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4909 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4910 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4911 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4912 | `		pPrev = pCur->pPrev;` |
|        79 | 4913 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4914 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4915 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4916 | `			break;` |
|         - | 4917 | `		}` |
|        79 | 4918 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4919 | `	}` |
|         - | 4920 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4921 | `	if( pRep ){` |
|         - | 4922 | `		ph7_value sSafeVal;` |
|        78 | 4923 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4924 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4925 | `			if( pRvalue ){` |
|         - | 4926 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4927 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4928 | `				 * since it points into that same pool. */` |
|        39 | 4929 | `				sSafeVal = *pRvalue;` |
|        39 | 4930 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4931 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4932 | `					pNewNode = pSrc->pLast;` |
|        39 | 4933 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4934 | `					pInsertAfter = pNewNode;` |
|        19 | 4935 | `				}` |
|        19 | 4936 | `			}` |
|         1 | 4937 | `		}` |
|        13 | 4938 | `	}` |
|         - | 4939 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4940 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4941 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4942 | `	 * and removals left gaps. */` |
|         - | 4943 | `	{` |
|        63 | 4944 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4945 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4946 | `		pSrc->iNextIdx = 0;` |
|       233 | 4947 | `		while( n > 0 ){` |
|       171 | 4948 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4949 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4950 | `			}` |
|       171 | 4951 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4952 | `			n--;` |
|         1 | 4953 | `		}` |
|        63 | 4954 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4955 | `	}` |
|         - | 4956 | `	/* Return the freshly created array */` |
|        63 | 4957 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4958 | `	return PH7_OK;` |
|        34 | 4959 | `}` |
|         - | 4960 | `/*` |
|         - | 4961 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4962 | ` *  Checks if a value exists in an array.` |
|         - | 4963 | ` * Parameters` |
|         - | 4964 | ` *  $needle` |
|         - | 4965 | ` *   The searched value.` |
|         - | 4966 | ` *   Note:` |
|         - | 4967 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4968 | ` * $haystack` |
|         - | 4969 | ` *  The target array.` |
|         - | 4970 | ` * $strict` |
|         - | 4971 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4972 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4973 | ` */` |
|     33742 | 4974 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4975 | `{` |
|         - | 4976 | `	ph7_value *pNeedle;` |
|         - | 4977 | `	int bStrict;` |
|         - | 4978 | `	int rc;` |
|     33747 | 4979 | `	if( nArg < 2 ){` |
|         - | 4980 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4981 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4982 | `		return PH7_OK;` |
|         - | 4983 | `	}` |
|     33747 | 4984 | `	pNeedle = apArg[0];` |
|     33747 | 4985 | `	bStrict = 0;` |
|     33747 | 4986 | `	if( nArg > 2 ){` |
|        58 | 4987 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        28 | 4988 | `	}` |
|     33747 | 4989 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4990 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4991 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4992 | `		/* Set the comparison result */` |
|       ! 0 | 4993 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4994 | `		return PH7_OK;` |
|         - | 4995 | `	}` |
|         - | 4996 | `	/* Perform the lookup */` |
|     33747 | 4997 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4998 | `	/* Lookup result */` |
|     33747 | 4999 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33747 | 5000 | `	return PH7_OK;` |
|     16876 | 5001 | `}` |
|         - | 5002 | `/*` |
|         - | 5003 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 5004 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 5005 | ` * Parameters` |
|         - | 5006 | ` * $needle` |
|         - | 5007 | ` *   The searched value.` |
|         - | 5008 | ` * $haystack` |
|         - | 5009 | ` *   The array.` |
|         - | 5010 | ` * $strict` |
|         - | 5011 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 5012 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 5013 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 5014 | ` * Return` |
|         - | 5015 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 5016 | ` */` |
|        28 | 5017 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 5018 | `{` |
|         - | 5019 | `	ph7_hashmap_node *pEntry;` |
|         - | 5020 | `	ph7_value *pVal,sNeedle;` |
|         - | 5021 | `	ph7_hashmap *pMap;` |
|         - | 5022 | `	ph7_value sVal;` |
|         - | 5023 | `	int bStrict;` |
|         - | 5024 | `	sxu32 n;` |
|         - | 5025 | `	int rc;` |
|        30 | 5026 | `	if( nArg < 2 ){` |
|         - | 5027 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 5028 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5029 | `			"ArgumentCountError",` |
|         - | 5030 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 5031 | `			nArg` |
|         - | 5032 | `			);` |
|         - | 5033 | `	}` |
|        30 | 5034 | `	bStrict = FALSE;` |
|        30 | 5035 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 5036 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 5037 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5038 | `			"TypeError",` |
|         - | 5039 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 5040 | `			ph7_type_name(apArg[1])` |
|         - | 5041 | `			);` |
|         - | 5042 | `	}` |
|        27 | 5043 | `	if( nArg > 2 ){` |
|         - | 5044 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        13 | 5045 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 5046 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5047 | `				"TypeError",` |
|         - | 5048 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 5049 | `				ph7_type_name(apArg[2])` |
|         - | 5050 | `				);` |
|         - | 5051 | `		}` |
|        13 | 5052 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         6 | 5053 | `	}` |
|         - | 5054 | `	/* Point to the internal representation of the internal hashmap */` |
|        27 | 5055 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 5056 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        27 | 5057 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        27 | 5058 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        27 | 5059 | `	pEntry = pMap->pFirst;` |
|        27 | 5060 | `	n = pMap->nEntry;` |
|        29 | 5061 | `	for(;;){` |
|        59 | 5062 | `		if( !n ){` |
|         9 | 5063 | `			break;` |
|         - | 5064 | `		}` |
|         - | 5065 | `		/* Extract node value */` |
|        51 | 5066 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        51 | 5067 | `		if( pVal ){` |
|         - | 5068 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 5069 | `			 * can change their type.` |
|         - | 5070 | `			 */` |
|        51 | 5071 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        51 | 5072 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        51 | 5073 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        51 | 5074 | `			PH7_MemObjRelease(&sVal);` |
|        51 | 5075 | `			PH7_MemObjRelease(&sNeedle);` |
|        51 | 5076 | `			if( rc == 0 ){` |
|         - | 5077 | `				/* Match found,return key */` |
|        19 | 5078 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 5079 | `					/* INT key */` |
|        13 | 5080 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         7 | 5081 | `				}else{` |
|         7 | 5082 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5083 | `					/* Blob key */` |
|         7 | 5084 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 5085 | `				}` |
|        19 | 5086 | `				return PH7_OK;` |
|         - | 5087 | `			}` |
|        16 | 5088 | `		}` |
|         - | 5089 | `		/* Point to the next entry */` |
|        33 | 5090 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5091 | `		n--;` |
|         1 | 5092 | `	}` |
|         - | 5093 | `	/* No such value,return FALSE */` |
|         9 | 5094 | `	ph7_result_bool(pCtx,0);` |
|         9 | 5095 | `	return PH7_OK;` |
|        16 | 5096 | `}` |
|         - | 5097 | `/*` |
|         - | 5098 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 5099 | ` *  Computes the difference of arrays.` |
|         - | 5100 | ` * Parameters` |
|         - | 5101 | ` *  $array1` |
|         - | 5102 | ` *    The array to compare from` |
|         - | 5103 | ` *  $array2` |
|         - | 5104 | ` *    An array to compare against` |
|         - | 5105 | ` *  $...` |
|         - | 5106 | ` *   More arrays to compare against` |
|         - | 5107 | ` * Return` |
|         - | 5108 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5109 | ` *  are not present in any of the other arrays.` |
|         - | 5110 | ` */` |
|        20 | 5111 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5112 | `{` |
|         - | 5113 | `	ph7_hashmap_node *pEntry;` |
|         - | 5114 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5115 | `	ph7_value *pArray;` |
|         - | 5116 | `	ph7_value *pVal;` |
|         - | 5117 | `	sxi32 rc;` |
|         - | 5118 | `	sxu32 n;` |
|         - | 5119 | `	int i;` |
|         - | 5120 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5121 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5122 | `	 * debugging difficult. */` |
|        23 | 5123 | `	if( nArg < 1 ){` |
|       ! 0 | 5124 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5125 | `			"ArgumentCountError",` |
|         - | 5126 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5127 | `			nArg` |
|         - | 5128 | `			);` |
|         - | 5129 | `	}` |
|        23 | 5130 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5131 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5132 | `			"TypeError",` |
|         - | 5133 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5134 | `			ph7_type_name(apArg[0])` |
|         - | 5135 | `			);` |
|         - | 5136 | `	}` |
|        36 | 5137 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5138 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5139 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5140 | `				"TypeError",` |
|         - | 5141 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5142 | `				i + 1,` |
|         2 | 5143 | `				ph7_type_name(apArg[i])` |
|         - | 5144 | `				);` |
|         - | 5145 | `		}` |
|         9 | 5146 | `	}` |
|        17 | 5147 | `	if( nArg == 1 ){` |
|         - | 5148 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5149 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5150 | `		return PH7_OK;` |
|         - | 5151 | `	}` |
|         - | 5152 | `	/* Create a new array */` |
|        15 | 5153 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5154 | `	if( pArray == 0 ){` |
|       ! 0 | 5155 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5156 | `		return PH7_OK;` |
|         - | 5157 | `	}` |
|         - | 5158 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5159 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5160 | `	/* Perform the diff */` |
|        15 | 5161 | `	pEntry = pSrc->pFirst;` |
|        15 | 5162 | `	n = pSrc->nEntry;` |
|        27 | 5163 | `	for(;;){` |
|        55 | 5164 | `		if( n < 1 ){` |
|        15 | 5165 | `			break;` |
|         - | 5166 | `		}` |
|         - | 5167 | `		/* Extract the node value */` |
|        41 | 5168 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5169 | `		if( pVal ){` |
|        69 | 5170 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5171 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5172 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5173 | `				/* Perform the lookup */` |
|        45 | 5174 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5175 | `				if( rc == SXRET_OK ){` |
|         - | 5176 | `					/* Value exist */` |
|        17 | 5177 | `					break;` |
|         - | 5178 | `				}` |
|        15 | 5179 | `			}` |
|        41 | 5180 | `			if( i >= nArg ){` |
|         - | 5181 | `				/* Perform the insertion */` |
|        25 | 5182 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5183 | `			}` |
|        20 | 5184 | `		}` |
|         - | 5185 | `		/* Point to the next entry */` |
|        41 | 5186 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5187 | `		n--;` |
|         1 | 5188 | `	}` |
|         - | 5189 | `	/* Return the freshly created array */` |
|        15 | 5190 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5191 | `	return PH7_OK;` |
|        13 | 5192 | `}` |
|         - | 5193 | `/*` |
|         - | 5194 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5195 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5196 | ` * Parameters` |
|         - | 5197 | ` *  $array1` |
|         - | 5198 | ` *    The array to compare from` |
|         - | 5199 | ` *  $array2` |
|         - | 5200 | ` *    An array to compare against` |
|         - | 5201 | ` *  $...` |
|         - | 5202 | ` *   More arrays to compare against.` |
|         - | 5203 | ` * $callback` |
|         - | 5204 | ` *  The callback comparison function.` |
|         - | 5205 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5206 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5207 | ` *  than the second.` |
|         - | 5208 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5209 | ` * Return` |
|         - | 5210 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5211 | ` *  are not present in any of the other arrays.` |
|         - | 5212 | ` */` |
|        20 | 5213 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5214 | `{` |
|         - | 5215 | `	ph7_hashmap_node *pEntry;` |
|         - | 5216 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5217 | `	ph7_value *pCallback;` |
|         - | 5218 | `	ph7_value *pArray;` |
|         - | 5219 | `	ph7_value *pVal;` |
|         - | 5220 | `	sxi32 rc;` |
|         - | 5221 | `	sxu32 n;` |
|         - | 5222 | `	int i;` |
|         - | 5223 |  |
|         - | 5224 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5225 | `	if( nArg < 2 ){` |
|       ! 0 | 5226 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5227 | `			"ArgumentCountError",` |
|         - | 5228 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5229 | `			nArg` |
|         - | 5230 | `			);` |
|         - | 5231 | `	}` |
|        25 | 5232 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5233 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5234 | `			"TypeError",` |
|         - | 5235 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5236 | `			ph7_type_name(apArg[0])` |
|         - | 5237 | `			);` |
|         - | 5238 | `	}` |
|         - | 5239 |  |
|        23 | 5240 | `	if( nArg == 2 ){` |
|         - | 5241 | `		/* Only the original array and the callback were provided. */` |
|         - | 5242 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5243 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5244 | `		 * validation order.` |
|         - | 5245 | `		 */` |
|         4 | 5246 | `	} else {` |
|         - | 5247 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5248 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5249 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5250 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5251 | `					"TypeError",` |
|         - | 5252 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5253 | `					i + 1,` |
|         6 | 5254 | `					ph7_type_name(apArg[i])` |
|         - | 5255 | `					);` |
|         - | 5256 | `			}` |
|         7 | 5257 | `		}` |
|         - | 5258 | `	}` |
|         - | 5259 |  |
|         - | 5260 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5261 | `	pCallback = apArg[nArg - 1];` |
|         - | 5262 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5263 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5264 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5265 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5266 | `				"TypeError",` |
|         - | 5267 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5268 | `				nArg` |
|         - | 5269 | `				);` |
|         - | 5270 | `		}` |
|         6 | 5271 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5272 | `			int len;` |
|         3 | 5273 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5274 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5275 | `				"TypeError",` |
|         - | 5276 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5277 | `				nArg,` |
|         1 | 5278 | `				zName` |
|         - | 5279 | `				);` |
|         - | 5280 | `		}` |
|         4 | 5281 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5282 | `			"TypeError",` |
|         - | 5283 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5284 | `			nArg` |
|         - | 5285 | `			);` |
|         - | 5286 | `	}` |
|         - | 5287 |  |
|         7 | 5288 | `	if( nArg == 2 ){` |
|         - | 5289 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5290 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5291 | `		return PH7_OK;` |
|         - | 5292 | `	}` |
|         - | 5293 |  |
|         - | 5294 | `	/* Create a new array */` |
|         5 | 5295 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5296 | `	if( pArray == 0 ){` |
|       ! 0 | 5297 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5298 | `		return PH7_OK;` |
|         - | 5299 | `	}` |
|         - | 5300 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5301 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5302 | `	/* Perform the diff */` |
|         5 | 5303 | `	pEntry = pSrc->pFirst;` |
|         5 | 5304 | `	n = pSrc->nEntry;` |
|         5 | 5305 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5306 | `	for(;;){` |
|        11 | 5307 | `		if( n < 1 ){` |
|         3 | 5308 | `			break;` |
|         - | 5309 | `		}` |
|         - | 5310 | `		/* Extract the node value */` |
|         9 | 5311 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5312 | `		if( pVal ){` |
|        15 | 5313 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5314 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5315 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5316 | `				/* Perform the lookup */` |
|         9 | 5317 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5318 | `				if( rc == SXRET_OK ){` |
|         - | 5319 | `					/* Value exist */` |
|         3 | 5320 | `					break;` |
|         - | 5321 | `				}` |
|         4 | 5322 | `			}` |
|         9 | 5323 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5324 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5325 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5326 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5327 | `				return PH7_EXCEPTION;` |
|         - | 5328 | `			}` |
|         7 | 5329 | `			if( i >= (nArg - 1)){` |
|         - | 5330 | `				/* Perform the insertion */` |
|         5 | 5331 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5332 | `			}` |
|         3 | 5333 | `		}` |
|         - | 5334 | `		/* Point to the next entry */` |
|         7 | 5335 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5336 | `		n--;` |
|         1 | 5337 | `	}` |
|         - | 5338 | `	/* Return the freshly created array */` |
|         3 | 5339 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5340 | `	return PH7_OK;` |
|        15 | 5341 | `}` |
|         - | 5342 | `/*` |
|         - | 5343 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5344 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5345 | ` * Parameters` |
|         - | 5346 | ` *  $array1` |
|         - | 5347 | ` *    The array to compare from` |
|         - | 5348 | ` *  $array2` |
|         - | 5349 | ` *    An array to compare against` |
|         - | 5350 | ` *  $...` |
|         - | 5351 | ` *   More arrays to compare against` |
|         - | 5352 | ` * Return` |
|         - | 5353 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5354 | ` *  are not present in any of the other arrays.` |
|         - | 5355 | ` */` |
|        20 | 5356 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5357 | `{` |
|         - | 5358 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5359 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5360 | `	ph7_value *pArray;` |
|         - | 5361 | `	ph7_value *pVal;` |
|         - | 5362 | `	sxi32 rc;` |
|         - | 5363 | `	sxu32 n;` |
|         - | 5364 | `	int i;` |
|         - | 5365 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5366 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5367 | `	 * accompanying integration tests to pass. */` |
|        24 | 5368 | `	if( nArg < 1 ){` |
|       ! 0 | 5369 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5370 | `			"ArgumentCountError",` |
|         - | 5371 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5372 | `			nArg` |
|         - | 5373 | `			);` |
|         - | 5374 | `	}` |
|        24 | 5375 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5376 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5377 | `			"TypeError",` |
|         - | 5378 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5379 | `			ph7_type_name(apArg[0])` |
|         - | 5380 | `			);` |
|         - | 5381 | `	}` |
|        37 | 5382 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5383 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5384 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5385 | `				"TypeError",` |
|         - | 5386 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5387 | `				i + 1,` |
|         4 | 5388 | `				ph7_type_name(apArg[i])` |
|         - | 5389 | `				);` |
|         - | 5390 | `		}` |
|        10 | 5391 | `	}` |
|        15 | 5392 | `	if( nArg == 1 ){` |
|         - | 5393 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5394 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5395 | `		return PH7_OK;` |
|         - | 5396 | `	}` |
|         - | 5397 | `	/* Create a new array */` |
|        13 | 5398 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5399 | `	if( pArray == 0 ){` |
|       ! 0 | 5400 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5401 | `		return PH7_OK;` |
|         - | 5402 | `	}` |
|         - | 5403 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5404 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5405 | `	/* Perform the diff */` |
|        13 | 5406 | `	pEntry = pSrc->pFirst;` |
|        13 | 5407 | `	n = pSrc->nEntry;` |
|        13 | 5408 | `	pN1 = pN2 = 0;` |
|        34 | 5409 | `	for(;;){` |
|         - | 5410 | `		int keep;` |
|        41 | 5411 | `		if( n < 1 ){` |
|        13 | 5412 | `			break;` |
|         - | 5413 | `		}` |
|         - | 5414 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5415 | `		keep = 1;` |
|        47 | 5416 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5417 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5418 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5419 | `			/* Perform a key lookup first */` |
|        33 | 5420 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5421 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5422 | `			}else{` |
|        21 | 5423 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5424 | `			}` |
|        33 | 5425 | `			if( rc != SXRET_OK ){` |
|         - | 5426 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5427 | `				continue;` |
|         - | 5428 | `			}` |
|         - | 5429 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5430 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5431 | `			if( pVal ){` |
|         - | 5432 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5433 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5434 | `				if( pVal2 ){` |
|         - | 5435 | `					ph7_value sV1,sV2;` |
|         - | 5436 | `					sxi32 cmp;` |
|         - | 5437 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5438 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5439 | `					 * null element used to come back bool(false) in the` |
|         - | 5440 | `					 * caller's array). */` |
|        17 | 5441 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5442 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5443 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5444 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5445 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5446 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5447 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5448 | `					if( cmp == 0 ){` |
|         - | 5449 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5450 | `						keep = 0;` |
|        15 | 5451 | `						break;` |
|         - | 5452 | `					}` |
|         1 | 5453 | `				}` |
|         1 | 5454 | `			}` |
|         2 | 5455 | `		}` |
|        29 | 5456 | `		if( keep ){` |
|         - | 5457 | `			/* Perform the insertion */` |
|        15 | 5458 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5459 | `		}` |
|         - | 5460 | `		/* Point to the next entry */` |
|        29 | 5461 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5462 | `		n--;` |
|         1 | 5463 | `	}` |
|         - | 5464 | `	/* Return the freshly created array */` |
|        13 | 5465 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5466 | `	return PH7_OK;` |
|        14 | 5467 | `}` |
|         - | 5468 | `/*` |
|         - | 5469 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5470 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5471 | ` *  by a user supplied callback function.` |
|         - | 5472 | ` * Parameters` |
|         - | 5473 | ` *  $array1` |
|         - | 5474 | ` *    The array to compare from` |
|         - | 5475 | ` *  $array2` |
|         - | 5476 | ` *    An array to compare against` |
|         - | 5477 | ` *  $...` |
|         - | 5478 | ` *   More arrays to compare against.` |
|         - | 5479 | ` *  $key_compare_func` |
|         - | 5480 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5481 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5482 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5483 | ` * Return` |
|         - | 5484 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5485 | ` *  are not present in any of the other arrays.` |
|         - | 5486 | ` */` |
|        22 | 5487 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5488 | `{` |
|         - | 5489 | `	ph7_hashmap_node *pEntry;` |
|         - | 5490 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5491 | `	ph7_value *pCallback;` |
|         - | 5492 | `	ph7_value *pArray;` |
|         - | 5493 | `	sxi32 rc;` |
|         - | 5494 | `	sxu32 n;` |
|         - | 5495 | `	int i;` |
|         - | 5496 |  |
|         - | 5497 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5498 | `	if( nArg < 2 ){` |
|       ! 0 | 5499 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5500 | `			"ArgumentCountError",` |
|         - | 5501 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5502 | `			nArg` |
|         - | 5503 | `			);` |
|         - | 5504 | `	}` |
|        26 | 5505 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5506 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5507 | `			"TypeError",` |
|         - | 5508 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5509 | `			ph7_type_name(apArg[0])` |
|         - | 5510 | `			);` |
|         - | 5511 | `	}` |
|         - | 5512 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5513 | `	 * expected to be a callback. */` |
|        38 | 5514 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5515 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5516 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5517 | `				"TypeError",` |
|         - | 5518 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5519 | `				i + 1,` |
|         2 | 5520 | `				ph7_type_name(apArg[i])` |
|         - | 5521 | `				);` |
|         - | 5522 | `		}` |
|         9 | 5523 | `	}` |
|         - | 5524 | `	/* Point to the callback value */` |
|        22 | 5525 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5526 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5527 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5528 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5529 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5530 | `		 * string given" which we also reproduce. */` |
|         9 | 5531 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5532 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5533 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5534 | `				"TypeError",` |
|         - | 5535 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5536 | `				nArg` |
|         - | 5537 | `				);` |
|         - | 5538 | `		}` |
|         6 | 5539 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5540 | `			/* neither array nor string */` |
|         8 | 5541 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5542 | `				"TypeError",` |
|         - | 5543 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5544 | `				nArg` |
|         - | 5545 | `				);` |
|         - | 5546 | `		}` |
|         - | 5547 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5548 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5549 | `			"TypeError",` |
|         - | 5550 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5551 | `			nArg,` |
|       ! 0 | 5552 | `			ph7_type_name(pCallback)` |
|         - | 5553 | `			);` |
|         - | 5554 | `	}` |
|        13 | 5555 | `	if( nArg == 2 ){` |
|         - | 5556 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5557 | `		 * input array. */` |
|         3 | 5558 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5559 | `		return PH7_OK;` |
|         - | 5560 | `	}` |
|         - | 5561 | `	/* Create a new array */` |
|        11 | 5562 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5563 | `	if( pArray == 0 ){` |
|       ! 0 | 5564 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5565 | `		return PH7_OK;` |
|         - | 5566 | `	}` |
|         - | 5567 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5568 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5569 | `	/* Perform the diff */` |
|        11 | 5570 | `	pEntry = pSrc->pFirst;` |
|        11 | 5571 | `	n = pSrc->nEntry;` |
|        21 | 5572 | `	for(;;){` |
|         - | 5573 | `		int keep;` |
|        27 | 5574 | `		if( n < 1 ){` |
|         9 | 5575 | `			break;` |
|         - | 5576 | `		}` |
|        19 | 5577 | `		keep = 1;` |
|        31 | 5578 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5579 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5580 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5581 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5582 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5583 | `			while( pIt ){` |
|         - | 5584 | `				/* build temporary key values for callback */` |
|         - | 5585 | `				ph7_value key1, key2, result;` |
|         - | 5586 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5587 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5588 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5589 | `				}else{` |
|         - | 5590 | `					SyString sStr;` |
|        33 | 5591 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5592 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5593 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5594 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5595 | `				}` |
|        33 | 5596 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5597 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5598 | `				}else{` |
|         - | 5599 | `					SyString sStr;` |
|        33 | 5600 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5601 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5602 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5603 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5604 | `				}` |
|        33 | 5605 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5606 | `				/* call user callback with (key1, key2) */` |
|         - | 5607 | `				{` |
|         - | 5608 | `					ph7_value *apK[2];` |
|        33 | 5609 | `					apK[0] = &key1;` |
|        33 | 5610 | `					apK[1] = &key2;` |
|        33 | 5611 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5612 | `				}` |
|        33 | 5613 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5614 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5615 | `					 * array_uintersect (which signal back from` |
|         - | 5616 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5617 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5618 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5619 | `					PH7_MemObjRelease(&result);` |
|         3 | 5620 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5621 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5622 | `					return PH7_EXCEPTION;` |
|         - | 5623 | `				}` |
|        31 | 5624 | `				if( rc == SXRET_OK ){` |
|        31 | 5625 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5626 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5627 | `					}` |
|        31 | 5628 | `					if( result.x.iVal == 0 ){` |
|         - | 5629 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5630 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5631 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5632 | `						if( pVal1 && pVal2 ){` |
|         - | 5633 | `							ph7_value sV1,sV2;` |
|         - | 5634 | `							sxi32 cmp;` |
|         - | 5635 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5636 | `							 * place and these are LIVE array elements. */` |
|        13 | 5637 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5638 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5639 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5640 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5641 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5642 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5643 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5644 | `							if( cmp == 0 ){` |
|         9 | 5645 | `								keep = 0;` |
|         9 | 5646 | `								PH7_MemObjRelease(&result);` |
|         - | 5647 | `								/* release keys too before breaking */` |
|         9 | 5648 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5649 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5650 | `								break;` |
|         - | 5651 | `							}` |
|         2 | 5652 | `						}` |
|         2 | 5653 | `					}` |
|        11 | 5654 | `				}` |
|        23 | 5655 | `				PH7_MemObjRelease(&result);` |
|        23 | 5656 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5657 | `				PH7_MemObjRelease(&key2);` |
|         - | 5658 | `				/* move to next node */` |
|        23 | 5659 | `				pIt = pIt->pPrev;` |
|        23 | 5660 | `				if( keep == 0 ) break;` |
|         1 | 5661 | `			}` |
|        21 | 5662 | `			if( keep == 0 ) break;` |
|         7 | 5663 | `		}` |
|        17 | 5664 | `		if( keep ){` |
|         - | 5665 | `			/* Perform the insertion */` |
|         9 | 5666 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5667 | `		}` |
|         - | 5668 | `		/* Point to the next entry */` |
|        17 | 5669 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5670 | `		n--;` |
|         1 | 5671 | `	}` |
|         - | 5672 | `	/* Return the freshly created array */` |
|         9 | 5673 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5674 | `	return PH7_OK;` |
|        15 | 5675 | `}` |
|         - | 5676 | `/*` |
|         - | 5677 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5678 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5679 | ` * Parameters` |
|         - | 5680 | ` *  $array1` |
|         - | 5681 | ` *    The array to compare from` |
|         - | 5682 | ` *  $array2` |
|         - | 5683 | ` *    An array to compare against` |
|         - | 5684 | ` *  $...` |
|         - | 5685 | ` *   More arrays to compare against` |
|         - | 5686 | ` * Return` |
|         - | 5687 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5688 | ` *  in any of the other arrays.` |
|         - | 5689 | ` * Note that NULL is returned on failure.` |
|         - | 5690 | ` */` |
|        12 | 5691 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5692 | `{` |
|         - | 5693 | `	ph7_hashmap_node *pEntry;` |
|         - | 5694 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5695 | `	ph7_value *pArray;` |
|         - | 5696 | `	sxi32 rc;` |
|         - | 5697 | `	sxu32 n;` |
|         - | 5698 | `	int i;` |
|         - | 5699 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5700 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5701 | `	 * helpers. */` |
|        15 | 5702 | `	if( nArg < 1 ){` |
|       ! 0 | 5703 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5704 | `			"ArgumentCountError",` |
|         - | 5705 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5706 | `			nArg` |
|         - | 5707 | `			);` |
|         - | 5708 | `	}` |
|        15 | 5709 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5710 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5711 | `			"TypeError",` |
|         - | 5712 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5713 | `			ph7_type_name(apArg[0])` |
|         - | 5714 | `			);` |
|         - | 5715 | `	}` |
|        20 | 5716 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5717 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5718 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5719 | `				"TypeError",` |
|         - | 5720 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5721 | `				i + 1,` |
|         2 | 5722 | `				ph7_type_name(apArg[i])` |
|         - | 5723 | `				);` |
|         - | 5724 | `		}` |
|         5 | 5725 | `	}` |
|         9 | 5726 | `	if( nArg == 1 ){` |
|         - | 5727 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5728 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5729 | `		return PH7_OK;` |
|         - | 5730 | `	}` |
|         - | 5731 | `	/* Create a new array */` |
|         7 | 5732 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5733 | `	if( pArray == 0 ){` |
|       ! 0 | 5734 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5735 | `		return PH7_OK;` |
|         - | 5736 | `	}` |
|         - | 5737 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5738 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5739 | `	/* Perfrom the diff */` |
|         7 | 5740 | `	pEntry = pSrc->pFirst;` |
|         7 | 5741 | `	n = pSrc->nEntry;` |
|        12 | 5742 | `	for(;;){` |
|        25 | 5743 | `		if( n < 1 ){` |
|         7 | 5744 | `			break;` |
|         - | 5745 | `		}` |
|        31 | 5746 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5747 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5748 | `				/* ignore */` |
|       ! 0 | 5749 | `				continue;` |
|         - | 5750 | `			}` |
|        23 | 5751 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5752 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5753 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5754 | `				/* Blob lookup */` |
|        17 | 5755 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5756 | `			}else{` |
|         - | 5757 | `				/* Int lookup */` |
|         7 | 5758 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5759 | `			}` |
|        23 | 5760 | `			if( rc == SXRET_OK ){` |
|         - | 5761 | `				/* Key exists,break immediately */` |
|        11 | 5762 | `				break;` |
|         - | 5763 | `			}` |
|         7 | 5764 | `		}` |
|        19 | 5765 | `		if( i >= nArg ){` |
|         - | 5766 | `			/* Perform the insertion */` |
|         9 | 5767 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5768 | `		}` |
|         - | 5769 | `		/* Point to the next entry */` |
|        19 | 5770 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5771 | `		n--;` |
|         1 | 5772 | `	}` |
|         - | 5773 | `	/* Return the freshly created array */` |
|         7 | 5774 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5775 | `	return PH7_OK;` |
|         9 | 5776 | `}` |
|         - | 5777 | `/*` |
|         - | 5778 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5779 | ` *  Computes the intersection of arrays.` |
|         - | 5780 | ` * Parameters` |
|         - | 5781 | ` *  $array1` |
|         - | 5782 | ` *    The array to compare from` |
|         - | 5783 | ` *  $array2` |
|         - | 5784 | ` *    An array to compare against` |
|         - | 5785 | ` *  $...` |
|         - | 5786 | ` *   More arrays to compare against` |
|         - | 5787 | ` * Return` |
|         - | 5788 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5789 | ` *  in all of the parameters.` |
|         - | 5790 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5791 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5792 | ` */` |
|        20 | 5793 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5794 | `{` |
|         - | 5795 | `	ph7_hashmap_node *pEntry;` |
|         - | 5796 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5797 | `	ph7_value *pArray;` |
|         - | 5798 | `	ph7_value *pVal;` |
|         - | 5799 | `	sxi32 rc;` |
|         - | 5800 | `	sxu32 n;` |
|         - | 5801 | `	int i;` |
|        23 | 5802 | `	if( nArg < 1 ){` |
|       ! 0 | 5803 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5804 | `			"ArgumentCountError",` |
|         - | 5805 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5806 | `			nArg` |
|         - | 5807 | `			);` |
|         - | 5808 | `	}` |
|        23 | 5809 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5810 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5811 | `			"TypeError",` |
|         - | 5812 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5813 | `			ph7_type_name(apArg[0])` |
|         - | 5814 | `			);` |
|         - | 5815 | `	}` |
|        36 | 5816 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5817 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5818 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5819 | `				"TypeError",` |
|         - | 5820 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5821 | `				i + 1,` |
|         2 | 5822 | `				ph7_type_name(apArg[i])` |
|         - | 5823 | `				);` |
|         - | 5824 | `		}` |
|         9 | 5825 | `	}` |
|        17 | 5826 | `	if( nArg == 1 ){` |
|         - | 5827 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5828 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5829 | `		return PH7_OK;` |
|         - | 5830 | `	}` |
|         - | 5831 | `	/* Create a new array */` |
|        15 | 5832 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5833 | `	if( pArray == 0 ){` |
|       ! 0 | 5834 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5835 | `		return PH7_OK;` |
|         - | 5836 | `	}` |
|         - | 5837 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5838 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5839 | `	/* Perform the intersection */` |
|        15 | 5840 | `	pEntry = pSrc->pFirst;` |
|        15 | 5841 | `	n = pSrc->nEntry;` |
|        31 | 5842 | `	for(;;){` |
|        63 | 5843 | `		if( n < 1 ){` |
|        15 | 5844 | `			break;` |
|         - | 5845 | `		}` |
|         - | 5846 | `		/* Extract the node value */` |
|        49 | 5847 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5848 | `		if( pVal ){` |
|        79 | 5849 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5850 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5851 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5852 | `				/* Perform the lookup */` |
|        55 | 5853 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5854 | `				if( rc != SXRET_OK ){` |
|         - | 5855 | `					/* Value does not exist */` |
|        25 | 5856 | `					break;` |
|         - | 5857 | `				}` |
|        16 | 5858 | `			}` |
|        49 | 5859 | `			if( i >= nArg ){` |
|         - | 5860 | `				/* Perform the insertion */` |
|        25 | 5861 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5862 | `			}` |
|        24 | 5863 | `		}` |
|         - | 5864 | `		/* Point to the next entry */` |
|        49 | 5865 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5866 | `		n--;` |
|         1 | 5867 | `	}` |
|         - | 5868 | `	/* Return the freshly created array */` |
|        15 | 5869 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5870 | `	return PH7_OK;` |
|        13 | 5871 | `}` |
|         - | 5872 | `/*` |
|         - | 5873 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5874 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5875 | ` * Parameters` |
|         - | 5876 | ` *  $array1` |
|         - | 5877 | ` *    The array to compare from` |
|         - | 5878 | ` *  $array2` |
|         - | 5879 | ` *    An array to compare against` |
|         - | 5880 | ` *  $...` |
|         - | 5881 | ` *   More arrays to compare against` |
|         - | 5882 | ` * Return` |
|         - | 5883 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5884 | ` *  in all the arguments, with matching keys.` |
|         - | 5885 | ` */` |
|        20 | 5886 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5887 | `{` |
|         - | 5888 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5889 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5890 | `	ph7_value *pArray;` |
|         - | 5891 | `	ph7_value *pVal;` |
|         - | 5892 | `	sxi32 rc;` |
|         - | 5893 | `	sxu32 n;` |
|         - | 5894 | `	int i;` |
|        23 | 5895 | `	if( nArg < 1 ){` |
|       ! 0 | 5896 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5897 | `			"ArgumentCountError",` |
|         - | 5898 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5899 | `			nArg` |
|         - | 5900 | `			);` |
|         - | 5901 | `	}` |
|        23 | 5902 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5903 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5904 | `			"TypeError",` |
|         - | 5905 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5906 | `			ph7_type_name(apArg[0])` |
|         - | 5907 | `			);` |
|         - | 5908 | `	}` |
|        36 | 5909 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5910 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5911 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5912 | `				"TypeError",` |
|         - | 5913 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5914 | `				i + 1,` |
|         2 | 5915 | `				ph7_type_name(apArg[i])` |
|         - | 5916 | `				);` |
|         - | 5917 | `		}` |
|         9 | 5918 | `	}` |
|        17 | 5919 | `	if( nArg == 1 ){` |
|         - | 5920 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5921 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5922 | `		return PH7_OK;` |
|         - | 5923 | `	}` |
|         - | 5924 | `	/* Create a new array */` |
|        15 | 5925 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5926 | `	if( pArray == 0 ){` |
|       ! 0 | 5927 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5928 | `		return PH7_OK;` |
|         - | 5929 | `	}` |
|         - | 5930 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5931 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5932 | `	/* Perform the intersection */` |
|        15 | 5933 | `	pEntry = pSrc->pFirst;` |
|        15 | 5934 | `	n = pSrc->nEntry;` |
|        15 | 5935 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5936 | `	for(;;){` |
|        47 | 5937 | `		if( n < 1 ){` |
|        15 | 5938 | `			break;` |
|         - | 5939 | `		}` |
|         - | 5940 | `		/* Extract the node value */` |
|        33 | 5941 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5942 | `		if( pVal ){` |
|        53 | 5943 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5944 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5945 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5946 | `				/* Perform a key lookup first */` |
|        37 | 5947 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5948 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5949 | `				}else{` |
|        23 | 5950 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5951 | `				}` |
|        37 | 5952 | `				if( rc != SXRET_OK ){` |
|         - | 5953 | `					/* No such key,break immediately */` |
|         7 | 5954 | `					break;` |
|         - | 5955 | `				}` |
|         - | 5956 | `				/* Perform the lookup */` |
|        31 | 5957 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5958 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5959 | `					/* Value does not exist */` |
|         6 | 5960 | `					break;` |
|         - | 5961 | `				}` |
|        11 | 5962 | `			}` |
|        33 | 5963 | `			if( i >= nArg ){` |
|         - | 5964 | `				/* Perform the insertion */` |
|        17 | 5965 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5966 | `			}` |
|        16 | 5967 | `		}` |
|         - | 5968 | `		/* Point to the next entry */` |
|        33 | 5969 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5970 | `		n--;` |
|         1 | 5971 | `	}` |
|         - | 5972 | `	/* Return the freshly created array */` |
|        15 | 5973 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5974 | `	return PH7_OK;` |
|        13 | 5975 | `}` |
|         - | 5976 | `/*` |
|         - | 5977 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5978 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5979 | ` * Parameters` |
|         - | 5980 | ` *  $array1` |
|         - | 5981 | ` *    The array to compare from` |
|         - | 5982 | ` *  $...` |
|         - | 5983 | ` *   More arrays to compare against` |
|         - | 5984 | ` * Return` |
|         - | 5985 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5986 | ` *  have keys that are present in all arguments.` |
|         - | 5987 | ` * Note that NULL is returned on failure.` |
|         - | 5988 | ` */` |
|        20 | 5989 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5990 | `{` |
|         - | 5991 | `	ph7_hashmap_node *pEntry;` |
|         - | 5992 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5993 | `	ph7_value *pArray;` |
|         - | 5994 | `	sxi32 rc;` |
|         - | 5995 | `	sxu32 n;` |
|         - | 5996 | `	int i;` |
|        23 | 5997 | `	if( nArg < 1 ){` |
|       ! 0 | 5998 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5999 | `			"ArgumentCountError",` |
|         - | 6000 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 6001 | `			nArg` |
|         - | 6002 | `			);` |
|         - | 6003 | `	}` |
|        23 | 6004 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6005 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6006 | `			"TypeError",` |
|         - | 6007 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6008 | `			ph7_type_name(apArg[0])` |
|         - | 6009 | `			);` |
|         - | 6010 | `	}` |
|        36 | 6011 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 6012 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6013 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6014 | `				"TypeError",` |
|         - | 6015 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 6016 | `				i + 1,` |
|         2 | 6017 | `				ph7_type_name(apArg[i])` |
|         - | 6018 | `				);` |
|         - | 6019 | `		}` |
|         9 | 6020 | `	}` |
|        17 | 6021 | `	if( nArg == 1 ){` |
|         - | 6022 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 6023 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 6024 | `		return PH7_OK;` |
|         - | 6025 | `	}` |
|         - | 6026 | `	/* Create a new array */` |
|        15 | 6027 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 6028 | `	if( pArray == 0 ){` |
|       ! 0 | 6029 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6030 | `		return PH7_OK;` |
|         - | 6031 | `	}` |
|         - | 6032 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 6033 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6034 | `	/* Perform the intersection */` |
|        15 | 6035 | `	pEntry = pSrc->pFirst;` |
|        15 | 6036 | `	n = pSrc->nEntry;` |
|        24 | 6037 | `	for(;;){` |
|        49 | 6038 | `		if( n < 1 ){` |
|        15 | 6039 | `			break;` |
|         - | 6040 | `		}` |
|        57 | 6041 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 6042 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 6043 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 6044 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 6045 | `				/* Blob lookup */` |
|        27 | 6046 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 6047 | `			}else{` |
|         - | 6048 | `				/* Int key */` |
|        13 | 6049 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 6050 | `			}` |
|        39 | 6051 | `			if( rc != SXRET_OK ){` |
|         - | 6052 | `				/* Key does not exist, break immediately */` |
|        17 | 6053 | `				break;` |
|         - | 6054 | `			}` |
|        12 | 6055 | `		}` |
|        35 | 6056 | `		if( i >= nArg ){` |
|         - | 6057 | `			/* Perform the insertion */` |
|        19 | 6058 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 6059 | `		}` |
|         - | 6060 | `		/* Point to the next entry */` |
|        35 | 6061 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6062 | `		n--;` |
|         1 | 6063 | `	}` |
|         - | 6064 | `	/* Return the freshly created array */` |
|        15 | 6065 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 6066 | `	return PH7_OK;` |
|        13 | 6067 | `}` |
|         - | 6068 | `/*` |
|         - | 6069 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 6070 | ` *  Computes the intersection of arrays.` |
|         - | 6071 | ` * Parameters` |
|         - | 6072 | ` *  $array1` |
|         - | 6073 | ` *    The array to compare from` |
|         - | 6074 | ` *  $array2` |
|         - | 6075 | ` *    An array to compare against` |
|         - | 6076 | ` *  $...` |
|         - | 6077 | ` *   More arrays to compare against` |
|         - | 6078 | ` * $callback` |
|         - | 6079 | ` *  The callback comparison function.` |
|         - | 6080 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 6081 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 6082 | ` *  than the second.` |
|         - | 6083 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 6084 | ` * Return` |
|         - | 6085 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 6086 | ` *  in all of the parameters. .` |
|         - | 6087 | ` * Note that NULL is returned on failure.` |
|         - | 6088 | ` */` |
|        24 | 6089 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6090 | `{` |
|         - | 6091 | `	ph7_hashmap_node *pEntry;` |
|         - | 6092 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 6093 | `	ph7_value *pCallback;` |
|         - | 6094 | `	ph7_value *pArray;` |
|         - | 6095 | `	ph7_value *pVal;` |
|         - | 6096 | `	sxi32 rc;` |
|         - | 6097 | `	sxu32 n;` |
|         - | 6098 | `	int i;` |
|         - | 6099 |  |
|         - | 6100 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 6101 | `	if( nArg < 2 ){` |
|       ! 0 | 6102 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6103 | `			"ArgumentCountError",` |
|         - | 6104 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 6105 | `			nArg` |
|         - | 6106 | `			);` |
|         - | 6107 | `	}` |
|        29 | 6108 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6109 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6110 | `			"TypeError",` |
|         - | 6111 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6112 | `			ph7_type_name(apArg[0])` |
|         - | 6113 | `			);` |
|         - | 6114 | `	}` |
|         - | 6115 |  |
|        27 | 6116 | `	if( nArg == 2 ){` |
|         - | 6117 | `		/* Only the original array and the callback were provided. */` |
|         - | 6118 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6119 | `		 * validation ordering. */` |
|         3 | 6120 | `	} else {` |
|         - | 6121 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6122 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6123 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6124 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6125 | `					"TypeError",` |
|         - | 6126 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6127 | `					i + 1,` |
|         2 | 6128 | `					ph7_type_name(apArg[i])` |
|         - | 6129 | `					);` |
|         - | 6130 | `			}` |
|        13 | 6131 | `		}` |
|         - | 6132 | `	}` |
|         - | 6133 |  |
|         - | 6134 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6135 | `	pCallback = apArg[nArg - 1];` |
|         - | 6136 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6137 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6138 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6139 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6140 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6141 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6142 | `			 */` |
|         9 | 6143 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6144 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6145 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6146 | `					"TypeError",` |
|         - | 6147 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6148 | `					nArg` |
|         - | 6149 | `					);` |
|         - | 6150 | `			}` |
|         - | 6151 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6152 | `			{` |
|         6 | 6153 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6154 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6155 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6156 | `					int nMethodLen;` |
|         6 | 6157 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6158 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6159 | `					if( pClass ){` |
|         - | 6160 | `						/* Class exists but method is missing. */` |
|         4 | 6161 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6162 | `							"TypeError",` |
|         - | 6163 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6164 | `							nArg,` |
|         1 | 6165 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6166 | `							zMethod` |
|         - | 6167 | `							);` |
|         - | 6168 | `					}` |
|         - | 6169 | `					/* Class not found */` |
|         - | 6170 | `					{` |
|         - | 6171 | `						int nName;` |
|         3 | 6172 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6173 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6174 | `							"TypeError",` |
|         - | 6175 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6176 | `							nArg,` |
|         1 | 6177 | `							zName` |
|         - | 6178 | `							);` |
|         - | 6179 | `					}` |
|         - | 6180 | `				}` |
|         - | 6181 | `			}` |
|         - | 6182 | `			/* Fallback message */` |
|       ! 0 | 6183 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6184 | `				"TypeError",` |
|         - | 6185 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6186 | `				nArg` |
|         - | 6187 | `				);` |
|         - | 6188 | `		}` |
|         6 | 6189 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6190 | `			int len;` |
|         3 | 6191 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6192 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6193 | `				"TypeError",` |
|         - | 6194 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6195 | `				nArg,` |
|         1 | 6196 | `				zName` |
|         - | 6197 | `				);` |
|         - | 6198 | `		}` |
|         4 | 6199 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6200 | `			"TypeError",` |
|         - | 6201 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6202 | `			nArg` |
|         - | 6203 | `			);` |
|         - | 6204 | `	}` |
|         - | 6205 |  |
|        11 | 6206 | `	if( nArg == 2 ){` |
|         - | 6207 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6208 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6209 | `		return PH7_OK;` |
|         - | 6210 | `	}` |
|         - | 6211 |  |
|         - | 6212 | `	/* Create a new array */` |
|         7 | 6213 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6214 | `	if( pArray == 0 ){` |
|       ! 0 | 6215 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6216 | `		return PH7_OK;` |
|         - | 6217 | `	}` |
|         - | 6218 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6219 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6220 | `	/* Perform the intersection */` |
|         7 | 6221 | `	pEntry = pSrc->pFirst;` |
|         7 | 6222 | `	n = pSrc->nEntry;` |
|         7 | 6223 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6224 | `	for(;;){` |
|        19 | 6225 | `		if( n < 1 ){` |
|         5 | 6226 | `			break;` |
|         - | 6227 | `		}` |
|         - | 6228 | `		/* Extract the node value */` |
|        15 | 6229 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6230 | `		if( pVal ){` |
|        23 | 6231 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6232 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6233 | `					/* ignore */` |
|       ! 0 | 6234 | `					continue;` |
|         - | 6235 | `				}` |
|         - | 6236 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6237 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6238 | `				/* Perform the lookup */` |
|        15 | 6239 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6240 | `				if( rc != SXRET_OK ){` |
|         - | 6241 | `					/* Value does not exist */` |
|         7 | 6242 | `					break;` |
|         - | 6243 | `				}` |
|         5 | 6244 | `			}` |
|        15 | 6245 | `			if( i >= (nArg-1) ){` |
|         - | 6246 | `				/* Perform the insertion */` |
|         9 | 6247 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6248 | `			}` |
|         7 | 6249 | `		}` |
|        15 | 6250 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6251 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6252 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6253 | `			return PH7_EXCEPTION;` |
|         - | 6254 | `		}` |
|         - | 6255 | `		/* Point to the next entry */` |
|        13 | 6256 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6257 | `		n--;` |
|         1 | 6258 | `	}` |
|         - | 6259 | `	/* Return the freshly created array */` |
|         5 | 6260 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6261 | `	return PH7_OK;` |
|        17 | 6262 | `}` |
|         - | 6263 | `/*` |
|         - | 6264 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6265 | ` *  Fill an array with values.` |
|         - | 6266 | ` * Parameters` |
|         - | 6267 | ` *  $start_index` |
|         - | 6268 | ` *    The first index of the returned array.` |
|         - | 6269 | ` *  $num` |
|         - | 6270 | ` *   Number of elements to insert.` |
|         - | 6271 | ` *  $value` |
|         - | 6272 | ` *    Value to use for filling.` |
|         - | 6273 | ` * Return` |
|         - | 6274 | ` *  The filled array or null on failure.` |
|         - | 6275 | ` */` |
|       240 | 6276 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6277 | `{` |
|         - | 6278 | `	ph7_value *pArray;` |
|         - | 6279 | `	int i,nEntry;` |
|         - | 6280 |  |
|         - | 6281 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6282 | `	if( nArg != 3 ){` |
|         - | 6283 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6284 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6285 | `			"ArgumentCountError",` |
|         - | 6286 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6287 | `			nArg` |
|         - | 6288 | `			);` |
|         - | 6289 | `	}` |
|         - | 6290 |  |
|         - | 6291 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6292 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6293 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6294 | `	 * and NULLs are rejected outright. */` |
|       357 | 6295 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6296 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6297 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6298 | `			"TypeError",` |
|         - | 6299 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6300 | `			ph7_type_name(apArg[0])` |
|         - | 6301 | `			);` |
|         - | 6302 | `	}` |
|       242 | 6303 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6304 | `		int len;` |
|         8 | 6305 | `		sxu8 bReal = FALSE;` |
|         8 | 6306 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6307 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6308 | `			/* Non‑numeric string is an error. */` |
|         3 | 6309 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6310 | `				"TypeError",` |
|         - | 6311 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6312 | `				);` |
|         - | 6313 | `		}` |
|         5 | 6314 | `		if( bReal ){` |
|         - | 6315 | `			/* float-string -> deprecation warning */` |
|         4 | 6316 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6317 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6318 | `				zStr` |
|         - | 6319 | `				);` |
|         1 | 6320 | `		}` |
|         2 | 6321 | `	}` |
|         - | 6322 |  |
|         - | 6323 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6324 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6325 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6326 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6327 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6328 | `			"TypeError",` |
|         - | 6329 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6330 | `			ph7_type_name(apArg[1])` |
|         - | 6331 | `			);` |
|         - | 6332 | `	}` |
|       239 | 6333 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6334 | `		int len;` |
|         3 | 6335 | `		sxu8 bReal = FALSE;` |
|         3 | 6336 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6337 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6338 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6339 | `				"TypeError",` |
|         - | 6340 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6341 | `				);` |
|         - | 6342 | `		}` |
|       ! 0 | 6343 | `	}` |
|         - | 6344 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6345 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6346 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6347 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6348 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6349 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6350 | `		if( d != (double)i64 ){` |
|         7 | 6351 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6352 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6353 | `				d` |
|         - | 6354 | `				);` |
|         2 | 6355 | `		}` |
|         2 | 6356 | `	}` |
|         - | 6357 |  |
|         - | 6358 | `	/* Total number of entries to insert */` |
|       236 | 6359 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6360 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6361 | `	if( nEntry < 0 ){` |
|         3 | 6362 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6363 | `			"ValueError",` |
|         - | 6364 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6365 | `			);` |
|         - | 6366 | `	}` |
|         - | 6367 |  |
|         - | 6368 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6369 | `	if( nEntry == 0 ){` |
|         7 | 6370 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6371 | `		return PH7_OK;` |
|         - | 6372 | `	}` |
|         - | 6373 |  |
|         - | 6374 | `	/* Create a new array */` |
|       227 | 6375 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6376 | `	if( pArray == 0 ){` |
|       ! 0 | 6377 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6378 | `	}` |
|         - | 6379 |  |
|         - | 6380 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6381 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6382 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6383 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6384 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6385 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6386 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6387 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6388 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6389 | `		}` |
|   1058803 | 6390 | `	}` |
|         - | 6391 | `	/* Return the filled array */` |
|       227 | 6392 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6393 | `	return PH7_OK;` |
|       124 | 6394 | `}` |
|         - | 6395 | `/*` |
|         - | 6396 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6397 | ` *  Fill an array with values, specifying keys.` |
|         - | 6398 | ` * Parameters` |
|         - | 6399 | ` *  $input` |
|         - | 6400 | ` *   Array of values that will be used as key.` |
|         - | 6401 | ` *  $value` |
|         - | 6402 | ` *    Value to use for filling.` |
|         - | 6403 | ` * Return` |
|         - | 6404 | ` *  The filled array.` |
|         - | 6405 | ` * Throws` |
|         - | 6406 | ` *  ValueError if $input is not an array.` |
|         - | 6407 | ` */` |
|        22 | 6408 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6409 | `{` |
|         - | 6410 | `	ph7_hashmap_node *pEntry;` |
|         - | 6411 | `	ph7_hashmap *pSrc;` |
|         - | 6412 | `	ph7_value *pArray;` |
|         - | 6413 | `	sxu32 n;` |
|         - | 6414 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6415 | `	if( nArg != 2 ){` |
|         4 | 6416 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6417 | `			"ArgumentCountError",` |
|         - | 6418 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6419 | `			nArg` |
|         - | 6420 | `			);` |
|         - | 6421 | `	}` |
|         - | 6422 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6423 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6424 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6425 | `			"TypeError",` |
|         - | 6426 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6427 | `			ph7_type_name(apArg[0])` |
|         - | 6428 | `			);` |
|         - | 6429 | `	}` |
|         - | 6430 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6431 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6432 | `	/* Create a new array */` |
|        17 | 6433 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6434 | `	if( pArray == 0 ){` |
|       ! 0 | 6435 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6436 | `		return PH7_OK;` |
|         - | 6437 | `	}` |
|         - | 6438 | `	/* Perform the requested operation */` |
|        17 | 6439 | `	pEntry = pSrc->pFirst;` |
|        45 | 6440 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6441 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6442 | `		/* Point to the next entry */` |
|        29 | 6443 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6444 | `	}` |
|         - | 6445 | `	/* Return the filled array */` |
|        17 | 6446 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6447 | `	return PH7_OK;` |
|        14 | 6448 | `}` |
|         - | 6449 | `/*` |
|         - | 6450 | ` * array array_combine(array $keys,array $values)` |
|         - | 6451 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6452 | ` * Parameters` |
|         - | 6453 | ` *  $keys` |
|         - | 6454 | ` *    Array of keys to be used.` |
|         - | 6455 | ` * $values` |
|         - | 6456 | ` *   Array of values to be used.` |
|         - | 6457 | ` * Return` |
|         - | 6458 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6459 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6460 | ` *  not an array.` |
|         - | 6461 | ` */` |
|        16 | 6462 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6463 | `{` |
|         - | 6464 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6465 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6466 | `	ph7_value *pArray;` |
|         - | 6467 | `	sxu32 n;` |
|         - | 6468 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6469 | `	if( nArg != 2 ){` |
|         - | 6470 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6471 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6472 | `			"ArgumentCountError",` |
|         - | 6473 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6474 | `			nArg` |
|         - | 6475 | `			);` |
|         - | 6476 | `	}` |
|         - | 6477 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6478 | `	 * argument index in the error message. */` |
|        20 | 6479 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6480 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6481 | `			"TypeError",` |
|         - | 6482 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6483 | `			ph7_type_name(apArg[0])` |
|         - | 6484 | `			);` |
|         - | 6485 | `	}` |
|        17 | 6486 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6487 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6488 | `			"TypeError",` |
|         - | 6489 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6490 | `			ph7_type_name(apArg[1])` |
|         - | 6491 | `			);` |
|         - | 6492 | `	}` |
|         - | 6493 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6494 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6495 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6496 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6497 | `		/* Length mismatch -> ValueError */` |
|         3 | 6498 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6499 | `			"ValueError",` |
|         - | 6500 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6501 | `			);` |
|         - | 6502 | `	}` |
|         - | 6503 | `	/* Create a new array */` |
|        11 | 6504 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6505 | `	if( pArray == 0 ){` |
|       ! 0 | 6506 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6507 | `		return PH7_OK;` |
|         - | 6508 | `	}` |
|         - | 6509 | `	/* Perform the requested operation */` |
|        11 | 6510 | `	pKe = pKey->pFirst;` |
|        11 | 6511 | `	pVe = pValue->pFirst;` |
|        33 | 6512 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6513 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6514 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6515 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6516 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6517 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6518 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6519 | `		 * original array must not be mutated. */` |
|        23 | 6520 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6521 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6522 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6523 | `			if( pTmpKey ){` |
|         5 | 6524 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6525 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6526 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6527 | `				pKeyCopy = pTmpKey;` |
|         2 | 6528 | `			}` |
|         2 | 6529 | `		}` |
|        23 | 6530 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6531 | `		/* Point to the next entry */` |
|        23 | 6532 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6533 | `		pVe = pVe->pPrev;` |
|        12 | 6534 | `	}` |
|         - | 6535 | `	/* Return the filled array */` |
|        11 | 6536 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6537 | `	return PH7_OK;` |
|        12 | 6538 | `}` |
|         - | 6539 | `/*` |
|         - | 6540 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6541 | ` *  Return an array with elements in reverse order.` |
|         - | 6542 | ` * Parameters` |
|         - | 6543 | ` *  $array` |
|         - | 6544 | ` *   The input array.` |
|         - | 6545 | ` *  $preserve_keys (optional)` |
|         - | 6546 | ` *   If set to TRUE keys are preserved.` |
|         - | 6547 | ` * Return` |
|         - | 6548 | ` *  The reversed array.` |
|         - | 6549 | ` */` |
|        18 | 6550 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6551 | `{` |
|         - | 6552 | `	ph7_hashmap_node *pEntry;` |
|         - | 6553 | `	ph7_hashmap *pSrc;` |
|         - | 6554 | `	ph7_value *pArray;` |
|         - | 6555 | `	int bPreserve;` |
|         - | 6556 | `	sxu32 n;` |
|        20 | 6557 | `	if( nArg < 1 ){` |
|       ! 0 | 6558 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6559 | `			"ArgumentCountError",` |
|         - | 6560 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6561 | `			nArg` |
|         - | 6562 | `			);` |
|         - | 6563 | `	}` |
|         - | 6564 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6565 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6566 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6567 | `			"TypeError",` |
|         - | 6568 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6569 | `			ph7_type_name(apArg[0])` |
|         - | 6570 | `			);` |
|         - | 6571 | `	}` |
|        17 | 6572 | `	bPreserve = FALSE;` |
|        17 | 6573 | `	if( nArg > 1 ){` |
|         7 | 6574 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6575 | `	}` |
|         - | 6576 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6577 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6578 | `	/* Create a new array */` |
|        17 | 6579 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6580 | `	if( pArray == 0 ){` |
|       ! 0 | 6581 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6582 | `		return PH7_OK;` |
|         - | 6583 | `	}` |
|         - | 6584 | `	/* Perform the requested operation */` |
|        17 | 6585 | `	pEntry = pSrc->pLast;` |
|        55 | 6586 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6587 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6588 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6589 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6590 | `		/* Point to the previous entry */` |
|        39 | 6591 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6592 | `	}` |
|        17 | 6593 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6594 | `	return PH7_OK;` |
|        11 | 6595 | `}` |
|         - | 6596 | `/*` |
|         - | 6597 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6598 | ` *  Removes duplicate values from an array.` |
|         - | 6599 | ` * Parameters` |
|         - | 6600 | ` *  $array` |
|         - | 6601 | ` *   The input array.` |
|         - | 6602 | ` *  $flags` |
|         - | 6603 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6604 | ` *   behavior using these values:` |
|         - | 6605 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6606 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6607 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6608 | ` * Return` |
|         - | 6609 | ` *  The filtered array.` |
|         - | 6610 | ` */` |
|        36 | 6611 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6612 | `{` |
|         - | 6613 | `	ph7_hashmap_node *pEntry;` |
|         - | 6614 | `	ph7_value *pNeedle;` |
|         - | 6615 | `	ph7_hashmap *pSrc;` |
|         - | 6616 | `	ph7_value *pArray;` |
|         - | 6617 | `	int iFlags,base,bFold;` |
|         - | 6618 | `	sxu32 n;` |
|        39 | 6619 | `	if( nArg < 1 ){` |
|         - | 6620 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6621 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6622 | `			"ArgumentCountError",` |
|         - | 6623 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6624 | `			);` |
|         - | 6625 | `	}` |
|        39 | 6626 | `	if( nArg > 2 ){` |
|         - | 6627 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6628 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6629 | `			"ArgumentCountError",` |
|         - | 6630 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6631 | `			nArg` |
|         - | 6632 | `			);` |
|         - | 6633 | `	}` |
|         - | 6634 | `	/* Make sure we are dealing with a valid hashmap */` |
|        36 | 6635 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6636 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6637 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6638 | `			"TypeError",` |
|         - | 6639 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6640 | `			ph7_type_name(apArg[0])` |
|         - | 6641 | `			);` |
|         - | 6642 | `	}` |
|         - | 6643 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|         - | 6644 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|        33 | 6645 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|        33 | 6646 | `	base = iFlags & ~8;` |
|        33 | 6647 | `	bFold = (iFlags & 8) != 0;` |
|         - | 6648 | `	/* Point to the internal representation of the input hashmap */` |
|        33 | 6649 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6650 | `	/* Create a new array */` |
|        33 | 6651 | `	pArray = ph7_context_new_array(pCtx);` |
|        33 | 6652 | `	if( pArray == 0 ){` |
|       ! 0 | 6653 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6654 | `		return PH7_OK;` |
|         - | 6655 | `	}` |
|         - | 6656 | `	/* Perform the requested operation */` |
|        33 | 6657 | `	pEntry = pSrc->pFirst;` |
|       145 | 6658 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       113 | 6659 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|       113 | 6660 | `		if( pNeedle ){` |
|         - | 6661 | `			/* Keep this element unless a flag-equal one is already present. */` |
|       113 | 6662 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|       113 | 6663 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|       113 | 6664 | `			int bDup = 0;` |
|         - | 6665 | `			sxu32 i;` |
|         - | 6666 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|         - | 6667 | `			 * loop over pSrc). */` |
|       177 | 6668 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|       117 | 6669 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|       117 | 6670 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|        53 | 6671 | `					bDup = 1;` |
|        53 | 6672 | `					break;` |
|         - | 6673 | `				}` |
|        65 | 6674 | `				pK = pK->pPrev;` |
|        33 | 6675 | `			}` |
|       113 | 6676 | `			if( !bDup ){` |
|        61 | 6677 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|        30 | 6678 | `			}` |
|        56 | 6679 | `		}` |
|         - | 6680 | `		/* Point to the next entry */` |
|       113 | 6681 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        57 | 6682 | `	}` |
|         - | 6683 | `	/* Return the freshly created array */` |
|        33 | 6684 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 6685 | `	return PH7_OK;` |
|        21 | 6686 | `}` |
|         - | 6687 | `/*` |
|         - | 6688 | ` * array array_flip(array $input)` |
|         - | 6689 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6690 | ` * Parameter` |
|         - | 6691 | ` *  $input` |
|         - | 6692 | ` *   Input array.` |
|         - | 6693 | ` * Return` |
|         - | 6694 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6695 | ` */` |
|        30 | 6696 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6697 | `{` |
|         - | 6698 | `	ph7_hashmap_node *pEntry;` |
|         - | 6699 | `	ph7_hashmap *pSrc;` |
|         - | 6700 | `	ph7_value *pArray;` |
|         - | 6701 | `	ph7_value *pKey;` |
|         - | 6702 | `	ph7_value sVal;` |
|         - | 6703 | `	sxu32 n;` |
|         - | 6704 |  |
|         - | 6705 | `	/* PHP requires exactly one argument */` |
|        33 | 6706 | `	if( nArg != 1 ){` |
|         - | 6707 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6708 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6709 | `			"ArgumentCountError",` |
|         - | 6710 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6711 | `			nArg` |
|         - | 6712 | `			);` |
|         - | 6713 | `	}` |
|         - | 6714 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6715 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6716 | `		/* Type mismatch -> TypeError */` |
|         4 | 6717 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6718 | `			"TypeError",` |
|         - | 6719 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6720 | `			ph7_type_name(apArg[0])` |
|         - | 6721 | `			);` |
|         - | 6722 | `	}` |
|         - | 6723 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6724 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6725 | `	/* Create a new array */` |
|        27 | 6726 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6727 | `	if( pArray == 0 ){` |
|       ! 0 | 6728 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6729 | `		return PH7_OK;` |
|         - | 6730 | `	}` |
|         - | 6731 | `	/* Start processing */` |
|        27 | 6732 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6733 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6734 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6735 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6736 | `		if( pKey ){` |
|         - | 6737 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6738 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6739 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6740 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6741 | `					);` |
|     22236 | 6742 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6743 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6744 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6745 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6746 | `				}else{` |
|         - | 6747 | `					SyString sStr;` |
|      2227 | 6748 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6749 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6750 | `				}` |
|         - | 6751 | `				/* Perform the insertion */` |
|     22227 | 6752 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6753 | `				/* Safely release the value because each inserted entry` |
|         - | 6754 | `				 * has its own private copy of the value.` |
|         - | 6755 | `				 */` |
|     22227 | 6756 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6757 | `			}else{` |
|         - | 6758 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6759 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6760 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6761 | `					);` |
|         - | 6762 | `			}` |
|     11118 | 6763 | `		}` |
|         - | 6764 | `		/* Point to the next entry */` |
|     22237 | 6765 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6766 | `	}` |
|         - | 6767 | `	/* Return the freshly created array */` |
|        27 | 6768 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6769 | `	return PH7_OK;` |
|        18 | 6770 | `}` |
|         - | 6771 | `/*` |
|         - | 6772 | ` * number array_sum(array $array )` |
|         - | 6773 | ` *  Calculate the sum of values in an array.` |
|         - | 6774 | ` * Parameters` |
|         - | 6775 | ` *  $array: The input array.` |
|         - | 6776 | ` * Return` |
|         - | 6777 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6778 | ` */` |
|        24 | 6779 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6780 | `{` |
|         - | 6781 | `	ph7_hashmap_node *pEntry;` |
|         - | 6782 | `	ph7_value *pObj;` |
|        26 | 6783 | `	double dSum = 0;` |
|         - | 6784 | `	sxu32 n;` |
|        26 | 6785 | `	pEntry = pMap->pFirst;` |
|        92 | 6786 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6787 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6788 | `		if( pObj ){` |
|        68 | 6789 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6790 | `				dSum += pObj->rVal;` |
|        54 | 6791 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6792 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6793 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6794 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6795 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6796 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6797 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6798 | `						"Addition is not supported on type string");` |
|        14 | 6799 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6800 | `					double dv = 0;` |
|        13 | 6801 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6802 | `					dSum += dv;` |
|         8 | 6803 | `				}` |
|        12 | 6804 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6805 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6806 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6807 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6808 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6809 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6810 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6811 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6812 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6813 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6814 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6815 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6816 | `			}` |
|         - | 6817 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6818 | `		}` |
|         - | 6819 | `		/* Point to the next entry */` |
|        68 | 6820 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6821 | `	}` |
|         - | 6822 | `	/* Return sum */` |
|        26 | 6823 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6824 | `}` |
|       690 | 6825 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         3 | 6826 | `{` |
|         - | 6827 | `	ph7_hashmap_node *pEntry;` |
|         - | 6828 | `	ph7_value *pObj;` |
|       693 | 6829 | `	sxi64 nSum = 0;` |
|         - | 6830 | `	sxu32 n;` |
|       693 | 6831 | `	pEntry = pMap->pFirst;` |
|      6705 | 6832 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      6015 | 6833 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      6015 | 6834 | `		if( pObj ){` |
|      6015 | 6835 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      5995 | 6836 | `				nSum += pObj->x.iVal;` |
|      3018 | 6837 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6838 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6839 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6840 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6841 | `						"Addition is not supported on type string");` |
|        10 | 6842 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6843 | `					sxi64 nv = 0;` |
|         8 | 6844 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6845 | `					nSum += nv;` |
|         5 | 6846 | `				}` |
|        17 | 6847 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6848 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6849 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6850 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6851 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6852 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6853 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6854 | `					"Addition is not supported on type %s",` |
|         2 | 6855 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6856 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6857 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6858 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6859 | `			}` |
|         - | 6860 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      3006 | 6861 | `		}` |
|         - | 6862 | `		/* Point to the next entry */` |
|      6015 | 6863 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3009 | 6864 | `	}` |
|         - | 6865 | `	/* Return sum */` |
|       693 | 6866 | `	ph7_result_int64(pCtx,nSum);` |
|       693 | 6867 | `}` |
|         - | 6868 | `/* number array_sum(array $array )` |
|         - | 6869 | ` * (See block-coment above)` |
|         - | 6870 | ` */` |
|       726 | 6871 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6872 | `{` |
|         - | 6873 | `	ph7_hashmap_node *pEntry;` |
|         - | 6874 | `	ph7_hashmap *pMap;` |
|         - | 6875 | `	ph7_value *pObj;` |
|       730 | 6876 | `	int useDouble = 0;` |
|         - | 6877 | `	sxu32 n;` |
|         - | 6878 | `	/* PHP requires exactly one argument */` |
|       730 | 6879 | `	if( nArg != 1 ){` |
|         4 | 6880 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6881 | `			"ArgumentCountError",` |
|         - | 6882 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6883 | `			nArg` |
|         - | 6884 | `			);` |
|         - | 6885 | `	}` |
|         - | 6886 | `	/* Make sure we are dealing with a valid hashmap */` |
|       728 | 6887 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6888 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6889 | `		char zBuf[64];` |
|         8 | 6890 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6891 | `			"TypeError",` |
|         - | 6892 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6893 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6894 | `			);` |
|         - | 6895 | `	}` |
|       723 | 6896 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       723 | 6897 | `	if( pMap->nEntry < 1 ){` |
|         - | 6898 | `		/* Nothing to compute,return 0 */` |
|         7 | 6899 | `		ph7_result_int(pCtx,0);` |
|         7 | 6900 | `		return PH7_OK;` |
|         - | 6901 | `	}` |
|         - | 6902 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6903 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6904 | `	 */` |
|       717 | 6905 | `	pEntry = pMap->pFirst;` |
|      6737 | 6906 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      6047 | 6907 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      6047 | 6908 | `		if( pObj ){` |
|      6047 | 6909 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6910 | `				useDouble = 1;` |
|        20 | 6911 | `				break;` |
|         - | 6912 | `			}` |
|      6029 | 6913 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6914 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6915 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6916 | `				sxu32 i;` |
|        32 | 6917 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6918 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6919 | `						useDouble = 1;` |
|         7 | 6920 | `						break;` |
|         - | 6921 | `					}` |
|         9 | 6922 | `				}` |
|        18 | 6923 | `				if( useDouble ){` |
|         7 | 6924 | `					break;` |
|         - | 6925 | `				}` |
|         5 | 6926 | `			}` |
|      3010 | 6927 | `		}` |
|      6023 | 6928 | `		pEntry = pEntry->pPrev;` |
|      3013 | 6929 | `	}` |
|       717 | 6930 | `	if( useDouble ){` |
|        26 | 6931 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6932 | `	}else{` |
|       693 | 6933 | `		Int64Sum(pCtx,pMap);` |
|         - | 6934 | `	}` |
|       717 | 6935 | `	return PH7_OK;` |
|       367 | 6936 | `}` |
|         - | 6937 | `/*` |
|         - | 6938 | ` * number array_product(array $array )` |
|         - | 6939 | ` *  Calculate the product of values in an array.` |
|         - | 6940 | ` * Parameters` |
|         - | 6941 | ` *  $array: The input array.` |
|         - | 6942 | ` * Return` |
|         - | 6943 | ` *  Returns the product of values as an integer or float.` |
|         - | 6944 | ` */` |
|         2 | 6945 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6946 | `{` |
|         - | 6947 | `	ph7_hashmap_node *pEntry;` |
|         - | 6948 | `	ph7_value *pObj;` |
|         - | 6949 | `	double dProd;` |
|         - | 6950 | `	sxu32 n;` |
|         3 | 6951 | `	pEntry = pMap->pFirst;` |
|         3 | 6952 | `	dProd = 1;` |
|         7 | 6953 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6954 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6955 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6956 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6957 | `				dProd *= pObj->rVal;` |
|         4 | 6958 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6959 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6960 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6961 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6962 | `					double dv = 0;` |
|       ! 0 | 6963 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6964 | `					dProd *= dv;` |
|       ! 0 | 6965 | `				}` |
|       ! 0 | 6966 | `			}` |
|         2 | 6967 | `		}` |
|         - | 6968 | `		/* Point to the next entry */` |
|         5 | 6969 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6970 | `	}` |
|         - | 6971 | `	/* Return product */` |
|         3 | 6972 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6973 | `}` |
|         2 | 6974 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6975 | `{` |
|         - | 6976 | `	ph7_hashmap_node *pEntry;` |
|         - | 6977 | `	ph7_value *pObj;` |
|         - | 6978 | `	sxi64 nProd;` |
|         - | 6979 | `	sxu32 n;` |
|         3 | 6980 | `	pEntry = pMap->pFirst;` |
|         3 | 6981 | `	nProd = 1;` |
|         9 | 6982 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6983 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6984 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6985 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6986 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6987 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6988 | `				nProd *= pObj->x.iVal;` |
|         3 | 6989 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6990 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6991 | `					sxi64 nv = 0;` |
|       ! 0 | 6992 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6993 | `					nProd *= nv;` |
|       ! 0 | 6994 | `				}` |
|       ! 0 | 6995 | `			}` |
|         3 | 6996 | `		}` |
|         - | 6997 | `		/* Point to the next entry */` |
|         7 | 6998 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6999 | `	}` |
|         - | 7000 | `	/* Return product */` |
|         3 | 7001 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 7002 | `}` |
|         - | 7003 | `/* number array_product(array $array )` |
|         - | 7004 | ` * (See block-block comment above)` |
|         - | 7005 | ` */` |
|        16 | 7006 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7007 | `{` |
|         - | 7008 | `	ph7_hashmap *pMap;` |
|         - | 7009 | `	ph7_value *pObj;` |
|        17 | 7010 | `	if( nArg < 1 ){` |
|         - | 7011 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 7012 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 7013 | `		return PH7_OK;` |
|         - | 7014 | `	}` |
|         - | 7015 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 7016 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7017 | `		char zBuf[64];` |
|        16 | 7018 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7019 | `			"TypeError",` |
|         - | 7020 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 7021 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7022 | `			);` |
|         - | 7023 | `	}` |
|         7 | 7024 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 7025 | `	if( pMap->nEntry < 1 ){` |
|         - | 7026 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 7027 | `		ph7_result_int(pCtx,1);` |
|         3 | 7028 | `		return PH7_OK;` |
|         - | 7029 | `	}` |
|         - | 7030 | `	/* If the first element is of type float,then perform floating` |
|         - | 7031 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 7032 | `	 */` |
|         5 | 7033 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 7034 | `	if( pObj == 0 ){` |
|       ! 0 | 7035 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 7036 | `		return PH7_OK;` |
|         - | 7037 | `	}` |
|         5 | 7038 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 7039 | `		DoubleProd(pCtx,pMap);` |
|         2 | 7040 | `	}else{` |
|         3 | 7041 | `		Int64Prod(pCtx,pMap);` |
|         - | 7042 | `	}` |
|         5 | 7043 | `	return PH7_OK;` |
|         9 | 7044 | `}` |
|         - | 7045 | `/*` |
|         - | 7046 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 7047 | ` *  Pick one or more random entries out of an array.` |
|         - | 7048 | ` * Parameters` |
|         - | 7049 | ` * $input` |
|         - | 7050 | ` *  The input array.` |
|         - | 7051 | ` * $num_req` |
|         - | 7052 | ` *  Specifies how many entries you want to pick.` |
|         - | 7053 | ` * Return` |
|         - | 7054 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 7055 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 7056 | ` *  NULL is returned on failure.` |
|         - | 7057 | ` */` |
|        36 | 7058 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7059 | `{` |
|         - | 7060 | `	ph7_hashmap_node *pNode;` |
|         - | 7061 | `	ph7_hashmap *pMap;` |
|        37 | 7062 | `	int nItem = 1;` |
|        37 | 7063 | `	if( nArg < 1 ){` |
|         - | 7064 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7065 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7066 | `		return PH7_OK;` |
|         - | 7067 | `	}` |
|         - | 7068 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 7069 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7070 | `		char zBuf[64];` |
|        10 | 7071 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7072 | `			"TypeError",` |
|         - | 7073 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7074 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7075 | `			);` |
|         - | 7076 | `	}` |
|         - | 7077 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 7078 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 7079 | `	if( nArg > 1 ){` |
|        23 | 7080 | `		ph7_value *pNum = apArg[1];` |
|        22 | 7081 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 7082 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 7083 | `			char zBuf[64];` |
|       ! 0 | 7084 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7085 | `				"TypeError",` |
|         - | 7086 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 7087 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 7088 | `				);` |
|         - | 7089 | `		}` |
|        23 | 7090 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 7091 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 7092 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 7093 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 7094 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 7095 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 7096 | `			int len;` |
|         9 | 7097 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 7098 | `			sxi64 iLong; double dReal;` |
|         9 | 7099 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 7100 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 7101 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7102 | `					"TypeError",` |
|         - | 7103 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 7104 | `					);` |
|         - | 7105 | `			}` |
|         - | 7106 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 7107 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 7108 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 7109 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 7110 | `			}` |
|         3 | 7111 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 7112 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 7113 | `			nItem = (int)iLong;` |
|         2 | 7114 | `		}else{` |
|        15 | 7115 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 7116 | `		}` |
|         8 | 7117 | `	}` |
|         - | 7118 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 7119 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7120 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 7121 | `	if( pMap->nEntry < 1 ){` |
|         5 | 7122 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7123 | `			"ValueError",` |
|         - | 7124 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7125 | `			);` |
|         - | 7126 | `	}` |
|         - | 7127 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7128 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7129 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7130 | `			"ValueError",` |
|         - | 7131 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7132 | `			);` |
|         - | 7133 | `	}` |
|        13 | 7134 | `	if( nItem < 2 ){` |
|         - | 7135 | `		sxu32 nEntry;` |
|         - | 7136 | `		/* Select a random number */` |
|         9 | 7137 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7138 | `		/* Extract the desired entry.` |
|         - | 7139 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7140 | `		 */` |
|         9 | 7141 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         3 | 7142 | `			pNode = pMap->pLast;` |
|         3 | 7143 | `			nEntry = pMap->nEntry - nEntry;` |
|         3 | 7144 | `			if( nEntry > 1 ){` |
|       ! 0 | 7145 | `				for(;;){` |
|       ! 0 | 7146 | `					if( nEntry == 0 ){` |
|       ! 0 | 7147 | `						break;` |
|         - | 7148 | `					}` |
|         - | 7149 | `					/* Point to the previous entry */` |
|       ! 0 | 7150 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7151 | `					nEntry--;` |
|       ! 0 | 7152 | `				}` |
|       ! 0 | 7153 | `			}` |
|         3 | 7154 | `		}else{` |
|         7 | 7155 | `			pNode = pMap->pFirst;` |
|         3 | 7156 | `			for(;;){` |
|        10 | 7157 | `				if( nEntry == 0 ){` |
|         7 | 7158 | `					break;` |
|         - | 7159 | `				}` |
|         - | 7160 | `				/* Point to the next entry */` |
|         3 | 7161 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         3 | 7162 | `				nEntry--;` |
|       ! 0 | 7163 | `			}` |
|         - | 7164 | `		}` |
|         9 | 7165 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7166 | `			/* Int key */` |
|         7 | 7167 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7168 | `		}else{` |
|         - | 7169 | `			/* Blob key */` |
|         3 | 7170 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7171 | `		}` |
|         5 | 7172 | `	}else{` |
|         - | 7173 | `		ph7_value sKey,*pArray;` |
|         - | 7174 | `		ph7_hashmap *pDest;` |
|         - | 7175 | `		/* Create a new array */` |
|         5 | 7176 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7177 | `		if( pArray == 0 ){` |
|       ! 0 | 7178 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7179 | `			return PH7_OK;` |
|         - | 7180 | `		}` |
|         - | 7181 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7182 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7183 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7184 | `		/* Copy the first n items */` |
|         5 | 7185 | `		pNode = pMap->pFirst;` |
|         5 | 7186 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7187 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7188 | `		}` |
|        15 | 7189 | `		while( nItem > 0){` |
|        11 | 7190 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7191 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7192 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7193 | `			/* Point to the next entry */` |
|        11 | 7194 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7195 | `			nItem--;` |
|         1 | 7196 | `		}` |
|         - | 7197 | `		/* Shuffle the array */` |
|         5 | 7198 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7199 | `		/* Rehash node */` |
|         5 | 7200 | `		HashmapSortRehash(pDest);` |
|         - | 7201 | `		/* Return the random array */` |
|         5 | 7202 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7203 | `	}` |
|        13 | 7204 | `	return PH7_OK;` |
|        19 | 7205 | `}` |
|         - | 7206 | `/*` |
|         - | 7207 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7208 | ` *  Split an array into chunks.` |
|         - | 7209 | ` * Parameters` |
|         - | 7210 | ` * $input` |
|         - | 7211 | ` *   The array to work on` |
|         - | 7212 | ` * $size` |
|         - | 7213 | ` *   The size of each chunk` |
|         - | 7214 | ` * $preserve_keys` |
|         - | 7215 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7216 | ` *   the chunk numerically.` |
|         - | 7217 | ` * Return` |
|         - | 7218 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7219 | ` *  zero, with each dimension containing size elements.` |
|         - | 7220 | ` */` |
|        36 | 7221 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7222 | `{` |
|         - | 7223 | `	ph7_value *pArray,*pChunk;` |
|         - | 7224 | `	ph7_hashmap_node *pEntry;` |
|         - | 7225 | `	ph7_hashmap *pMap;` |
|         - | 7226 | `	int bPreserve;` |
|         - | 7227 | `	sxu32 nChunk;` |
|         - | 7228 | `	sxu32 nSize;` |
|         - | 7229 | `	sxu32 n;` |
|         - | 7230 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7231 | `	if( nArg < 2 ){` |
|         - | 7232 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7233 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7234 | `			"ArgumentCountError",` |
|         - | 7235 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7236 | `			nArg` |
|         - | 7237 | `			);` |
|         - | 7238 | `	}` |
|        41 | 7239 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7240 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7241 | `			"TypeError",` |
|         - | 7242 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7243 | `			ph7_type_name(apArg[0])` |
|         - | 7244 | `			);` |
|         - | 7245 | `	}` |
|         - | 7246 | `	/* Create a new array */` |
|        38 | 7247 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7248 | `	if( pArray == 0 ){` |
|       ! 0 | 7249 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7250 | `		return PH7_OK;` |
|         - | 7251 | `	}` |
|         - | 7252 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7253 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7254 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7255 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7256 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7257 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7258 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7259 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7260 | `			"TypeError",` |
|         - | 7261 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7262 | `			ph7_type_name(apArg[1])` |
|         - | 7263 | `			);` |
|         - | 7264 | `	}` |
|         - | 7265 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7266 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7267 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7268 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7269 | `		int len;` |
|         3 | 7270 | `		sxu8 bReal = FALSE;` |
|         3 | 7271 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7272 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7273 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7274 | `				"TypeError",` |
|         - | 7275 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7276 | `				);` |
|         - | 7277 | `		}` |
|       ! 0 | 7278 | `		if( bReal ){` |
|         - | 7279 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7280 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7281 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7282 | `				zStr` |
|         - | 7283 | `				);` |
|       ! 0 | 7284 | `		}` |
|       ! 0 | 7285 | `	}` |
|         - | 7286 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7287 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7288 | `	 * later via ph7_value_to_int. */` |
|        35 | 7289 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7290 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7291 | `		sxi64 i = (sxi64)d;` |
|         3 | 7292 | `		if( d != (double)i ){` |
|         4 | 7293 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7294 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7295 | `				d` |
|         - | 7296 | `				);` |
|         1 | 7297 | `		}` |
|         1 | 7298 | `	}` |
|         - | 7299 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7300 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7301 | `	{` |
|        35 | 7302 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7303 | `		if( nSizeSigned < 1 ){` |
|         - | 7304 | `			/* size <= 0 -> ValueError */` |
|         6 | 7305 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7306 | `				"ValueError",` |
|         - | 7307 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7308 | `				);` |
|         - | 7309 | `		}` |
|        29 | 7310 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7311 | `	}` |
|        29 | 7312 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7313 | `		/* Return the whole array */` |
|         3 | 7314 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7315 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7316 | `		return PH7_OK;` |
|         - | 7317 | `	}` |
|        27 | 7318 | `	bPreserve = 0;` |
|        27 | 7319 | `	if( nArg > 2 ){` |
|         - | 7320 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7321 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7322 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7323 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7324 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7325 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7326 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7327 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7328 | `				"TypeError",` |
|         - | 7329 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7330 | `				ph7_type_name(apArg[2])` |
|         - | 7331 | `				);` |
|         - | 7332 | `		}` |
|        21 | 7333 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7334 | `	}` |
|         - | 7335 | `	/* Start processing */` |
|        27 | 7336 | `	pEntry = pMap->pFirst;` |
|        27 | 7337 | `	nChunk = 0;` |
|        27 | 7338 | `	pChunk = 0;` |
|        27 | 7339 | `	n = pMap->nEntry;` |
|        56 | 7340 | `	for( ;; ){` |
|       113 | 7341 | `		if( n < 1 ){` |
|         - | 7342 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7343 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7344 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7345 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7346 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7347 | `			 * exists. */` |
|        27 | 7348 | `			if( pChunk ){` |
|        27 | 7349 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7350 | `			}` |
|        27 | 7351 | `			break;` |
|         - | 7352 | `		}` |
|        87 | 7353 | `		if( nChunk < 1 ){` |
|        71 | 7354 | `			if( pChunk ){` |
|         - | 7355 | `				/* Put the first chunk */` |
|        45 | 7356 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7357 | `			}` |
|         - | 7358 | `			/* Create a new dimension */` |
|        71 | 7359 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7360 | `												   * will be automatically released as soon we return` |
|         - | 7361 | `												   * from this function */` |
|        71 | 7362 | `			if( pChunk == 0 ){` |
|       ! 0 | 7363 | `				break;` |
|         - | 7364 | `			}` |
|        71 | 7365 | `			nChunk = nSize;` |
|        35 | 7366 | `		}` |
|         - | 7367 | `		/* Insert the entry */` |
|        87 | 7368 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7369 | `		/* Point to the next entry */` |
|        87 | 7370 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7371 | `		nChunk--;` |
|        87 | 7372 | `		n--;` |
|         1 | 7373 | `	}` |
|         - | 7374 | `	/* Return the multidimensional array */` |
|        27 | 7375 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7376 | `	return PH7_OK;` |
|        23 | 7377 | `}` |
|         - | 7378 | `/*` |
|         - | 7379 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7380 | ` *  Pad array to the specified length with a value.` |
|         - | 7381 | ` * $input` |
|         - | 7382 | ` *   Initial array of values to pad.` |
|         - | 7383 | ` * $pad_size` |
|         - | 7384 | ` *   New size of the array.` |
|         - | 7385 | ` * $pad_value` |
|         - | 7386 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7387 | ` */` |
|         - | 7388 | `/*` |
|         - | 7389 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7390 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7391 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7392 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7393 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7394 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7395 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7396 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7397 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7398 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7399 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7400 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7401 | ` */` |
|        50 | 7402 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7403 | `	ph7_context *pCtx,` |
|         - | 7404 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7405 | `	int iArg,              /* 1-based argument position */` |
|         - | 7406 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7407 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7408 | `	)` |
|         1 | 7409 | `{` |
|        51 | 7410 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7411 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7412 | `			"ValueError",` |
|         - | 7413 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7414 | `			zFunc,iArg,zParam` |
|         - | 7415 | `			);` |
|         - | 7416 | `	}` |
|        37 | 7417 | `	return SXRET_OK;` |
|        26 | 7418 | `}` |
|        62 | 7419 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7420 | `{` |
|         - | 7421 | `	ph7_hashmap *pMap;` |
|         - | 7422 | `	ph7_value *pArray;` |
|         - | 7423 | `	sxi64 iLen,iAbs;` |
|         - | 7424 | `	int nEntry;` |
|         - | 7425 | `	sxi32 rc;` |
|        65 | 7426 | `	if( nArg != 3 ){` |
|         4 | 7427 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7428 | `			"ArgumentCountError",` |
|         - | 7429 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7430 | `			nArg` |
|         - | 7431 | `			);` |
|         - | 7432 | `	}` |
|        62 | 7433 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7434 | `		char zBuf[64];` |
|        11 | 7435 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7436 | `			"TypeError",` |
|         - | 7437 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7438 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7439 | `			);` |
|         - | 7440 | `	}` |
|         - | 7441 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7442 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7443 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7444 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7445 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7446 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7447 | `		char zBuf[64];` |
|       ! 0 | 7448 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7449 | `			"TypeError",` |
|         - | 7450 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7451 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7452 | `			);` |
|         - | 7453 | `	}` |
|        55 | 7454 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7455 | `		int nStr;` |
|        11 | 7456 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7457 | `		sxi64 iLong; double dReal;` |
|        11 | 7458 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7459 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7460 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7461 | `				"TypeError",` |
|         - | 7462 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7463 | `				);` |
|         - | 7464 | `		}` |
|         7 | 7465 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7466 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7467 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7468 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7469 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7470 | `					"TypeError",` |
|         - | 7471 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7472 | `					);` |
|         - | 7473 | `			}` |
|         3 | 7474 | `			iLen = (sxi64)dReal;` |
|         3 | 7475 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7476 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7477 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7478 | `					zStr` |
|         - | 7479 | `					);` |
|       ! 0 | 7480 | `			}` |
|         2 | 7481 | `		}else{` |
|         5 | 7482 | `			iLen = iLong;` |
|         - | 7483 | `		}` |
|         4 | 7484 | `	}else{` |
|        45 | 7485 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7486 | `	}` |
|         - | 7487 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7488 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7489 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7490 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7491 | `	 * overflow). */` |
|        51 | 7492 | `	iAbs = iLen;` |
|        51 | 7493 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7494 | `		iAbs = -iAbs;` |
|         7 | 7495 | `	}` |
|        51 | 7496 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7497 | `	if( rc != SXRET_OK ){` |
|        15 | 7498 | `		return rc;` |
|         - | 7499 | `	}` |
|        37 | 7500 | `	nEntry = (int)iLen;` |
|         - | 7501 | `	/* Create a new array */` |
|        37 | 7502 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7503 | `	if( pArray == 0 ){` |
|       ! 0 | 7504 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7505 | `	}` |
|        37 | 7506 | `	if( nEntry < 0 ){` |
|        11 | 7507 | `		nEntry = -nEntry;` |
|        11 | 7508 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7509 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7510 | `			/* Insert given items first */` |
|        25 | 7511 | `			while( nEntry > 0 ){` |
|        19 | 7512 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7513 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7514 | `				}` |
|        19 | 7515 | `				nEntry--;` |
|         1 | 7516 | `			}` |
|         - | 7517 | `			/* Merge the two arrays */` |
|         7 | 7518 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7519 | `		}else{` |
|         5 | 7520 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7521 | `		}` |
|        32 | 7522 | `	}else if( nEntry > 0 ){` |
|        25 | 7523 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7524 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7525 | `			/* Merge the two arrays first */` |
|        19 | 7526 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7527 | `			/* Insert given items */` |
|       275 | 7528 | `			while( nEntry > 0 ){` |
|       257 | 7529 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7530 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7531 | `				}` |
|       257 | 7532 | `				nEntry--;` |
|         1 | 7533 | `			}` |
|        10 | 7534 | `		}else{` |
|         7 | 7535 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7536 | `		}` |
|        13 | 7537 | `	}else{` |
|         - | 7538 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7539 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7540 | `	}` |
|         - | 7541 | `	/* Return the new array */` |
|        37 | 7542 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7543 | `	return PH7_OK;` |
|        34 | 7544 | `}` |
|         - | 7545 | `/*` |
|         - | 7546 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7547 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7548 | ` * Parameters` |
|         - | 7549 | ` * $array` |
|         - | 7550 | ` *   The array in which elements are replaced.` |
|         - | 7551 | ` * $array1` |
|         - | 7552 | ` *   The array from which elements will be extracted.` |
|         - | 7553 | ` * ....` |
|         - | 7554 | ` *  More arrays from which elements will be extracted.` |
|         - | 7555 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7556 | ` * Return` |
|         - | 7557 | ` *  Returns an array.` |
|         - | 7558 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7559 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7560 | ` */` |
|        20 | 7561 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7562 | `{` |
|         - | 7563 | `	ph7_hashmap *pMap;` |
|         - | 7564 | `	ph7_value *pArray;` |
|         - | 7565 | `	int i;` |
|        23 | 7566 | `	if( nArg < 1 ){` |
|       ! 0 | 7567 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7568 | `			"ArgumentCountError",` |
|         - | 7569 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7570 | `			);` |
|         - | 7571 | `	}` |
|        23 | 7572 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7573 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7574 | `			"TypeError",` |
|         - | 7575 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7576 | `			ph7_type_name(apArg[0])` |
|         - | 7577 | `			);` |
|         - | 7578 | `	}` |
|         - | 7579 | `	/* Create a new array */` |
|        20 | 7580 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7581 | `	if( pArray == 0 ){` |
|       ! 0 | 7582 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7583 | `		return PH7_OK;` |
|         - | 7584 | `	}` |
|         - | 7585 | `	/* Overwrite from the first array */` |
|        20 | 7586 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7587 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7588 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7589 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7590 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7591 | `			/* Type mismatch -> TypeError */` |
|         4 | 7592 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7593 | `				"TypeError",` |
|         - | 7594 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7595 | `				i + 1,` |
|         2 | 7596 | `				ph7_type_name(apArg[i])` |
|         - | 7597 | `				);` |
|         - | 7598 | `		}` |
|         - | 7599 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7600 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7601 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7602 | `	}` |
|         - | 7603 | `	/* Return the new array */` |
|        17 | 7604 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7605 | `	return PH7_OK;` |
|        13 | 7606 | `}` |
|         - | 7607 | `/*` |
|         - | 7608 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7609 | ` *  Filters elements of an array using a callback function.` |
|         - | 7610 | ` * Parameters` |
|         - | 7611 | ` *  $input` |
|         - | 7612 | ` *    The array to iterate over` |
|         - | 7613 | ` * $callback` |
|         - | 7614 | ` *    The callback function to use` |
|         - | 7615 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7616 | ` *    will be removed.` |
|         - | 7617 | ` * Return` |
|         - | 7618 | ` *  The filtered array.` |
|         - | 7619 | ` */` |
|        30 | 7620 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7621 | `{` |
|         - | 7622 | `	ph7_hashmap_node *pEntry;` |
|         - | 7623 | `	ph7_hashmap *pMap;` |
|         - | 7624 | `	ph7_value *pArray;` |
|         - | 7625 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7626 | `	ph7_value *pValue;` |
|         - | 7627 | `	sxi32 rc;` |
|         - | 7628 | `	int keep;` |
|         - | 7629 | `	sxu32 n;` |
|        32 | 7630 | `	if( nArg < 1 ){` |
|         - | 7631 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7632 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7633 | `		return PH7_OK;` |
|         - | 7634 | `	}` |
|         - | 7635 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7636 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7637 | `		char zBuf[64];` |
|        19 | 7638 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7639 | `			"TypeError",` |
|         - | 7640 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7641 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7642 | `			);` |
|         - | 7643 | `	}` |
|         - | 7644 | `	/* Create a new array */` |
|        20 | 7645 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7646 | `	if( pArray == 0 ){` |
|       ! 0 | 7647 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7648 | `		return PH7_OK;` |
|         - | 7649 | `	}` |
|         - | 7650 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7651 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7652 | `	pEntry = pMap->pFirst;` |
|        20 | 7653 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7654 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7655 | `	/* Perform the requested operation */` |
|        78 | 7656 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7657 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7658 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7659 | `		if( pValue == 0 ){` |
|         - | 7660 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7661 | `			keep = FALSE;` |
|        64 | 7662 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7663 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7664 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7665 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7666 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7667 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7668 | `					int len;` |
|         3 | 7669 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7670 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7671 | `						"TypeError",` |
|         - | 7672 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7673 | `						zName` |
|         - | 7674 | `						);` |
|       ! 0 | 7675 | `				}else{` |
|       ! 0 | 7676 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7677 | `						"TypeError",` |
|         - | 7678 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7679 | `						ph7_type_name(apArg[1])` |
|         - | 7680 | `						);` |
|         - | 7681 | `				}` |
|         - | 7682 | `			}` |
|        33 | 7683 | `			keep = FALSE;` |
|        33 | 7684 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7685 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7686 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7687 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7688 | `				return PH7_EXCEPTION;` |
|         - | 7689 | `			}` |
|        31 | 7690 | `			if( rc == SXRET_OK ){` |
|         - | 7691 | `				/* Perform a boolean cast */` |
|        31 | 7692 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7693 | `			}` |
|        31 | 7694 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7695 | `		}else{` |
|         - | 7696 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7697 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7698 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7699 | `			 */` |
|        29 | 7700 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7701 | `		}` |
|        59 | 7702 | `		if( keep ){` |
|         - | 7703 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7704 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7705 | `		}` |
|         - | 7706 | `		/* Point to the next entry */` |
|        59 | 7707 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7708 | `	}` |
|        15 | 7709 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7710 | `	return PH7_OK;` |
|        17 | 7711 | `}` |
|         - | 7712 | `/*` |
|         - | 7713 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7714 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7715 | ` * Parameters` |
|         - | 7716 | ` *  $callback` |
|         - | 7717 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7718 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7719 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7720 | ` *   are zipped together.` |
|         - | 7721 | ` *  $array` |
|         - | 7722 | ` *   The first array to run through the callback function.` |
|         - | 7723 | ` *  $arrays` |
|         - | 7724 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7725 | ` * Return` |
|         - | 7726 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7727 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7728 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7729 | ` *  padding shorter arrays with NULL.` |
|         - | 7730 | ` */` |
|        80 | 7731 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7732 | `{` |
|         - | 7733 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7734 | `	ph7_hashmap_node *pEntry;` |
|         - | 7735 | `	ph7_hashmap *pMap;` |
|         - | 7736 | `	ph7_vm *pVm;` |
|         - | 7737 | `	int bNullCallback;` |
|         - | 7738 | `	sxi32 rc;` |
|         - | 7739 | `	int i;` |
|         - | 7740 | `	sxu32 n;` |
|        84 | 7741 | `	if( nArg < 2 ){` |
|       ! 0 | 7742 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7743 | `			"ArgumentCountError",` |
|         - | 7744 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7745 | `			nArg` |
|         - | 7746 | `			);` |
|         - | 7747 | `	}` |
|        84 | 7748 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        84 | 7749 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         8 | 7750 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         6 | 7751 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         8 | 7752 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7753 | `				"TypeError",` |
|         - | 7754 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7755 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7756 | `				zFunc` |
|         - | 7757 | `				);` |
|         - | 7758 | `		}` |
|         3 | 7759 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7760 | `			"TypeError",` |
|         - | 7761 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7762 | `			"no array or string given"` |
|         - | 7763 | `			);` |
|         - | 7764 | `	}` |
|         - | 7765 | `	/* Every remaining argument must be an array */` |
|       162 | 7766 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        90 | 7767 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7768 | `			if( i == 1 ){` |
|         4 | 7769 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7770 | `					"TypeError",` |
|         - | 7771 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7772 | `					ph7_type_name(apArg[1])` |
|         - | 7773 | `					);` |
|         - | 7774 | `			}` |
|       ! 0 | 7775 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7776 | `				"TypeError",` |
|         - | 7777 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7778 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7779 | `				);` |
|         - | 7780 | `		}` |
|        45 | 7781 | `	}` |
|        75 | 7782 | `	pVm = pCtx->pVm;` |
|         - | 7783 | `	/* Create a new array */` |
|        75 | 7784 | `	pArray = ph7_context_new_array(pCtx);` |
|        75 | 7785 | `	if( pArray == 0 ){` |
|       ! 0 | 7786 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7787 | `		return PH7_OK;` |
|         - | 7788 | `	}` |
|        75 | 7789 | `	PH7_MemObjInit(pVm,&sResult);` |
|        75 | 7790 | `	PH7_MemObjInit(pVm,&sKey);` |
|        75 | 7791 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        75 | 7792 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        75 | 7793 | `	if( nArg == 2 ){` |
|         - | 7794 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        65 | 7795 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        65 | 7796 | `		pEntry = pMap->pFirst;` |
|       213 | 7797 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7798 | `			/* Extract the node value */` |
|       155 | 7799 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|       155 | 7800 | `			if( pValue ){` |
|         - | 7801 | `				/* Extract the node key */` |
|       155 | 7802 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       155 | 7803 | `				if( bNullCallback ){` |
|         - | 7804 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7805 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7806 | `				}else{` |
|         - | 7807 | `					/* Invoke the supplied callback */` |
|       145 | 7808 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|       145 | 7809 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7810 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7811 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7812 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7813 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7814 | `						return PH7_EXCEPTION;` |
|         - | 7815 | `					}` |
|         - | 7816 | `					/* Insert the callback return value */` |
|       141 | 7817 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7818 | `				}` |
|       151 | 7819 | `				PH7_MemObjRelease(&sKey);` |
|       151 | 7820 | `				PH7_MemObjRelease(&sResult);` |
|        74 | 7821 | `			}` |
|         - | 7822 | `			/* Point to the next entry */` |
|       151 | 7823 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        77 | 7824 | `		}` |
|        32 | 7825 | `	}else{` |
|         - | 7826 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7827 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7828 | `		int nArrays = nArg - 1;` |
|         - | 7829 | `		ph7_hashmap_node **apCur;` |
|         - | 7830 | `		ph7_value **apCallArg;` |
|         - | 7831 | `		ph7_value sNull;` |
|        11 | 7832 | `		sxu32 nMax = 0;` |
|        11 | 7833 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7834 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7835 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7836 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7837 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7838 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7839 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7840 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7841 | `			return PH7_OK;` |
|         - | 7842 | `		}` |
|        11 | 7843 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7844 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7845 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7846 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7847 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7848 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7849 | `				nMax = pMap->nEntry;` |
|         6 | 7850 | `			}` |
|        12 | 7851 | `		}` |
|        35 | 7852 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7853 | `			ph7_value *pZip = 0;` |
|        25 | 7854 | `			if( bNullCallback ){` |
|         - | 7855 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7856 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7857 | `			}` |
|        79 | 7858 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7859 | `				ph7_value *pv = &sNull;` |
|        55 | 7860 | `				if( apCur[i] ){` |
|        53 | 7861 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7862 | `					if( pNodeVal ){` |
|        53 | 7863 | `						pv = pNodeVal;` |
|        26 | 7864 | `					}` |
|        53 | 7865 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7866 | `				}` |
|        55 | 7867 | `				if( bNullCallback ){` |
|         9 | 7868 | `					if( pZip ){` |
|         9 | 7869 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7870 | `					}` |
|         5 | 7871 | `				}else{` |
|        47 | 7872 | `					apCallArg[i] = pv;` |
|         - | 7873 | `				}` |
|        28 | 7874 | `			}` |
|        25 | 7875 | `			if( bNullCallback ){` |
|         5 | 7876 | `				if( pZip ){` |
|         5 | 7877 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7878 | `				}` |
|         3 | 7879 | `			}else{` |
|        21 | 7880 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7881 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7882 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7883 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7884 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7885 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7886 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7887 | `					return PH7_EXCEPTION;` |
|         - | 7888 | `				}` |
|        21 | 7889 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7890 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7891 | `			}` |
|        13 | 7892 | `		}` |
|        11 | 7893 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7894 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7895 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7896 | `	}` |
|        71 | 7897 | `	PH7_MemObjRelease(&sKey);` |
|        71 | 7898 | `	PH7_MemObjRelease(&sResult);` |
|        71 | 7899 | `	ph7_result_value(pCtx,pArray);` |
|        71 | 7900 | `	return PH7_OK;` |
|        44 | 7901 | `}` |
|         - | 7902 | `/*` |
|         - | 7903 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7904 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7905 | ` * Parameters` |
|         - | 7906 | ` *  $array` |
|         - | 7907 | ` *   The input array.` |
|         - | 7908 | ` *  $callback` |
|         - | 7909 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7910 | ` *  $initial` |
|         - | 7911 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7912 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7913 | ` * Return` |
|         - | 7914 | ` *  Returns the resulting value.` |
|         - | 7915 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7916 | ` */` |
|        30 | 7917 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7918 | `{` |
|         - | 7919 | `	ph7_hashmap_node *pEntry;` |
|         - | 7920 | `	ph7_hashmap *pMap;` |
|         - | 7921 | `	ph7_value *pValue;` |
|         - | 7922 | `	ph7_value sResult;` |
|         - | 7923 | `	sxi32 rc;` |
|         - | 7924 | `	sxu32 n;` |
|        35 | 7925 | `	if( nArg < 2 ){` |
|       ! 0 | 7926 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7927 | `			"ArgumentCountError",` |
|         - | 7928 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7929 | `			nArg` |
|         - | 7930 | `			);` |
|         - | 7931 | `	}` |
|        35 | 7932 | `	if( nArg > 3 ){` |
|         4 | 7933 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7934 | `			"ArgumentCountError",` |
|         - | 7935 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7936 | `			nArg` |
|         - | 7937 | `			);` |
|         - | 7938 | `	}` |
|        33 | 7939 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7940 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7941 | `			"TypeError",` |
|         - | 7942 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7943 | `			ph7_type_name(apArg[0])` |
|         - | 7944 | `			);` |
|         - | 7945 | `	}` |
|        31 | 7946 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7947 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7948 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7949 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7950 | `				"TypeError",` |
|         - | 7951 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7952 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7953 | `				zFunc` |
|         - | 7954 | `				);` |
|         - | 7955 | `		}` |
|         9 | 7956 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7957 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7958 | `				"TypeError",` |
|         - | 7959 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7960 | `				"array callback must have exactly two members"` |
|         - | 7961 | `				);` |
|         - | 7962 | `		}` |
|         6 | 7963 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7964 | `			"TypeError",` |
|         - | 7965 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7966 | `			"no array or string given"` |
|         - | 7967 | `			);` |
|         - | 7968 | `	}` |
|         - | 7969 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7970 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7971 | `	/* Assume a NULL initial value */` |
|        19 | 7972 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7973 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7974 | `	if( nArg > 2 ){` |
|         - | 7975 | `		/* Set the initial value */` |
|        13 | 7976 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7977 | `	}` |
|         - | 7978 | `	/* Perform the requested operation */` |
|        19 | 7979 | `	pEntry = pMap->pFirst;` |
|        55 | 7980 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7981 | `		/* Extract the node value */` |
|        39 | 7982 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7983 | `		/* Invoke the supplied callback */` |
|        39 | 7984 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7985 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7986 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7987 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7988 | `			return PH7_EXCEPTION;` |
|         - | 7989 | `		}` |
|         - | 7990 | `		/* Point to the next entry */` |
|        37 | 7991 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7992 | `	}` |
|        17 | 7993 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7994 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7995 | `	return PH7_OK;` |
|        20 | 7996 | `}` |
|         - | 7997 | `/*` |
|         - | 7998 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7999 | ` *  Apply a user function to every member of an array.` |
|         - | 8000 | ` * Parameters` |
|         - | 8001 | ` *  $array` |
|         - | 8002 | ` *   The input array.` |
|         - | 8003 | ` *  $funcname` |
|         - | 8004 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8005 | ` *   the first, and the key/index second.` |
|         - | 8006 | ` * Note:` |
|         - | 8007 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8008 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8009 | ` *  be made in the original array itself.` |
|         - | 8010 | ` *  $userdata` |
|         - | 8011 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8012 | ` *   to the callback funcname.` |
|         - | 8013 | ` * Return` |
|         - | 8014 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8015 | ` */` |
|        36 | 8016 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8017 | `{` |
|         - | 8018 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 8019 | `	ph7_hashmap_node *pEntry;` |
|         - | 8020 | `	ph7_hashmap *pMap;` |
|         - | 8021 | `	sxu32 n;` |
|        41 | 8022 | `	if( nArg < 2 ){` |
|       ! 0 | 8023 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8024 | `			"ArgumentCountError",` |
|         - | 8025 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 8026 | `			nArg` |
|         - | 8027 | `			);` |
|         - | 8028 | `	}` |
|        41 | 8029 | `	if( nArg > 3 ){` |
|         4 | 8030 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8031 | `			"ArgumentCountError",` |
|         - | 8032 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 8033 | `			nArg` |
|         - | 8034 | `			);` |
|         - | 8035 | `	}` |
|        39 | 8036 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8037 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8038 | `			"TypeError",` |
|         - | 8039 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8040 | `			ph7_type_name(apArg[0])` |
|         - | 8041 | `			);` |
|         - | 8042 | `	}` |
|        37 | 8043 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        17 | 8044 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         6 | 8045 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         8 | 8046 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8047 | `				"TypeError",` |
|         - | 8048 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8049 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 8050 | `				zFunc` |
|         - | 8051 | `				);` |
|         - | 8052 | `		}` |
|        12 | 8053 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8054 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8055 | `				"TypeError",` |
|         - | 8056 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8057 | `				"array callback must have exactly two members"` |
|         - | 8058 | `				);` |
|         - | 8059 | `		}` |
|         6 | 8060 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8061 | `			"TypeError",` |
|         - | 8062 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8063 | `			"no array or string given"` |
|         - | 8064 | `			);` |
|         - | 8065 | `	}` |
|        21 | 8066 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 8067 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 8068 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 8069 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8070 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 8071 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 8072 | `	/* Perform the desired operation */` |
|        21 | 8073 | `	pEntry = pMap->pFirst;` |
|        61 | 8074 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8075 | `		/* Extract the node value */` |
|        43 | 8076 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 8077 | `		if( pValue ){` |
|         - | 8078 | `			sxi32 rcW;` |
|         - | 8079 | `			/* Extract the entry key */` |
|        43 | 8080 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8081 | `			/* Invoke the supplied callback */` |
|        43 | 8082 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 8083 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 8084 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 8085 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 8086 | `				return PH7_EXCEPTION;` |
|         - | 8087 | `			}` |
|        20 | 8088 | `		}` |
|         - | 8089 | `		/* Point to the next entry */` |
|        41 | 8090 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 8091 | `	}` |
|         - | 8092 | `	/* All done, return TRUE */` |
|        19 | 8093 | `	ph7_result_bool(pCtx,1);` |
|        19 | 8094 | `	return PH7_OK;` |
|        23 | 8095 | `}` |
|         - | 8096 | `/*` |
|         - | 8097 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 8098 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 8099 | ` */` |
|        22 | 8100 | `static sxi32 HashmapWalkRecursive(` |
|         - | 8101 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 8102 | `	ph7_value *pCallback, /* User callback */` |
|         - | 8103 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 8104 | `	int iNest             /* Nesting level */` |
|         - | 8105 | `	)` |
|         1 | 8106 | `{` |
|         - | 8107 | `	ph7_hashmap_node *pEntry;` |
|         - | 8108 | `	ph7_value *pValue,sKey;` |
|         - | 8109 | `	sxi32 rc;` |
|         - | 8110 | `	sxu32 n;` |
|         - | 8111 | `	/* Iterate through hashmap entries */` |
|        23 | 8112 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 8113 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 8114 | `	pEntry = pMap->pFirst;` |
|        59 | 8115 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8116 | `		/* Extract the node value */` |
|        37 | 8117 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 8118 | `		if( pValue ){` |
|        37 | 8119 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 8120 | `				if( iNest < 32 ){` |
|         - | 8121 | `					/* Recurse */` |
|        11 | 8122 | `					iNest++;` |
|        11 | 8123 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8124 | `					iNest--;` |
|        11 | 8125 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8126 | `						return PH7_EXCEPTION;` |
|         - | 8127 | `					}` |
|         5 | 8128 | `				}` |
|         6 | 8129 | `			}else{` |
|         - | 8130 | `				/* Extract the node key */` |
|        27 | 8131 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8132 | `				/* Invoke the supplied callback */` |
|        27 | 8133 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8134 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8135 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8136 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8137 | `					return PH7_EXCEPTION;` |
|         - | 8138 | `				}` |
|         - | 8139 | `			}` |
|        18 | 8140 | `		}` |
|         - | 8141 | `		/* Point to the next entry */` |
|        37 | 8142 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8143 | `	}` |
|        23 | 8144 | `	return PH7_OK;` |
|        12 | 8145 | `}` |
|         - | 8146 | `/*` |
|         - | 8147 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8148 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8149 | ` * Parameters` |
|         - | 8150 | ` *  $array` |
|         - | 8151 | ` *   The input array.` |
|         - | 8152 | ` *  $funcname` |
|         - | 8153 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8154 | ` *   the first, and the key/index second.` |
|         - | 8155 | ` * Note:` |
|         - | 8156 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8157 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8158 | ` *  be made in the original array itself.` |
|         - | 8159 | ` *  $userdata` |
|         - | 8160 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8161 | ` *   to the callback funcname.` |
|         - | 8162 | ` * Return` |
|         - | 8163 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8164 | ` */` |
|        26 | 8165 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8166 | `{` |
|         - | 8167 | `	ph7_hashmap *pMap;` |
|        31 | 8168 | `	if( nArg < 2 ){` |
|       ! 0 | 8169 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8170 | `			"ArgumentCountError",` |
|         - | 8171 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8172 | `			nArg` |
|         - | 8173 | `			);` |
|         - | 8174 | `	}` |
|        31 | 8175 | `	if( nArg > 3 ){` |
|         4 | 8176 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8177 | `			"ArgumentCountError",` |
|         - | 8178 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8179 | `			nArg` |
|         - | 8180 | `			);` |
|         - | 8181 | `	}` |
|        29 | 8182 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8183 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8184 | `			"TypeError",` |
|         - | 8185 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8186 | `			ph7_type_name(apArg[0])` |
|         - | 8187 | `			);` |
|         - | 8188 | `	}` |
|        27 | 8189 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8190 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8191 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8192 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8193 | `				"TypeError",` |
|         - | 8194 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8195 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8196 | `				zFunc` |
|         - | 8197 | `				);` |
|         - | 8198 | `		}` |
|        12 | 8199 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8200 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8201 | `				"TypeError",` |
|         - | 8202 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8203 | `				"array callback must have exactly two members"` |
|         - | 8204 | `				);` |
|         - | 8205 | `		}` |
|         6 | 8206 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8207 | `			"TypeError",` |
|         - | 8208 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8209 | `			"no array or string given"` |
|         - | 8210 | `			);` |
|         - | 8211 | `	}` |
|         - | 8212 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8213 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8214 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8215 | `	/* Perform the desired operation */` |
|        13 | 8216 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8217 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8218 | `		return PH7_EXCEPTION;` |
|         - | 8219 | `	}` |
|         - | 8220 | `	/* All done, return TRUE */` |
|        13 | 8221 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8222 | `	return PH7_OK;` |
|        18 | 8223 | `}` |
|         - | 8224 | `/*` |
|         - | 8225 | ` * bool array_is_list(array $array)` |
|         - | 8226 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8227 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8228 | ` * Return` |
|         - | 8229 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8230 | ` */` |
|         - | 8231 | `/*` |
|         - | 8232 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8233 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8234 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8235 | ` */` |
|       336 | 8236 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         3 | 8237 | `{` |
|       339 | 8238 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       339 | 8239 | `	sxi64 iExpect = 0;` |
|         - | 8240 | `	sxu32 n;` |
|       777 | 8241 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       583 | 8242 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8243 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       145 | 8244 | `			return 0;` |
|         - | 8245 | `		}` |
|       441 | 8246 | `		++iExpect;` |
|       441 | 8247 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       222 | 8248 | `	}` |
|       197 | 8249 | `	return 1;` |
|       171 | 8250 | `}` |
|        12 | 8251 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8252 | `{` |
|        13 | 8253 | `	if( nArg < 1 ){` |
|       ! 0 | 8254 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8255 | `			"ArgumentCountError",` |
|         - | 8256 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8257 | `			);` |
|         - | 8258 | `	}` |
|        13 | 8259 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8260 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8261 | `			"TypeError",` |
|         - | 8262 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8263 | `			ph7_type_name(apArg[0])` |
|         - | 8264 | `			);` |
|         - | 8265 | `	}` |
|        13 | 8266 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8267 | `	return PH7_OK;` |
|         7 | 8268 | `}` |
|         - | 8269 | `/*` |
|         - | 8270 | ` * mixed array_first(array $array)` |
|         - | 8271 | ` * mixed array_last(array $array)` |
|         - | 8272 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8273 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8274 | ` *  untouched (unlike reset()/end()).` |
|         - | 8275 | ` */` |
|        18 | 8276 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8277 | `{` |
|         - | 8278 | `	ph7_hashmap *pMap;` |
|         - | 8279 | `	ph7_hashmap_node *pNode;` |
|         - | 8280 | `	ph7_value *pVal;` |
|        19 | 8281 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8282 | `	if( nArg < 1 ){` |
|       ! 0 | 8283 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8284 | `			"ArgumentCountError",` |
|         - | 8285 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8286 | `			zName` |
|         - | 8287 | `			);` |
|         - | 8288 | `	}` |
|        19 | 8289 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8290 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8291 | `			"TypeError",` |
|         - | 8292 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8293 | `			zName,` |
|         1 | 8294 | `			ph7_type_name(apArg[0])` |
|         - | 8295 | `			);` |
|         - | 8296 | `	}` |
|        17 | 8297 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8298 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8299 | `	if( pNode == 0 ){` |
|         - | 8300 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8301 | `		ph7_result_null(pCtx);` |
|         5 | 8302 | `		return PH7_OK;` |
|         - | 8303 | `	}` |
|        13 | 8304 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8305 | `	if( pVal ){` |
|        13 | 8306 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8307 | `	}else{` |
|       ! 0 | 8308 | `		ph7_result_null(pCtx);` |
|         - | 8309 | `	}` |
|        13 | 8310 | `	return PH7_OK;` |
|        10 | 8311 | `}` |
|         8 | 8312 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8313 | `{` |
|         9 | 8314 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8315 | `}` |
|        10 | 8316 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8317 | `{` |
|        11 | 8318 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8319 | `}` |
|         - | 8320 | `/*` |
|         - | 8321 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8322 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8323 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8324 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8325 | ` *  untouched.` |
|         - | 8326 | ` */` |
|        22 | 8327 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8328 | `{` |
|         - | 8329 | `	ph7_hashmap *pMap;` |
|         - | 8330 | `	ph7_hashmap_node *pNode;` |
|        23 | 8331 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8332 | `	if( nArg < 1 ){` |
|       ! 0 | 8333 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8334 | `			"ArgumentCountError",` |
|         - | 8335 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8336 | `			zName` |
|         - | 8337 | `			);` |
|         - | 8338 | `	}` |
|        23 | 8339 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8340 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8341 | `			"TypeError",` |
|         - | 8342 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8343 | `			zName,` |
|         1 | 8344 | `			ph7_type_name(apArg[0])` |
|         - | 8345 | `			);` |
|         - | 8346 | `	}` |
|        21 | 8347 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8348 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8349 | `	if( pNode == 0 ){` |
|         - | 8350 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8351 | `		ph7_result_null(pCtx);` |
|         5 | 8352 | `		return PH7_OK;` |
|         - | 8353 | `	}` |
|        17 | 8354 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8355 | `	return PH7_OK;` |
|        12 | 8356 | `}` |
|        10 | 8357 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8358 | `{` |
|        11 | 8359 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8360 | `}` |
|        12 | 8361 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8362 | `{` |
|        13 | 8363 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8364 | `}` |
|         - | 8365 | `/*` |
|         - | 8366 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8367 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8368 | ` * array_column() for both the column value and the index key.` |
|         - | 8369 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8370 | ` * container or the key is absent.` |
|         - | 8371 | ` */` |
|        32 | 8372 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8373 | `{` |
|        33 | 8374 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8375 | `		ph7_hashmap_node *pNode;` |
|        25 | 8376 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8377 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8378 | `		}` |
|        11 | 8379 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8380 | `		ph7_value sName;` |
|         - | 8381 | `		const char *zName;` |
|         - | 8382 | `		ph7_value *pAttr;` |
|         - | 8383 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8384 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8385 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8386 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8387 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8388 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8389 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8390 | `		return pAttr;` |
|         - | 8391 | `	}` |
|         5 | 8392 | `	return 0;` |
|        17 | 8393 | `}` |
|         - | 8394 | `/*` |
|         - | 8395 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8396 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8397 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8398 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8399 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8400 | ` *  Each row may be an array or an object.` |
|         - | 8401 | ` */` |
|        12 | 8402 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8403 | `{` |
|         - | 8404 | `	ph7_hashmap_node *pNode;` |
|         - | 8405 | `	ph7_hashmap *pMap;` |
|         - | 8406 | `	ph7_value *pArray;` |
|         - | 8407 | `	ph7_value *pRow;` |
|         - | 8408 | `	ph7_value *pCol;` |
|         - | 8409 | `	ph7_value *pIdx;` |
|         - | 8410 | `	int bWantCol;` |
|         - | 8411 | `	int bWantIdx;` |
|         - | 8412 | `	sxu32 n;` |
|        13 | 8413 | `	if( nArg < 2 ){` |
|       ! 0 | 8414 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8415 | `			"ArgumentCountError",` |
|         - | 8416 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8417 | `			nArg` |
|         - | 8418 | `			);` |
|         - | 8419 | `	}` |
|        13 | 8420 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8421 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8422 | `			"TypeError",` |
|         - | 8423 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8424 | `			ph7_type_name(apArg[0])` |
|         - | 8425 | `			);` |
|         - | 8426 | `	}` |
|        13 | 8427 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8428 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8429 | `	if( pArray == 0 ){` |
|       ! 0 | 8430 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8431 | `		return PH7_OK;` |
|         - | 8432 | `	}` |
|         - | 8433 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8434 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8435 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8436 | `	pNode = pMap->pFirst;` |
|        33 | 8437 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8438 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8439 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8440 | `		if( pRow == 0 ){` |
|       ! 0 | 8441 | `			continue;` |
|         - | 8442 | `		}` |
|        21 | 8443 | `		if( bWantCol ){` |
|        19 | 8444 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8445 | `			if( pCol == 0 ){` |
|         - | 8446 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8447 | `				continue;` |
|         - | 8448 | `			}` |
|         9 | 8449 | `		}else{` |
|         3 | 8450 | `			pCol = pRow;` |
|         - | 8451 | `		}` |
|        19 | 8452 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8453 | `		if( pIdx ){` |
|        13 | 8454 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8455 | `		}else{` |
|         7 | 8456 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8457 | `		}` |
|        10 | 8458 | `	}` |
|        13 | 8459 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8460 | `	return PH7_OK;` |
|         7 | 8461 | `}` |
|         - | 8462 | `/*` |
|         - | 8463 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8464 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8465 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8466 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8467 | ` */` |
|        28 | 8468 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8469 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8470 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8471 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8472 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8473 | `	)` |
|         1 | 8474 | `{` |
|         - | 8475 | `	ph7_hashmap_node *pEntry;` |
|         - | 8476 | `	ph7_hashmap *pMap;` |
|         - | 8477 | `	ph7_value *pValue;` |
|         - | 8478 | `	ph7_value *apCbArg[2];` |
|         - | 8479 | `	ph7_value sKey;` |
|         - | 8480 | `	ph7_value sResult;` |
|         - | 8481 | `	sxi32 rc;` |
|         - | 8482 | `	sxu32 n;` |
|        29 | 8483 | `	*ppMatch = 0;` |
|        29 | 8484 | `	if( nArg < 2 ){` |
|       ! 0 | 8485 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8486 | `			"ArgumentCountError",` |
|         - | 8487 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8488 | `			zName,nArg` |
|         - | 8489 | `			);` |
|         - | 8490 | `	}` |
|        29 | 8491 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8492 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8493 | `			"TypeError",` |
|         - | 8494 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8495 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8496 | `			);` |
|         - | 8497 | `	}` |
|        29 | 8498 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8499 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8500 | `			"TypeError",` |
|         - | 8501 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8502 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8503 | `			);` |
|         - | 8504 | `	}` |
|        29 | 8505 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8506 | `	pEntry = pMap->pFirst;` |
|        29 | 8507 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8508 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8509 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8510 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8511 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8512 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8513 | `		if( pValue ){` |
|         - | 8514 | `			/* The callback receives ($value, $key). */` |
|        59 | 8515 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8516 | `			apCbArg[0] = pValue;` |
|        59 | 8517 | `			apCbArg[1] = &sKey;` |
|        59 | 8518 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8519 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8520 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8521 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8522 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8523 | `				return PH7_EXCEPTION;` |
|         - | 8524 | `			}` |
|        59 | 8525 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8526 | `				*ppMatch = pEntry;` |
|        15 | 8527 | `				break;` |
|         - | 8528 | `			}` |
|        22 | 8529 | `		}` |
|        45 | 8530 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8531 | `	}` |
|        29 | 8532 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8533 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8534 | `	return PH7_OK;` |
|        15 | 8535 | `}` |
|         - | 8536 | `/*` |
|         - | 8537 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8538 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8539 | ` *  is truthy, or NULL if none match.` |
|         - | 8540 | ` */` |
|         6 | 8541 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8542 | `{` |
|         - | 8543 | `	ph7_hashmap_node *pMatch;` |
|         - | 8544 | `	ph7_value *pVal;` |
|         - | 8545 | `	sxi32 rc;` |
|         7 | 8546 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8547 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8548 | `		return rc;` |
|         - | 8549 | `	}` |
|         7 | 8550 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8551 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8552 | `	}else{` |
|         3 | 8553 | `		ph7_result_null(pCtx);` |
|         - | 8554 | `	}` |
|         7 | 8555 | `	return PH7_OK;` |
|         4 | 8556 | `}` |
|         - | 8557 | `/*` |
|         - | 8558 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8559 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8560 | ` *  is truthy, or NULL if none match.` |
|         - | 8561 | ` */` |
|         6 | 8562 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8563 | `{` |
|         - | 8564 | `	ph7_hashmap_node *pMatch;` |
|         - | 8565 | `	sxi32 rc;` |
|         7 | 8566 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8567 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8568 | `		return rc;` |
|         - | 8569 | `	}` |
|         7 | 8570 | `	if( pMatch == 0 ){` |
|         3 | 8571 | `		ph7_result_null(pCtx);` |
|         6 | 8572 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8573 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8574 | `	}else{` |
|         4 | 8575 | `		ph7_result_string(pCtx,` |
|         2 | 8576 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8577 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8578 | `	}` |
|         7 | 8579 | `	return PH7_OK;` |
|         4 | 8580 | `}` |
|         - | 8581 | `/*` |
|         - | 8582 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8583 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8584 | ` *  FALSE for an empty array.` |
|         - | 8585 | ` */` |
|         8 | 8586 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8587 | `{` |
|         - | 8588 | `	ph7_hashmap_node *pMatch;` |
|         - | 8589 | `	sxi32 rc;` |
|         9 | 8590 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8591 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8592 | `		return rc;` |
|         - | 8593 | `	}` |
|         9 | 8594 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8595 | `	return PH7_OK;` |
|         5 | 8596 | `}` |
|         - | 8597 | `/*` |
|         - | 8598 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8599 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8600 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8601 | ` */` |
|         8 | 8602 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8603 | `{` |
|         - | 8604 | `	ph7_hashmap_node *pMatch;` |
|         - | 8605 | `	sxi32 rc;` |
|         9 | 8606 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8607 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8608 | `		return rc;` |
|         - | 8609 | `	}` |
|         9 | 8610 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8611 | `	return PH7_OK;` |
|         5 | 8612 | `}` |
|         - | 8613 | `/*` |
|         - | 8614 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8615 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8616 | ` */` |
|         - | 8617 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8618 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8619 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8620 | `{` |
|        84 | 8621 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8622 | `	(void)pVm;` |
|        84 | 8623 | `	p->nCount++;` |
|        84 | 8624 | `	if( p->pArray ){` |
|         - | 8625 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8626 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8627 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8628 | `	}` |
|        84 | 8629 | `	return SXRET_OK;` |
|         4 | 8630 | `}` |
|         - | 8631 | `/*` |
|         - | 8632 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8633 | ` */` |
|        30 | 8634 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8635 | `{` |
|         - | 8636 | `	struct IterCollect sCol;` |
|         - | 8637 | `	ph7_value *pArray;` |
|         - | 8638 | `	sxi32 rc;` |
|        34 | 8639 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8640 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8641 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8642 | `	sCol.pArray = pArray;` |
|        34 | 8643 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8644 | `	sCol.nCount = 0;` |
|        34 | 8645 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8646 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8647 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8648 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8649 | `		sxu32 n;` |
|         9 | 8650 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8651 | `			ph7_value sKey, *pVal;` |
|         7 | 8652 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8653 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8654 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8655 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8656 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8657 | `			pEntry = pEntry->pPrev;` |
|         4 | 8658 | `		}` |
|         3 | 8659 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8660 | `		return PH7_OK;` |
|         - | 8661 | `	}` |
|        32 | 8662 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8663 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8664 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8665 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8666 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8667 | `			ph7_type_name(apArg[0]));` |
|         - | 8668 | `	}` |
|        30 | 8669 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8670 | `	return PH7_OK;` |
|        19 | 8671 | `}` |
|         - | 8672 | `/*` |
|         - | 8673 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8674 | ` */` |
|         8 | 8675 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8676 | `{` |
|         - | 8677 | `	struct IterCollect sCol;` |
|         - | 8678 | `	sxi32 rc;` |
|         9 | 8679 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8680 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8681 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8682 | `		return PH7_OK;` |
|         - | 8683 | `	}` |
|         7 | 8684 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8685 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8686 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8687 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8688 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8689 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8690 | `			ph7_type_name(apArg[0]));` |
|         - | 8691 | `	}` |
|         7 | 8692 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8693 | `	return PH7_OK;` |
|         5 | 8694 | `}` |
|         - | 8695 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8696 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8697 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8698 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8699 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8700 | `{` |
|        33 | 8701 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8702 | `	ph7_value sResult;` |
|         - | 8703 | `	SySet aArg;` |
|         - | 8704 | `	sxi32 rc;` |
|         - | 8705 | `	int bContinue;` |
|        16 | 8706 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8707 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8708 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8709 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8710 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8711 | `		sxu32 n;` |
|        17 | 8712 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8713 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8714 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8715 | `			pEntry = pEntry->pPrev;` |
|         5 | 8716 | `		}` |
|         4 | 8717 | `	}` |
|        33 | 8718 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8719 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8720 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8721 | `	SySetRelease(&aArg);` |
|        33 | 8722 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8723 | `	p->nCount++;` |
|        31 | 8724 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8725 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8726 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8727 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8728 | `}` |
|         - | 8729 | `/*` |
|         - | 8730 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8731 | ` */` |
|        12 | 8732 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8733 | `{` |
|         - | 8734 | `	struct IterApply sApp;` |
|         - | 8735 | `	sxi32 rc;` |
|        13 | 8736 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8737 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8738 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8739 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8740 | `	}` |
|        13 | 8741 | `	sApp.pCallback = apArg[1];` |
|        13 | 8742 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8743 | `	sApp.nCount = 0;` |
|        13 | 8744 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8745 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8746 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8747 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8748 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8749 | `			ph7_type_name(apArg[0]));` |
|         - | 8750 | `	}` |
|        11 | 8751 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8752 | `	return PH7_OK;` |
|         7 | 8753 | `}` |
|         - | 8754 | `/*` |
|         - | 8755 | ` * Table of hashmap functions.` |
|         - | 8756 | ` */` |
|         - | 8757 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8758 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8759 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8760 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8761 | `	{"count",             ph7_hashmap_count },` |
|         - | 8762 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8763 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8764 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8765 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8766 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8767 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8768 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8769 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8770 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8771 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8772 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8773 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8774 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8775 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8776 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8777 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8778 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8779 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8780 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8781 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8782 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8783 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8784 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8785 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8786 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8787 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8788 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8789 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8790 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8791 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8792 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8793 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8794 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8795 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8796 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8797 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8798 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8799 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8800 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8801 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8802 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8803 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8804 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8805 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8806 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8807 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8808 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8809 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8810 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8811 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8812 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8813 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8814 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8815 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8816 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8817 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8818 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8819 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8820 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8821 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8822 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8823 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8824 | `	{"current",           ph7_hashmap_current },` |
|         - | 8825 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8826 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8827 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8828 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8829 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8830 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8831 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8832 | `};` |
|         - | 8833 | `/*` |
|         - | 8834 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8835 | ` */` |
|      3428 | 8836 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8837 | `{` |
|         - | 8838 | `	sxu32 n;` |
|    257105 | 8839 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    253677 | 8840 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    126841 | 8841 | `	}` |
|      3433 | 8842 | `}` |
|         - | 8843 | `/*` |
|         - | 8844 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8845 | ` * the BLOB given as the first argument.` |
|         - | 8846 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8847 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8848 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8849 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8850 | ` */` |
|         - | 8851 | `/*` |
|         - | 8852 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8853 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8854 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8855 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8856 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8857 | ` */` |
|       130 | 8858 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8859 | `{` |
|       133 | 8860 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8861 | `	ph7_value *pObj;` |
|       133 | 8862 | `	sxu32 n = 0;` |
|         - | 8863 | `	int isRef;` |
|       133 | 8864 | `	sxi32 rc = SXRET_OK;` |
|         - | 8865 | `	int i;` |
|       207 | 8866 | `	for(;;){` |
|       417 | 8867 | `		if( n >= pMap->nEntry ){` |
|       133 | 8868 | `			break;` |
|         - | 8869 | `		}` |
|       287 | 8870 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8871 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8872 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       569 | 8873 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       284 | 8874 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       287 | 8875 | `		if( ShowType ){` |
|         - | 8876 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8877 | `			 * on the next line at the same indent (php). */` |
|       147 | 8878 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        99 | 8879 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        51 | 8880 | `			}` |
|        51 | 8881 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        37 | 8882 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        19 | 8883 | `			}else{` |
|        21 | 8884 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8885 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8886 | `			}` |
|        51 | 8887 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        51 | 8888 | `			if( pObj ){` |
|        51 | 8889 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        51 | 8890 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8891 | `					break;` |
|         - | 8892 | `				}` |
|        24 | 8893 | `			}` |
|        27 | 8894 | `		}else{` |
|         - | 8895 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8896 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8897 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8898 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8899 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8900 | `			}` |
|       238 | 8901 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8902 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8903 | `			}else{` |
|       170 | 8904 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8905 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8906 | `			}` |
|       236 | 8907 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8908 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8909 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8910 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8911 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8912 | `					break;` |
|         - | 8913 | `				}` |
|        13 | 8914 | `			}else{` |
|       214 | 8915 | `				if( pObj ){` |
|       214 | 8916 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8917 | `				}` |
|       214 | 8918 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8919 | `			}` |
|         - | 8920 | `		}` |
|         - | 8921 | `		/* Point to the next entry */` |
|       287 | 8922 | `		n++;` |
|       287 | 8923 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8924 | `	}` |
|       133 | 8925 | `	return rc;` |
|         3 | 8926 | `}` |
|       126 | 8927 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8928 | `{` |
|         - | 8929 | `	sxi32 rc;` |
|         - | 8930 | `	int i;` |
|       128 | 8931 | `	if( nDepth > 31 ){` |
|         - | 8932 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8933 | `		/* Nesting limit reached */` |
|       ! 0 | 8934 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8935 | `		return SXERR_LIMIT;` |
|         - | 8936 | `	}` |
|       128 | 8937 | `	if( ShowType ){` |
|         - | 8938 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8939 | `		 * newline (a nested array is itself an entry value line). */` |
|        24 | 8940 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        24 | 8941 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        24 | 8942 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        24 | 8943 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8944 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8945 | `		}` |
|        24 | 8946 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        24 | 8947 | `		return rc;` |
|         - | 8948 | `	}` |
|         - | 8949 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8950 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8951 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8952 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8953 | `	}` |
|       105 | 8954 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8955 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8956 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8957 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8958 | `	}` |
|       105 | 8959 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8960 | `	return rc;` |
|        65 | 8961 | `}` |
|         - | 8962 | `/*` |
|         - | 8963 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8964 | ` * retrieved entry.` |
|         - | 8965 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8966 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8967 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8968 | ` * a value different from PH7_OK.` |
|         - | 8969 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8970 | ` */` |
|     34994 | 8971 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8972 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8973 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8974 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8975 | `	)` |
|         5 | 8976 | `{` |
|         - | 8977 | `	ph7_hashmap_node *pEntry;` |
|         - | 8978 | `	ph7_value sKey,sValue;` |
|         - | 8979 | `	sxi32 rc;` |
|         - | 8980 | `	sxu32 n;` |
|         - | 8981 | `	/* Initialize walker parameter */` |
|     34999 | 8982 | `	rc = SXRET_OK;` |
|     34999 | 8983 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34999 | 8984 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34999 | 8985 | `	n = pMap->nEntry;` |
|     34999 | 8986 | `	pEntry = pMap->pFirst;` |
|         - | 8987 | `	/* Start the iteration process */` |
|     97820 | 8988 | `	for(;;){` |
|    195645 | 8989 | `		if( n < 1 ){` |
|     34997 | 8990 | `			break;` |
|         - | 8991 | `		}` |
|         - | 8992 | `		/* Extract a copy of the key and a copy the current value */` |
|    160653 | 8993 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    160653 | 8994 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8995 | `		/* Invoke the user callback */` |
|    160653 | 8996 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8997 | `		/* Release the copy of the key and the value */` |
|    160653 | 8998 | `		PH7_MemObjRelease(&sKey);` |
|    160653 | 8999 | `		PH7_MemObjRelease(&sValue);` |
|    160653 | 9000 | `		if( rc != PH7_OK ){` |
|         - | 9001 | `			/* Callback request an operation abort */` |
|         3 | 9002 | `			return SXERR_ABORT;` |
|         - | 9003 | `		}` |
|         - | 9004 | `		/* Point to the next entry */` |
|    160651 | 9005 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    160651 | 9006 | `		n--;` |
|         5 | 9007 | `	}` |
|         - | 9008 | `	/* All done */` |
|     34997 | 9009 | `	return SXRET_OK;` |
|     17502 | 9010 | `}` |
|         - | 9011 |  |
