# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3924/4375 lines (89.69%)

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
|   7437832 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7437837 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7437837 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    639316 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    639321 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    639321 |   35 | `	sxu32 nH = 5381;` |
|    639321 |   36 | `	zEnd = &zIn[nLen];` |
|    724474 |   37 | `	for(;;){` |
|   1448953 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1233417 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1106611 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    964631 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    639321 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1356 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1361 |   54 | `	sxi64 iCount = 0;` |
|      1361 |   55 | `	if( !bRecursive ){` |
|      1187 |   56 | `		iCount = pMap->nEntry;` |
|       596 |   57 | `	}else{` |
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
|      1361 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3140256 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3140261 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3140261 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3140261 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3140261 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3140261 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3140261 |  112 | `	pNode->nHash = nHash;` |
|   3140261 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3140261 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3140261 |  115 | `	return pNode;` |
|   1570133 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    270262 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    270267 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    270267 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    270267 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    270267 |  133 | `	pNode->pMap  = &(*pMap);` |
|    270267 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    270267 |  135 | `	pNode->nHash = nHash;` |
|    270267 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    270267 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    270267 |  138 | `	pNode->nValIdx = nValIdx;` |
|    270267 |  139 | `	return pNode;` |
|    135136 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3410518 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3410523 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2933059 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2933059 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1466527 |  150 | `	}` |
|   3410523 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3410523 |  153 | `	if( pMap->pFirst == 0 ){` |
|     91043 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     91043 |  156 | `		pMap->pCur = pNode;` |
|     45524 |  157 | `	}else{` |
|   3319485 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3410523 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3410523 |  174 | `	++pMap->nEntry;` |
|   3410523 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7524 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7529 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7529 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7529 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7077 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3541 |  187 | `	}else{` |
|       454 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7529 |  190 | `	if( pNode->pNextCollide ){` |
|      4935 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2466 |  192 | `	}` |
|      7529 |  193 | `	if( pMap->pFirst == pNode ){` |
|       139 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        67 |  195 | `	}` |
|      7529 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       141 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        68 |  199 | `	}` |
|      7529 |  200 | `	if( pMap->pActiveSteps ){` |
|         - |  201 | `		/* Advance any live foreach cursor parked on this node (delete during` |
|         - |  202 | `		 * live-map iteration: by-ref foreach, $GLOBALS, snapshot fallbacks). */` |
|         - |  203 | `		ph7_foreach_step *pStep;` |
|        37 |  204 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        19 |  205 | `			if( pStep->pCursor == pNode ){` |
|         5 |  206 | `				pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|         2 |  207 | `			}` |
|        10 |  208 | `		}` |
|         9 |  209 | `	}` |
|         - |  210 | `	/* Unlink from the map list */` |
|      7529 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7529 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       107 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       107 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       107 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|        51 |  218 | `		}` |
|        51 |  219 | `	}` |
|      7529 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7376 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3686 |  222 | `	}` |
|      7529 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7529 |  224 | `	pMap->nEntry--;` |
|      7529 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|        83 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        83 |  228 | `		pMap->apBucket = 0;` |
|        83 |  229 | `		pMap->nSize = 0;` |
|        83 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        39 |  231 | `	}` |
|      7529 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3410518 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3410523 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     96021 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     96021 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     96021 |  245 | `		if( nNew < 1 ){` |
|     91043 |  246 | `			nNew = 16;` |
|     45519 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     96021 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     96021 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     96021 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     96021 |  260 | `		pMap->apBucket = apNew;` |
|     96021 |  261 | `		pMap->nSize = nNew;` |
|     96021 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     91043 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      4983 |  267 | `		pEntry = pMap->pFirst;` |
|      4983 |  268 | `		n = 0;` |
|   2101481 |  269 | `		for( ;; ){` |
|   4202967 |  270 | `			if( n >= pMap->nEntry ){` |
|      4983 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4197989 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4197989 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4197989 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3587423 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3587423 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1793709 |  280 | `			}` |
|   4197989 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4197989 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4197989 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      4983 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2489 |  288 | `	}` |
|   3319485 |  289 | `	return SXRET_OK;` |
|   1705264 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3140256 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3140261 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3140223 |  310 | `		if( pValue ){` |
|   3140217 |  311 | `			sSafeVal = *pValue;` |
|   3140217 |  312 | `			pValue = &sSafeVal;` |
|   1570106 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3140223 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3140223 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3140223 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3140217 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1570106 |  322 | `		}` |
|   3140223 |  323 | `		nIdx = pObj->nIdx;` |
|   1570114 |  324 | `	}else{` |
|        39 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3140261 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3140261 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3140261 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3140261 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        39 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3140261 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3140261 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3140261 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3140261 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3140261 |  349 | `	return SXRET_OK;` |
|   1570133 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    270262 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    270267 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    223077 |  370 | `		if( pValue ){` |
|    222787 |  371 | `			sSafeVal = *pValue;` |
|    222787 |  372 | `			pValue = &sSafeVal;` |
|    111391 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    223077 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    223077 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    223077 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    222787 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    111391 |  382 | `		}` |
|    223077 |  383 | `		nIdx = pObj->nIdx;` |
|    111541 |  384 | `	}else{` |
|     47195 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    270267 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    270267 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    270267 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    270267 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     47195 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23595 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    270267 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    270267 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    270267 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    270267 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    270267 |  409 | `	return SXRET_OK;` |
|    135136 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4284476 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4284481 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       597 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4283889 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4283889 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110562473 |  433 | `	for(;;){` |
| 221124951 |  434 | `		if( pNode == 0 ){` |
|   4281197 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216843754 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216840744 |  438 | `			&& pNode->nHash == nHash` |
| 108420218 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      2697 |  441 | `				if( ppNode ){` |
|      2679 |  442 | `					*ppNode = pNode;` |
|      1337 |  443 | `				}` |
|      2697 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841063 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4281197 |  450 | `	return SXERR_NOTFOUND;` |
|   2142243 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    404874 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    404879 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     35825 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    369059 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    369059 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    305400 |  475 | `	for(;;){` |
|    610805 |  476 | `		if( pNode == 0 ){` |
|    310447 |  477 | `			break;` |
|         - |  478 | `		}` |
|    300358 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    298848 |  480 | `			&& pNode->nHash == nHash` |
|    178025 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     58717 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     58617 |  484 | `				if( ppNode ){` |
|     58589 |  485 | `					*ppNode = pNode;` |
|     29292 |  486 | `				}` |
|     58617 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    241751 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    310447 |  493 | `	return SXERR_NOTFOUND;` |
|    202442 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    405004 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    405009 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    405009 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    405009 |  504 | `	int isNeg = FALSE, nDigit;` |
|    405009 |  505 | `	if( zIn >= zEnd ){` |
|       ! 0 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    405009 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    405005 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    405005 |  516 | `	zDigit = zIn;` |
|    202932 |  517 | `	for(;;){` |
|    405869 |  518 | `		if( zIn >= zEnd ){` |
|       249 |  519 | `			break;` |
|         - |  520 | `		}` |
|    405621 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    404757 |  523 | `			return FALSE;` |
|         - |  524 | `		}` |
|       865 |  525 | `		zIn++;` |
|         1 |  526 | `	}` |
|         - |  527 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  528 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  529 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  530 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       249 |  531 | `	nDigit = (int)(zEnd - zDigit);` |
|       249 |  532 | `	if( nDigit < 1 ){` |
|         - |  533 | `		/* A lone sign ("-"/"+") */` |
|       ! 0 |  534 | `		return FALSE;` |
|         - |  535 | `	}` |
|       253 |  536 | `	if( nDigit > 19 \|\|` |
|       127 |  537 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|         7 |  538 | `		return FALSE;` |
|         - |  539 | `	}` |
|       243 |  540 | `	return TRUE;` |
|    202507 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    137302 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    137307 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    137307 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    134839 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast */` |
|       ! 0 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  559 | `		}` |
|    134839 |  560 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Perform a blob lookup */` |
|    134819 |  562 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    134819 |  563 | `			goto result;` |
|         - |  564 | `		}` |
|        10 |  565 | `	}` |
|         - |  566 | `	/* Perform an int lookup */` |
|      2493 |  567 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  568 | `		/* Force an integer cast */` |
|        35 |  569 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  570 | `	}` |
|         - |  571 | `	/* Perform an int lookup */` |
|      2493 |  572 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     68651 |  573 | `result:` |
|    137307 |  574 | `	if( rc == SXRET_OK ){` |
|         - |  575 | `		/* Node found */` |
|     60657 |  576 | `		if( ppNode ){` |
|     60611 |  577 | `			*ppNode = pNode;` |
|     30303 |  578 | `		}` |
|     60657 |  579 | `		return SXRET_OK;` |
|         - |  580 | `	}` |
|         - |  581 | `	/* No such entry */` |
|     76655 |  582 | `	return SXERR_NOTFOUND;` |
|     68656 |  583 | `}` |
|         - |  584 | `/*` |
|         - |  585 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  586 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  587 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  588 | ` * via HashmapAppendIndexBusy.` |
|         - |  589 | ` */` |
|   2141008 |  590 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  591 | `{` |
|   2141013 |  592 | `	if( iKey >= pMap->iNextIdx ){` |
|   2140749 |  593 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  594 | `		/* Make sure the automatic index is not reserved */` |
|   2140749 |  595 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  596 | `			pMap->iNextIdx++;` |
|       ! 0 |  597 | `		}` |
|   1070372 |  598 | `	}` |
|   2141013 |  599 | `}` |
|         - |  600 | `/*` |
|         - |  601 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  602 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  603 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  604 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  605 | ` */` |
|    998890 |  606 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  607 | `{` |
|    998895 |  608 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  609 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  610 | `		return TRUE;` |
|         - |  611 | `	}` |
|    998889 |  612 | `	return FALSE;` |
|    499450 |  613 | `}` |
|         - |  614 | `/*` |
|         - |  615 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  616 | ` * hashmap.` |
|         - |  617 | ` * If a node with the given key already exists in the database` |
|         - |  618 | ` * then this function overwrite the old value.` |
|         - |  619 | ` */` |
|   3362698 |  620 | `static sxi32 HashmapInsert(` |
|         - |  621 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  622 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  623 | `	ph7_value *pVal    /* Node value */` |
|         - |  624 | `	)` |
|         5 |  625 | `{` |
|   3362703 |  626 | `	ph7_hashmap_node *pNode = 0;` |
|   3362703 |  627 | `	sxi32 rc = SXRET_OK;` |
|   3362703 |  628 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    226499 |  629 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  630 | `			/* Force a string cast */` |
|         3 |  631 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  632 | `		}` |
|    226499 |  633 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3751 |  634 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  635 | `				/* Automatic index assign */` |
|      3525 |  636 | `				pKey = 0;` |
|      1760 |  637 | `			}` |
|      3751 |  638 | `			goto IntKey;` |
|         - |  639 | `		}` |
|    334127 |  640 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    111374 |  641 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  642 | `				/* Overwrite the old value */` |
|         - |  643 | `				ph7_value *pElem;` |
|       371 |  644 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       371 |  645 | `				if( pElem ){` |
|       371 |  646 | `					if( pVal ){` |
|       371 |  647 | `						PH7_MemObjStore(pVal,pElem);` |
|       187 |  648 | `					}else{` |
|         - |  649 | `						/* Nullify the entry */` |
|       ! 0 |  650 | `						PH7_MemObjToNull(pElem);` |
|         - |  651 | `					}` |
|       184 |  652 | `				}` |
|       371 |  653 | `				return SXRET_OK;` |
|         - |  654 | `		}` |
|    222385 |  655 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  656 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  657 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       127 |  658 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  659 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  660 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  661 | `				return SXRET_OK;` |
|         - |  662 | `			}` |
|       190 |  663 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       126 |  664 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        63 |  665 | `				pVal,SXU32_HIGH);` |
|         - |  666 | `		}` |
|         - |  667 | `		/* Perform a blob-key insertion */` |
|    222259 |  668 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    222259 |  669 | `		return rc;` |
|         - |  670 | `	}` |
|   1568102 |  671 | `IntKey:` |
|   3139955 |  672 | `	if( pKey ){` |
|   2141095 |  673 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  674 | `			/* Force an integer cast */` |
|       259 |  675 | `			PH7_MemObjToInteger(pKey);` |
|       129 |  676 | `		}` |
|   2141095 |  677 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  678 | `			/* Overwrite the old value */` |
|         - |  679 | `			ph7_value *pElem;` |
|        87 |  680 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        87 |  681 | `			if( pElem ){` |
|        87 |  682 | `				if( pVal ){` |
|        87 |  683 | `					PH7_MemObjStore(pVal,pElem);` |
|        44 |  684 | `				}else{` |
|         - |  685 | `					/* Nullify the entry */` |
|       ! 0 |  686 | `					PH7_MemObjToNull(pElem);` |
|         - |  687 | `				}` |
|        43 |  688 | `			}` |
|        87 |  689 | `			return SXRET_OK;` |
|         - |  690 | `		}` |
|   2141009 |  691 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  692 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  693 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  694 | `			char zKey[24];` |
|         3 |  695 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  696 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  697 | `		}` |
|         - |  698 | `		/* Perform a 64-bit-int-key insertion */` |
|   2141007 |  699 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2141007 |  700 | `		if( rc == SXRET_OK ){` |
|   2141007 |  701 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070501 |  702 | `		}` |
|   1070506 |  703 | `	}else{` |
|    998865 |  704 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  705 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  706 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  707 | `		}` |
|    998863 |  708 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  709 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  710 | `		}` |
|         - |  711 | `		/* Assign an automatic index */` |
|    998857 |  712 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|    998857 |  713 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|    998855 |  714 | `			++pMap->iNextIdx;` |
|    499425 |  715 | `		}` |
|         - |  716 | `	}` |
|         - |  717 | `	/* Insertion result */` |
|   3139859 |  718 | `	return rc;` |
|   1681354 |  719 | `}` |
|         - |  720 | `/*` |
|         - |  721 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  722 | ` * hashmap.` |
|         - |  723 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  724 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  725 | ` * The insertion by reference is triggered when the following` |
|         - |  726 | ` * expression is encountered.` |
|         - |  727 | ` * $var = 10;` |
|         - |  728 | ` *  $a = array(&var);` |
|         - |  729 | ` * OR` |
|         - |  730 | ` *  $a[] =& $var;` |
|         - |  731 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  732 | ` * over it's contents.` |
|         - |  733 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  734 | ` * removed when the foreign ph7_value is unset.` |
|         - |  735 | ` * Example:` |
|         - |  736 | ` *  $var = 10;` |
|         - |  737 | ` *  $a[] =& $var;` |
|         - |  738 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  739 | ` *  //Unset the foreign ph7_value now` |
|         - |  740 | ` *  unset($var);` |
|         - |  741 | ` *  echo count($a); //0` |
|         - |  742 | ` * Note that this is a PH7 eXtension.` |
|         - |  743 | ` * Refer to the official documentation for more information.` |
|         - |  744 | ` * If a node with the given key already exists in the database` |
|         - |  745 | ` * then this function overwrite the old value.` |
|         - |  746 | ` */` |
|     47238 |  747 | `static sxi32 HashmapInsertByRef(` |
|         - |  748 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  749 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  750 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  751 | `	)` |
|         5 |  752 | `{` |
|     47243 |  753 | `	ph7_hashmap_node *pNode = 0;` |
|     47243 |  754 | `	sxi32 rc = SXRET_OK;` |
|     47243 |  755 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     47207 |  756 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  757 | `			/* Force a string cast */` |
|       ! 0 |  758 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  759 | `		}` |
|     47207 |  760 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  761 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  762 | `				/* Automatic index assign */` |
|       ! 0 |  763 | `				pKey = 0;` |
|       ! 0 |  764 | `			}` |
|         3 |  765 | `			goto IntKey;` |
|         - |  766 | `		}` |
|     70805 |  767 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23600 |  768 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  769 | `				/* Overwrite */` |
|        11 |  770 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  771 | `				pNode->nValIdx = nRefIdx;` |
|         - |  772 | `				/* Install in the reference table */` |
|        11 |  773 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  774 | `				return SXRET_OK;` |
|         - |  775 | `		}` |
|         - |  776 | `		/* Perform a blob-key insertion */` |
|     47195 |  777 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     47195 |  778 | `		return rc;` |
|         - |  779 | `	}` |
|        18 |  780 | `IntKey:` |
|        39 |  781 | `	if( pKey ){` |
|         7 |  782 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  783 | `			/* Force an integer cast */` |
|         3 |  784 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  785 | `		}` |
|         7 |  786 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  787 | `			/* Overwrite */` |
|       ! 0 |  788 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  789 | `			pNode->nValIdx = nRefIdx;` |
|         - |  790 | `			/* Install in the reference table */` |
|       ! 0 |  791 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  792 | `			return SXRET_OK;` |
|         - |  793 | `		}` |
|         - |  794 | `		/* Perform a 64-bit-int-key insertion */` |
|         7 |  795 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|         7 |  796 | `		if( rc == SXRET_OK ){` |
|         7 |  797 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         3 |  798 | `		}` |
|         4 |  799 | `	}else{` |
|        33 |  800 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  801 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  802 | `		}` |
|         - |  803 | `		/* Assign an automatic index */` |
|        33 |  804 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        33 |  805 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        33 |  806 | `			++pMap->iNextIdx;` |
|        16 |  807 | `		}` |
|         - |  808 | `	}` |
|         - |  809 | `	/* Insertion result */` |
|        39 |  810 | `	return rc;` |
|     23624 |  811 | `}` |
|         - |  812 | `/*` |
|         - |  813 | ` * Extract node value.` |
|         - |  814 | ` */` |
|   1420488 |  815 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  816 | `{` |
|         - |  817 | `	/* Point to the desired object */` |
|         - |  818 | `	ph7_value *pObj;` |
|   1420493 |  819 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1420493 |  820 | `	return pObj;` |
|         5 |  821 | `}` |
|         - |  822 | `/*` |
|         - |  823 | ` * Insert a node in the given hashmap.` |
|         - |  824 | ` * If a node with the given key already exists in the database` |
|         - |  825 | ` * then this function overwrite the old value.` |
|         - |  826 | ` */` |
|       448 |  827 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  828 | `{` |
|         - |  829 | `	ph7_value *pObj;` |
|         - |  830 | `	sxi32 rc;` |
|         - |  831 | `	/* Extract the node value */` |
|       453 |  832 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       453 |  833 | `	if( pObj == 0 ){` |
|       ! 0 |  834 | `		return SXERR_EMPTY;` |
|         - |  835 | `	}` |
|         - |  836 | `	/* Preserve key */` |
|       453 |  837 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  838 | `		/* Int64 key */` |
|       321 |  839 | `		if( !bPreserve ){` |
|         - |  840 | `			/* Assign an automatic index */` |
|       173 |  841 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        89 |  842 | `		}else{` |
|       149 |  843 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  844 | `		}` |
|       163 |  845 | `	}else{` |
|         - |  846 | `		/* Blob key */` |
|       133 |  847 | `		if( !bPreserve ){` |
|         - |  848 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  849 | `			 * original string key entirely */` |
|        35 |  850 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  851 | `		}else{` |
|       148 |  852 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        49 |  853 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  854 | `		}` |
|         - |  855 | `	}` |
|       453 |  856 | `	return rc;` |
|       229 |  857 | `}` |
|         - |  858 | `/*` |
|         - |  859 | ` * Compare two node values.` |
|         - |  860 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  861 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  862 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  863 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  864 | ` * documenation.` |
|         - |  865 | ` */` |
|     71144 |  866 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  867 | `{` |
|         - |  868 | `	ph7_value sObj1,sObj2;` |
|         - |  869 | `	sxi32 rc;` |
|     71149 |  870 | `	if( pLeft == pRight ){` |
|         - |  871 | `		/*` |
|         - |  872 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  873 | `		 * below for more information on this sceanario.` |
|         - |  874 | `		 */` |
|       ! 0 |  875 | `		return 0;` |
|         - |  876 | `	}` |
|         - |  877 | `	/* Do the comparison */` |
|     71149 |  878 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71149 |  879 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71149 |  880 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71149 |  881 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71149 |  882 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71149 |  883 | `	PH7_MemObjRelease(&sObj1);` |
|     71149 |  884 | `	PH7_MemObjRelease(&sObj2);` |
|     71149 |  885 | `	return rc;` |
|     35595 |  886 | `}` |
|         - |  887 | `/*` |
|         - |  888 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  889 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  890 | ` */` |
|     13692 |  891 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  892 | `{` |
|     13697 |  893 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  894 | `	sxu32 nBucket;` |
|         - |  895 | `	/* Remove old collision links */` |
|     13697 |  896 | `	if( pEntry->pPrevCollide ){` |
|     11227 |  897 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5628 |  898 | `	}else{` |
|      2475 |  899 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  900 | `	}` |
|     13697 |  901 | `	if( pEntry->pNextCollide ){` |
|      1108 |  902 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       573 |  903 | `	}` |
|     13697 |  904 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  905 | `	/* Compute the new hash */` |
|     13697 |  906 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13697 |  907 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13697 |  908 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  909 | `	/* Link to the new bucket */` |
|     13697 |  910 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13697 |  911 | `	if( pMap->apBucket[nBucket] ){` |
|     11552 |  912 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5788 |  913 | `	}` |
|     13697 |  914 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13697 |  915 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  916 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  917 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  918 | `	 * the no-overflow invariant uniform). */` |
|     13697 |  919 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13697 |  920 | `		pMap->iNextIdx++;` |
|      6846 |  921 | `	}` |
|     13697 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Perform a linear search on a given hashmap.` |
|         - |  925 | ` * Write a pointer to the target node on success.` |
|         - |  926 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  927 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  928 | ` * for more information.` |
|         - |  929 | ` */` |
|     33218 |  930 | `static int HashmapFindValue(` |
|         - |  931 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  932 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  933 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  934 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  935 | `	)` |
|         5 |  936 | `{` |
|         - |  937 | `	ph7_hashmap_node *pEntry;` |
|         - |  938 | `	ph7_value sVal,*pVal;` |
|         - |  939 | `	ph7_value sNeedle;` |
|         - |  940 | `	sxi32 rc;` |
|         - |  941 | `	sxu32 n;` |
|         - |  942 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     33223 |  943 | `	pEntry = pMap->pFirst;` |
|     33223 |  944 | `	n = pMap->nEntry;` |
|     33223 |  945 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33223 |  946 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     78824 |  947 | `	for(;;){` |
|    157655 |  948 | `		if( n < 1 ){` |
|       115 |  949 | `			break;` |
|         - |  950 | `		}` |
|         - |  951 | `		/* Extract node value */` |
|    157541 |  952 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    157541 |  953 | `		if( pVal ){` |
|         - |  954 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  955 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  956 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  957 | `			 * so null needles/values take the same path as everything else` |
|         - |  958 | `			 * (the historical null-to-null shortcut here made` |
|         - |  959 | `			 * in_array(null, [""]) false where php says true). */` |
|    157541 |  960 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    157541 |  961 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    157541 |  962 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    157541 |  963 | `			PH7_MemObjRelease(&sVal);` |
|    157541 |  964 | `			PH7_MemObjRelease(&sNeedle);` |
|    157541 |  965 | `			if( rc == 0 ){` |
|     33109 |  966 | `				if( ppNode ){` |
|        23 |  967 | `					*ppNode = pEntry;` |
|        11 |  968 | `				}` |
|         - |  969 | `				/* Match found*/` |
|     33109 |  970 | `				return SXRET_OK;` |
|         - |  971 | `			}` |
|     62215 |  972 | `		}` |
|         - |  973 | `		/* Point to the next entry */` |
|    124437 |  974 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    124437 |  975 | `		n--;` |
|         5 |  976 | `	}` |
|         - |  977 | `	/* No such entry */` |
|       115 |  978 | `	return SXERR_NOTFOUND;` |
|     16614 |  979 | `}` |
|         - |  980 | `/*` |
|         - |  981 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - |  982 | ` * for values comparison.` |
|         - |  983 | ` * Write a pointer to the target node on success.` |
|         - |  984 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  985 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - |  986 | ` * for more information.` |
|         - |  987 | ` */` |
|        22 |  988 | `static int HashmapFindValueByCallback(` |
|         - |  989 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - |  990 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - |  991 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - |  992 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - |  993 | `	)` |
|         1 |  994 | `{` |
|         - |  995 | `	ph7_hashmap_node *pEntry;` |
|         - |  996 | `	ph7_value sResult,*pVal;` |
|         - |  997 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - |  998 | `	sxi32 rc;` |
|         - |  999 | `	sxu32 n;` |
|        23 | 1000 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - | 1001 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - | 1002 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 | 1003 | `		return SXERR_NOTFOUND;` |
|         - | 1004 | `	}` |
|         - | 1005 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        23 | 1006 | `	pEntry = pMap->pFirst;` |
|        23 | 1007 | `	n = pMap->nEntry;` |
|         - | 1008 | `	/* Store callback result here */` |
|        23 | 1009 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - | 1010 | `	/* First argument to the callback */` |
|        23 | 1011 | `	apArg[0] = pNeedle;` |
|        25 | 1012 | `	for(;;){` |
|        51 | 1013 | `		if( n < 1 ){` |
|         9 | 1014 | `			break;` |
|         - | 1015 | `		}` |
|         - | 1016 | `		/* Extract node value */` |
|        43 | 1017 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        43 | 1018 | `		if( pVal ){` |
|         - | 1019 | `			/* Invoke the user callback */` |
|        43 | 1020 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        43 | 1021 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        43 | 1022 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 1023 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1024 | `				 * and report no match for the rest of the run. */` |
|         5 | 1025 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1026 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1027 | `				return SXERR_NOTFOUND;` |
|         - | 1028 | `			}` |
|        39 | 1029 | `			if( rc == SXRET_OK ){` |
|         - | 1030 | `				/* Extract callback result */` |
|        39 | 1031 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1032 | `					/* Perform an int cast */` |
|       ! 0 | 1033 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1034 | `				}` |
|        39 | 1035 | `				rc = (sxi32)sResult.x.iVal;` |
|        39 | 1036 | `				PH7_MemObjRelease(&sResult);` |
|        39 | 1037 | `				if( rc == 0 ){` |
|         - | 1038 | `					/* Match found*/` |
|        11 | 1039 | `					if( ppNode ){` |
|       ! 0 | 1040 | `						*ppNode = pEntry;` |
|       ! 0 | 1041 | `					}` |
|        11 | 1042 | `					return SXRET_OK;` |
|         - | 1043 | `				}` |
|        14 | 1044 | `			}` |
|        14 | 1045 | `		}` |
|         - | 1046 | `		/* Point to the next entry */` |
|        29 | 1047 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 1048 | `		n--;` |
|         1 | 1049 | `	}` |
|         - | 1050 | `	/* No such entry */` |
|         9 | 1051 | `	return SXERR_NOTFOUND;` |
|        12 | 1052 | `}` |
|         - | 1053 | `/*` |
|         - | 1054 | ` * Compare two hashmaps.` |
|         - | 1055 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1056 | ` * Note on array comparison operators.` |
|         - | 1057 | ` *  According to the PHP language reference manual.` |
|         - | 1058 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1059 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1060 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1061 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1062 | ` *                          order and of the same types.` |
|         - | 1063 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1064 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1065 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1066 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1067 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1068 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1069 | ` * <?php` |
|         - | 1070 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1071 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1072 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1073 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1074 | ` * var_dump($c);` |
|         - | 1075 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1076 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1077 | ` * var_dump($c);` |
|         - | 1078 | ` * ?>` |
|         - | 1079 | ` * When executed, this script will print the following:` |
|         - | 1080 | ` * Union of $a and $b:` |
|         - | 1081 | ` * array(3) {` |
|         - | 1082 | ` *  ["a"]=>` |
|         - | 1083 | ` *  string(5) "apple"` |
|         - | 1084 | ` *  ["b"]=>` |
|         - | 1085 | ` * string(6) "banana"` |
|         - | 1086 | ` *  ["c"]=>` |
|         - | 1087 | ` * string(6) "cherry"` |
|         - | 1088 | ` * }` |
|         - | 1089 | ` * Union of $b and $a:` |
|         - | 1090 | ` * array(3) {` |
|         - | 1091 | ` * ["a"]=>` |
|         - | 1092 | ` * string(4) "pear"` |
|         - | 1093 | ` * ["b"]=>` |
|         - | 1094 | ` * string(10) "strawberry"` |
|         - | 1095 | ` * ["c"]=>` |
|         - | 1096 | ` * string(6) "cherry"` |
|         - | 1097 | ` * }` |
|         - | 1098 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1099 | ` */` |
|        30 | 1100 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1101 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1102 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1103 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1104 | `	)` |
|         1 | 1105 | `{` |
|         - | 1106 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1107 | `	sxi32 rc;` |
|         - | 1108 | `	sxu32 n;` |
|        31 | 1109 | `	if( pLeft == pRight ){` |
|         - | 1110 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1111 | `		 * Unlike the zend engine.` |
|         - | 1112 | `		 */` |
|         3 | 1113 | `		return 0;` |
|         - | 1114 | `	}` |
|        29 | 1115 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1116 | `		/* Must have the same number of entries */` |
|         5 | 1117 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1118 | `	}` |
|         - | 1119 | `	/* Point to the first inserted entry of the left hashmap */` |
|        25 | 1120 | `	pLe = pLeft->pFirst;` |
|        25 | 1121 | `	pRe = 0; /* cc warning */` |
|         - | 1122 | `	/* Perform the comparison */` |
|        25 | 1123 | `	n = pLeft->nEntry;` |
|        59 | 1124 | `	for(;;){` |
|       119 | 1125 | `		if( n < 1 ){` |
|        23 | 1126 | `			break;` |
|         - | 1127 | `		}` |
|        97 | 1128 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1129 | `			/* Int key */` |
|        89 | 1130 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        45 | 1131 | `		}else{` |
|         9 | 1132 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1133 | `			/* Blob key */` |
|         9 | 1134 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1135 | `		}` |
|        97 | 1136 | `		if( rc != SXRET_OK ){` |
|         - | 1137 | `			/* No such entry in the right side */` |
|       ! 0 | 1138 | `			return 1;` |
|         - | 1139 | `		}` |
|        97 | 1140 | `		rc = 0;` |
|        97 | 1141 | `		if( bStrict ){` |
|         - | 1142 | `			/* Make sure,the keys are of the same type */` |
|        81 | 1143 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1144 | `				rc = 1;` |
|       ! 0 | 1145 | `			}` |
|        40 | 1146 | `		}` |
|        97 | 1147 | `		if( !rc ){` |
|         - | 1148 | `			/* Compare nodes */` |
|        97 | 1149 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        48 | 1150 | `		}` |
|        97 | 1151 | `		if( rc != 0 ){` |
|         - | 1152 | `			/* Nodes key/value differ */` |
|         3 | 1153 | `			return rc;` |
|         - | 1154 | `		}` |
|         - | 1155 | `		/* Point to the next entry */` |
|        95 | 1156 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        95 | 1157 | `		n--;` |
|         1 | 1158 | `	}` |
|        23 | 1159 | `	return 0; /* Hashmaps are equals */` |
|        16 | 1160 | `}` |
|         - | 1161 | `/*` |
|         - | 1162 | ` * Duplicate a hashmap node.` |
|         - | 1163 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1164 | ` */` |
|    650818 | 1165 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1166 | `	ph7_hashmap *pDest,` |
|         - | 1167 | `	ph7_hashmap_node *pEntry,` |
|         - | 1168 | `	ph7_value *pVal,` |
|         - | 1169 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1170 | `	)` |
|         5 | 1171 | `{` |
|         - | 1172 | `	ph7_value sSafeVal;` |
|         - | 1173 | `	ph7_value sKey;` |
|         - | 1174 | `	sxi32 rc;` |
|         - | 1175 |  |
|    650823 | 1176 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 1177 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|         - | 1178 | `		 * Re-insert it by reference so the reference survives the duplication` |
|         - | 1179 | `		 * instead of being flattened to a value copy. This keeps spread` |
|         - | 1180 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|         - | 1181 | `		 * with PHP semantics. */` |
|         7 | 1182 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         7 | 1183 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1184 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1185 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1186 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1187 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1188 | `		}else{` |
|         5 | 1189 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         5 | 1190 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         2 | 1191 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1192 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1193 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1194 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1195 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1196 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1197 | `			}` |
|         - | 1198 | `		}` |
|         7 | 1199 | `		return rc;` |
|         - | 1200 | `	}` |
|    650817 | 1201 | `	sSafeVal = *pVal;` |
|         - | 1202 |  |
|    650817 | 1203 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1204 | `		/* Blob key insertion */` |
|      4069 | 1205 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4069 | 1206 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4069 | 1207 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4069 | 1208 | `		PH7_MemObjRelease(&sKey);` |
|      2037 | 1209 | `	}else{` |
|         - | 1210 | `		/* Int key */` |
|    646753 | 1211 | `		if( iAction == 0 ){ /* Merge */` |
|    646521 | 1212 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    323493 | 1213 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1214 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1215 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1216 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1217 | `		}else{ /* Dup */` |
|       205 | 1218 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1219 | `		}` |
|         - | 1220 | `	}` |
|    650817 | 1221 | `	return rc;` |
|    325414 | 1222 | `}` |
|         - | 1223 | `/*` |
|         - | 1224 | ` * Merge two hashmaps.` |
|         - | 1225 | ` * Note on the merge process` |
|         - | 1226 | ` * According to the PHP language reference manual.` |
|         - | 1227 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1228 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1229 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1230 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1231 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1232 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1233 | ` *  keys starting from zero in the result array.` |
|         - | 1234 | ` */` |
|      2744 | 1235 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1236 | `{` |
|         - | 1237 | `	ph7_hashmap_node *pEntry;` |
|         - | 1238 | `	ph7_value *pVal;` |
|         - | 1239 | `	sxi32 rc;` |
|         - | 1240 | `	sxu32 n;` |
|      2749 | 1241 | `	if( pSrc == pDest ){` |
|         - | 1242 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1243 | `		 * Unlike the zend engine.` |
|         - | 1244 | `		 */` |
|       ! 0 | 1245 | `		return SXRET_OK;` |
|         - | 1246 | `	}` |
|         - | 1247 | `	/* Point to the first inserted entry in the source */` |
|      2749 | 1248 | `	pEntry = pSrc->pFirst;` |
|         - | 1249 | `	/* Perform the merge */` |
|    649323 | 1250 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1251 | `		/* Extract the node value */` |
|    646579 | 1252 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    646579 | 1253 | `		if( pVal ){` |
|         - | 1254 | `			/* Make a local copy of the value.` |
|         - | 1255 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1256 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1257 | `			 * to the old pool.` |
|         - | 1258 | `			 */` |
|    646579 | 1259 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    323292 | 1260 | `		}else{` |
|       ! 0 | 1261 | `			rc = SXRET_OK;` |
|         - | 1262 | `		}` |
|    646579 | 1263 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1264 | `			return rc;` |
|         - | 1265 | `		}` |
|         - | 1266 | `		/* Point to the next entry */` |
|    646579 | 1267 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    323292 | 1268 | `	}` |
|      2749 | 1269 | `	return SXRET_OK;` |
|      1377 | 1270 | `}` |
|         - | 1271 | `/*` |
|         - | 1272 | ` * Overwrite entries with the same key.` |
|         - | 1273 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1274 | ` *  According to the PHP language reference manual.` |
|         - | 1275 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1276 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1277 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1278 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1279 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1280 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1281 | ` *  overwriting the previous values.` |
|         - | 1282 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1283 | ` *  by whatever type is in the second array.` |
|         - | 1284 | ` */` |
|        34 | 1285 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1286 | `{` |
|         - | 1287 | `	ph7_hashmap_node *pEntry;` |
|         - | 1288 | `	ph7_value *pVal;` |
|         - | 1289 | `	sxi32 rc;` |
|         - | 1290 | `	sxu32 n;` |
|        36 | 1291 | `	if( pSrc == pDest ){` |
|         - | 1292 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1293 | `		 * Unlike the zend engine.` |
|         - | 1294 | `		 */` |
|       ! 0 | 1295 | `		return SXRET_OK;` |
|         - | 1296 | `	}` |
|         - | 1297 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1298 | `	pEntry = pSrc->pFirst;` |
|         - | 1299 | `	/* Perform the merge */` |
|        80 | 1300 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1301 | `		/* Extract the node value */` |
|        46 | 1302 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1303 | `		if( pVal ){` |
|        46 | 1304 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1305 | `		}else{` |
|       ! 0 | 1306 | `			rc = SXRET_OK;` |
|         - | 1307 | `		}` |
|        46 | 1308 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1309 | `			return rc;` |
|         - | 1310 | `		}` |
|         - | 1311 | `		/* Point to the next entry */` |
|        46 | 1312 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1313 | `	}` |
|        36 | 1314 | `	return SXRET_OK;` |
|        19 | 1315 | `}` |
|         - | 1316 | `/*` |
|         - | 1317 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1318 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1319 | ` */` |
|      3968 | 1320 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1321 | `{` |
|         - | 1322 | `	ph7_hashmap_node *pEntry;` |
|         - | 1323 | `	ph7_value *pVal;` |
|         - | 1324 | `	sxi32 rc;` |
|         - | 1325 | `	sxu32 n;` |
|      3973 | 1326 | `	if( pSrc == pDest ){` |
|         - | 1327 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1328 | `		 * Unlike the zend engine.` |
|         - | 1329 | `		 */` |
|       ! 0 | 1330 | `		return SXRET_OK;` |
|         - | 1331 | `	}` |
|         - | 1332 | `	/* Point to the first inserted entry in the source */` |
|      3973 | 1333 | `	pEntry = pSrc->pFirst;` |
|         - | 1334 | `	/* Perform the duplication */` |
|      8173 | 1335 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1336 | `		/* Extract the node value */` |
|      4205 | 1337 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4205 | 1338 | `		if( pVal ){` |
|      4205 | 1339 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2105 | 1340 | `		}else{` |
|       ! 0 | 1341 | `			rc = SXRET_OK;` |
|         - | 1342 | `		}` |
|      4205 | 1343 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1344 | `			return rc;` |
|         - | 1345 | `		}` |
|         - | 1346 | `		/* Point to the next entry */` |
|      4205 | 1347 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2105 | 1348 | `	}` |
|      3973 | 1349 | `	return SXRET_OK;` |
|      1989 | 1350 | `}` |
|         - | 1351 | `/*` |
|         - | 1352 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1353 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1354 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1355 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1356 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1357 | ` * this instead of PH7_HashmapDup.` |
|         - | 1358 | ` */` |
|        12 | 1359 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1360 | `{` |
|         - | 1361 | `	ph7_hashmap_node *pEntry;` |
|         - | 1362 | `	ph7_value *pVal;` |
|         - | 1363 | `	sxi32 rc;` |
|         - | 1364 | `	sxu32 n;` |
|        13 | 1365 | `	if( pSrc == pDest ){` |
|       ! 0 | 1366 | `		return SXRET_OK;` |
|         - | 1367 | `	}` |
|        13 | 1368 | `	pEntry = pSrc->pFirst;` |
|       725 | 1369 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1370 | `		/* Extract the node value (resolves foreign references) */` |
|       713 | 1371 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       712 | 1372 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       478 | 1373 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1374 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1375 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1376 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1377 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1378 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1379 | `			pVal = 0;` |
|         2 | 1380 | `		}` |
|       713 | 1381 | `		if( pVal ){` |
|       709 | 1382 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1057 | 1383 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       352 | 1384 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       353 | 1385 | `			}else{` |
|         5 | 1386 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1387 | `			}` |
|       709 | 1388 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1389 | `				return rc;` |
|         - | 1390 | `			}` |
|       354 | 1391 | `		}` |
|         - | 1392 | `		/* Point to the next entry */` |
|       713 | 1393 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       357 | 1394 | `	}` |
|        13 | 1395 | `	return SXRET_OK;` |
|         7 | 1396 | `}` |
|         - | 1397 | `/*` |
|         - | 1398 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1399 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1400 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1401 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1402 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1403 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1404 | ` * iterate-a-snapshot semantic.` |
|         - | 1405 | ` */` |
|        46 | 1406 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1407 | `{` |
|         - | 1408 | `	ph7_foreach_step *pStep;` |
|        49 | 1409 | `	sxi32 nRef = 0;` |
|        95 | 1410 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        49 | 1411 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1412 | `			nRef++;` |
|        21 | 1413 | `		}` |
|        26 | 1414 | `	}` |
|        49 | 1415 | `	return nRef;` |
|         3 | 1416 | `}` |
|         - | 1417 | `/*` |
|         - | 1418 | ` * Copy-on-write separation for arrays.` |
|         - | 1419 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1420 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1421 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1422 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1423 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1424 | ` * on the live map the loop is walking, like php.` |
|         - | 1425 | ` */` |
|    228958 | 1426 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1427 | `{` |
|    228963 | 1428 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1429 | `	ph7_hashmap *pNew;` |
|         - | 1430 | `	ph7_value *pBacking;` |
|         - | 1431 | `	sxu32 nValIdx;` |
|         - | 1432 | `	int bValueInPool;` |
|    228963 | 1433 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    228963 | 1434 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1435 | `		/* Sole owner, no separation needed */` |
|    226601 | 1436 | `		return pMap;` |
|         - | 1437 | `	}` |
|      2367 | 1438 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1439 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1440 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1441 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       123 | 1442 | `		return pMap;` |
|         - | 1443 | `	}` |
|         - | 1444 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1445 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1446 | `	 * frame is popped. */` |
|      2245 | 1447 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2245 | 1448 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2240 | 1449 | `		if( pBacking && pBacking != pValue` |
|      2217 | 1450 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2199 | 1451 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1452 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2199 | 1453 | `			pMap->iRef--;` |
|      2199 | 1454 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1455 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2157 | 1456 | `				pMap->iRef++;` |
|      2157 | 1457 | `				return pMap;` |
|         - | 1458 | `			}` |
|        44 | 1459 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        44 | 1460 | `			if( pNew == 0 ){` |
|       ! 0 | 1461 | `				pMap->iRef++;` |
|       ! 0 | 1462 | `				return pMap;` |
|         - | 1463 | `			}` |
|        44 | 1464 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1465 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1466 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1467 | `				pMap->iRef++;` |
|       ! 0 | 1468 | `				return pMap;` |
|         - | 1469 | `			}` |
|        44 | 1470 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        44 | 1471 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1472 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1473 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1474 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1475 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1476 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1477 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1478 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        44 | 1479 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        44 | 1480 | `			if( pBacking ){` |
|        44 | 1481 | `				pBacking->x.pOther = pNew;` |
|        21 | 1482 | `			}` |
|         - | 1483 | `			/* Update the stack value to match */` |
|        44 | 1484 | `			pValue->x.pOther = pNew;` |
|        44 | 1485 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        44 | 1486 | `			return pNew;` |
|         - | 1487 | `		}` |
|        23 | 1488 | `	}` |
|         - | 1489 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1490 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1491 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1492 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1493 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1494 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1495 | `	 * pBacking in the backing-variable branch above). */` |
|        48 | 1496 | `	nValIdx = pValue->nIdx;` |
|        71 | 1497 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        46 | 1498 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        48 | 1499 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        48 | 1500 | `	if( pNew == 0 ){` |
|         - | 1501 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1502 | `		return pMap;` |
|         - | 1503 | `	}` |
|        48 | 1504 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1505 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1506 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1507 | `		return pMap;` |
|         - | 1508 | `	}` |
|        48 | 1509 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        48 | 1510 | `	pMap->iRef--;` |
|        48 | 1511 | `	if( bValueInPool ){` |
|         - | 1512 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        48 | 1513 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        48 | 1514 | `		if( pValue == 0 ){` |
|       ! 0 | 1515 | `			return pNew;` |
|         - | 1516 | `		}` |
|        23 | 1517 | `	}` |
|        48 | 1518 | `	pValue->x.pOther = pNew;` |
|        48 | 1519 | `	return pNew;` |
|    114484 | 1520 | `}` |
|         - | 1521 | `/*` |
|         - | 1522 | ` * Perform the union of two hashmaps.` |
|         - | 1523 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1524 | ` * with a variable holding an array as follows:` |
|         - | 1525 | ` * <?php` |
|         - | 1526 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1527 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1528 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1529 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1530 | ` * var_dump($c);` |
|         - | 1531 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1532 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1533 | ` * var_dump($c);` |
|         - | 1534 | ` * ?>` |
|         - | 1535 | ` * When executed, this script will print the following:` |
|         - | 1536 | ` * Union of $a and $b:` |
|         - | 1537 | ` * array(3) {` |
|         - | 1538 | ` *  ["a"]=>` |
|         - | 1539 | ` *  string(5) "apple"` |
|         - | 1540 | ` *  ["b"]=>` |
|         - | 1541 | ` * string(6) "banana"` |
|         - | 1542 | ` *  ["c"]=>` |
|         - | 1543 | ` * string(6) "cherry"` |
|         - | 1544 | ` * }` |
|         - | 1545 | ` * Union of $b and $a:` |
|         - | 1546 | ` * array(3) {` |
|         - | 1547 | ` * ["a"]=>` |
|         - | 1548 | ` * string(4) "pear"` |
|         - | 1549 | ` * ["b"]=>` |
|         - | 1550 | ` * string(10) "strawberry"` |
|         - | 1551 | ` * ["c"]=>` |
|         - | 1552 | ` * string(6) "cherry"` |
|         - | 1553 | ` * }` |
|         - | 1554 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1555 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1556 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1557 | ` */` |
|      3854 | 1558 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1559 | `{` |
|         - | 1560 | `	ph7_hashmap_node *pEntry;` |
|      3859 | 1561 | `	sxi32 rc = SXRET_OK;` |
|         - | 1562 | `	ph7_value *pObj;` |
|         - | 1563 | `	sxu32 n;` |
|      3859 | 1564 | `	if( pLeft == pRight ){` |
|         - | 1565 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1566 | `		 * Unlike the zend engine.` |
|         - | 1567 | `		 */` |
|       ! 0 | 1568 | `		return SXRET_OK;` |
|         - | 1569 | `	}` |
|         - | 1570 | `	/* Perform the union */` |
|      3859 | 1571 | `	pEntry = pRight->pFirst;` |
|      3893 | 1572 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1573 | `		/* Make sure the given key does not exists in the left array */` |
|        39 | 1574 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1575 | `			/* BLOB key */` |
|        24 | 1576 | `			if( SXRET_OK !=` |
|        20 | 1577 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1578 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1579 | `					if( pObj ){` |
|        20 | 1580 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1581 | `						/* Perform the insertion */` |
|        20 | 1582 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1583 | `							&sSafeVal,0,FALSE);` |
|        20 | 1584 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1585 | `							return rc;` |
|         - | 1586 | `						}` |
|         8 | 1587 | `					}` |
|         8 | 1588 | `			}` |
|        14 | 1589 | `		}else{` |
|         - | 1590 | `			/* INT key */` |
|        16 | 1591 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        11 | 1592 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        11 | 1593 | `				if( pObj ){` |
|        11 | 1594 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1595 | `					/* Perform the insertion */` |
|        11 | 1596 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        11 | 1597 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1598 | `						return rc;` |
|         - | 1599 | `					}` |
|         5 | 1600 | `				}` |
|         5 | 1601 | `			}` |
|         - | 1602 | `		}` |
|         - | 1603 | `		/* Point to the next entry */` |
|        39 | 1604 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        22 | 1605 | `	}` |
|      3859 | 1606 | `	return SXRET_OK;` |
|      1932 | 1607 | `}` |
|         - | 1608 | `/*` |
|         - | 1609 | ` * Allocate a new hashmap.` |
|         - | 1610 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1611 | ` */` |
|    143592 | 1612 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1613 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1614 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1615 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1616 | `	)` |
|         5 | 1617 | `{` |
|         - | 1618 | `	ph7_hashmap *pMap;` |
|         - | 1619 | `	/* Allocate a new instance */` |
|    143597 | 1620 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    143597 | 1621 | `	if( pMap == 0 ){` |
|       ! 0 | 1622 | `		return 0;` |
|         - | 1623 | `	}` |
|         - | 1624 | `	/* Zero the structure */` |
|    143597 | 1625 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1626 | `	/* Fill in the structure */` |
|    143597 | 1627 | `	pMap->pVm = &(*pVm);` |
|    143597 | 1628 | `	pMap->iRef = 1;` |
|         - | 1629 | `	/* Default hash functions */` |
|    143597 | 1630 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    143597 | 1631 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    143597 | 1632 | `	return pMap;` |
|     71801 | 1633 | `}` |
|         - | 1634 | `/*` |
|         - | 1635 | ` * Install superglobals in the given virtual machine.` |
|         - | 1636 | ` * Note on superglobals.` |
|         - | 1637 | ` *  According to the PHP language reference manual.` |
|         - | 1638 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1639 | `*   Description` |
|         - | 1640 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1641 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1642 | `*   global $variable; to access them within functions or methods.` |
|         - | 1643 | `*   These superglobal variables are:` |
|         - | 1644 | `*    $GLOBALS` |
|         - | 1645 | `*    $_SERVER` |
|         - | 1646 | `*    $_GET` |
|         - | 1647 | `*    $_POST` |
|         - | 1648 | `*    $_FILES` |
|         - | 1649 | `*    $_COOKIE` |
|         - | 1650 | `*    $_SESSION` |
|         - | 1651 | `*    $_REQUEST` |
|         - | 1652 | `*    $_ENV` |
|         - | 1653 | `*/` |
|      3510 | 1654 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1655 | `{` |
|         - | 1656 | `	static const char * azSuper[] = {` |
|         - | 1657 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1658 | `		"_GET",      /* $_GET */` |
|         - | 1659 | `		"_POST",     /* $_POST */` |
|         - | 1660 | `		"_FILES",    /* $_FILES */` |
|         - | 1661 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1662 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1663 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1664 | `		"_ENV",      /* $_ENV */` |
|         - | 1665 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1666 | `		"argv"       /* $argv */` |
|         - | 1667 | `	};` |
|         - | 1668 | `	ph7_hashmap *pMap;` |
|         - | 1669 | `	ph7_value *pObj;` |
|         - | 1670 | `	SyString *pFile;` |
|         - | 1671 | `	sxi32 rc;` |
|         - | 1672 | `	sxu32 n;` |
|         - | 1673 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3515 | 1674 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3515 | 1675 | `	if( pMap == 0 ){` |
|       ! 0 | 1676 | `		return SXERR_MEM;` |
|         - | 1677 | `	}` |
|      3515 | 1678 | `	pVm->pGlobal = pMap;` |
|         - | 1679 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3515 | 1680 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3515 | 1681 | `	if( pObj == 0 ){` |
|       ! 0 | 1682 | `		return SXERR_MEM;` |
|         - | 1683 | `	}` |
|      3515 | 1684 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1685 | `	/* Record object index */` |
|      3515 | 1686 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1687 | `	/* Install the special $GLOBALS array */` |
|      3515 | 1688 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3515 | 1689 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1690 | `		return rc;` |
|         - | 1691 | `	}` |
|         - | 1692 | `	/* Install superglobals now */` |
|     38615 | 1693 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1694 | `		ph7_value *pSuper;` |
|         - | 1695 | `		/* Request an empty array */` |
|     35105 | 1696 | `		pSuper = ph7_new_array(&(*pVm));` |
|     35105 | 1697 | `		if( pSuper == 0 ){` |
|       ! 0 | 1698 | `			return SXERR_MEM;` |
|         - | 1699 | `		}` |
|         - | 1700 | `		/* Install */` |
|     35105 | 1701 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     35105 | 1702 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1703 | `			return rc;` |
|         - | 1704 | `		}` |
|         - | 1705 | `		/* Release the value now it have been installed */` |
|     35105 | 1706 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17555 | 1707 | `	}` |
|         - | 1708 | `	/* Set some $_SERVER entries */` |
|      3515 | 1709 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1710 | `	/*` |
|         - | 1711 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1712 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1713 | `	 */` |
|      7021 | 1714 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1715 | `		"SCRIPT_FILENAME",` |
|      1755 | 1716 | `		pFile ? pFile->zString : ":Memory:",` |
|      3506 | 1717 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1718 | `		);` |
|         - | 1719 | `	/* All done,all super-global are installed now */` |
|      3515 | 1720 | `	return SXRET_OK;` |
|      1760 | 1721 | `}` |
|         - | 1722 | `/*` |
|         - | 1723 | ` * Release a hashmap.` |
|         - | 1724 | ` */` |
|    100192 | 1725 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1726 | `{` |
|         - | 1727 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    100197 | 1728 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1729 | `	sxu32 n;` |
|    100197 | 1730 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1731 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1732 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1733 | `		return SXRET_OK;` |
|         - | 1734 | `	}` |
|    100197 | 1735 | `	if( pMap->pActiveSteps ){` |
|         - | 1736 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1737 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1738 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1739 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1740 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1741 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1742 | `		ph7_foreach_step *pStep;` |
|        17 | 1743 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1744 | `			pStep->pCursor = 0;` |
|         5 | 1745 | `		}` |
|         4 | 1746 | `	}` |
|         - | 1747 | `	/* Start the release process */` |
|    100197 | 1748 | `	n = 0;` |
|    100197 | 1749 | `	pEntry = pMap->pFirst;` |
|   1711842 | 1750 | `	for(;;){` |
|   3423689 | 1751 | `		if( n >= pMap->nEntry ){` |
|    100197 | 1752 | `			break;` |
|         - | 1753 | `		}` |
|   3323497 | 1754 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1755 | `		/* Remove the reference from the foreign table */` |
|   3323497 | 1756 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3323497 | 1757 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1758 | `			/* Restore the ph7_value to the free list */` |
|   3323487 | 1759 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1661741 | 1760 | `		}` |
|         - | 1761 | `		/* Release the node */` |
|   3323497 | 1762 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    194911 | 1763 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     97453 | 1764 | `		}` |
|   3323497 | 1765 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1766 | `		/* Point to the next entry */` |
|   3323497 | 1767 | `		pEntry = pNext;` |
|   3323497 | 1768 | `		n++;` |
|         5 | 1769 | `	}` |
|    100197 | 1770 | `	if( pMap->nEntry > 0 ){` |
|         - | 1771 | `		/* Release the hash bucket */` |
|     75621 | 1772 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     37808 | 1773 | `	}` |
|    100197 | 1774 | `	if( FreeDS ){` |
|         - | 1775 | `		/* Free the whole instance */` |
|    100173 | 1776 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     50089 | 1777 | `	}else{` |
|         - | 1778 | `		/* Keep the instance but reset it's fields */` |
|        26 | 1779 | `		pMap->apBucket = 0;` |
|        26 | 1780 | `		pMap->iNextIdx = 0;` |
|        26 | 1781 | `		pMap->nEntry = pMap->nSize = 0;` |
|        26 | 1782 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1783 | `	}` |
|    100197 | 1784 | `	return SXRET_OK;` |
|     50101 | 1785 | `}` |
|         - | 1786 | `/*` |
|         - | 1787 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1788 | ` * If the count reaches zero which mean no more variables` |
|         - | 1789 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1790 | ` */` |
|    825130 | 1791 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1792 | `{` |
|    825135 | 1793 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1794 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    825135 | 1795 | `	pMap->iRef--;` |
|    825135 | 1796 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    100153 | 1797 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     50074 | 1798 | `	}` |
|    825135 | 1799 | `}` |
|         - | 1800 | `/*` |
|         - | 1801 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1802 | ` * Write a pointer to the target node on success.` |
|         - | 1803 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1804 | ` */` |
|    137450 | 1805 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1806 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1807 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1808 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1809 | `	)` |
|         5 | 1810 | `{` |
|         - | 1811 | `	sxi32 rc;` |
|    137455 | 1812 | `	if( pMap->nEntry < 1 ){` |
|         - | 1813 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1814 | `		 */` |
|       152 | 1815 | `		return SXERR_NOTFOUND;` |
|         - | 1816 | `	}` |
|    137307 | 1817 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    137307 | 1818 | `	return rc;` |
|     68730 | 1819 | `}` |
|         - | 1820 | `/*` |
|         - | 1821 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1822 | ` * hashmap.` |
|         - | 1823 | ` * If a node with the given key already exists in the database` |
|         - | 1824 | ` * then this function overwrite the old value.` |
|         - | 1825 | ` */` |
|   2715950 | 1826 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1827 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1828 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1829 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1830 | `	)` |
|         5 | 1831 | `{` |
|         - | 1832 | `	sxi32 rc;` |
|         - | 1833 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1834 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1835 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1836 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   2715955 | 1837 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2715955 | 1838 | `	return rc;` |
|         5 | 1839 | `}` |
|         - | 1840 | `/*` |
|         - | 1841 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1842 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1843 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1844 | ` * This is the same routine that backs array_merge().` |
|         - | 1845 | ` */` |
|       654 | 1846 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1847 | `{` |
|       655 | 1848 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1849 | `}` |
|         - | 1850 | `/*` |
|         - | 1851 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1852 | ` * hashmap.` |
|         - | 1853 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1854 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1855 | ` * The insertion by reference is triggered when the following` |
|         - | 1856 | ` * expression is encountered.` |
|         - | 1857 | ` * $var = 10;` |
|         - | 1858 | ` *  $a = array(&var);` |
|         - | 1859 | ` * OR` |
|         - | 1860 | ` *  $a[] =& $var;` |
|         - | 1861 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1862 | ` * over it's contents.` |
|         - | 1863 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1864 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1865 | ` * Example:` |
|         - | 1866 | ` *  $var = 10;` |
|         - | 1867 | ` *  $a[] =& $var;` |
|         - | 1868 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1869 | ` *  //Unset the foreign ph7_value now` |
|         - | 1870 | ` *  unset($var);` |
|         - | 1871 | ` *  echo count($a); //0` |
|         - | 1872 | ` * Note that this is a PH7 eXtension.` |
|         - | 1873 | ` * Refer to the official documentation for more information.` |
|         - | 1874 | ` * If a node with the given key already exists in the database` |
|         - | 1875 | ` * then this function overwrite the old value.` |
|         - | 1876 | ` */` |
|     47232 | 1877 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1878 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1879 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1880 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1881 | `	)` |
|         5 | 1882 | `{` |
|         - | 1883 | `	sxi32 rc;` |
|     47237 | 1884 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1885 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1886 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1887 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1888 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1889 | `		return PH7_ABORT;` |
|         - | 1890 | `	}` |
|     47237 | 1891 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     47237 | 1892 | `	return rc;` |
|     23621 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1896 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1897 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1898 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1899 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1900 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1901 | ` */` |
|     18712 | 1902 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1903 | `{` |
|     18717 | 1904 | `	pStep->pCursor = pMap->pFirst;` |
|     18717 | 1905 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     18717 | 1906 | `	pMap->pActiveSteps = pStep;` |
|     18717 | 1907 | `}` |
|         - | 1908 | `/*` |
|         - | 1909 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1910 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1911 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1912 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1913 | ` */` |
|     18614 | 1914 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1915 | `{` |
|     18619 | 1916 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18619 | 1917 | `	while( *ppLink ){` |
|     18619 | 1918 | `		if( *ppLink == pStep ){` |
|     18619 | 1919 | `			*ppLink = pStep->pNextActive;` |
|     18619 | 1920 | `			pStep->pNextActive = 0;` |
|     18619 | 1921 | `			return;` |
|         - | 1922 | `		}` |
|       ! 0 | 1923 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1924 | `	}` |
|      9312 | 1925 | `}` |
|         - | 1926 | `/*` |
|         - | 1927 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1928 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1929 | ` * return NULL.` |
|         - | 1930 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1931 | ` */` |
|        50 | 1932 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 1933 | `{` |
|        51 | 1934 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        51 | 1935 | `	if( pCur == 0 ){` |
|         - | 1936 | `		/* End of the list,return null */` |
|        21 | 1937 | `		return 0;` |
|         - | 1938 | `	}` |
|         - | 1939 | `	/* Advance the node cursor */` |
|        31 | 1940 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        31 | 1941 | `	return pCur;` |
|        26 | 1942 | `}` |
|         - | 1943 | `/*` |
|         - | 1944 | ` * Extract a node value.` |
|         - | 1945 | ` */` |
|    579236 | 1946 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1947 | `{` |
|    579241 | 1948 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    579241 | 1949 | `	if( pEntry ){` |
|    579241 | 1950 | `		if( bStore ){` |
|    229953 | 1951 | `			PH7_MemObjStore(pEntry,pValue);` |
|    114979 | 1952 | `		}else{` |
|    349293 | 1953 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1954 | `		}` |
|    289659 | 1955 | `	}else{` |
|       ! 0 | 1956 | `		PH7_MemObjRelease(pValue);` |
|         - | 1957 | `	}` |
|    579241 | 1958 | `}` |
|         - | 1959 | `/*` |
|         - | 1960 | ` * Extract a node key.` |
|         - | 1961 | ` */` |
|    151726 | 1962 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1963 | `{` |
|         - | 1964 | `	/* Fill with the current key */` |
|    151731 | 1965 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    146769 | 1966 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 1967 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 1968 | `		}` |
|    146769 | 1969 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    146769 | 1970 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     73387 | 1971 | `	}else{` |
|      4967 | 1972 | `		SyBlobReset(&pKey->sBlob);` |
|      4967 | 1973 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      4967 | 1974 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1975 | `	}` |
|    151731 | 1976 | `}` |
|         - | 1977 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 1978 | `/*` |
|         - | 1979 | ` * Store the address of nodes value in the given container.` |
|         - | 1980 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 1981 | ` * defined in 'builtin.c' for more information.` |
|         - | 1982 | ` */` |
|        12 | 1983 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 1984 | `{` |
|        13 | 1985 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 1986 | `	ph7_value *pValue;` |
|         - | 1987 | `	sxu32 n;` |
|         - | 1988 | `	/* Initialize the container */` |
|        13 | 1989 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 1990 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 1991 | `		/* Extract node value */` |
|        21 | 1992 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        21 | 1993 | `		if( pValue ){` |
|        21 | 1994 | `			SySetPut(pOut,(const void *)&pValue);` |
|        10 | 1995 | `		}` |
|         - | 1996 | `		/* Point to the next entry */` |
|        21 | 1997 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        11 | 1998 | `	}` |
|         - | 1999 | `	/* Total inserted entries */` |
|        13 | 2000 | `	return (int)SySetUsed(pOut);` |
|         1 | 2001 | `}` |
|         - | 2002 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2003 | `/* SPDX-SnippetBegin */` |
|         - | 2004 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 2005 | `/* SPDX-License-Identifier: blessing */` |
|         - | 2006 | `/*` |
|         - | 2007 | ` * Merge sort.` |
|         - | 2008 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 2009 | ` * Status: Public domain` |
|         - | 2010 | ` */` |
|         - | 2011 | `/* Node comparison callback signature */` |
|         - | 2012 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 2013 | `/*` |
|         - | 2014 | `** Inputs:` |
|         - | 2015 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2016 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2017 | `**   cmp:     A pointer to the comparison function.` |
|         - | 2018 | `**` |
|         - | 2019 | `** Return Value:` |
|         - | 2020 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 2021 | `**   of both a and b.` |
|         - | 2022 | `**` |
|         - | 2023 | `** Side effects:` |
|         - | 2024 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 2025 | `**   changed.` |
|         - | 2026 | `*/` |
|     34972 | 2027 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2028 | `{` |
|         - | 2029 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2030 | `    /* Prevent compiler warning */` |
|     34977 | 2031 | `	result.pNext = result.pPrev = 0;` |
|     34977 | 2032 | `	pTail = &result;` |
|    106215 | 2033 | `	while( pA && pB ){` |
|     71243 | 2034 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47069 | 2035 | `			pTail->pPrev = pA;` |
|     47069 | 2036 | `			pA->pNext = pTail;` |
|     47069 | 2037 | `			pTail = pA;` |
|     47069 | 2038 | `			pA = pA->pPrev;` |
|     23549 | 2039 | `		}else{` |
|     24179 | 2040 | `			pTail->pPrev = pB;` |
|     24179 | 2041 | `			pB->pNext = pTail;` |
|     24179 | 2042 | `			pTail = pB;` |
|     24179 | 2043 | `			pB = pB->pPrev;` |
|         - | 2044 | `		}` |
|         5 | 2045 | `	}` |
|     34977 | 2046 | `	if( pA ){` |
|     24611 | 2047 | `		pTail->pPrev = pA;` |
|     24611 | 2048 | `		pA->pNext = pTail;` |
|     22690 | 2049 | `	}else if( pB ){` |
|     10145 | 2050 | `		pTail->pPrev = pB;` |
|     10145 | 2051 | `		pB->pNext = pTail;` |
|      5059 | 2052 | `	}else{` |
|       231 | 2053 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2054 | `	}` |
|     34977 | 2055 | `	return result.pPrev;` |
|         5 | 2056 | `}` |
|         - | 2057 | `/*` |
|         - | 2058 | `** Inputs:` |
|         - | 2059 | `**   Map:       Input hashmap` |
|         - | 2060 | `**   cmp:       A comparison function.` |
|         - | 2061 | `**` |
|         - | 2062 | `** Return Value:` |
|         - | 2063 | `**   Sorted hashmap.` |
|         - | 2064 | `**` |
|         - | 2065 | `** Side effects:` |
|         - | 2066 | `**   The "next" pointers for elements in list are changed.` |
|         - | 2067 | `*/` |
|         - | 2068 | `#define N_SORT_BUCKET  32` |
|       724 | 2069 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2070 | `{` |
|         - | 2071 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2072 | `	sxu32 i;` |
|       729 | 2073 | `	SyZero(a,sizeof(a));` |
|         - | 2074 | `	/* Point to the first inserted entry */` |
|       729 | 2075 | `	pIn = pMap->pFirst;` |
|     14541 | 2076 | `	while( pIn ){` |
|     13817 | 2077 | `		p = pIn;` |
|     13817 | 2078 | `		pIn = p->pPrev;` |
|     13817 | 2079 | `		p->pPrev = 0;` |
|     26345 | 2080 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26345 | 2081 | `			if( a[i]==0 ){` |
|     13817 | 2082 | `				a[i] = p;` |
|     13817 | 2083 | `				break;` |
|       ! 0 | 2084 | `			}else{` |
|     12533 | 2085 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12533 | 2086 | `				a[i] = 0;` |
|         - | 2087 | `			}` |
|      6269 | 2088 | `		}` |
|     13817 | 2089 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2090 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2091 | `			 * But that is impossible.` |
|         - | 2092 | `			 */` |
|       ! 0 | 2093 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2094 | `		}` |
|         5 | 2095 | `	}` |
|       729 | 2096 | `	p = a[0];` |
|     23173 | 2097 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     22449 | 2098 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11227 | 2099 | `	}` |
|       729 | 2100 | `	p->pNext = 0;` |
|         - | 2101 | `	/* Reflect the change */` |
|       729 | 2102 | `	pMap->pFirst = p;` |
|         - | 2103 | `	/* Reset the loop cursor */` |
|       729 | 2104 | `	pMap->pCur = pMap->pFirst;` |
|       729 | 2105 | `	return SXRET_OK;` |
|         5 | 2106 | `}` |
|         - | 2107 | `/* SPDX-SnippetEnd */` |
|         - | 2108 | `/*` |
|         - | 2109 | ` * Node comparison callback.` |
|         - | 2110 | ` * used-by: [sort(),asort(),...]` |
|         - | 2111 | ` */` |
|     71014 | 2112 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2113 | `{` |
|         - | 2114 | `	ph7_value sA,sB;` |
|         - | 2115 | `	sxi32 iFlags;` |
|         - | 2116 | `	int rc;` |
|     71019 | 2117 | `	if( pCmpData == 0 ){` |
|         - | 2118 | `		/* Perform a standard comparison */` |
|     70995 | 2119 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     70995 | 2120 | `		return rc;` |
|         - | 2121 | `	}` |
|        25 | 2122 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2123 | `	/* Duplicate node values */` |
|        25 | 2124 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        25 | 2125 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        25 | 2126 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        25 | 2127 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        25 | 2128 | `	if( iFlags == 5 ){` |
|         - | 2129 | `		/* String cast */` |
|         - | 2130 | `		const char *zA,*zB;` |
|         - | 2131 | `		sxu32 nA,nB,nMin;` |
|        15 | 2132 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2133 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2134 | `		}` |
|        15 | 2135 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2136 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2137 | `		}` |
|         - | 2138 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        15 | 2139 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        15 | 2140 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        15 | 2141 | `		nA = SyBlobLength(&sA.sBlob);` |
|        15 | 2142 | `		nB = SyBlobLength(&sB.sBlob);` |
|        15 | 2143 | `		nMin = nA < nB ? nA : nB;` |
|        15 | 2144 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        15 | 2145 | `		if( rc == 0 ){` |
|         5 | 2146 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2147 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2148 | `		}` |
|         8 | 2149 | `	}else{` |
|         - | 2150 | `		/* Numeric cast */` |
|        11 | 2151 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2152 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2153 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2154 | `	}` |
|        25 | 2155 | `	PH7_MemObjRelease(&sA);` |
|        25 | 2156 | `	PH7_MemObjRelease(&sB);` |
|        25 | 2157 | `	return rc;` |
|     35530 | 2158 | `}` |
|         - | 2159 | `/*` |
|         - | 2160 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2161 | ` * used-by: [ksort()]` |
|         - | 2162 | ` */` |
|        16 | 2163 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2164 | `{` |
|         - | 2165 | `	sxi32 rc;` |
|         8 | 2166 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        17 | 2167 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2168 | `		/* Perform a string comparison */` |
|         7 | 2169 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         4 | 2170 | `	}else{` |
|         - | 2171 | `		SyString sStr;` |
|         - | 2172 | `		sxi64 iA,iB;` |
|         - | 2173 | `		/* Perform a numeric comparison */` |
|        11 | 2174 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2175 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2176 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2177 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2178 | `				iA = 0;` |
|       ! 0 | 2179 | `			}else{` |
|       ! 0 | 2180 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2181 | `			}` |
|       ! 0 | 2182 | `		}else{` |
|        11 | 2183 | `			iA = pA->xKey.iKey;` |
|         - | 2184 | `		}` |
|        11 | 2185 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2186 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2187 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2188 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2189 | `				iB = 0;` |
|       ! 0 | 2190 | `			}else{` |
|       ! 0 | 2191 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2192 | `			}` |
|       ! 0 | 2193 | `		}else{` |
|        11 | 2194 | `			iB = pB->xKey.iKey;` |
|         - | 2195 | `		}` |
|        11 | 2196 | `		rc = (sxi32)(iA-iB);` |
|         - | 2197 | `	}` |
|         - | 2198 | `	/* Comparison result */` |
|        17 | 2199 | `	return rc;` |
|         1 | 2200 | `}` |
|         - | 2201 | `/*` |
|         - | 2202 | ` * Node comparison callback.` |
|         - | 2203 | ` * Used by: [rsort(),arsort()];` |
|         - | 2204 | ` */` |
|        78 | 2205 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2206 | `{` |
|         - | 2207 | `	ph7_value sA,sB;` |
|         - | 2208 | `	sxi32 iFlags;` |
|         - | 2209 | `	int rc;` |
|        79 | 2210 | `	if( pCmpData == 0 ){` |
|         - | 2211 | `		/* Perform a standard comparison */` |
|        59 | 2212 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2213 | `		return -rc;` |
|         - | 2214 | `	}` |
|        21 | 2215 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2216 | `	/* Duplicate node values */` |
|        21 | 2217 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2218 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2219 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2220 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2221 | `	if( iFlags == 5 ){` |
|         - | 2222 | `		/* String cast */` |
|         - | 2223 | `		const char *zA,*zB;` |
|         - | 2224 | `		sxu32 nA,nB,nMin;` |
|        11 | 2225 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2226 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2227 | `		}` |
|        11 | 2228 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2229 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2230 | `		}` |
|         - | 2231 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2232 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2233 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2234 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2235 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2236 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2237 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2238 | `		if( rc == 0 ){` |
|         3 | 2239 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2240 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2241 | `		}` |
|         6 | 2242 | `	}else{` |
|         - | 2243 | `		/* Numeric cast */` |
|        11 | 2244 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2245 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2246 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2247 | `	}` |
|        21 | 2248 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2249 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2250 | `	return -rc;` |
|        40 | 2251 | `}` |
|         - | 2252 | `/*` |
|         - | 2253 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2254 | ` * used-by: [usort(),uasort()]` |
|         - | 2255 | ` */` |
|       100 | 2256 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2257 | `{` |
|         - | 2258 | `	ph7_value sResult,*pCallback;` |
|         - | 2259 | `	ph7_value *pV1,*pV2;` |
|         - | 2260 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2261 | `	sxi32 rc;` |
|         - | 2262 | `	/* Point to the desired callback */` |
|       103 | 2263 | `	pCallback = (ph7_value *)pCmpData;` |
|       103 | 2264 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2265 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2266 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2267 | `		return 0;` |
|         - | 2268 | `	}` |
|         - | 2269 | `	/* initialize the result value */` |
|        97 | 2270 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2271 | `	/* Extract nodes values */` |
|        97 | 2272 | `	pV1 = HashmapExtractNodeValue(pA);` |
|        97 | 2273 | `	pV2 = HashmapExtractNodeValue(pB);` |
|        97 | 2274 | `	apArg[0] = pV1;` |
|        97 | 2275 | `	apArg[1] = pV2;` |
|         - | 2276 | `	/* Invoke the callback */` |
|        97 | 2277 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|        97 | 2278 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2279 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2280 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2281 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2282 | `		rc = 0;` |
|        92 | 2283 | `	}else if( rc != SXRET_OK ){` |
|         - | 2284 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2285 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2286 | `	}else{` |
|         - | 2287 | `		/* Extract callback result */` |
|        88 | 2288 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2289 | `			/* Perform an int cast */` |
|       ! 0 | 2290 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2291 | `		}` |
|        88 | 2292 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2293 | `	}` |
|        97 | 2294 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2295 | `	/* Callback result */` |
|        97 | 2296 | `	return rc;` |
|        53 | 2297 | `}` |
|         - | 2298 | `/*` |
|         - | 2299 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2300 | ` * used-by: [krsort()]` |
|         - | 2301 | ` */` |
|         4 | 2302 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2303 | `{` |
|         - | 2304 | `	sxi32 rc;` |
|         2 | 2305 | `	SXUNUSED(pCmpData); /* cc warning */` |
|         5 | 2306 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2307 | `		/* Perform a string comparison */` |
|         5 | 2308 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         3 | 2309 | `	}else{` |
|         - | 2310 | `		SyString sStr;` |
|         - | 2311 | `		sxi64 iA,iB;` |
|         - | 2312 | `		/* Perform a numeric comparison */` |
|       ! 0 | 2313 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2314 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2315 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2316 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2317 | `				iA = 0;` |
|       ! 0 | 2318 | `			}else{` |
|       ! 0 | 2319 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2320 | `			}` |
|       ! 0 | 2321 | `		}else{` |
|       ! 0 | 2322 | `			iA = pA->xKey.iKey;` |
|         - | 2323 | `		}` |
|       ! 0 | 2324 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2325 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2326 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2327 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2328 | `				iB = 0;` |
|       ! 0 | 2329 | `			}else{` |
|       ! 0 | 2330 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2331 | `			}` |
|       ! 0 | 2332 | `		}else{` |
|       ! 0 | 2333 | `			iB = pB->xKey.iKey;` |
|         - | 2334 | `		}` |
|       ! 0 | 2335 | `		rc = (sxi32)(iA-iB);` |
|         - | 2336 | `	}` |
|         5 | 2337 | `	return -rc; /* Reverse result */` |
|         1 | 2338 | `}` |
|         - | 2339 | `/*` |
|         - | 2340 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2341 | ` * used-by: [uksort()]` |
|         - | 2342 | ` */` |
|         6 | 2343 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2344 | `{` |
|         - | 2345 | `	ph7_value sResult,*pCallback;` |
|         - | 2346 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2347 | `	ph7_value sK1,sK2;` |
|         - | 2348 | `	sxi32 rc;` |
|         - | 2349 | `	/* Point to the desired callback */` |
|         7 | 2350 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2351 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2352 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2353 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2354 | `		return 0;` |
|         - | 2355 | `	}` |
|         - | 2356 | `	/* initialize the result value */` |
|         7 | 2357 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2358 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2359 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2360 | `	/* Extract nodes keys */` |
|         7 | 2361 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2362 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2363 | `	apArg[0] = &sK1;` |
|         7 | 2364 | `	apArg[1] = &sK2;` |
|         - | 2365 | `	/* Mark keys as constants */` |
|         7 | 2366 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2367 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2368 | `	/* Invoke the callback */` |
|         7 | 2369 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2370 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2371 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2372 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2373 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2374 | `		rc = 0;` |
|         7 | 2375 | `	}else if( rc != SXRET_OK ){` |
|         - | 2376 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2377 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2378 | `	}else{` |
|         - | 2379 | `		/* Extract callback result */` |
|         7 | 2380 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2381 | `			/* Perform an int cast */` |
|       ! 0 | 2382 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2383 | `		}` |
|         7 | 2384 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2385 | `	}` |
|         7 | 2386 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2387 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2388 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2389 | `	/* Callback result */` |
|         7 | 2390 | `	return rc;` |
|         4 | 2391 | `}` |
|         - | 2392 | `/*` |
|         - | 2393 | ` * Node comparison callback: Random node comparison.` |
|         - | 2394 | ` * used-by: [shuffle()]` |
|         - | 2395 | ` */` |
|        20 | 2396 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2397 | `{` |
|         - | 2398 | `	sxu32 n;` |
|         9 | 2399 | `	SXUNUSED(pB); /* cc warning */` |
|         9 | 2400 | `	SXUNUSED(pCmpData);` |
|         - | 2401 | `	/* Grab a random number */` |
|        21 | 2402 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2403 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2404 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2405 | `	 */` |
|        21 | 2406 | `	return n&1 ? 1 : -1;` |
|         1 | 2407 | `}` |
|         - | 2408 | `/*` |
|         - | 2409 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2410 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2411 | ` */` |
|       674 | 2412 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2413 | `{` |
|         - | 2414 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2415 | `	sxu32 i;` |
|         - | 2416 | `	/* Rehash all entries */` |
|       679 | 2417 | `	pLast = p = pMap->pFirst;` |
|       679 | 2418 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|       679 | 2419 | `	i = 0;` |
|      7156 | 2420 | `	for( ;; ){` |
|     14317 | 2421 | `		if( i >= pMap->nEntry ){` |
|       679 | 2422 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       679 | 2423 | `			break;` |
|         - | 2424 | `		}` |
|     13643 | 2425 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2426 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2427 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2428 | `			/* Change key type */` |
|         5 | 2429 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2430 | `		}` |
|     13643 | 2431 | `		HashmapRehashIntNode(p);` |
|         - | 2432 | `		/* Point to the next entry */` |
|     13643 | 2433 | `		i++;` |
|     13643 | 2434 | `		pLast = p;` |
|     13643 | 2435 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2436 | `	}` |
|       679 | 2437 | `}` |
|         - | 2438 | `/*` |
|         - | 2439 | ` * Array functions implementation.` |
|         - | 2440 | ` * Status:` |
|         - | 2441 | ` *  Stable.` |
|         - | 2442 | ` */` |
|         - | 2443 | `/*` |
|         - | 2444 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2445 | ` * Sort an array.` |
|         - | 2446 | ` * Parameters` |
|         - | 2447 | ` *  $array` |
|         - | 2448 | ` *   The input array.` |
|         - | 2449 | ` * $sort_flags` |
|         - | 2450 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2451 | ` *  Sorting type flags:` |
|         - | 2452 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2453 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2454 | ` *   SORT_STRING - compare items as strings` |
|         - | 2455 | ` * Return` |
|         - | 2456 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2457 | ` *` |
|         - | 2458 | ` */` |
|      1000 | 2459 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2460 | `{` |
|         - | 2461 | `	ph7_hashmap *pMap;` |
|         - | 2462 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1005 | 2463 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2464 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2465 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2466 | `		return PH7_OK;` |
|         - | 2467 | `	}` |
|         - | 2468 | `	/* Point to the internal representation of the input hashmap */` |
|      1005 | 2469 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1005 | 2470 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1005 | 2471 | `	if( pMap->nEntry > 1 ){` |
|       655 | 2472 | `		sxi32 iCmpFlags = 0;` |
|       655 | 2473 | `		if( nArg > 1 ){` |
|         - | 2474 | `			/* Extract comparison flags */` |
|         3 | 2475 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2476 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2477 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2478 | `			}` |
|         1 | 2479 | `		}` |
|         - | 2480 | `		/* Do the merge sort */` |
|       655 | 2481 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2482 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       655 | 2483 | `		HashmapSortRehash(pMap);` |
|       325 | 2484 | `	}` |
|         - | 2485 | `	/* All done,return TRUE */` |
|      1005 | 2486 | `	ph7_result_bool(pCtx,1);` |
|      1005 | 2487 | `	return PH7_OK;` |
|       505 | 2488 | `}` |
|         - | 2489 | `/*` |
|         - | 2490 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2491 | ` *  Sort an array and maintain index association.` |
|         - | 2492 | ` * Parameters` |
|         - | 2493 | ` *  $array` |
|         - | 2494 | ` *   The input array.` |
|         - | 2495 | ` * $sort_flags` |
|         - | 2496 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2497 | ` *  Sorting type flags:` |
|         - | 2498 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2499 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2500 | ` *   SORT_STRING - compare items as strings` |
|         - | 2501 | ` * Return` |
|         - | 2502 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2503 | ` */` |
|        32 | 2504 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2505 | `{` |
|         - | 2506 | `	ph7_hashmap *pMap;` |
|         - | 2507 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2508 | `	if( nArg < 1 ){` |
|         3 | 2509 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2510 | `			"ArgumentCountError",` |
|         - | 2511 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2512 | `			);` |
|         - | 2513 | `	}` |
|         - | 2514 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2515 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2516 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2517 | `			"TypeError",` |
|         - | 2518 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2519 | `			ph7_type_name(apArg[0])` |
|         - | 2520 | `			);` |
|         - | 2521 | `	}` |
|         - | 2522 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2523 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2524 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2525 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2526 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2527 | `		if( nArg > 1 ){` |
|         - | 2528 | `			/* Extract comparison flags */` |
|         5 | 2529 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2530 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2531 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2532 | `			}` |
|         2 | 2533 | `		}` |
|         - | 2534 | `		/* Do the merge sort */` |
|        19 | 2535 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2536 | `		/* Fix the last link broken by the merge */` |
|        45 | 2537 | `		while(pMap->pLast->pPrev){` |
|        27 | 2538 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2539 | `		}` |
|         9 | 2540 | `	}` |
|         - | 2541 | `	/* All done,return TRUE */` |
|        23 | 2542 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2543 | `	return PH7_OK;` |
|        21 | 2544 | `}` |
|         - | 2545 | `/*` |
|         - | 2546 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2547 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2548 | ` * Parameters` |
|         - | 2549 | ` *  $array` |
|         - | 2550 | ` *   The input array.` |
|         - | 2551 | ` * $sort_flags` |
|         - | 2552 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2553 | ` *  Sorting type flags:` |
|         - | 2554 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2555 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2556 | ` *   SORT_STRING - compare items as strings` |
|         - | 2557 | ` * Return` |
|         - | 2558 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2559 | ` */` |
|        32 | 2560 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2561 | `{` |
|         - | 2562 | `	ph7_hashmap *pMap;` |
|         - | 2563 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2564 | `	if( nArg < 1 ){` |
|         3 | 2565 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2566 | `			"ArgumentCountError",` |
|         - | 2567 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2568 | `			);` |
|         - | 2569 | `	}` |
|         - | 2570 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2571 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2572 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2573 | `			"TypeError",` |
|         - | 2574 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2575 | `			ph7_type_name(apArg[0])` |
|         - | 2576 | `			);` |
|         - | 2577 | `	}` |
|         - | 2578 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2579 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2580 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2581 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2582 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2583 | `		if( nArg > 1 ){` |
|         - | 2584 | `			/* Extract comparison flags */` |
|         5 | 2585 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2586 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2587 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2588 | `			}` |
|         2 | 2589 | `		}` |
|         - | 2590 | `		/* Do the merge sort */` |
|        19 | 2591 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2592 | `		/* Fix the last link broken by the merge */` |
|        35 | 2593 | `		while(pMap->pLast->pPrev){` |
|        17 | 2594 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2595 | `		}` |
|         9 | 2596 | `	}` |
|         - | 2597 | `	/* All done,return TRUE */` |
|        23 | 2598 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2599 | `	return PH7_OK;` |
|        21 | 2600 | `}` |
|         - | 2601 | `/*` |
|         - | 2602 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2603 | ` *  Sort an array by key.` |
|         - | 2604 | ` * Parameters` |
|         - | 2605 | ` *  $array` |
|         - | 2606 | ` *   The input array.` |
|         - | 2607 | ` * $sort_flags` |
|         - | 2608 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2609 | ` *  Sorting type flags:` |
|         - | 2610 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2611 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2612 | ` *   SORT_STRING - compare items as strings` |
|         - | 2613 | ` * Return` |
|         - | 2614 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2615 | ` */` |
|         6 | 2616 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2617 | `{` |
|         - | 2618 | `	ph7_hashmap *pMap;` |
|         - | 2619 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2620 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2621 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2622 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2623 | `		return PH7_OK;` |
|         - | 2624 | `	}` |
|         - | 2625 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2626 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2627 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2628 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2629 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2630 | `		if( nArg > 1 ){` |
|         - | 2631 | `			/* Extract comparison flags */` |
|       ! 0 | 2632 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2633 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2634 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2635 | `			}` |
|       ! 0 | 2636 | `		}` |
|         - | 2637 | `		/* Do the merge sort */` |
|         7 | 2638 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2639 | `		/* Fix the last link broken by the merge */` |
|        17 | 2640 | `		while(pMap->pLast->pPrev){` |
|        11 | 2641 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2642 | `		}` |
|         3 | 2643 | `	}` |
|         - | 2644 | `	/* All done,return TRUE */` |
|         7 | 2645 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2646 | `	return PH7_OK;` |
|         4 | 2647 | `}` |
|         - | 2648 | `/*` |
|         - | 2649 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2650 | ` *  Sort an array by key in reverse order.` |
|         - | 2651 | ` * Parameters` |
|         - | 2652 | ` *  $array` |
|         - | 2653 | ` *   The input array.` |
|         - | 2654 | ` * $sort_flags` |
|         - | 2655 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2656 | ` *  Sorting type flags:` |
|         - | 2657 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2658 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2659 | ` *   SORT_STRING - compare items as strings` |
|         - | 2660 | ` * Return` |
|         - | 2661 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2662 | ` */` |
|         2 | 2663 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2664 | `{` |
|         - | 2665 | `	ph7_hashmap *pMap;` |
|         - | 2666 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2667 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2668 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2669 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2670 | `		return PH7_OK;` |
|         - | 2671 | `	}` |
|         - | 2672 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2673 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2674 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2675 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2676 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2677 | `		if( nArg > 1 ){` |
|         - | 2678 | `			/* Extract comparison flags */` |
|       ! 0 | 2679 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2680 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2681 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2682 | `			}` |
|       ! 0 | 2683 | `		}` |
|         - | 2684 | `		/* Do the merge sort */` |
|         3 | 2685 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2686 | `		/* Fix the last link broken by the merge */` |
|         7 | 2687 | `		while(pMap->pLast->pPrev){` |
|         5 | 2688 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2689 | `		}` |
|         1 | 2690 | `	}` |
|         - | 2691 | `	/* All done,return TRUE */` |
|         3 | 2692 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2693 | `	return PH7_OK;` |
|         2 | 2694 | `}` |
|         - | 2695 | `/*` |
|         - | 2696 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2697 | ` * Sort an array in reverse order.` |
|         - | 2698 | ` * Parameters` |
|         - | 2699 | ` *  $array` |
|         - | 2700 | ` *   The input array.` |
|         - | 2701 | ` * $sort_flags` |
|         - | 2702 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2703 | ` *  Sorting type flags:` |
|         - | 2704 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2705 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2706 | ` *   SORT_STRING - compare items as strings` |
|         - | 2707 | ` * Return` |
|         - | 2708 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2709 | ` */` |
|         2 | 2710 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2711 | `{` |
|         - | 2712 | `	ph7_hashmap *pMap;` |
|         - | 2713 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2714 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2715 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2716 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2717 | `		return PH7_OK;` |
|         - | 2718 | `	}` |
|         - | 2719 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2720 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2721 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2722 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2723 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2724 | `		if( nArg > 1 ){` |
|         - | 2725 | `			/* Extract comparison flags */` |
|       ! 0 | 2726 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2727 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2728 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2729 | `			}` |
|       ! 0 | 2730 | `		}` |
|         - | 2731 | `		/* Do the merge sort */` |
|         3 | 2732 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2733 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2734 | `		HashmapSortRehash(pMap);` |
|         1 | 2735 | `	}` |
|         - | 2736 | `	/* All done,return TRUE */` |
|         3 | 2737 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2738 | `	return PH7_OK;` |
|         2 | 2739 | `}` |
|         - | 2740 | `/*` |
|         - | 2741 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2742 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2743 | ` * Parameters` |
|         - | 2744 | ` *  $array` |
|         - | 2745 | ` *   The input array.` |
|         - | 2746 | ` * $cmp_function` |
|         - | 2747 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2748 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2749 | ` *  to, or greater than the second.` |
|         - | 2750 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2751 | ` * Return` |
|         - | 2752 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2753 | ` */` |
|        18 | 2754 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2755 | `{` |
|         - | 2756 | `	ph7_hashmap *pMap;` |
|         - | 2757 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 2758 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2759 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2760 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2761 | `		return PH7_OK;` |
|         - | 2762 | `	}` |
|         - | 2763 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 2764 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 2765 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 2766 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2767 | `		ph7_value *pCallback = 0;` |
|         - | 2768 | `		ProcNodeCmp xCmp;` |
|        21 | 2769 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        21 | 2770 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2771 | `			/* Point to the desired callback */` |
|        21 | 2772 | `			pCallback = apArg[1];` |
|        12 | 2773 | `		}else{` |
|         - | 2774 | `			/* Use the default comparison function */` |
|       ! 0 | 2775 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2776 | `		}` |
|         - | 2777 | `		/* Do the merge sort */` |
|        21 | 2778 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        21 | 2779 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2780 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        21 | 2781 | `		HashmapSortRehash(pMap);` |
|        21 | 2782 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2783 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2784 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2785 | `			return PH7_EXCEPTION;` |
|         - | 2786 | `		}` |
|         5 | 2787 | `	}` |
|         - | 2788 | `	/* All done,return TRUE */` |
|        12 | 2789 | `	ph7_result_bool(pCtx,1);` |
|        12 | 2790 | `	return PH7_OK;` |
|        12 | 2791 | `}` |
|         - | 2792 | `/*` |
|         - | 2793 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2794 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2795 | ` *  and maintain index association.` |
|         - | 2796 | ` * Parameters` |
|         - | 2797 | ` *  $array` |
|         - | 2798 | ` *   The input array.` |
|         - | 2799 | ` * $cmp_function` |
|         - | 2800 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2801 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2802 | ` *  to, or greater than the second.` |
|         - | 2803 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2804 | ` * Return` |
|         - | 2805 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2806 | ` */` |
|         2 | 2807 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2808 | `{` |
|         - | 2809 | `	ph7_hashmap *pMap;` |
|         - | 2810 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2811 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2812 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2813 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2814 | `		return PH7_OK;` |
|         - | 2815 | `	}` |
|         - | 2816 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2817 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2818 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2819 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2820 | `		ph7_value *pCallback = 0;` |
|         - | 2821 | `		ProcNodeCmp xCmp;` |
|         3 | 2822 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|         3 | 2823 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2824 | `			/* Point to the desired callback */` |
|         3 | 2825 | `			pCallback = apArg[1];` |
|         2 | 2826 | `		}else{` |
|         - | 2827 | `			/* Use the default comparison function */` |
|       ! 0 | 2828 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2829 | `		}` |
|         - | 2830 | `		/* Do the merge sort */` |
|         3 | 2831 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2832 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2833 | `		/* Fix the last link broken by the merge */` |
|         5 | 2834 | `		while(pMap->pLast->pPrev){` |
|         3 | 2835 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2836 | `		}` |
|         3 | 2837 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2838 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2839 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2840 | `			return PH7_EXCEPTION;` |
|         - | 2841 | `		}` |
|         1 | 2842 | `	}` |
|         - | 2843 | `	/* All done,return TRUE */` |
|         3 | 2844 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2845 | `	return PH7_OK;` |
|         2 | 2846 | `}` |
|         - | 2847 | `/*` |
|         - | 2848 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2849 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2850 | ` *  function and maintain index association.` |
|         - | 2851 | ` * Parameters` |
|         - | 2852 | ` *  $array` |
|         - | 2853 | ` *   The input array.` |
|         - | 2854 | ` * $cmp_function` |
|         - | 2855 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2856 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2857 | ` *  to, or greater than the second.` |
|         - | 2858 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2859 | ` * Return` |
|         - | 2860 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2861 | ` */` |
|         2 | 2862 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2863 | `{` |
|         - | 2864 | `	ph7_hashmap *pMap;` |
|         - | 2865 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2866 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2867 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2868 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2869 | `		return PH7_OK;` |
|         - | 2870 | `	}` |
|         - | 2871 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2872 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2873 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2874 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2875 | `		ph7_value *pCallback = 0;` |
|         - | 2876 | `		ProcNodeCmp xCmp;` |
|         3 | 2877 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2878 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2879 | `			/* Point to the desired callback */` |
|         3 | 2880 | `			pCallback = apArg[1];` |
|         2 | 2881 | `		}else{` |
|         - | 2882 | `			/* Use the default comparison function */` |
|       ! 0 | 2883 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2884 | `		}` |
|         - | 2885 | `		/* Do the merge sort */` |
|         3 | 2886 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2887 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2888 | `		/* Fix the last link broken by the merge */` |
|         3 | 2889 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2890 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2891 | `		}` |
|         3 | 2892 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2893 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2894 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2895 | `			return PH7_EXCEPTION;` |
|         - | 2896 | `		}` |
|         1 | 2897 | `	}` |
|         - | 2898 | `	/* All done,return TRUE */` |
|         3 | 2899 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2900 | `	return PH7_OK;` |
|         2 | 2901 | `}` |
|         - | 2902 | `/*` |
|         - | 2903 | ` * bool shuffle(array &$array)` |
|         - | 2904 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2905 | ` * Parameters` |
|         - | 2906 | ` *  $array` |
|         - | 2907 | ` *   The input array.` |
|         - | 2908 | ` * Return` |
|         - | 2909 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2910 | ` *` |
|         - | 2911 | ` */` |
|         2 | 2912 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2913 | `{` |
|         - | 2914 | `	ph7_hashmap *pMap;` |
|         - | 2915 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2916 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2917 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2918 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2919 | `		return PH7_OK;` |
|         - | 2920 | `	}` |
|         - | 2921 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2922 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2923 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2924 | `	if( pMap->nEntry > 1 ){` |
|         - | 2925 | `		/* Do the merge sort */` |
|         3 | 2926 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2927 | `		/* Fix the last link broken by the merge */` |
|         9 | 2928 | `		while(pMap->pLast->pPrev){` |
|         7 | 2929 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2930 | `		}` |
|         1 | 2931 | `	}` |
|         - | 2932 | `	/* All done,return TRUE */` |
|         3 | 2933 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2934 | `	return PH7_OK;` |
|         2 | 2935 | `}` |
|         - | 2936 | `/*` |
|         - | 2937 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2938 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2939 | ` * Parameters` |
|         - | 2940 | ` *  $var` |
|         - | 2941 | ` *   The array or the object.` |
|         - | 2942 | ` * $mode` |
|         - | 2943 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2944 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2945 | ` *  all the elements of a multidimensional array.` |
|         - | 2946 | ` * Return` |
|         - | 2947 | ` *  Returns the number of elements in the array.` |
|         - | 2948 | ` */` |
|      1252 | 2949 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2950 | `{` |
|      1257 | 2951 | `	int bRecursive = FALSE;` |
|      1257 | 2952 | `	int bCycleDetected = FALSE;` |
|         - | 2953 | `	sxi64 iCount;` |
|      1257 | 2954 | `	if( nArg < 1 ){` |
|         3 | 2955 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2956 | `			"ArgumentCountError",` |
|         - | 2957 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2958 | `			);` |
|         - | 2959 | `	}` |
|      1255 | 2960 | `	if( nArg > 2 ){` |
|         4 | 2961 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2962 | `			"ArgumentCountError",` |
|         - | 2963 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2964 | `			nArg` |
|         - | 2965 | `			);` |
|         - | 2966 | `	}` |
|         - | 2967 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2968 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2969 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1253 | 2970 | `	if( nArg > 1 ){` |
|        44 | 2971 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 2972 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        10 | 2973 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2974 | `				"ValueError",` |
|         - | 2975 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2976 | `				);` |
|         - | 2977 | `		}` |
|        34 | 2978 | `		bRecursive = iMode == 1;` |
|        16 | 2979 | `	}` |
|      1245 | 2980 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2981 | `		/* Countable object: dispatch to ->count() */` |
|        37 | 2982 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        26 | 2983 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        26 | 2984 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        26 | 2985 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        23 | 2986 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 2987 | `					"count",sizeof("count")-1);` |
|        23 | 2988 | `				if( pMeth ){` |
|         - | 2989 | `					ph7_value sResult;` |
|        23 | 2990 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        23 | 2991 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        23 | 2992 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        23 | 2993 | `					PH7_MemObjRelease(&sResult);` |
|        23 | 2994 | `					return PH7_OK;` |
|         - | 2995 | `				}` |
|       ! 0 | 2996 | `			}` |
|         1 | 2997 | `		}` |
|        22 | 2998 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2999 | `			"TypeError",` |
|         - | 3000 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3001 | `			ph7_type_name(apArg[0])` |
|         - | 3002 | `			);` |
|         - | 3003 | `	}` |
|         - | 3004 | `	/* Count */` |
|      1213 | 3005 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1213 | 3006 | `	if( bCycleDetected ){` |
|         3 | 3007 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3008 | `	}` |
|      1213 | 3009 | `	ph7_result_int64(pCtx,iCount);` |
|      1213 | 3010 | `	return PH7_OK;` |
|       631 | 3011 | `}` |
|         - | 3012 | `/*` |
|         - | 3013 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3014 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3015 | ` * Parameters` |
|         - | 3016 | ` * $key` |
|         - | 3017 | ` *   Value to check.` |
|         - | 3018 | ` * $search` |
|         - | 3019 | ` *  An array with keys to check.` |
|         - | 3020 | ` * Return` |
|         - | 3021 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3022 | ` */` |
|        86 | 3023 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3024 | `{` |
|         - | 3025 | `	sxi32 rc;` |
|        91 | 3026 | `	if( nArg != 2 ){` |
|         - | 3027 | `		/* PHP requires exactly two arguments */` |
|        12 | 3028 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3029 | `			"ArgumentCountError",` |
|         - | 3030 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         3 | 3031 | `			nArg` |
|         - | 3032 | `			);` |
|         - | 3033 | `	}` |
|         - | 3034 | `	/* Make sure we are dealing with a valid hashmap */` |
|        84 | 3035 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3036 | `		/* Type mismatch -> TypeError */` |
|         8 | 3037 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3038 | `			"TypeError",` |
|         - | 3039 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3040 | `			ph7_type_name(apArg[1])` |
|         - | 3041 | `			);` |
|         - | 3042 | `	}` |
|         - | 3043 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        80 | 3044 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         3 | 3045 | `		ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3046 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3047 | `			"use an empty string instead"` |
|         - | 3048 | `			);` |
|        79 | 3049 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3050 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3051 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3052 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3053 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3054 | `				,rVal` |
|         - | 3055 | `				);` |
|         1 | 3056 | `		}` |
|         1 | 3057 | `	}` |
|         - | 3058 | `	/* Perform the lookup */` |
|        80 | 3059 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3060 | `	/* lookup result */` |
|        80 | 3061 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        80 | 3062 | `	return PH7_OK;` |
|        48 | 3063 | `}` |
|         - | 3064 | `/*` |
|         - | 3065 | ` * value array_pop(array $array)` |
|         - | 3066 | ` *   POP the last inserted element from the array.` |
|         - | 3067 | ` * Parameter` |
|         - | 3068 | ` *  The array to get the value from.` |
|         - | 3069 | ` * Return` |
|         - | 3070 | ` *  Poped value or NULL on failure.` |
|         - | 3071 | ` */` |
|        18 | 3072 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3073 | `{` |
|         - | 3074 | `	ph7_hashmap *pMap;` |
|         - | 3075 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        23 | 3076 | `	if( nArg != 1 ){` |
|         8 | 3077 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3078 | `			"ArgumentCountError",` |
|         - | 3079 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         2 | 3080 | `			nArg` |
|         - | 3081 | `			);` |
|         - | 3082 | `	}` |
|         - | 3083 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3084 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        18 | 3085 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3086 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3087 | `			"Error",` |
|         - | 3088 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3089 | `			);` |
|         - | 3090 | `	}` |
|         - | 3091 | `	/* Make sure we are dealing with a valid hashmap */` |
|        12 | 3092 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3093 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3094 | `			"TypeError",` |
|         - | 3095 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3096 | `			ph7_type_name(apArg[0])` |
|         - | 3097 | `			);` |
|         - | 3098 | `	}` |
|         9 | 3099 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         9 | 3100 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         9 | 3101 | `	if( pMap->nEntry < 1 ){` |
|         - | 3102 | `		/* Nothing to pop,return NULL */` |
|         3 | 3103 | `		ph7_result_null(pCtx);` |
|         2 | 3104 | `	}else{` |
|         7 | 3105 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3106 | `		ph7_value *pObj;` |
|         7 | 3107 | `		pObj = HashmapExtractNodeValue(pLast);` |
|         7 | 3108 | `		if( pObj ){` |
|         - | 3109 | `			/* Node value */` |
|         7 | 3110 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3111 | `			/* Unlink the node */` |
|         7 | 3112 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|         4 | 3113 | `		}else{` |
|       ! 0 | 3114 | `			ph7_result_null(pCtx);` |
|         - | 3115 | `		}` |
|         - | 3116 | `		/* Reset the cursor */` |
|         7 | 3117 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3118 | `	}` |
|         9 | 3119 | `	return PH7_OK;` |
|        14 | 3120 | `}` |
|         - | 3121 | `/*` |
|         - | 3122 | ` * int array_push($array,$var,...)` |
|         - | 3123 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3124 | ` * Parameters` |
|         - | 3125 | ` *  array` |
|         - | 3126 | ` *    The input array.` |
|         - | 3127 | ` *  var` |
|         - | 3128 | ` *   On or more value to push.` |
|         - | 3129 | ` * Return` |
|         - | 3130 | ` *  New array count (including old items).` |
|         - | 3131 | ` */` |
|        24 | 3132 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3133 | `{` |
|         - | 3134 | `	ph7_hashmap *pMap;` |
|         - | 3135 | `	sxi32 rc;` |
|         - | 3136 | `	int i;` |
|        29 | 3137 | `	if( nArg < 1 ){` |
|         4 | 3138 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3139 | `			"ArgumentCountError",` |
|         - | 3140 | `			"array_push() expects at least 1 argument, %d given",` |
|         1 | 3141 | `			nArg` |
|         - | 3142 | `			);` |
|         - | 3143 | `	}` |
|         - | 3144 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3145 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3146 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3147 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3148 | `			"Error",` |
|         - | 3149 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3150 | `			);` |
|         - | 3151 | `	}` |
|         - | 3152 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3153 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3154 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3155 | `			"TypeError",` |
|         - | 3156 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3157 | `			ph7_type_name(apArg[0])` |
|         - | 3158 | `			);` |
|         - | 3159 | `	}` |
|         - | 3160 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3161 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3162 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3163 | `	/* Start pushing given values */` |
|        34 | 3164 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3165 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3166 | `		if( rc != SXRET_OK ){` |
|         3 | 3167 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3168 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3169 | `				return rc;` |
|         - | 3170 | `			}` |
|       ! 0 | 3171 | `			break;` |
|         - | 3172 | `		}` |
|         9 | 3173 | `	}` |
|         - | 3174 | `	/* Return the new count */` |
|        15 | 3175 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3176 | `	return PH7_OK;` |
|        17 | 3177 | `}` |
|         - | 3178 | `/*` |
|         - | 3179 | ` * value array_shift(array $array)` |
|         - | 3180 | ` *   Shift an element off the beginning of array.` |
|         - | 3181 | ` * Parameter` |
|         - | 3182 | ` *  The array to get the value from.` |
|         - | 3183 | ` * Return` |
|         - | 3184 | ` *  Shifted value or NULL on failure.` |
|         - | 3185 | ` */` |
|        38 | 3186 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3187 | `{` |
|         - | 3188 | `	ph7_hashmap *pMap;` |
|         - | 3189 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        43 | 3190 | `	if( nArg != 1 ){` |
|         8 | 3191 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3192 | `			"ArgumentCountError",` |
|         - | 3193 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         2 | 3194 | `			nArg` |
|         - | 3195 | `			);` |
|         - | 3196 | `	}` |
|         - | 3197 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        39 | 3198 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3199 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3200 | `			"Error",` |
|         - | 3201 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3202 | `			);` |
|         - | 3203 | `	}` |
|         - | 3204 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 3205 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3206 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3207 | `			"TypeError",` |
|         - | 3208 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3209 | `			ph7_type_name(apArg[0])` |
|         - | 3210 | `			);` |
|         - | 3211 | `	}` |
|         - | 3212 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 3213 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        33 | 3214 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        33 | 3215 | `	if( pMap->nEntry < 1 ){` |
|         - | 3216 | `		/* Empty hashmap,return NULL */` |
|         3 | 3217 | `		ph7_result_null(pCtx);` |
|         2 | 3218 | `	}else{` |
|        31 | 3219 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3220 | `		ph7_value *pObj;` |
|         - | 3221 | `		sxu32 n;` |
|        31 | 3222 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        31 | 3223 | `		if( pObj ){` |
|         - | 3224 | `			/* Node value */` |
|        31 | 3225 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3226 | `			/* Unlink the first node */` |
|        31 | 3227 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        18 | 3228 | `		}else{` |
|       ! 0 | 3229 | `			ph7_result_null(pCtx);` |
|         - | 3230 | `		}` |
|         - | 3231 | `		/* Rehash all int keys */` |
|        31 | 3232 | `		n = pMap->nEntry;` |
|        31 | 3233 | `		pEntry = pMap->pFirst;` |
|        31 | 3234 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|        40 | 3235 | `		for(;;){` |
|        85 | 3236 | `			if( n < 1 ){` |
|        31 | 3237 | `				break;` |
|         - | 3238 | `			}` |
|        59 | 3239 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        59 | 3240 | `				HashmapRehashIntNode(pEntry);` |
|        27 | 3241 | `			}` |
|         - | 3242 | `			/* Point to the next entry */` |
|        59 | 3243 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        59 | 3244 | `			n--;` |
|         5 | 3245 | `		}` |
|         - | 3246 | `		/* Reset the cursor */` |
|        31 | 3247 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3248 | `	}` |
|        33 | 3249 | `	return PH7_OK;` |
|        24 | 3250 | `}` |
|         - | 3251 | `/*` |
|         - | 3252 | ` * Extract the node cursor value.` |
|         - | 3253 | ` */` |
|        36 | 3254 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3255 | `{` |
|        37 | 3256 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3257 | `	ph7_value *pVal;` |
|        37 | 3258 | `	if( pCur == 0 ){` |
|         - | 3259 | `		/* Cursor does not point to anything,return FALSE */` |
|       ! 0 | 3260 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3261 | `		return PH7_OK;` |
|         - | 3262 | `	}` |
|        37 | 3263 | `	if( iDirection != 0 ){` |
|        15 | 3264 | `		if( iDirection > 0 ){` |
|         - | 3265 | `			/* Point to the next entry */` |
|        13 | 3266 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        13 | 3267 | `			pCur = pMap->pCur;` |
|         7 | 3268 | `		}else{` |
|         - | 3269 | `			/* Point to the previous entry */` |
|         3 | 3270 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3271 | `			pCur = pMap->pCur;` |
|         - | 3272 | `		}` |
|        15 | 3273 | `		if( pCur == 0 ){` |
|         - | 3274 | `			/* End of input reached,return FALSE */` |
|       ! 0 | 3275 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 3276 | `			return PH7_OK;` |
|         - | 3277 | `		}` |
|         7 | 3278 | `	}` |
|         - | 3279 | `	/* Point to the desired element */` |
|        37 | 3280 | `	pVal = HashmapExtractNodeValue(pCur);` |
|        37 | 3281 | `	if( pVal ){` |
|        37 | 3282 | `		ph7_result_value(pCtx,pVal);` |
|        19 | 3283 | `	}else{` |
|       ! 0 | 3284 | `		ph7_result_bool(pCtx,0);` |
|         - | 3285 | `	}` |
|        37 | 3286 | `	return PH7_OK;` |
|        19 | 3287 | `}` |
|         - | 3288 | `/*` |
|         - | 3289 | ` * value current(array $array)` |
|         - | 3290 | ` *  Return the current element in an array.` |
|         - | 3291 | ` * Parameter` |
|         - | 3292 | ` *  $input: The input array.` |
|         - | 3293 | ` * Return` |
|         - | 3294 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3295 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3296 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3297 | ` *  is empty, current() returns FALSE.` |
|         - | 3298 | ` */` |
|        16 | 3299 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3300 | `{` |
|        17 | 3301 | `	if( nArg < 1 ){` |
|         - | 3302 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3303 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3304 | `		return PH7_OK;` |
|         - | 3305 | `	}` |
|         - | 3306 | `	/* Make sure we are dealing with a valid hashmap */` |
|        17 | 3307 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3308 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3309 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3310 | `		return PH7_OK;` |
|         - | 3311 | `	}` |
|        17 | 3312 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|        17 | 3313 | `	return PH7_OK;` |
|         9 | 3314 | `}` |
|         - | 3315 | `/*` |
|         - | 3316 | ` * value next(array $input)` |
|         - | 3317 | ` *  Advance the internal array pointer of an array.` |
|         - | 3318 | ` * Parameter` |
|         - | 3319 | ` *  $input: The input array.` |
|         - | 3320 | ` * Return` |
|         - | 3321 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3322 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3323 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3324 | ` */` |
|        12 | 3325 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3326 | `{` |
|        13 | 3327 | `	if( nArg < 1 ){` |
|         - | 3328 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3329 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3330 | `		return PH7_OK;` |
|         - | 3331 | `	}` |
|         - | 3332 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 3333 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3334 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3335 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3336 | `		return PH7_OK;` |
|         - | 3337 | `	}` |
|        13 | 3338 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|        13 | 3339 | `	return PH7_OK;` |
|         7 | 3340 | `}` |
|         - | 3341 | `/*` |
|         - | 3342 | ` * value prev(array $input)` |
|         - | 3343 | ` *  Rewind the internal array pointer.` |
|         - | 3344 | ` * Parameter` |
|         - | 3345 | ` *  $input: The input array.` |
|         - | 3346 | ` * Return` |
|         - | 3347 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3348 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3349 | ` *  elements.` |
|         - | 3350 | ` */` |
|         2 | 3351 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3352 | `{` |
|         3 | 3353 | `	if( nArg < 1 ){` |
|         - | 3354 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3355 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3356 | `		return PH7_OK;` |
|         - | 3357 | `	}` |
|         - | 3358 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3359 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3360 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3361 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3362 | `		return PH7_OK;` |
|         - | 3363 | `	}` |
|         3 | 3364 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3365 | `	return PH7_OK;` |
|         2 | 3366 | `}` |
|         - | 3367 | `/*` |
|         - | 3368 | ` * value end(array $input)` |
|         - | 3369 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3370 | ` * Parameter` |
|         - | 3371 | ` *  $input: The input array.` |
|         - | 3372 | ` * Return` |
|         - | 3373 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3374 | ` */` |
|         2 | 3375 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3376 | `{` |
|         - | 3377 | `	ph7_hashmap *pMap;` |
|         3 | 3378 | `	if( nArg < 1 ){` |
|         - | 3379 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3380 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3381 | `		return PH7_OK;` |
|         - | 3382 | `	}` |
|         - | 3383 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3384 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3385 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3386 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3387 | `		return PH7_OK;` |
|         - | 3388 | `	}` |
|         - | 3389 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3390 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3391 | `	/* Point to the last node */` |
|         3 | 3392 | `	pMap->pCur = pMap->pLast;` |
|         - | 3393 | `	/* Return the last node value */` |
|         3 | 3394 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         3 | 3395 | `	return PH7_OK;` |
|         2 | 3396 | `}` |
|         - | 3397 | `/*` |
|         - | 3398 | ` * value reset(array $array )` |
|         - | 3399 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3400 | ` * Parameter` |
|         - | 3401 | ` *  $input: The input array.` |
|         - | 3402 | ` * Return` |
|         - | 3403 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3404 | ` */` |
|         4 | 3405 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3406 | `{` |
|         - | 3407 | `	ph7_hashmap *pMap;` |
|         5 | 3408 | `	if( nArg < 1 ){` |
|         - | 3409 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3410 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3411 | `		return PH7_OK;` |
|         - | 3412 | `	}` |
|         - | 3413 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3414 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3415 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3416 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3417 | `		return PH7_OK;` |
|         - | 3418 | `	}` |
|         - | 3419 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 3420 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3421 | `	/* Point to the first node */` |
|         5 | 3422 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3423 | `	/* Return the last node value if available */` |
|         5 | 3424 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         5 | 3425 | `	return PH7_OK;` |
|         3 | 3426 | `}` |
|         - | 3427 | `/*` |
|         - | 3428 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3429 | ` * array_key_first() and array_key_last().` |
|         - | 3430 | ` */` |
|        20 | 3431 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3432 | `{` |
|        21 | 3433 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3434 | `		/* Key is integer */` |
|        15 | 3435 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         8 | 3436 | `	}else{` |
|         - | 3437 | `		/* Key is blob */` |
|        10 | 3438 | `		ph7_result_string(pCtx,` |
|         6 | 3439 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3440 | `	}` |
|        21 | 3441 | `}` |
|         - | 3442 | `/*` |
|         - | 3443 | ` * value key(array $array)` |
|         - | 3444 | ` *   Fetch a key from an array` |
|         - | 3445 | ` * Parameter` |
|         - | 3446 | ` *  $input` |
|         - | 3447 | ` *   The input array.` |
|         - | 3448 | ` * Return` |
|         - | 3449 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3450 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3451 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3452 | ` *  is empty, key() returns NULL.` |
|         - | 3453 | ` */` |
|         4 | 3454 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3455 | `{` |
|         - | 3456 | `	ph7_hashmap_node *pCur;` |
|         - | 3457 | `	ph7_hashmap *pMap;` |
|         5 | 3458 | `	if( nArg < 1 ){` |
|         - | 3459 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3460 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3461 | `		return PH7_OK;` |
|         - | 3462 | `	}` |
|         - | 3463 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3464 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3465 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3466 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3467 | `		return PH7_OK;` |
|         - | 3468 | `	}` |
|         5 | 3469 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 3470 | `	pCur = pMap->pCur;` |
|         5 | 3471 | `	if( pCur == 0 ){` |
|         - | 3472 | `		/* Cursor does not point to anything,return NULL */` |
|       ! 0 | 3473 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3474 | `		return PH7_OK;` |
|         - | 3475 | `	}` |
|         5 | 3476 | `	HashmapResultNodeKey(pCtx,pCur);` |
|         5 | 3477 | `	return PH7_OK;` |
|         3 | 3478 | `}` |
|         - | 3479 | `/*` |
|         - | 3480 | ` * array each(array $input)` |
|         - | 3481 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3482 | ` * Parameter` |
|         - | 3483 | ` *  $input` |
|         - | 3484 | ` *    The input array.` |
|         - | 3485 | ` * Return` |
|         - | 3486 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3487 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3488 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3489 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3490 | ` *  each() returns FALSE.` |
|         - | 3491 | ` */` |
|        22 | 3492 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3493 | `{` |
|         - | 3494 | `	ph7_hashmap_node *pCur;` |
|         - | 3495 | `	ph7_hashmap *pMap;` |
|         - | 3496 | `	ph7_value *pArray;` |
|         - | 3497 | `	ph7_value *pVal;` |
|         - | 3498 | `	ph7_value sKey;` |
|        23 | 3499 | `	if( nArg < 1 ){` |
|         - | 3500 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3501 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3502 | `		return PH7_OK;` |
|         - | 3503 | `	}` |
|         - | 3504 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3505 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3506 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3507 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3508 | `		return PH7_OK;` |
|         - | 3509 | `	}` |
|         - | 3510 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3511 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3512 | `	if( pMap->pCur == 0 ){` |
|         - | 3513 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3514 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3515 | `		return PH7_OK;` |
|         - | 3516 | `	}` |
|        15 | 3517 | `	pCur = pMap->pCur;` |
|         - | 3518 | `	/* Create a new array */` |
|        15 | 3519 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3520 | `	if( pArray == 0 ){` |
|       ! 0 | 3521 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3522 | `		return PH7_OK;` |
|         - | 3523 | `	}` |
|        15 | 3524 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3525 | `	/* Insert the current value */` |
|        15 | 3526 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3527 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3528 | `	/* Make the key */` |
|        15 | 3529 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3530 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3531 | `	}else{` |
|         9 | 3532 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3533 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3534 | `	}` |
|         - | 3535 | `	/* Insert the current key */` |
|        15 | 3536 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3537 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3538 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3539 | `	/* Advance the cursor */` |
|        15 | 3540 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3541 | `	/* Return the current entry */` |
|        15 | 3542 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3543 | `	return PH7_OK;` |
|        12 | 3544 | `}` |
|         - | 3545 | `/*` |
|         - | 3546 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3547 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3548 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3549 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3550 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3551 | ` */` |
|         - | 3552 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3553 | `/*` |
|         - | 3554 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3555 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3556 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3557 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3558 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3559 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3560 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3561 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3562 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3563 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3564 | ` */` |
|         - | 3565 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3566 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3567 | `/*` |
|         - | 3568 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3569 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3570 | ` */` |
|         8 | 3571 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|         1 | 3572 | `{` |
|         9 | 3573 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|         3 | 3574 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|         3 | 3575 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|         3 | 3576 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|         3 | 3577 | `		zBuf[n] = 0;` |
|         3 | 3578 | `		return zBuf;` |
|         - | 3579 | `	}` |
|         7 | 3580 | `	return ph7_type_name(pVal);` |
|         5 | 3581 | `}` |
|         - | 3582 | `/*` |
|         - | 3583 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3584 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3585 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3586 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3587 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3588 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3589 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3590 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3591 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3592 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3593 | ` */` |
|       156 | 3594 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3595 | `{` |
|       157 | 3596 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3597 | `	sxu64 uVal = 0;` |
|       157 | 3598 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3599 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3600 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3601 | `		bNeg = (z[0] == '-');` |
|         3 | 3602 | `		z++;` |
|         1 | 3603 | `	}` |
|       237 | 3604 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3605 | `		int d = z[0] - '0';` |
|         - | 3606 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3607 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3608 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3609 | `			bOverflow = 1;` |
|       ! 0 | 3610 | `		}else{` |
|        81 | 3611 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3612 | `		}` |
|        81 | 3613 | `		bDigit = 1;` |
|        81 | 3614 | `		z++;` |
|         1 | 3615 | `	}` |
|       157 | 3616 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3617 | `		bReal = 1;` |
|         3 | 3618 | `		z++;` |
|         5 | 3619 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3620 | `			bDigit = 1;` |
|         3 | 3621 | `			z++;` |
|         1 | 3622 | `		}` |
|         1 | 3623 | `	}` |
|         - | 3624 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3625 | `	if( !bDigit ){` |
|        61 | 3626 | `		return RANGE_IN_ERROR;` |
|         - | 3627 | `	}` |
|         - | 3628 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3629 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3630 | `		z++;` |
|         9 | 3631 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3632 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3633 | `			return RANGE_IN_ERROR;` |
|         - | 3634 | `		}` |
|         9 | 3635 | `		bReal = 1;` |
|        17 | 3636 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3637 | `	}` |
|         - | 3638 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3639 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3640 | `	if( z != zEnd ){` |
|        13 | 3641 | `		return RANGE_IN_ERROR;` |
|         - | 3642 | `	}` |
|        84 | 3643 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3644 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3645 | `		bReal = 1;` |
|        84 | 3646 | `	}` |
|        43 | 3647 | `	if( bReal ){` |
|        11 | 3648 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3649 | `		return RANGE_IN_DOUBLE;` |
|         - | 3650 | `	}` |
|         - | 3651 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3652 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3653 | `	return RANGE_IN_LONG;` |
|        58 | 3654 | `}` |
|         - | 3655 | `/*` |
|         - | 3656 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3657 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3658 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3659 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3660 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3661 | ` */` |
|       338 | 3662 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3663 | `{` |
|         - | 3664 | `	char zMsg[160];` |
|       339 | 3665 | `	*pRc = PH7_OK;` |
|       339 | 3666 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3667 | `		char zType[80];` |
|        10 | 3668 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3669 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|         3 | 3670 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         7 | 3671 | `		return FALSE;` |
|         - | 3672 | `	}` |
|       333 | 3673 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3674 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3675 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3676 | `			iArg,zName);` |
|         5 | 3677 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3678 | `		*pbNullCoerced = TRUE;` |
|         2 | 3679 | `	}` |
|       333 | 3680 | `	return TRUE;` |
|       170 | 3681 | `}` |
|         - | 3682 | `/*` |
|         - | 3683 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3684 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3685 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3686 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3687 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3688 | ` */` |
|        62 | 3689 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3690 | `{` |
|        63 | 3691 | `	*pRc = PH7_OK;` |
|        63 | 3692 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3693 | `		char zType[80];` |
|         4 | 3694 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3695 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|         1 | 3696 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         3 | 3697 | `		return RANGE_IN_ERROR;` |
|         - | 3698 | `	}` |
|        61 | 3699 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3700 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3701 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3702 | `		*pLong = 0;` |
|         3 | 3703 | `		return RANGE_IN_LONG;` |
|         - | 3704 | `	}` |
|        59 | 3705 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3706 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3707 | `		return RANGE_IN_DOUBLE;` |
|         - | 3708 | `	}` |
|        35 | 3709 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3710 | `		const char *zStr;` |
|         - | 3711 | `		int nLen;` |
|         - | 3712 | `		sxu8 iKind;` |
|         3 | 3713 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3714 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3715 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3716 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3717 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3718 | `		}` |
|         3 | 3719 | `		return iKind;` |
|         - | 3720 | `	}` |
|         - | 3721 | `	/* int / bool */` |
|        33 | 3722 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3723 | `	return RANGE_IN_LONG;` |
|        32 | 3724 | `}` |
|         - | 3725 | `/*` |
|         - | 3726 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3727 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3728 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3729 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3730 | ` */` |
|       296 | 3731 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3732 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3733 | `{` |
|         - | 3734 | `	char zMsg[160];` |
|         - | 3735 | `	double r;` |
|       297 | 3736 | `	*pRc = PH7_OK;` |
|       297 | 3737 | `	if( bNullCoerced ){` |
|         - | 3738 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3739 | `		*pLong = 0;` |
|         5 | 3740 | `		*pDouble = 0.0;` |
|         5 | 3741 | `		return RANGE_IN_LONG;` |
|         - | 3742 | `	}` |
|       293 | 3743 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3744 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3745 | `check_dval:` |
|        25 | 3746 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3747 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3748 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3749 | `			return RANGE_IN_ERROR;` |
|         - | 3750 | `		}` |
|        21 | 3751 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3752 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3753 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3754 | `			return RANGE_IN_ERROR;` |
|         - | 3755 | `		}` |
|        17 | 3756 | `		*pDouble = r;` |
|        17 | 3757 | `		return RANGE_IN_DOUBLE;` |
|         - | 3758 | `	}` |
|       273 | 3759 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3760 | `		const char *zStr;` |
|         - | 3761 | `		int nLen;` |
|         - | 3762 | `		sxu8 iKind;` |
|        81 | 3763 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3764 | `		if( nLen == 0 ){` |
|         7 | 3765 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3766 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3767 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3768 | `			*pLong = 0;` |
|         5 | 3769 | `			*pDouble = 0.0;` |
|        41 | 3770 | `			return RANGE_IN_LONG;` |
|         - | 3771 | `		}` |
|        77 | 3772 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3773 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3774 | `			r = *pDouble;` |
|         5 | 3775 | `			goto check_dval;` |
|         - | 3776 | `		}` |
|        73 | 3777 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3778 | `			*pDouble = (double)*pLong;` |
|        23 | 3779 | `			if( nLen == 1 ){` |
|         - | 3780 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3781 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3782 | `				return RANGE_IN_DIGIT;` |
|         - | 3783 | `			}` |
|        15 | 3784 | `			return RANGE_IN_LONG;` |
|         - | 3785 | `		}` |
|        51 | 3786 | `		if( nLen != 1 ){` |
|        10 | 3787 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3788 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3789 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3790 | `		}` |
|        51 | 3791 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3792 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3793 | `		*pLong = 0;` |
|        51 | 3794 | `		*pDouble = 0.0;` |
|        51 | 3795 | `		return RANGE_IN_STRING;` |
|         - | 3796 | `	}` |
|         - | 3797 | `	/* int / bool */` |
|       193 | 3798 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3799 | `	*pDouble = (double)*pLong;` |
|       193 | 3800 | `	return RANGE_IN_LONG;` |
|       149 | 3801 | `}` |
|         - | 3802 | `/*` |
|         - | 3803 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3804 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3805 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3806 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3807 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3808 | ` * exactly like php's two macros.` |
|         - | 3809 | ` */` |
|         6 | 3810 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3811 | `{` |
|        10 | 3812 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3813 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3814 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3815 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3816 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3817 | `}` |
|         6 | 3818 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3819 | `{` |
|         - | 3820 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3821 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3822 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3823 | `	const unsigned int nBuf = 1500;` |
|         7 | 3824 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3825 | `	if( zMsg == 0 ){` |
|       ! 0 | 3826 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3827 | `	}` |
|         7 | 3828 | `	snprintf(zMsg,nBuf,` |
|         - | 3829 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3830 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3831 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3832 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3833 | `}` |
|         - | 3834 | `/*` |
|         - | 3835 | ` * Set the element container to the next range element and append it to the` |
|         - | 3836 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3837 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3838 | ` * below stay one line per iteration.` |
|         - | 3839 | ` */` |
|      1680 | 3840 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3841 | `{` |
|      1681 | 3842 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3843 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3844 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3845 | `	}` |
|      1681 | 3846 | `	return PH7_OK;` |
|       841 | 3847 | `}` |
|        70 | 3848 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3849 | `{` |
|        71 | 3850 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3851 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3852 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3853 | `	}` |
|        71 | 3854 | `	return PH7_OK;` |
|        36 | 3855 | `}` |
|       168 | 3856 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3857 | `{` |
|       169 | 3858 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3859 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3860 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3861 | `	}` |
|       169 | 3862 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3863 | `	return PH7_OK;` |
|        85 | 3864 | `}` |
|         - | 3865 | `/*` |
|         - | 3866 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3867 | ` *  Create an array containing a range of elements.` |
|         - | 3868 | ` * Return` |
|         - | 3869 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3870 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3871 | ` */` |
|       174 | 3872 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3873 | `{` |
|         - | 3874 | `	ph7_value *pValue,*pArray;` |
|       175 | 3875 | `	sxi32 rc = PH7_OK;` |
|       175 | 3876 | `	int is_step_double = 0,is_step_negative = 0;` |
|       175 | 3877 | `	double step_double = 1.0;` |
|       175 | 3878 | `	sxi64 step = 1;` |
|         - | 3879 | `	sxu8 start_type,end_type;` |
|       175 | 3880 | `	sxi64 start_long = 0,end_long = 0;` |
|       175 | 3881 | `	double start_double = 0.0,end_double = 0.0;` |
|       175 | 3882 | `	unsigned char cStart = 0,cEnd = 0;` |
|       175 | 3883 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3884 | `	sxu32 i,size;` |
|         - | 3885 |  |
|         - | 3886 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       175 | 3887 | `	if( nArg > 3 ){` |
|         4 | 3888 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3889 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3890 | `	}` |
|       173 | 3891 | `	if( nArg < 2 ){` |
|         - | 3892 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3893 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3894 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3895 | `	}` |
|         - | 3896 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3897 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       173 | 3898 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|         7 | 3899 | `		return rc;` |
|         - | 3900 | `	}` |
|       167 | 3901 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3902 | `		return rc;` |
|         - | 3903 | `	}` |
|       167 | 3904 | `	if( nArg > 2 ){` |
|        63 | 3905 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        63 | 3906 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         5 | 3907 | `			return rc;` |
|         - | 3908 | `		}` |
|        59 | 3909 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3910 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3911 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3912 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3913 | `			}` |
|        23 | 3914 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3915 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3916 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3917 | `			}` |
|         - | 3918 | `			/* We only want positive step values. */` |
|        21 | 3919 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3920 | `				is_step_negative = 1;` |
|       ! 0 | 3921 | `				step_double *= -1;` |
|       ! 0 | 3922 | `			}` |
|         - | 3923 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3924 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3925 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3926 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3927 | `				step = (sxi64)step_double;` |
|        19 | 3928 | `				if( (double)step != step_double ){` |
|        17 | 3929 | `					is_step_double = 1;` |
|         8 | 3930 | `				}` |
|        10 | 3931 | `			}else{` |
|         - | 3932 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3933 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3934 | `				is_step_double = 1;` |
|         - | 3935 | `			}` |
|        11 | 3936 | `		}else{` |
|         - | 3937 | `			/* We only want positive step values. */` |
|        35 | 3938 | `			if( step < 0 ){` |
|        11 | 3939 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3940 | `					/* -step would overflow */` |
|         4 | 3941 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3942 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3943 | `				}` |
|         9 | 3944 | `				is_step_negative = 1;` |
|         9 | 3945 | `				step = -step;` |
|         4 | 3946 | `			}` |
|        33 | 3947 | `			step_double = (double)step;` |
|         - | 3948 | `		}` |
|        53 | 3949 | `		if( step_double == 0.0 ){` |
|         7 | 3950 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3951 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3952 | `		}` |
|        23 | 3953 | `	}` |
|       151 | 3954 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 3955 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3956 | `		return rc;` |
|         - | 3957 | `	}` |
|       147 | 3958 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 3959 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3960 | `		return rc;` |
|         - | 3961 | `	}` |
|         - | 3962 | `	/* Element container + result array */` |
|       143 | 3963 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 3964 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 3965 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3966 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3967 | `	}` |
|         - | 3968 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 3969 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3970 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3971 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3972 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3973 | `			 * and the range is numeric. */` |
|        15 | 3974 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3975 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3976 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3977 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3978 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3979 | `				}` |
|         7 | 3980 | `				end_type = RANGE_IN_LONG;` |
|         4 | 3981 | `			}else{` |
|         9 | 3982 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 3983 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3984 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 3985 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 3986 | `				}` |
|         9 | 3987 | `				start_type = RANGE_IN_LONG;` |
|         - | 3988 | `			}` |
|        15 | 3989 | `			goto handle_numeric_inputs;` |
|         - | 3990 | `		}` |
|        23 | 3991 | `		if( is_step_double ){` |
|         - | 3992 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 3993 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 3994 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3995 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 3996 | `					" of characters, inputs converted to 0");` |
|         1 | 3997 | `			}` |
|         5 | 3998 | `			start_type = RANGE_IN_LONG;` |
|         5 | 3999 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4000 | `			goto handle_numeric_inputs;` |
|         - | 4001 | `		}` |
|         - | 4002 | `		/* Generate an array of characters */` |
|        19 | 4003 | `		if( cStart > cEnd ){` |
|         - | 4004 | `			/* Decreasing char range */` |
|         - | 4005 | `			int iCur;` |
|         3 | 4006 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4007 | `				goto boundary_error;` |
|         - | 4008 | `			}` |
|        17 | 4009 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4010 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4011 | `					return rc;` |
|         - | 4012 | `				}` |
|         8 | 4013 | `			}` |
|        18 | 4014 | `		}else if( cEnd > cStart ){` |
|         - | 4015 | `			/* Increasing char range */` |
|         - | 4016 | `			int iCur;` |
|        15 | 4017 | `			if( is_step_negative ){` |
|         3 | 4018 | `				goto negative_step_error;` |
|         - | 4019 | `			}` |
|        13 | 4020 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4021 | `				goto boundary_error;` |
|         - | 4022 | `			}` |
|       163 | 4023 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4024 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4025 | `					return rc;` |
|         - | 4026 | `				}` |
|        77 | 4027 | `			}` |
|         6 | 4028 | `		}else{` |
|         3 | 4029 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4030 | `				return rc;` |
|         - | 4031 | `			}` |
|         - | 4032 | `		}` |
|        15 | 4033 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4034 | `		return PH7_OK;` |
|         - | 4035 | `	}` |
|        53 | 4036 | `handle_numeric_inputs:` |
|       133 | 4037 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4038 | `		/* Float range */` |
|         - | 4039 | `		double elem,calc;` |
|        25 | 4040 | `		if( start_double > end_double ){` |
|         - | 4041 | `			/* Decreasing float range */` |
|         7 | 4042 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4043 | `				goto boundary_error;` |
|         - | 4044 | `			}` |
|         7 | 4045 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4046 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4047 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4048 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4049 | `			}` |
|         5 | 4050 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4051 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4052 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4053 | `					return rc;` |
|         - | 4054 | `				}` |
|         8 | 4055 | `			}` |
|        21 | 4056 | `		}else if( end_double > start_double ){` |
|         - | 4057 | `			/* Increasing float range */` |
|        17 | 4058 | `			if( is_step_negative ){` |
|       ! 0 | 4059 | `				goto negative_step_error;` |
|         - | 4060 | `			}` |
|        17 | 4061 | `			if( end_double - start_double < step_double ){` |
|         3 | 4062 | `				goto boundary_error;` |
|         - | 4063 | `			}` |
|        15 | 4064 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4065 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4066 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4067 | `			}` |
|        11 | 4068 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4069 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4070 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4071 | `					return rc;` |
|         - | 4072 | `				}` |
|        28 | 4073 | `			}` |
|         6 | 4074 | `		}else{` |
|         3 | 4075 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4076 | `				return rc;` |
|         - | 4077 | `			}` |
|         - | 4078 | `		}` |
|         9 | 4079 | `	}else{` |
|         - | 4080 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4081 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4082 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4083 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4084 | `		sxu64 calc;` |
|       101 | 4085 | `		if( start_long > end_long ){` |
|         - | 4086 | `			/* Decreasing int range */` |
|        19 | 4087 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4088 | `				goto boundary_error;` |
|         - | 4089 | `			}` |
|        17 | 4090 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4091 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4092 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4093 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4094 | `			}` |
|        15 | 4095 | `			size = (sxu32)(calc + 1);` |
|       101 | 4096 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4097 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4098 | `					return rc;` |
|         - | 4099 | `				}` |
|        44 | 4100 | `			}` |
|        90 | 4101 | `		}else if( end_long > start_long ){` |
|         - | 4102 | `			/* Increasing int range */` |
|        77 | 4103 | `			if( is_step_negative ){` |
|         3 | 4104 | `				goto negative_step_error;` |
|         - | 4105 | `			}` |
|        75 | 4106 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4107 | `				goto boundary_error;` |
|         - | 4108 | `			}` |
|        73 | 4109 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4110 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4111 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4112 | `			}` |
|        69 | 4113 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4114 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4115 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4116 | `					return rc;` |
|         - | 4117 | `				}` |
|       795 | 4118 | `			}` |
|        35 | 4119 | `		}else{` |
|         7 | 4120 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4121 | `				return rc;` |
|         - | 4122 | `			}` |
|         - | 4123 | `		}` |
|         - | 4124 | `	}` |
|         - | 4125 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4126 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4127 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4128 | `	return PH7_OK;` |
|         2 | 4129 | `negative_step_error:` |
|         5 | 4130 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4131 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4132 | `boundary_error:` |
|         9 | 4133 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4134 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        88 | 4135 | `}` |
|         - | 4136 | `/*` |
|         - | 4137 | ` * array array_values(array $array)` |
|         - | 4138 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4139 | ` * Parameters` |
|         - | 4140 | ` *  $array` |
|         - | 4141 | ` *   The input array.` |
|         - | 4142 | ` * Return` |
|         - | 4143 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4144 | ` */` |
|        38 | 4145 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4146 | `{` |
|         - | 4147 | `	ph7_hashmap_node *pNode;` |
|         - | 4148 | `	ph7_hashmap *pMap;` |
|         - | 4149 | `	ph7_value *pArray;` |
|         - | 4150 | `	ph7_value *pObj;` |
|         - | 4151 | `	sxu32 n;` |
|        43 | 4152 | `	if( nArg != 1 ){` |
|         - | 4153 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4154 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4155 | `			"ArgumentCountError",` |
|         - | 4156 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4157 | `			nArg` |
|         - | 4158 | `			);` |
|         - | 4159 | `	}` |
|         - | 4160 | `	/* Make sure we are dealing with a valid hashmap */` |
|        37 | 4161 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4162 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4163 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4164 | `			"TypeError",` |
|         - | 4165 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4166 | `			ph7_type_name(apArg[0])` |
|         - | 4167 | `			);` |
|         - | 4168 | `	}` |
|         - | 4169 | `	/* Point to the internal representation that describe the input hashmap */` |
|        34 | 4170 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4171 | `	/* Create a new array */` |
|        34 | 4172 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 4173 | `	if( pArray == 0 ){` |
|       ! 0 | 4174 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4175 | `		return PH7_OK;` |
|         - | 4176 | `	}` |
|         - | 4177 | `	/* Perform the requested operation */` |
|        34 | 4178 | `	pNode = pMap->pFirst;` |
|       110 | 4179 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        78 | 4180 | `		pObj = HashmapExtractNodeValue(pNode);` |
|        78 | 4181 | `		if( pObj ){` |
|         - | 4182 | `			/* perform the insertion */` |
|        78 | 4183 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        38 | 4184 | `		}` |
|         - | 4185 | `		/* Point to the next entry */` |
|        78 | 4186 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        40 | 4187 | `	}` |
|         - | 4188 | `	/* return the new array */` |
|        34 | 4189 | `	ph7_result_value(pCtx,pArray);` |
|        34 | 4190 | `	return PH7_OK;` |
|        24 | 4191 | `}` |
|         - | 4192 | `/*` |
|         - | 4193 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4194 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4195 | ` * Parameters` |
|         - | 4196 | ` *  $input` |
|         - | 4197 | ` *   An array containing keys to return.` |
|         - | 4198 | ` * $search_value` |
|         - | 4199 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4200 | ` * $strict` |
|         - | 4201 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4202 | ` * Return` |
|         - | 4203 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4204 | ` */` |
|       156 | 4205 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4206 | `{` |
|         - | 4207 | `	ph7_hashmap_node *pNode;` |
|         - | 4208 | `	ph7_hashmap *pMap;` |
|         - | 4209 | `	ph7_value *pArray;` |
|         - | 4210 | `	ph7_value sObj;` |
|         - | 4211 | `	ph7_value sVal;` |
|         - | 4212 | `	SyString sKey;` |
|         - | 4213 | `	int bStrict;` |
|         - | 4214 | `	sxi32 rc;` |
|         - | 4215 | `	sxu32 n;` |
|       160 | 4216 | `	if( nArg < 1 ){` |
|         - | 4217 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4218 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4219 | `			"ArgumentCountError",` |
|         - | 4220 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4221 | `			);` |
|         - | 4222 | `	}` |
|         - | 4223 | `	/* Make sure we are dealing with a valid hashmap */` |
|       158 | 4224 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4225 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4226 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4227 | `			"TypeError",` |
|         - | 4228 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4229 | `			ph7_type_name(apArg[0])` |
|         - | 4230 | `			);` |
|         - | 4231 | `	}` |
|         - | 4232 | `	/* Point to the internal representation of the input hashmap */` |
|       155 | 4233 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4234 | `	/* Create a new array */` |
|       155 | 4235 | `	pArray = ph7_context_new_array(pCtx);` |
|       155 | 4236 | `	if( pArray == 0 ){` |
|       ! 0 | 4237 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4238 | `		return PH7_OK;` |
|         - | 4239 | `	}` |
|       155 | 4240 | `	bStrict = FALSE;` |
|       155 | 4241 | `	if( nArg > 2 ){` |
|         - | 4242 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        12 | 4243 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4244 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4245 | `				"TypeError",` |
|         - | 4246 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4247 | `				ph7_type_name(apArg[2])` |
|         - | 4248 | `				);` |
|         - | 4249 | `		}` |
|         9 | 4250 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4251 | `	}` |
|         - | 4252 | `	/* Perform the requested operation */` |
|       153 | 4253 | `	pNode = pMap->pFirst;` |
|       153 | 4254 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1421 | 4255 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1271 | 4256 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       167 | 4257 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        85 | 4258 | `		}else{` |
|      1106 | 4259 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1106 | 4260 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4261 | `		}` |
|      1271 | 4262 | `		rc = 0;` |
|      1271 | 4263 | `		if( nArg > 1 ){` |
|        65 | 4264 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4265 | `			if( pValue ){` |
|         - | 4266 | `				ph7_value sNeedle;` |
|        65 | 4267 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4268 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4269 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4270 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4271 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4272 | `				 * corrupt every later comparison. */` |
|        65 | 4273 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4274 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4275 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4276 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4277 | `			}` |
|        32 | 4278 | `		}` |
|      1271 | 4279 | `		if( rc == 0 ){` |
|         - | 4280 | `			/* Perform the insertion */` |
|      1239 | 4281 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       618 | 4282 | `		}` |
|      1271 | 4283 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4284 | `		/* Point to the next entry */` |
|      1271 | 4285 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       637 | 4286 | `	}` |
|         - | 4287 | `	/* return the new array */` |
|       153 | 4288 | `	ph7_result_value(pCtx,pArray);` |
|       153 | 4289 | `	return PH7_OK;` |
|        82 | 4290 | `}` |
|         - | 4291 | `/*` |
|         - | 4292 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4293 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4294 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4295 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4296 | ` * Parameters` |
|         - | 4297 | ` *  $arr1` |
|         - | 4298 | ` *   First array` |
|         - | 4299 | ` *  $arr2` |
|         - | 4300 | ` *   Second array` |
|         - | 4301 | ` * Return` |
|         - | 4302 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4303 | ` * Note` |
|         - | 4304 | ` *  This function is a symisc eXtension.` |
|         - | 4305 | ` */` |
|         4 | 4306 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4307 | `{` |
|         - | 4308 | `	ph7_hashmap *p1,*p2;` |
|         - | 4309 | `	int rc;` |
|         5 | 4310 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4311 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4312 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4313 | `		return PH7_OK;` |
|         - | 4314 | `	}` |
|         - | 4315 | `	/* Point to the hashmaps */` |
|         5 | 4316 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4317 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4318 | `	rc = (p1 == p2);` |
|         - | 4319 | `	/* Same instance? */` |
|         5 | 4320 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4321 | `	return PH7_OK;` |
|         3 | 4322 | `}` |
|         - | 4323 | `/*` |
|         - | 4324 | ` * array array_merge(array ...$arrays)` |
|         - | 4325 | ` *  Merge one or more arrays.` |
|         - | 4326 | ` * Parameters` |
|         - | 4327 | ` *  ...$arrays` |
|         - | 4328 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4329 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4330 | ` * Return` |
|         - | 4331 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4332 | ` *  with no arguments.` |
|         - | 4333 | ` */` |
|      1038 | 4334 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4335 | `{` |
|         - | 4336 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4337 | `	ph7_value *pArray;` |
|         - | 4338 | `	int i;` |
|         - | 4339 | `	/* Create a new array */` |
|      1043 | 4340 | `	pArray = ph7_context_new_array(pCtx);` |
|      1043 | 4341 | `	if( pArray == 0 ){` |
|       ! 0 | 4342 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4343 | `		return PH7_OK;` |
|         - | 4344 | `	}` |
|         - | 4345 | `	/* Point to the internal representation of the hashmap */` |
|      1043 | 4346 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4347 | `	/* Start merging */` |
|      3109 | 4348 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4349 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2075 | 4350 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4351 | `			/* Type mismatch -> TypeError */` |
|         8 | 4352 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4353 | `				"TypeError",` |
|         - | 4354 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4355 | `				i + 1,` |
|         4 | 4356 | `				ph7_type_name(apArg[i])` |
|         - | 4357 | `				);` |
|       ! 0 | 4358 | `		}else{` |
|      2071 | 4359 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4360 | `			/* Merge the two hashmaps */` |
|      2071 | 4361 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4362 | `		}` |
|      1038 | 4363 | `	}` |
|         - | 4364 | `	/* Return the freshly created array */` |
|      1039 | 4365 | `	ph7_result_value(pCtx,pArray);` |
|      1039 | 4366 | `	return PH7_OK;` |
|       524 | 4367 | `}` |
|         - | 4368 | `/*` |
|         - | 4369 | ` * array array_copy(array $source)` |
|         - | 4370 | ` *  Make a blind copy of the target array.` |
|         - | 4371 | ` * Parameters` |
|         - | 4372 | ` *  $source` |
|         - | 4373 | ` *   Target array` |
|         - | 4374 | ` * Return` |
|         - | 4375 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4376 | ` * Note` |
|         - | 4377 | ` *  This function is a symisc eXtension.` |
|         - | 4378 | ` */` |
|        16 | 4379 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4380 | `{` |
|         - | 4381 | `	ph7_hashmap *pMap;` |
|         - | 4382 | `	ph7_value *pArray;` |
|        17 | 4383 | `	if( nArg < 1 ){` |
|         - | 4384 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4385 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4386 | `		return PH7_OK;` |
|         - | 4387 | `	}` |
|         - | 4388 | `	/* Create a new array */` |
|        17 | 4389 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 4390 | `	if( pArray == 0 ){` |
|       ! 0 | 4391 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4392 | `		return PH7_OK;` |
|         - | 4393 | `	}` |
|         - | 4394 | `	/* Point to the internal representation of the hashmap */` |
|        17 | 4395 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        17 | 4396 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4397 | `		/* Point to the internal representation of the source */` |
|        17 | 4398 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4399 | `		/* Perform the copy */` |
|        17 | 4400 | `		PH7_HashmapDup(pSrc,pMap);` |
|         9 | 4401 | `	}else{` |
|         - | 4402 | `		/* Simple insertion */` |
|       ! 0 | 4403 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4404 | `	}` |
|         - | 4405 | `	/* Return the duplicated array */` |
|        17 | 4406 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 4407 | `	return PH7_OK;` |
|         9 | 4408 | `}` |
|         - | 4409 | `/*` |
|         - | 4410 | ` * bool array_erase(array $source)` |
|         - | 4411 | ` *  Remove all elements from a given array.` |
|         - | 4412 | ` * Parameters` |
|         - | 4413 | ` *  $source` |
|         - | 4414 | ` *   Target array` |
|         - | 4415 | ` * Return` |
|         - | 4416 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4417 | ` * Note` |
|         - | 4418 | ` *  This function is a symisc eXtension.` |
|         - | 4419 | ` */` |
|        24 | 4420 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4421 | `{` |
|         - | 4422 | `	ph7_hashmap *pMap;` |
|        26 | 4423 | `	if( nArg < 1 ){` |
|         - | 4424 | `		/* Missing arguments */` |
|       ! 0 | 4425 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4426 | `		return PH7_OK;` |
|         - | 4427 | `	}` |
|         - | 4428 | `	/* Point to the target hashmap */` |
|        26 | 4429 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        26 | 4430 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4431 | `	/* Erase */` |
|        26 | 4432 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        26 | 4433 | `	return PH7_OK;` |
|        14 | 4434 | `}` |
|         - | 4435 | `/*` |
|         - | 4436 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4437 | ` *  Extract a slice of the array.` |
|         - | 4438 | ` * Parameters` |
|         - | 4439 | ` *  $array` |
|         - | 4440 | ` *    The input array.` |
|         - | 4441 | ` * $offset` |
|         - | 4442 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4443 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4444 | ` * $length (optional, nullable)` |
|         - | 4445 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4446 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4447 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4448 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4449 | ` * $preserve_keys (optional)` |
|         - | 4450 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4451 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4452 | ` * Return` |
|         - | 4453 | ` *   The new slice.` |
|         - | 4454 | ` */` |
|        50 | 4455 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4456 | `{` |
|         - | 4457 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4458 | `	ph7_hashmap_node *pCur;` |
|         - | 4459 | `	ph7_value *pArray;` |
|         - | 4460 | `	int iLength,iOfft;` |
|         - | 4461 | `	int bPreserve;` |
|         - | 4462 | `	sxi32 rc;` |
|        55 | 4463 | `	if( nArg < 2 ){` |
|         8 | 4464 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4465 | `			"ArgumentCountError",` |
|         - | 4466 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4467 | `			nArg` |
|         - | 4468 | `			);` |
|         - | 4469 | `	}` |
|        51 | 4470 | `	if( nArg > 4 ){` |
|         4 | 4471 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4472 | `			"ArgumentCountError",` |
|         - | 4473 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4474 | `			nArg` |
|         - | 4475 | `			);` |
|         - | 4476 | `	}` |
|        49 | 4477 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4478 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4479 | `			"TypeError",` |
|         - | 4480 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4481 | `			ph7_type_name(apArg[0])` |
|         - | 4482 | `			);` |
|         - | 4483 | `	}` |
|         - | 4484 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4485 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4486 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4487 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4488 | `			"TypeError",` |
|         - | 4489 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4490 | `			ph7_type_name(apArg[1])` |
|         - | 4491 | `			);` |
|         - | 4492 | `	}` |
|         - | 4493 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4494 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4495 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4496 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4497 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4498 | `				"TypeError",` |
|         - | 4499 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4500 | `				ph7_type_name(apArg[2])` |
|         - | 4501 | `				);` |
|         - | 4502 | `		}` |
|         8 | 4503 | `	}` |
|         - | 4504 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4505 | `	if( nArg > 3 ){` |
|        10 | 4506 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4507 | `			ph7_value_is_resource(apArg[3]) ){` |
|         4 | 4508 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4509 | `				"TypeError",` |
|         - | 4510 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|         2 | 4511 | `				ph7_type_name(apArg[3])` |
|         - | 4512 | `				);` |
|         - | 4513 | `		}` |
|         2 | 4514 | `	}` |
|         - | 4515 | `	/* Point the internal representation of the target array */` |
|        41 | 4516 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 4517 | `	bPreserve = FALSE;` |
|         - | 4518 | `	/* Get the offset */` |
|        41 | 4519 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        41 | 4520 | `	if( iOfft < 0 ){` |
|         5 | 4521 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4522 | `		if( iOfft < 0 ){` |
|         3 | 4523 | `			iOfft = 0;` |
|         1 | 4524 | `		}` |
|         2 | 4525 | `	}` |
|        41 | 4526 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4527 | `		/* Offset past end of array, return empty array */` |
|         5 | 4528 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4529 | `		if( pArray == 0 ){` |
|       ! 0 | 4530 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4531 | `			return PH7_OK;` |
|         - | 4532 | `		}` |
|         5 | 4533 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4534 | `		return PH7_OK;` |
|         - | 4535 | `	}` |
|         - | 4536 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        37 | 4537 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        37 | 4538 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        15 | 4539 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        15 | 4540 | `		if( iLength < 0 ){` |
|         5 | 4541 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4542 | `		}` |
|        15 | 4543 | `		if( iLength < 0 ){` |
|         3 | 4544 | `			iLength = 0;` |
|         1 | 4545 | `		}` |
|        15 | 4546 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4547 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4548 | `		}` |
|         7 | 4549 | `	}` |
|        37 | 4550 | `	if( nArg > 3 ){` |
|         5 | 4551 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4552 | `	}` |
|         - | 4553 | `	/* Create a new array */` |
|        37 | 4554 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 4555 | `	if( pArray == 0 ){` |
|       ! 0 | 4556 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4557 | `		return PH7_OK;` |
|         - | 4558 | `	}` |
|        37 | 4559 | `	if( iLength < 1 ){` |
|         - | 4560 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4561 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4562 | `		return PH7_OK;` |
|         - | 4563 | `	}` |
|         - | 4564 | `	/* Point to the desired entry */` |
|        33 | 4565 | `	pCur = pSrc->pFirst;` |
|        28 | 4566 | `	for(;;){` |
|        61 | 4567 | `		if( iOfft < 1 ){` |
|        33 | 4568 | `			break;` |
|         - | 4569 | `		}` |
|         - | 4570 | `		/* Point to the next entry */` |
|        33 | 4571 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4572 | `		iOfft--;` |
|         5 | 4573 | `	}` |
|         - | 4574 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 4575 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        51 | 4576 | `	for(;;){` |
|       107 | 4577 | `		if( iLength < 1 ){` |
|        33 | 4578 | `			break;` |
|         - | 4579 | `		}` |
|         - | 4580 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4581 | `		{` |
|        79 | 4582 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        79 | 4583 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4584 | `		}` |
|        79 | 4585 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4586 | `			break;` |
|         - | 4587 | `		}` |
|         - | 4588 | `		/* Point to the next entry */` |
|        79 | 4589 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        79 | 4590 | `		iLength--;` |
|         5 | 4591 | `	}` |
|         - | 4592 | `	/* Return the freshly created array */` |
|        33 | 4593 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 4594 | `	return PH7_OK;` |
|        30 | 4595 | `}` |
|         - | 4596 | `/*` |
|         - | 4597 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4598 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4599 | ` * beginning (becomes the new pFirst).` |
|         - | 4600 | ` */` |
|        30 | 4601 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4602 | `{` |
|         - | 4603 | `	ph7_hashmap_node *pNode;` |
|         - | 4604 | `	ph7_hashmap_node *pOldNext;` |
|        31 | 4605 | `	pNode = pMap->pLast;` |
|        31 | 4606 | `	if( pNode == 0 ){` |
|       ! 0 | 4607 | `		return;` |
|         - | 4608 | `	}` |
|        31 | 4609 | `	if( pNode->pNext == 0 ){` |
|         - | 4610 | `		/* Only node in the list, nothing to move */` |
|         5 | 4611 | `		return;` |
|         - | 4612 | `	}` |
|        27 | 4613 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4614 | `		/* Already in the correct position */` |
|         9 | 4615 | `		return;` |
|         - | 4616 | `	}` |
|         - | 4617 | `	/* Unlink pNode from the end of the list */` |
|        19 | 4618 | `	pMap->pLast = pNode->pNext;` |
|        19 | 4619 | `	pMap->pLast->pPrev = 0;` |
|         - | 4620 | `	/* Insert pNode after pAfter in iteration order */` |
|        19 | 4621 | `	if( pAfter == 0 ){` |
|         - | 4622 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4623 | `		pNode->pNext = 0;` |
|         3 | 4624 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4625 | `		if( pMap->pFirst ){` |
|         3 | 4626 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4627 | `		}` |
|         3 | 4628 | `		pMap->pFirst = pNode;` |
|         2 | 4629 | `	}else{` |
|        17 | 4630 | `		pOldNext = pAfter->pPrev;` |
|        17 | 4631 | `		pNode->pPrev = pOldNext;` |
|        17 | 4632 | `		pNode->pNext = pAfter;` |
|        17 | 4633 | `		pAfter->pPrev = pNode;` |
|        17 | 4634 | `		if( pOldNext ){` |
|        17 | 4635 | `			pOldNext->pNext = pNode;` |
|         9 | 4636 | `		}else{` |
|       ! 0 | 4637 | `			pMap->pLast = pNode;` |
|         - | 4638 | `		}` |
|         - | 4639 | `	}` |
|        16 | 4640 | `}` |
|         - | 4641 | `/*` |
|         - | 4642 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4643 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4644 | ` * Parameters` |
|         - | 4645 | ` *  $array` |
|         - | 4646 | ` *    The input array.` |
|         - | 4647 | ` *  $offset` |
|         - | 4648 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4649 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4650 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4651 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4652 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4653 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4654 | ` *  $length (optional)` |
|         - | 4655 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4656 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4657 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4658 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4659 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4660 | ` *  $replacement (optional)` |
|         - | 4661 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4662 | ` *    with elements from this array.` |
|         - | 4663 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4664 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4665 | ` *    offset.` |
|         - | 4666 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4667 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4668 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4669 | ` * Return` |
|         - | 4670 | ` *   A new array consisting of the extracted elements.` |
|         - | 4671 | ` */` |
|        54 | 4672 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4673 | `{` |
|         - | 4674 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4675 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4676 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4677 | `	int iLength,iOfft,i;` |
|         - | 4678 | `	sxi32 rc;` |
|        58 | 4679 | `	if( nArg < 2 ){` |
|         8 | 4680 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4681 | `			"ArgumentCountError",` |
|         - | 4682 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4683 | `			nArg` |
|         - | 4684 | `			);` |
|         - | 4685 | `	}` |
|        52 | 4686 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4687 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4688 | `			"TypeError",` |
|         - | 4689 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4690 | `			ph7_type_name(apArg[0])` |
|         - | 4691 | `			);` |
|         - | 4692 | `	}` |
|         - | 4693 | `	/* Point to the internal representation of the target array */` |
|        49 | 4694 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        49 | 4695 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4696 | `	/* Get the offset and clamp to valid range */` |
|        49 | 4697 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        49 | 4698 | `	if( iOfft < 0 ){` |
|         7 | 4699 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         7 | 4700 | `		if( iOfft < 0 ){` |
|         3 | 4701 | `			iOfft = 0;` |
|         2 | 4702 | `		}` |
|        46 | 4703 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4704 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4705 | `	}` |
|         - | 4706 | `	/* Get the length and clamp to valid range.` |
|         - | 4707 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        49 | 4708 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        49 | 4709 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        31 | 4710 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        31 | 4711 | `		if( iLength < 0 ){` |
|         7 | 4712 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4713 | `			if( iLength < 0 ){` |
|         3 | 4714 | `				iLength = 0;` |
|         1 | 4715 | `			}` |
|         3 | 4716 | `		}` |
|        31 | 4717 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4718 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4719 | `		}` |
|        15 | 4720 | `	}` |
|         - | 4721 | `	/* Create the result array for removed elements */` |
|        49 | 4722 | `	pArray = ph7_context_new_array(pCtx);` |
|        49 | 4723 | `	if( pArray == 0 ){` |
|       ! 0 | 4724 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4725 | `		return PH7_OK;` |
|         - | 4726 | `	}` |
|         - | 4727 | `	/* Get replacement array if provided */` |
|        49 | 4728 | `	pRep = 0;` |
|        49 | 4729 | `	if( nArg > 3 ){` |
|        21 | 4730 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4731 | `			/* Perform an array cast */` |
|         3 | 4732 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4733 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4734 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4735 | `			}` |
|         2 | 4736 | `		}else{` |
|        19 | 4737 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4738 | `		}` |
|        21 | 4739 | `		if( pRep ){` |
|         - | 4740 | `			/* Reset the loop cursor */` |
|        21 | 4741 | `			pRep->pCur = pRep->pFirst;` |
|        10 | 4742 | `		}` |
|        10 | 4743 | `	}` |
|         - | 4744 | `	/* Early return if nothing to remove and no replacement */` |
|        49 | 4745 | `	if( iLength < 1 && pRep == 0 ){` |
|         9 | 4746 | `		ph7_result_value(pCtx,pArray);` |
|         9 | 4747 | `		return PH7_OK;` |
|         - | 4748 | `	}` |
|         - | 4749 | `	/* Navigate to the offset position */` |
|        41 | 4750 | `	pCur = pSrc->pFirst;` |
|        85 | 4751 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        45 | 4752 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        23 | 4753 | `	}` |
|         - | 4754 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4755 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4756 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        41 | 4757 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4758 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        41 | 4759 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       111 | 4760 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        71 | 4761 | `		pPrev = pCur->pPrev;` |
|        71 | 4762 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        71 | 4763 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        71 | 4764 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4765 | `			break;` |
|         - | 4766 | `		}` |
|        71 | 4767 | `		pCur = pPrev; /* Reverse link */` |
|        36 | 4768 | `	}` |
|         - | 4769 | `	/* Insert replacement elements at the correct position */` |
|        41 | 4770 | `	if( pRep ){` |
|         - | 4771 | `		ph7_value sSafeVal;` |
|        61 | 4772 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        31 | 4773 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        31 | 4774 | `			if( pRvalue ){` |
|         - | 4775 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4776 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4777 | `				 * since it points into that same pool. */` |
|        31 | 4778 | `				sSafeVal = *pRvalue;` |
|        31 | 4779 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        31 | 4780 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        31 | 4781 | `					pNewNode = pSrc->pLast;` |
|        31 | 4782 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        31 | 4783 | `					pInsertAfter = pNewNode;` |
|        15 | 4784 | `				}` |
|        15 | 4785 | `			}` |
|         1 | 4786 | `		}` |
|        10 | 4787 | `	}` |
|         - | 4788 | `	/* Return the freshly created array */` |
|        41 | 4789 | `	ph7_result_value(pCtx,pArray);` |
|        41 | 4790 | `	return PH7_OK;` |
|        31 | 4791 | `}` |
|         - | 4792 | `/*` |
|         - | 4793 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4794 | ` *  Checks if a value exists in an array.` |
|         - | 4795 | ` * Parameters` |
|         - | 4796 | ` *  $needle` |
|         - | 4797 | ` *   The searched value.` |
|         - | 4798 | ` *   Note:` |
|         - | 4799 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4800 | ` * $haystack` |
|         - | 4801 | ` *  The target array.` |
|         - | 4802 | ` * $strict` |
|         - | 4803 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4804 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4805 | ` */` |
|     33026 | 4806 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4807 | `{` |
|         - | 4808 | `	ph7_value *pNeedle;` |
|         - | 4809 | `	int bStrict;` |
|         - | 4810 | `	int rc;` |
|     33031 | 4811 | `	if( nArg < 2 ){` |
|         - | 4812 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4813 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4814 | `		return PH7_OK;` |
|         - | 4815 | `	}` |
|     33031 | 4816 | `	pNeedle = apArg[0];` |
|     33031 | 4817 | `	bStrict = 0;` |
|     33031 | 4818 | `	if( nArg > 2 ){` |
|        53 | 4819 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4820 | `	}` |
|     33031 | 4821 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4822 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4823 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4824 | `		/* Set the comparison result */` |
|       ! 0 | 4825 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4826 | `		return PH7_OK;` |
|         - | 4827 | `	}` |
|         - | 4828 | `	/* Perform the lookup */` |
|     33031 | 4829 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4830 | `	/* Lookup result */` |
|     33031 | 4831 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33031 | 4832 | `	return PH7_OK;` |
|     16518 | 4833 | `}` |
|         - | 4834 | `/*` |
|         - | 4835 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4836 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4837 | ` * Parameters` |
|         - | 4838 | ` * $needle` |
|         - | 4839 | ` *   The searched value.` |
|         - | 4840 | ` * $haystack` |
|         - | 4841 | ` *   The array.` |
|         - | 4842 | ` * $strict` |
|         - | 4843 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4844 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4845 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4846 | ` * Return` |
|         - | 4847 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4848 | ` */` |
|        32 | 4849 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4850 | `{` |
|         - | 4851 | `	ph7_hashmap_node *pEntry;` |
|         - | 4852 | `	ph7_value *pVal,sNeedle;` |
|         - | 4853 | `	ph7_hashmap *pMap;` |
|         - | 4854 | `	ph7_value sVal;` |
|         - | 4855 | `	int bStrict;` |
|         - | 4856 | `	sxu32 n;` |
|         - | 4857 | `	int rc;` |
|        37 | 4858 | `	if( nArg < 2 ){` |
|         - | 4859 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4860 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4861 | `			"ArgumentCountError",` |
|         - | 4862 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4863 | `			nArg` |
|         - | 4864 | `			);` |
|         - | 4865 | `	}` |
|        31 | 4866 | `	bStrict = FALSE;` |
|        31 | 4867 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4868 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4869 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4870 | `			"TypeError",` |
|         - | 4871 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4872 | `			ph7_type_name(apArg[1])` |
|         - | 4873 | `			);` |
|         - | 4874 | `	}` |
|        28 | 4875 | `	if( nArg > 2 ){` |
|         - | 4876 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        14 | 4877 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4878 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4879 | `				"TypeError",` |
|         - | 4880 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4881 | `				ph7_type_name(apArg[2])` |
|         - | 4882 | `				);` |
|         - | 4883 | `		}` |
|        11 | 4884 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4885 | `	}` |
|         - | 4886 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4887 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4888 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4889 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4890 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4891 | `	pEntry = pMap->pFirst;` |
|        25 | 4892 | `	n = pMap->nEntry;` |
|        28 | 4893 | `	for(;;){` |
|        57 | 4894 | `		if( !n ){` |
|         9 | 4895 | `			break;` |
|         - | 4896 | `		}` |
|         - | 4897 | `		/* Extract node value */` |
|        49 | 4898 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4899 | `		if( pVal ){` |
|         - | 4900 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4901 | `			 * can change their type.` |
|         - | 4902 | `			 */` |
|        49 | 4903 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4904 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4905 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4906 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4907 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4908 | `			if( rc == 0 ){` |
|         - | 4909 | `				/* Match found,return key */` |
|        17 | 4910 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4911 | `					/* INT key */` |
|        11 | 4912 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4913 | `				}else{` |
|         7 | 4914 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4915 | `					/* Blob key */` |
|         7 | 4916 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4917 | `				}` |
|        17 | 4918 | `				return PH7_OK;` |
|         - | 4919 | `			}` |
|        16 | 4920 | `		}` |
|         - | 4921 | `		/* Point to the next entry */` |
|        33 | 4922 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4923 | `		n--;` |
|         1 | 4924 | `	}` |
|         - | 4925 | `	/* No such value,return FALSE */` |
|         9 | 4926 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4927 | `	return PH7_OK;` |
|        21 | 4928 | `}` |
|         - | 4929 | `/*` |
|         - | 4930 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4931 | ` *  Computes the difference of arrays.` |
|         - | 4932 | ` * Parameters` |
|         - | 4933 | ` *  $array1` |
|         - | 4934 | ` *    The array to compare from` |
|         - | 4935 | ` *  $array2` |
|         - | 4936 | ` *    An array to compare against` |
|         - | 4937 | ` *  $...` |
|         - | 4938 | ` *   More arrays to compare against` |
|         - | 4939 | ` * Return` |
|         - | 4940 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4941 | ` *  are not present in any of the other arrays.` |
|         - | 4942 | ` */` |
|        22 | 4943 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4944 | `{` |
|         - | 4945 | `	ph7_hashmap_node *pEntry;` |
|         - | 4946 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4947 | `	ph7_value *pArray;` |
|         - | 4948 | `	ph7_value *pVal;` |
|         - | 4949 | `	sxi32 rc;` |
|         - | 4950 | `	sxu32 n;` |
|         - | 4951 | `	int i;` |
|         - | 4952 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4953 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4954 | `	 * debugging difficult. */` |
|        26 | 4955 | `	if( nArg < 1 ){` |
|         4 | 4956 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4957 | `			"ArgumentCountError",` |
|         - | 4958 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4959 | `			nArg` |
|         - | 4960 | `			);` |
|         - | 4961 | `	}` |
|        23 | 4962 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4963 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4964 | `			"TypeError",` |
|         - | 4965 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4966 | `			ph7_type_name(apArg[0])` |
|         - | 4967 | `			);` |
|         - | 4968 | `	}` |
|        36 | 4969 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4970 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4971 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4972 | `				"TypeError",` |
|         - | 4973 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4974 | `				i + 1,` |
|         2 | 4975 | `				ph7_type_name(apArg[i])` |
|         - | 4976 | `				);` |
|         - | 4977 | `		}` |
|         9 | 4978 | `	}` |
|        17 | 4979 | `	if( nArg == 1 ){` |
|         - | 4980 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 4981 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 4982 | `		return PH7_OK;` |
|         - | 4983 | `	}` |
|         - | 4984 | `	/* Create a new array */` |
|        15 | 4985 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 4986 | `	if( pArray == 0 ){` |
|       ! 0 | 4987 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4988 | `		return PH7_OK;` |
|         - | 4989 | `	}` |
|         - | 4990 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 4991 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4992 | `	/* Perform the diff */` |
|        15 | 4993 | `	pEntry = pSrc->pFirst;` |
|        15 | 4994 | `	n = pSrc->nEntry;` |
|        27 | 4995 | `	for(;;){` |
|        55 | 4996 | `		if( n < 1 ){` |
|        15 | 4997 | `			break;` |
|         - | 4998 | `		}` |
|         - | 4999 | `		/* Extract the node value */` |
|        41 | 5000 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5001 | `		if( pVal ){` |
|        69 | 5002 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5003 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5004 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5005 | `				/* Perform the lookup */` |
|        45 | 5006 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5007 | `				if( rc == SXRET_OK ){` |
|         - | 5008 | `					/* Value exist */` |
|        17 | 5009 | `					break;` |
|         - | 5010 | `				}` |
|        15 | 5011 | `			}` |
|        41 | 5012 | `			if( i >= nArg ){` |
|         - | 5013 | `				/* Perform the insertion */` |
|        25 | 5014 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5015 | `			}` |
|        20 | 5016 | `		}` |
|         - | 5017 | `		/* Point to the next entry */` |
|        41 | 5018 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5019 | `		n--;` |
|         1 | 5020 | `	}` |
|         - | 5021 | `	/* Return the freshly created array */` |
|        15 | 5022 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5023 | `	return PH7_OK;` |
|        15 | 5024 | `}` |
|         - | 5025 | `/*` |
|         - | 5026 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5027 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5028 | ` * Parameters` |
|         - | 5029 | ` *  $array1` |
|         - | 5030 | ` *    The array to compare from` |
|         - | 5031 | ` *  $array2` |
|         - | 5032 | ` *    An array to compare against` |
|         - | 5033 | ` *  $...` |
|         - | 5034 | ` *   More arrays to compare against.` |
|         - | 5035 | ` * $callback` |
|         - | 5036 | ` *  The callback comparison function.` |
|         - | 5037 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5038 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5039 | ` *  than the second.` |
|         - | 5040 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5041 | ` * Return` |
|         - | 5042 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5043 | ` *  are not present in any of the other arrays.` |
|         - | 5044 | ` */` |
|        22 | 5045 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5046 | `{` |
|         - | 5047 | `	ph7_hashmap_node *pEntry;` |
|         - | 5048 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5049 | `	ph7_value *pCallback;` |
|         - | 5050 | `	ph7_value *pArray;` |
|         - | 5051 | `	ph7_value *pVal;` |
|         - | 5052 | `	sxi32 rc;` |
|         - | 5053 | `	sxu32 n;` |
|         - | 5054 | `	int i;` |
|         - | 5055 |  |
|         - | 5056 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 5057 | `	if( nArg < 2 ){` |
|         4 | 5058 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5059 | `			"ArgumentCountError",` |
|         - | 5060 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 5061 | `			nArg` |
|         - | 5062 | `			);` |
|         - | 5063 | `	}` |
|        25 | 5064 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5065 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5066 | `			"TypeError",` |
|         - | 5067 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5068 | `			ph7_type_name(apArg[0])` |
|         - | 5069 | `			);` |
|         - | 5070 | `	}` |
|         - | 5071 |  |
|        23 | 5072 | `	if( nArg == 2 ){` |
|         - | 5073 | `		/* Only the original array and the callback were provided. */` |
|         - | 5074 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5075 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5076 | `		 * validation order.` |
|         - | 5077 | `		 */` |
|         4 | 5078 | `	} else {` |
|         - | 5079 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5080 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5081 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5082 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5083 | `					"TypeError",` |
|         - | 5084 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5085 | `					i + 1,` |
|         6 | 5086 | `					ph7_type_name(apArg[i])` |
|         - | 5087 | `					);` |
|         - | 5088 | `			}` |
|         7 | 5089 | `		}` |
|         - | 5090 | `	}` |
|         - | 5091 |  |
|         - | 5092 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5093 | `	pCallback = apArg[nArg - 1];` |
|         - | 5094 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5095 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5096 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5097 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5098 | `				"TypeError",` |
|         - | 5099 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5100 | `				nArg` |
|         - | 5101 | `				);` |
|         - | 5102 | `		}` |
|         6 | 5103 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5104 | `			int len;` |
|         3 | 5105 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5106 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5107 | `				"TypeError",` |
|         - | 5108 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5109 | `				nArg,` |
|         1 | 5110 | `				zName` |
|         - | 5111 | `				);` |
|         - | 5112 | `		}` |
|         4 | 5113 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5114 | `			"TypeError",` |
|         - | 5115 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5116 | `			nArg` |
|         - | 5117 | `			);` |
|         - | 5118 | `	}` |
|         - | 5119 |  |
|         7 | 5120 | `	if( nArg == 2 ){` |
|         - | 5121 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5122 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5123 | `		return PH7_OK;` |
|         - | 5124 | `	}` |
|         - | 5125 |  |
|         - | 5126 | `	/* Create a new array */` |
|         5 | 5127 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5128 | `	if( pArray == 0 ){` |
|       ! 0 | 5129 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5130 | `		return PH7_OK;` |
|         - | 5131 | `	}` |
|         - | 5132 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5133 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5134 | `	/* Perform the diff */` |
|         5 | 5135 | `	pEntry = pSrc->pFirst;` |
|         5 | 5136 | `	n = pSrc->nEntry;` |
|         5 | 5137 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5138 | `	for(;;){` |
|        11 | 5139 | `		if( n < 1 ){` |
|         3 | 5140 | `			break;` |
|         - | 5141 | `		}` |
|         - | 5142 | `		/* Extract the node value */` |
|         9 | 5143 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5144 | `		if( pVal ){` |
|        15 | 5145 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5146 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5147 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5148 | `				/* Perform the lookup */` |
|         9 | 5149 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5150 | `				if( rc == SXRET_OK ){` |
|         - | 5151 | `					/* Value exist */` |
|         3 | 5152 | `					break;` |
|         - | 5153 | `				}` |
|         4 | 5154 | `			}` |
|         9 | 5155 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5156 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5157 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5158 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5159 | `				return PH7_EXCEPTION;` |
|         - | 5160 | `			}` |
|         7 | 5161 | `			if( i >= (nArg - 1)){` |
|         - | 5162 | `				/* Perform the insertion */` |
|         5 | 5163 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5164 | `			}` |
|         3 | 5165 | `		}` |
|         - | 5166 | `		/* Point to the next entry */` |
|         7 | 5167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5168 | `		n--;` |
|         1 | 5169 | `	}` |
|         - | 5170 | `	/* Return the freshly created array */` |
|         3 | 5171 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5172 | `	return PH7_OK;` |
|        16 | 5173 | `}` |
|         - | 5174 | `/*` |
|         - | 5175 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5176 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5177 | ` * Parameters` |
|         - | 5178 | ` *  $array1` |
|         - | 5179 | ` *    The array to compare from` |
|         - | 5180 | ` *  $array2` |
|         - | 5181 | ` *    An array to compare against` |
|         - | 5182 | ` *  $...` |
|         - | 5183 | ` *   More arrays to compare against` |
|         - | 5184 | ` * Return` |
|         - | 5185 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5186 | ` *  are not present in any of the other arrays.` |
|         - | 5187 | ` */` |
|        22 | 5188 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5189 | `{` |
|         - | 5190 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5191 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5192 | `	ph7_value *pArray;` |
|         - | 5193 | `	ph7_value *pVal;` |
|         - | 5194 | `	sxi32 rc;` |
|         - | 5195 | `	sxu32 n;` |
|         - | 5196 | `	int i;` |
|         - | 5197 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5198 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5199 | `	 * accompanying integration tests to pass. */` |
|        27 | 5200 | `	if( nArg < 1 ){` |
|         4 | 5201 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5202 | `			"ArgumentCountError",` |
|         - | 5203 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5204 | `			nArg` |
|         - | 5205 | `			);` |
|         - | 5206 | `	}` |
|        24 | 5207 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5208 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5209 | `			"TypeError",` |
|         - | 5210 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5211 | `			ph7_type_name(apArg[0])` |
|         - | 5212 | `			);` |
|         - | 5213 | `	}` |
|        37 | 5214 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5215 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5216 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5217 | `				"TypeError",` |
|         - | 5218 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5219 | `				i + 1,` |
|         4 | 5220 | `				ph7_type_name(apArg[i])` |
|         - | 5221 | `				);` |
|         - | 5222 | `		}` |
|        10 | 5223 | `	}` |
|        15 | 5224 | `	if( nArg == 1 ){` |
|         - | 5225 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5226 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5227 | `		return PH7_OK;` |
|         - | 5228 | `	}` |
|         - | 5229 | `	/* Create a new array */` |
|        13 | 5230 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5231 | `	if( pArray == 0 ){` |
|       ! 0 | 5232 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5233 | `		return PH7_OK;` |
|         - | 5234 | `	}` |
|         - | 5235 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5236 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5237 | `	/* Perform the diff */` |
|        13 | 5238 | `	pEntry = pSrc->pFirst;` |
|        13 | 5239 | `	n = pSrc->nEntry;` |
|        13 | 5240 | `	pN1 = pN2 = 0;` |
|        34 | 5241 | `	for(;;){` |
|         - | 5242 | `		int keep;` |
|        41 | 5243 | `		if( n < 1 ){` |
|        13 | 5244 | `			break;` |
|         - | 5245 | `		}` |
|         - | 5246 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5247 | `		keep = 1;` |
|        47 | 5248 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5249 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5250 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5251 | `			/* Perform a key lookup first */` |
|        33 | 5252 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5253 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5254 | `			}else{` |
|        21 | 5255 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5256 | `			}` |
|        33 | 5257 | `			if( rc != SXRET_OK ){` |
|         - | 5258 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5259 | `				continue;` |
|         - | 5260 | `			}` |
|         - | 5261 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5262 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5263 | `			if( pVal ){` |
|         - | 5264 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5265 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5266 | `				if( pVal2 ){` |
|         - | 5267 | `					ph7_value sV1,sV2;` |
|         - | 5268 | `					sxi32 cmp;` |
|         - | 5269 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5270 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5271 | `					 * null element used to come back bool(false) in the` |
|         - | 5272 | `					 * caller's array). */` |
|        17 | 5273 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5274 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5275 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5276 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5277 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5278 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5279 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5280 | `					if( cmp == 0 ){` |
|         - | 5281 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5282 | `						keep = 0;` |
|        15 | 5283 | `						break;` |
|         - | 5284 | `					}` |
|         1 | 5285 | `				}` |
|         1 | 5286 | `			}` |
|         2 | 5287 | `		}` |
|        29 | 5288 | `		if( keep ){` |
|         - | 5289 | `			/* Perform the insertion */` |
|        15 | 5290 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5291 | `		}` |
|         - | 5292 | `		/* Point to the next entry */` |
|        29 | 5293 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5294 | `		n--;` |
|         1 | 5295 | `	}` |
|         - | 5296 | `	/* Return the freshly created array */` |
|        13 | 5297 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5298 | `	return PH7_OK;` |
|        16 | 5299 | `}` |
|         - | 5300 | `/*` |
|         - | 5301 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5302 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5303 | ` *  by a user supplied callback function.` |
|         - | 5304 | ` * Parameters` |
|         - | 5305 | ` *  $array1` |
|         - | 5306 | ` *    The array to compare from` |
|         - | 5307 | ` *  $array2` |
|         - | 5308 | ` *    An array to compare against` |
|         - | 5309 | ` *  $...` |
|         - | 5310 | ` *   More arrays to compare against.` |
|         - | 5311 | ` *  $key_compare_func` |
|         - | 5312 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5313 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5314 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5315 | ` * Return` |
|         - | 5316 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5317 | ` *  are not present in any of the other arrays.` |
|         - | 5318 | ` */` |
|        24 | 5319 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5320 | `{` |
|         - | 5321 | `	ph7_hashmap_node *pEntry;` |
|         - | 5322 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5323 | `	ph7_value *pCallback;` |
|         - | 5324 | `	ph7_value *pArray;` |
|         - | 5325 | `	sxi32 rc;` |
|         - | 5326 | `	sxu32 n;` |
|         - | 5327 | `	int i;` |
|         - | 5328 |  |
|         - | 5329 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5330 | `	if( nArg < 2 ){` |
|         4 | 5331 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5332 | `			"ArgumentCountError",` |
|         - | 5333 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5334 | `			nArg` |
|         - | 5335 | `			);` |
|         - | 5336 | `	}` |
|        26 | 5337 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5338 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5339 | `			"TypeError",` |
|         - | 5340 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5341 | `			ph7_type_name(apArg[0])` |
|         - | 5342 | `			);` |
|         - | 5343 | `	}` |
|         - | 5344 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5345 | `	 * expected to be a callback. */` |
|        38 | 5346 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5347 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5348 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5349 | `				"TypeError",` |
|         - | 5350 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5351 | `				i + 1,` |
|         2 | 5352 | `				ph7_type_name(apArg[i])` |
|         - | 5353 | `				);` |
|         - | 5354 | `		}` |
|         9 | 5355 | `	}` |
|         - | 5356 | `	/* Point to the callback value */` |
|        22 | 5357 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5358 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5359 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5360 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5361 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5362 | `		 * string given" which we also reproduce. */` |
|         9 | 5363 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5364 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5365 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5366 | `				"TypeError",` |
|         - | 5367 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5368 | `				nArg` |
|         - | 5369 | `				);` |
|         - | 5370 | `		}` |
|         6 | 5371 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5372 | `			/* neither array nor string */` |
|         8 | 5373 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5374 | `				"TypeError",` |
|         - | 5375 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5376 | `				nArg` |
|         - | 5377 | `				);` |
|         - | 5378 | `		}` |
|         - | 5379 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5380 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5381 | `			"TypeError",` |
|         - | 5382 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5383 | `			nArg,` |
|       ! 0 | 5384 | `			ph7_type_name(pCallback)` |
|         - | 5385 | `			);` |
|         - | 5386 | `	}` |
|        13 | 5387 | `	if( nArg == 2 ){` |
|         - | 5388 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5389 | `		 * input array. */` |
|         3 | 5390 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5391 | `		return PH7_OK;` |
|         - | 5392 | `	}` |
|         - | 5393 | `	/* Create a new array */` |
|        11 | 5394 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5395 | `	if( pArray == 0 ){` |
|       ! 0 | 5396 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5397 | `		return PH7_OK;` |
|         - | 5398 | `	}` |
|         - | 5399 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5400 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5401 | `	/* Perform the diff */` |
|        11 | 5402 | `	pEntry = pSrc->pFirst;` |
|        11 | 5403 | `	n = pSrc->nEntry;` |
|        21 | 5404 | `	for(;;){` |
|         - | 5405 | `		int keep;` |
|        27 | 5406 | `		if( n < 1 ){` |
|         9 | 5407 | `			break;` |
|         - | 5408 | `		}` |
|        19 | 5409 | `		keep = 1;` |
|        31 | 5410 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5411 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5412 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5413 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5414 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5415 | `			while( pIt ){` |
|         - | 5416 | `				/* build temporary key values for callback */` |
|         - | 5417 | `				ph7_value key1, key2, result;` |
|         - | 5418 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5419 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5420 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5421 | `				}else{` |
|         - | 5422 | `					SyString sStr;` |
|        33 | 5423 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5424 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5425 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5426 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5427 | `				}` |
|        33 | 5428 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5429 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5430 | `				}else{` |
|         - | 5431 | `					SyString sStr;` |
|        33 | 5432 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5433 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5434 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5435 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5436 | `				}` |
|        33 | 5437 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5438 | `				/* call user callback with (key1, key2) */` |
|         - | 5439 | `				{` |
|         - | 5440 | `					ph7_value *apK[2];` |
|        33 | 5441 | `					apK[0] = &key1;` |
|        33 | 5442 | `					apK[1] = &key2;` |
|        33 | 5443 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5444 | `				}` |
|        33 | 5445 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5446 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5447 | `					 * array_uintersect (which signal back from` |
|         - | 5448 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5449 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5450 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5451 | `					PH7_MemObjRelease(&result);` |
|         3 | 5452 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5453 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5454 | `					return PH7_EXCEPTION;` |
|         - | 5455 | `				}` |
|        31 | 5456 | `				if( rc == SXRET_OK ){` |
|        31 | 5457 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5458 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5459 | `					}` |
|        31 | 5460 | `					if( result.x.iVal == 0 ){` |
|         - | 5461 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5462 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5463 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5464 | `						if( pVal1 && pVal2 ){` |
|         - | 5465 | `							ph7_value sV1,sV2;` |
|         - | 5466 | `							sxi32 cmp;` |
|         - | 5467 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5468 | `							 * place and these are LIVE array elements. */` |
|        13 | 5469 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5470 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5471 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5472 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5473 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5474 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5475 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5476 | `							if( cmp == 0 ){` |
|         9 | 5477 | `								keep = 0;` |
|         9 | 5478 | `								PH7_MemObjRelease(&result);` |
|         - | 5479 | `								/* release keys too before breaking */` |
|         9 | 5480 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5481 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5482 | `								break;` |
|         - | 5483 | `							}` |
|         2 | 5484 | `						}` |
|         2 | 5485 | `					}` |
|        11 | 5486 | `				}` |
|        23 | 5487 | `				PH7_MemObjRelease(&result);` |
|        23 | 5488 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5489 | `				PH7_MemObjRelease(&key2);` |
|         - | 5490 | `				/* move to next node */` |
|        23 | 5491 | `				pIt = pIt->pPrev;` |
|        23 | 5492 | `				if( keep == 0 ) break;` |
|         1 | 5493 | `			}` |
|        21 | 5494 | `			if( keep == 0 ) break;` |
|         7 | 5495 | `		}` |
|        17 | 5496 | `		if( keep ){` |
|         - | 5497 | `			/* Perform the insertion */` |
|         9 | 5498 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5499 | `		}` |
|         - | 5500 | `		/* Point to the next entry */` |
|        17 | 5501 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5502 | `		n--;` |
|         1 | 5503 | `	}` |
|         - | 5504 | `	/* Return the freshly created array */` |
|         9 | 5505 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5506 | `	return PH7_OK;` |
|        17 | 5507 | `}` |
|         - | 5508 | `/*` |
|         - | 5509 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5510 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5511 | ` * Parameters` |
|         - | 5512 | ` *  $array1` |
|         - | 5513 | ` *    The array to compare from` |
|         - | 5514 | ` *  $array2` |
|         - | 5515 | ` *    An array to compare against` |
|         - | 5516 | ` *  $...` |
|         - | 5517 | ` *   More arrays to compare against` |
|         - | 5518 | ` * Return` |
|         - | 5519 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5520 | ` *  in any of the other arrays.` |
|         - | 5521 | ` * Note that NULL is returned on failure.` |
|         - | 5522 | ` */` |
|        14 | 5523 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5524 | `{` |
|         - | 5525 | `	ph7_hashmap_node *pEntry;` |
|         - | 5526 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5527 | `	ph7_value *pArray;` |
|         - | 5528 | `	sxi32 rc;` |
|         - | 5529 | `	sxu32 n;` |
|         - | 5530 | `	int i;` |
|         - | 5531 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5532 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5533 | `	 * helpers. */` |
|        18 | 5534 | `	if( nArg < 1 ){` |
|         4 | 5535 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5536 | `			"ArgumentCountError",` |
|         - | 5537 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5538 | `			nArg` |
|         - | 5539 | `			);` |
|         - | 5540 | `	}` |
|        15 | 5541 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5542 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5543 | `			"TypeError",` |
|         - | 5544 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5545 | `			ph7_type_name(apArg[0])` |
|         - | 5546 | `			);` |
|         - | 5547 | `	}` |
|        20 | 5548 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5549 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5550 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5551 | `				"TypeError",` |
|         - | 5552 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5553 | `				i + 1,` |
|         2 | 5554 | `				ph7_type_name(apArg[i])` |
|         - | 5555 | `				);` |
|         - | 5556 | `		}` |
|         5 | 5557 | `	}` |
|         9 | 5558 | `	if( nArg == 1 ){` |
|         - | 5559 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5560 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5561 | `		return PH7_OK;` |
|         - | 5562 | `	}` |
|         - | 5563 | `	/* Create a new array */` |
|         7 | 5564 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5565 | `	if( pArray == 0 ){` |
|       ! 0 | 5566 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5567 | `		return PH7_OK;` |
|         - | 5568 | `	}` |
|         - | 5569 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5570 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5571 | `	/* Perfrom the diff */` |
|         7 | 5572 | `	pEntry = pSrc->pFirst;` |
|         7 | 5573 | `	n = pSrc->nEntry;` |
|        12 | 5574 | `	for(;;){` |
|        25 | 5575 | `		if( n < 1 ){` |
|         7 | 5576 | `			break;` |
|         - | 5577 | `		}` |
|        31 | 5578 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5579 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5580 | `				/* ignore */` |
|       ! 0 | 5581 | `				continue;` |
|         - | 5582 | `			}` |
|        23 | 5583 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5584 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5585 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5586 | `				/* Blob lookup */` |
|        17 | 5587 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5588 | `			}else{` |
|         - | 5589 | `				/* Int lookup */` |
|         7 | 5590 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5591 | `			}` |
|        23 | 5592 | `			if( rc == SXRET_OK ){` |
|         - | 5593 | `				/* Key exists,break immediately */` |
|        11 | 5594 | `				break;` |
|         - | 5595 | `			}` |
|         7 | 5596 | `		}` |
|        19 | 5597 | `		if( i >= nArg ){` |
|         - | 5598 | `			/* Perform the insertion */` |
|         9 | 5599 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5600 | `		}` |
|         - | 5601 | `		/* Point to the next entry */` |
|        19 | 5602 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5603 | `		n--;` |
|         1 | 5604 | `	}` |
|         - | 5605 | `	/* Return the freshly created array */` |
|         7 | 5606 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5607 | `	return PH7_OK;` |
|        11 | 5608 | `}` |
|         - | 5609 | `/*` |
|         - | 5610 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5611 | ` *  Computes the intersection of arrays.` |
|         - | 5612 | ` * Parameters` |
|         - | 5613 | ` *  $array1` |
|         - | 5614 | ` *    The array to compare from` |
|         - | 5615 | ` *  $array2` |
|         - | 5616 | ` *    An array to compare against` |
|         - | 5617 | ` *  $...` |
|         - | 5618 | ` *   More arrays to compare against` |
|         - | 5619 | ` * Return` |
|         - | 5620 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5621 | ` *  in all of the parameters.` |
|         - | 5622 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5623 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5624 | ` */` |
|        22 | 5625 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5626 | `{` |
|         - | 5627 | `	ph7_hashmap_node *pEntry;` |
|         - | 5628 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5629 | `	ph7_value *pArray;` |
|         - | 5630 | `	ph7_value *pVal;` |
|         - | 5631 | `	sxi32 rc;` |
|         - | 5632 | `	sxu32 n;` |
|         - | 5633 | `	int i;` |
|        26 | 5634 | `	if( nArg < 1 ){` |
|         4 | 5635 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5636 | `			"ArgumentCountError",` |
|         - | 5637 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5638 | `			nArg` |
|         - | 5639 | `			);` |
|         - | 5640 | `	}` |
|        23 | 5641 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5642 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5643 | `			"TypeError",` |
|         - | 5644 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5645 | `			ph7_type_name(apArg[0])` |
|         - | 5646 | `			);` |
|         - | 5647 | `	}` |
|        36 | 5648 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5649 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5650 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5651 | `				"TypeError",` |
|         - | 5652 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5653 | `				i + 1,` |
|         2 | 5654 | `				ph7_type_name(apArg[i])` |
|         - | 5655 | `				);` |
|         - | 5656 | `		}` |
|         9 | 5657 | `	}` |
|        17 | 5658 | `	if( nArg == 1 ){` |
|         - | 5659 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5660 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5661 | `		return PH7_OK;` |
|         - | 5662 | `	}` |
|         - | 5663 | `	/* Create a new array */` |
|        15 | 5664 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5665 | `	if( pArray == 0 ){` |
|       ! 0 | 5666 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5667 | `		return PH7_OK;` |
|         - | 5668 | `	}` |
|         - | 5669 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5670 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5671 | `	/* Perform the intersection */` |
|        15 | 5672 | `	pEntry = pSrc->pFirst;` |
|        15 | 5673 | `	n = pSrc->nEntry;` |
|        31 | 5674 | `	for(;;){` |
|        63 | 5675 | `		if( n < 1 ){` |
|        15 | 5676 | `			break;` |
|         - | 5677 | `		}` |
|         - | 5678 | `		/* Extract the node value */` |
|        49 | 5679 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5680 | `		if( pVal ){` |
|        79 | 5681 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5682 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5683 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5684 | `				/* Perform the lookup */` |
|        55 | 5685 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5686 | `				if( rc != SXRET_OK ){` |
|         - | 5687 | `					/* Value does not exist */` |
|        25 | 5688 | `					break;` |
|         - | 5689 | `				}` |
|        16 | 5690 | `			}` |
|        49 | 5691 | `			if( i >= nArg ){` |
|         - | 5692 | `				/* Perform the insertion */` |
|        25 | 5693 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5694 | `			}` |
|        24 | 5695 | `		}` |
|         - | 5696 | `		/* Point to the next entry */` |
|        49 | 5697 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5698 | `		n--;` |
|         1 | 5699 | `	}` |
|         - | 5700 | `	/* Return the freshly created array */` |
|        15 | 5701 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5702 | `	return PH7_OK;` |
|        15 | 5703 | `}` |
|         - | 5704 | `/*` |
|         - | 5705 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5706 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5707 | ` * Parameters` |
|         - | 5708 | ` *  $array1` |
|         - | 5709 | ` *    The array to compare from` |
|         - | 5710 | ` *  $array2` |
|         - | 5711 | ` *    An array to compare against` |
|         - | 5712 | ` *  $...` |
|         - | 5713 | ` *   More arrays to compare against` |
|         - | 5714 | ` * Return` |
|         - | 5715 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5716 | ` *  in all the arguments, with matching keys.` |
|         - | 5717 | ` */` |
|        22 | 5718 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5719 | `{` |
|         - | 5720 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5721 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5722 | `	ph7_value *pArray;` |
|         - | 5723 | `	ph7_value *pVal;` |
|         - | 5724 | `	sxi32 rc;` |
|         - | 5725 | `	sxu32 n;` |
|         - | 5726 | `	int i;` |
|        26 | 5727 | `	if( nArg < 1 ){` |
|         4 | 5728 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5729 | `			"ArgumentCountError",` |
|         - | 5730 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5731 | `			nArg` |
|         - | 5732 | `			);` |
|         - | 5733 | `	}` |
|        23 | 5734 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5735 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5736 | `			"TypeError",` |
|         - | 5737 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5738 | `			ph7_type_name(apArg[0])` |
|         - | 5739 | `			);` |
|         - | 5740 | `	}` |
|        36 | 5741 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5742 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5743 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5744 | `				"TypeError",` |
|         - | 5745 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5746 | `				i + 1,` |
|         2 | 5747 | `				ph7_type_name(apArg[i])` |
|         - | 5748 | `				);` |
|         - | 5749 | `		}` |
|         9 | 5750 | `	}` |
|        17 | 5751 | `	if( nArg == 1 ){` |
|         - | 5752 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5753 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5754 | `		return PH7_OK;` |
|         - | 5755 | `	}` |
|         - | 5756 | `	/* Create a new array */` |
|        15 | 5757 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5758 | `	if( pArray == 0 ){` |
|       ! 0 | 5759 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5760 | `		return PH7_OK;` |
|         - | 5761 | `	}` |
|         - | 5762 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5763 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5764 | `	/* Perform the intersection */` |
|        15 | 5765 | `	pEntry = pSrc->pFirst;` |
|        15 | 5766 | `	n = pSrc->nEntry;` |
|        15 | 5767 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5768 | `	for(;;){` |
|        47 | 5769 | `		if( n < 1 ){` |
|        15 | 5770 | `			break;` |
|         - | 5771 | `		}` |
|         - | 5772 | `		/* Extract the node value */` |
|        33 | 5773 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5774 | `		if( pVal ){` |
|        53 | 5775 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5776 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5777 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5778 | `				/* Perform a key lookup first */` |
|        37 | 5779 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5780 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5781 | `				}else{` |
|        23 | 5782 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5783 | `				}` |
|        37 | 5784 | `				if( rc != SXRET_OK ){` |
|         - | 5785 | `					/* No such key,break immediately */` |
|         7 | 5786 | `					break;` |
|         - | 5787 | `				}` |
|         - | 5788 | `				/* Perform the lookup */` |
|        31 | 5789 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5790 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5791 | `					/* Value does not exist */` |
|         6 | 5792 | `					break;` |
|         - | 5793 | `				}` |
|        11 | 5794 | `			}` |
|        33 | 5795 | `			if( i >= nArg ){` |
|         - | 5796 | `				/* Perform the insertion */` |
|        17 | 5797 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5798 | `			}` |
|        16 | 5799 | `		}` |
|         - | 5800 | `		/* Point to the next entry */` |
|        33 | 5801 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5802 | `		n--;` |
|         1 | 5803 | `	}` |
|         - | 5804 | `	/* Return the freshly created array */` |
|        15 | 5805 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5806 | `	return PH7_OK;` |
|        15 | 5807 | `}` |
|         - | 5808 | `/*` |
|         - | 5809 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5810 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5811 | ` * Parameters` |
|         - | 5812 | ` *  $array1` |
|         - | 5813 | ` *    The array to compare from` |
|         - | 5814 | ` *  $...` |
|         - | 5815 | ` *   More arrays to compare against` |
|         - | 5816 | ` * Return` |
|         - | 5817 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5818 | ` *  have keys that are present in all arguments.` |
|         - | 5819 | ` * Note that NULL is returned on failure.` |
|         - | 5820 | ` */` |
|        22 | 5821 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5822 | `{` |
|         - | 5823 | `	ph7_hashmap_node *pEntry;` |
|         - | 5824 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5825 | `	ph7_value *pArray;` |
|         - | 5826 | `	sxi32 rc;` |
|         - | 5827 | `	sxu32 n;` |
|         - | 5828 | `	int i;` |
|        26 | 5829 | `	if( nArg < 1 ){` |
|         4 | 5830 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5831 | `			"ArgumentCountError",` |
|         - | 5832 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5833 | `			nArg` |
|         - | 5834 | `			);` |
|         - | 5835 | `	}` |
|        23 | 5836 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5837 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5838 | `			"TypeError",` |
|         - | 5839 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5840 | `			ph7_type_name(apArg[0])` |
|         - | 5841 | `			);` |
|         - | 5842 | `	}` |
|        36 | 5843 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5844 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5845 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5846 | `				"TypeError",` |
|         - | 5847 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5848 | `				i + 1,` |
|         2 | 5849 | `				ph7_type_name(apArg[i])` |
|         - | 5850 | `				);` |
|         - | 5851 | `		}` |
|         9 | 5852 | `	}` |
|        17 | 5853 | `	if( nArg == 1 ){` |
|         - | 5854 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5855 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5856 | `		return PH7_OK;` |
|         - | 5857 | `	}` |
|         - | 5858 | `	/* Create a new array */` |
|        15 | 5859 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5860 | `	if( pArray == 0 ){` |
|       ! 0 | 5861 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5862 | `		return PH7_OK;` |
|         - | 5863 | `	}` |
|         - | 5864 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5865 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5866 | `	/* Perform the intersection */` |
|        15 | 5867 | `	pEntry = pSrc->pFirst;` |
|        15 | 5868 | `	n = pSrc->nEntry;` |
|        24 | 5869 | `	for(;;){` |
|        49 | 5870 | `		if( n < 1 ){` |
|        15 | 5871 | `			break;` |
|         - | 5872 | `		}` |
|        57 | 5873 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5874 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5875 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5876 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5877 | `				/* Blob lookup */` |
|        27 | 5878 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5879 | `			}else{` |
|         - | 5880 | `				/* Int key */` |
|        13 | 5881 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5882 | `			}` |
|        39 | 5883 | `			if( rc != SXRET_OK ){` |
|         - | 5884 | `				/* Key does not exist, break immediately */` |
|        17 | 5885 | `				break;` |
|         - | 5886 | `			}` |
|        12 | 5887 | `		}` |
|        35 | 5888 | `		if( i >= nArg ){` |
|         - | 5889 | `			/* Perform the insertion */` |
|        19 | 5890 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5891 | `		}` |
|         - | 5892 | `		/* Point to the next entry */` |
|        35 | 5893 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5894 | `		n--;` |
|         1 | 5895 | `	}` |
|         - | 5896 | `	/* Return the freshly created array */` |
|        15 | 5897 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5898 | `	return PH7_OK;` |
|        15 | 5899 | `}` |
|         - | 5900 | `/*` |
|         - | 5901 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5902 | ` *  Computes the intersection of arrays.` |
|         - | 5903 | ` * Parameters` |
|         - | 5904 | ` *  $array1` |
|         - | 5905 | ` *    The array to compare from` |
|         - | 5906 | ` *  $array2` |
|         - | 5907 | ` *    An array to compare against` |
|         - | 5908 | ` *  $...` |
|         - | 5909 | ` *   More arrays to compare against` |
|         - | 5910 | ` * $callback` |
|         - | 5911 | ` *  The callback comparison function.` |
|         - | 5912 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5913 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5914 | ` *  than the second.` |
|         - | 5915 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5916 | ` * Return` |
|         - | 5917 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5918 | ` *  in all of the parameters. .` |
|         - | 5919 | ` * Note that NULL is returned on failure.` |
|         - | 5920 | ` */` |
|        26 | 5921 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5922 | `{` |
|         - | 5923 | `	ph7_hashmap_node *pEntry;` |
|         - | 5924 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5925 | `	ph7_value *pCallback;` |
|         - | 5926 | `	ph7_value *pArray;` |
|         - | 5927 | `	ph7_value *pVal;` |
|         - | 5928 | `	sxi32 rc;` |
|         - | 5929 | `	sxu32 n;` |
|         - | 5930 | `	int i;` |
|         - | 5931 |  |
|         - | 5932 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5933 | `	if( nArg < 2 ){` |
|         4 | 5934 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5935 | `			"ArgumentCountError",` |
|         - | 5936 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5937 | `			nArg` |
|         - | 5938 | `			);` |
|         - | 5939 | `	}` |
|        29 | 5940 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5941 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5942 | `			"TypeError",` |
|         - | 5943 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5944 | `			ph7_type_name(apArg[0])` |
|         - | 5945 | `			);` |
|         - | 5946 | `	}` |
|         - | 5947 |  |
|        27 | 5948 | `	if( nArg == 2 ){` |
|         - | 5949 | `		/* Only the original array and the callback were provided. */` |
|         - | 5950 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5951 | `		 * validation ordering. */` |
|         3 | 5952 | `	} else {` |
|         - | 5953 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5954 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5955 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5956 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5957 | `					"TypeError",` |
|         - | 5958 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5959 | `					i + 1,` |
|         2 | 5960 | `					ph7_type_name(apArg[i])` |
|         - | 5961 | `					);` |
|         - | 5962 | `			}` |
|        13 | 5963 | `		}` |
|         - | 5964 | `	}` |
|         - | 5965 |  |
|         - | 5966 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5967 | `	pCallback = apArg[nArg - 1];` |
|         - | 5968 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5969 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5970 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5971 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5972 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5973 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5974 | `			 */` |
|         9 | 5975 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5976 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5977 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5978 | `					"TypeError",` |
|         - | 5979 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5980 | `					nArg` |
|         - | 5981 | `					);` |
|         - | 5982 | `			}` |
|         - | 5983 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 5984 | `			{` |
|         6 | 5985 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 5986 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 5987 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 5988 | `					int nMethodLen;` |
|         6 | 5989 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 5990 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 5991 | `					if( pClass ){` |
|         - | 5992 | `						/* Class exists but method is missing. */` |
|         4 | 5993 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5994 | `							"TypeError",` |
|         - | 5995 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 5996 | `							nArg,` |
|         1 | 5997 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 5998 | `							zMethod` |
|         - | 5999 | `							);` |
|         - | 6000 | `					}` |
|         - | 6001 | `					/* Class not found */` |
|         - | 6002 | `					{` |
|         - | 6003 | `						int nName;` |
|         3 | 6004 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6005 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6006 | `							"TypeError",` |
|         - | 6007 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6008 | `							nArg,` |
|         1 | 6009 | `							zName` |
|         - | 6010 | `							);` |
|         - | 6011 | `					}` |
|         - | 6012 | `				}` |
|         - | 6013 | `			}` |
|         - | 6014 | `			/* Fallback message */` |
|       ! 0 | 6015 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6016 | `				"TypeError",` |
|         - | 6017 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6018 | `				nArg` |
|         - | 6019 | `				);` |
|         - | 6020 | `		}` |
|         6 | 6021 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6022 | `			int len;` |
|         3 | 6023 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6024 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6025 | `				"TypeError",` |
|         - | 6026 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6027 | `				nArg,` |
|         1 | 6028 | `				zName` |
|         - | 6029 | `				);` |
|         - | 6030 | `		}` |
|         4 | 6031 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6032 | `			"TypeError",` |
|         - | 6033 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6034 | `			nArg` |
|         - | 6035 | `			);` |
|         - | 6036 | `	}` |
|         - | 6037 |  |
|        11 | 6038 | `	if( nArg == 2 ){` |
|         - | 6039 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6040 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6041 | `		return PH7_OK;` |
|         - | 6042 | `	}` |
|         - | 6043 |  |
|         - | 6044 | `	/* Create a new array */` |
|         7 | 6045 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6046 | `	if( pArray == 0 ){` |
|       ! 0 | 6047 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6048 | `		return PH7_OK;` |
|         - | 6049 | `	}` |
|         - | 6050 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6051 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6052 | `	/* Perform the intersection */` |
|         7 | 6053 | `	pEntry = pSrc->pFirst;` |
|         7 | 6054 | `	n = pSrc->nEntry;` |
|         7 | 6055 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6056 | `	for(;;){` |
|        19 | 6057 | `		if( n < 1 ){` |
|         5 | 6058 | `			break;` |
|         - | 6059 | `		}` |
|         - | 6060 | `		/* Extract the node value */` |
|        15 | 6061 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6062 | `		if( pVal ){` |
|        23 | 6063 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6064 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6065 | `					/* ignore */` |
|       ! 0 | 6066 | `					continue;` |
|         - | 6067 | `				}` |
|         - | 6068 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6069 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6070 | `				/* Perform the lookup */` |
|        15 | 6071 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6072 | `				if( rc != SXRET_OK ){` |
|         - | 6073 | `					/* Value does not exist */` |
|         7 | 6074 | `					break;` |
|         - | 6075 | `				}` |
|         5 | 6076 | `			}` |
|        15 | 6077 | `			if( i >= (nArg-1) ){` |
|         - | 6078 | `				/* Perform the insertion */` |
|         9 | 6079 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6080 | `			}` |
|         7 | 6081 | `		}` |
|        15 | 6082 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6083 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6084 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6085 | `			return PH7_EXCEPTION;` |
|         - | 6086 | `		}` |
|         - | 6087 | `		/* Point to the next entry */` |
|        13 | 6088 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6089 | `		n--;` |
|         1 | 6090 | `	}` |
|         - | 6091 | `	/* Return the freshly created array */` |
|         5 | 6092 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6093 | `	return PH7_OK;` |
|        18 | 6094 | `}` |
|         - | 6095 | `/*` |
|         - | 6096 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6097 | ` *  Fill an array with values.` |
|         - | 6098 | ` * Parameters` |
|         - | 6099 | ` *  $start_index` |
|         - | 6100 | ` *    The first index of the returned array.` |
|         - | 6101 | ` *  $num` |
|         - | 6102 | ` *   Number of elements to insert.` |
|         - | 6103 | ` *  $value` |
|         - | 6104 | ` *    Value to use for filling.` |
|         - | 6105 | ` * Return` |
|         - | 6106 | ` *  The filled array or null on failure.` |
|         - | 6107 | ` */` |
|       244 | 6108 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6109 | `{` |
|         - | 6110 | `	ph7_value *pArray;` |
|         - | 6111 | `	int i,nEntry;` |
|         - | 6112 |  |
|         - | 6113 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 6114 | `	if( nArg != 3 ){` |
|         - | 6115 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6116 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6117 | `			"ArgumentCountError",` |
|         - | 6118 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6119 | `			nArg` |
|         - | 6120 | `			);` |
|         - | 6121 | `	}` |
|         - | 6122 |  |
|         - | 6123 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6124 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6125 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6126 | `	 * and NULLs are rejected outright. */` |
|       359 | 6127 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6128 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6129 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6130 | `			"TypeError",` |
|         - | 6131 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6132 | `			ph7_type_name(apArg[0])` |
|         - | 6133 | `			);` |
|         - | 6134 | `	}` |
|       242 | 6135 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6136 | `		int len;` |
|         8 | 6137 | `		sxu8 bReal = FALSE;` |
|         8 | 6138 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6139 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6140 | `			/* Non‑numeric string is an error. */` |
|         3 | 6141 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6142 | `				"TypeError",` |
|         - | 6143 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6144 | `				);` |
|         - | 6145 | `		}` |
|         5 | 6146 | `		if( bReal ){` |
|         - | 6147 | `			/* float-string -> deprecation warning */` |
|         4 | 6148 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6149 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6150 | `				zStr` |
|         - | 6151 | `				);` |
|         1 | 6152 | `		}` |
|         2 | 6153 | `	}` |
|         - | 6154 |  |
|         - | 6155 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6156 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6157 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6158 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6159 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6160 | `			"TypeError",` |
|         - | 6161 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6162 | `			ph7_type_name(apArg[1])` |
|         - | 6163 | `			);` |
|         - | 6164 | `	}` |
|       239 | 6165 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6166 | `		int len;` |
|         3 | 6167 | `		sxu8 bReal = FALSE;` |
|         3 | 6168 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6169 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6170 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6171 | `				"TypeError",` |
|         - | 6172 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6173 | `				);` |
|         - | 6174 | `		}` |
|       ! 0 | 6175 | `	}` |
|         - | 6176 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6177 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6178 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6179 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6180 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6181 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6182 | `		if( d != (double)i64 ){` |
|         7 | 6183 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6184 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6185 | `				d` |
|         - | 6186 | `				);` |
|         2 | 6187 | `		}` |
|         2 | 6188 | `	}` |
|         - | 6189 |  |
|         - | 6190 | `	/* Total number of entries to insert */` |
|       236 | 6191 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6192 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6193 | `	if( nEntry < 0 ){` |
|         3 | 6194 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6195 | `			"ValueError",` |
|         - | 6196 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6197 | `			);` |
|         - | 6198 | `	}` |
|         - | 6199 |  |
|         - | 6200 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6201 | `	if( nEntry == 0 ){` |
|         7 | 6202 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6203 | `		return PH7_OK;` |
|         - | 6204 | `	}` |
|         - | 6205 |  |
|         - | 6206 | `	/* Create a new array */` |
|       227 | 6207 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6208 | `	if( pArray == 0 ){` |
|       ! 0 | 6209 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6210 | `	}` |
|         - | 6211 |  |
|         - | 6212 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6213 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6214 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6215 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6216 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6217 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6218 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6219 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6220 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6221 | `		}` |
|   1058803 | 6222 | `	}` |
|         - | 6223 | `	/* Return the filled array */` |
|       227 | 6224 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6225 | `	return PH7_OK;` |
|       127 | 6226 | `}` |
|         - | 6227 | `/*` |
|         - | 6228 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6229 | ` *  Fill an array with values, specifying keys.` |
|         - | 6230 | ` * Parameters` |
|         - | 6231 | ` *  $input` |
|         - | 6232 | ` *   Array of values that will be used as key.` |
|         - | 6233 | ` *  $value` |
|         - | 6234 | ` *    Value to use for filling.` |
|         - | 6235 | ` * Return` |
|         - | 6236 | ` *  The filled array.` |
|         - | 6237 | ` * Throws` |
|         - | 6238 | ` *  ValueError if $input is not an array.` |
|         - | 6239 | ` */` |
|        26 | 6240 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6241 | `{` |
|         - | 6242 | `	ph7_hashmap_node *pEntry;` |
|         - | 6243 | `	ph7_hashmap *pSrc;` |
|         - | 6244 | `	ph7_value *pArray;` |
|         - | 6245 | `	sxu32 n;` |
|         - | 6246 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6247 | `	if( nArg != 2 ){` |
|        12 | 6248 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6249 | `			"ArgumentCountError",` |
|         - | 6250 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6251 | `			nArg` |
|         - | 6252 | `			);` |
|         - | 6253 | `	}` |
|         - | 6254 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6255 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6256 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6257 | `			"TypeError",` |
|         - | 6258 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6259 | `			ph7_type_name(apArg[0])` |
|         - | 6260 | `			);` |
|         - | 6261 | `	}` |
|         - | 6262 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6263 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6264 | `	/* Create a new array */` |
|        17 | 6265 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6266 | `	if( pArray == 0 ){` |
|       ! 0 | 6267 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6268 | `		return PH7_OK;` |
|         - | 6269 | `	}` |
|         - | 6270 | `	/* Perform the requested operation */` |
|        17 | 6271 | `	pEntry = pSrc->pFirst;` |
|        45 | 6272 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6273 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6274 | `		/* Point to the next entry */` |
|        29 | 6275 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6276 | `	}` |
|         - | 6277 | `	/* Return the filled array */` |
|        17 | 6278 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6279 | `	return PH7_OK;` |
|        18 | 6280 | `}` |
|         - | 6281 | `/*` |
|         - | 6282 | ` * array array_combine(array $keys,array $values)` |
|         - | 6283 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6284 | ` * Parameters` |
|         - | 6285 | ` *  $keys` |
|         - | 6286 | ` *    Array of keys to be used.` |
|         - | 6287 | ` * $values` |
|         - | 6288 | ` *   Array of values to be used.` |
|         - | 6289 | ` * Return` |
|         - | 6290 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6291 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6292 | ` *  not an array.` |
|         - | 6293 | ` */` |
|        18 | 6294 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6295 | `{` |
|         - | 6296 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6297 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6298 | `	ph7_value *pArray;` |
|         - | 6299 | `	sxu32 n;` |
|         - | 6300 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6301 | `	if( nArg != 2 ){` |
|         - | 6302 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6303 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6304 | `			"ArgumentCountError",` |
|         - | 6305 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6306 | `			nArg` |
|         - | 6307 | `			);` |
|         - | 6308 | `	}` |
|         - | 6309 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6310 | `	 * argument index in the error message. */` |
|        20 | 6311 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6312 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6313 | `			"TypeError",` |
|         - | 6314 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6315 | `			ph7_type_name(apArg[0])` |
|         - | 6316 | `			);` |
|         - | 6317 | `	}` |
|        17 | 6318 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6319 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6320 | `			"TypeError",` |
|         - | 6321 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6322 | `			ph7_type_name(apArg[1])` |
|         - | 6323 | `			);` |
|         - | 6324 | `	}` |
|         - | 6325 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6326 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6327 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6328 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6329 | `		/* Length mismatch -> ValueError */` |
|         3 | 6330 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6331 | `			"ValueError",` |
|         - | 6332 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6333 | `			);` |
|         - | 6334 | `	}` |
|         - | 6335 | `	/* Create a new array */` |
|        11 | 6336 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6337 | `	if( pArray == 0 ){` |
|       ! 0 | 6338 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6339 | `		return PH7_OK;` |
|         - | 6340 | `	}` |
|         - | 6341 | `	/* Perform the requested operation */` |
|        11 | 6342 | `	pKe = pKey->pFirst;` |
|        11 | 6343 | `	pVe = pValue->pFirst;` |
|        33 | 6344 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6345 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6346 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6347 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6348 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6349 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6350 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6351 | `		 * original array must not be mutated. */` |
|        23 | 6352 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6353 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6354 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6355 | `			if( pTmpKey ){` |
|         5 | 6356 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6357 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6358 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6359 | `				pKeyCopy = pTmpKey;` |
|         2 | 6360 | `			}` |
|         2 | 6361 | `		}` |
|        23 | 6362 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6363 | `		/* Point to the next entry */` |
|        23 | 6364 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6365 | `		pVe = pVe->pPrev;` |
|        12 | 6366 | `	}` |
|         - | 6367 | `	/* Return the filled array */` |
|        11 | 6368 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6369 | `	return PH7_OK;` |
|        14 | 6370 | `}` |
|         - | 6371 | `/*` |
|         - | 6372 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6373 | ` *  Return an array with elements in reverse order.` |
|         - | 6374 | ` * Parameters` |
|         - | 6375 | ` *  $array` |
|         - | 6376 | ` *   The input array.` |
|         - | 6377 | ` *  $preserve_keys (optional)` |
|         - | 6378 | ` *   If set to TRUE keys are preserved.` |
|         - | 6379 | ` * Return` |
|         - | 6380 | ` *  The reversed array.` |
|         - | 6381 | ` */` |
|        20 | 6382 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6383 | `{` |
|         - | 6384 | `	ph7_hashmap_node *pEntry;` |
|         - | 6385 | `	ph7_hashmap *pSrc;` |
|         - | 6386 | `	ph7_value *pArray;` |
|         - | 6387 | `	int bPreserve;` |
|         - | 6388 | `	sxu32 n;` |
|        23 | 6389 | `	if( nArg < 1 ){` |
|         4 | 6390 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6391 | `			"ArgumentCountError",` |
|         - | 6392 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6393 | `			nArg` |
|         - | 6394 | `			);` |
|         - | 6395 | `	}` |
|         - | 6396 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6397 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6398 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6399 | `			"TypeError",` |
|         - | 6400 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6401 | `			ph7_type_name(apArg[0])` |
|         - | 6402 | `			);` |
|         - | 6403 | `	}` |
|        17 | 6404 | `	bPreserve = FALSE;` |
|        17 | 6405 | `	if( nArg > 1 ){` |
|         7 | 6406 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6407 | `	}` |
|         - | 6408 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6409 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6410 | `	/* Create a new array */` |
|        17 | 6411 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6412 | `	if( pArray == 0 ){` |
|       ! 0 | 6413 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6414 | `		return PH7_OK;` |
|         - | 6415 | `	}` |
|         - | 6416 | `	/* Perform the requested operation */` |
|        17 | 6417 | `	pEntry = pSrc->pLast;` |
|        55 | 6418 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6419 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6420 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6421 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6422 | `		/* Point to the previous entry */` |
|        39 | 6423 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6424 | `	}` |
|        17 | 6425 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6426 | `	return PH7_OK;` |
|        13 | 6427 | `}` |
|         - | 6428 | `/*` |
|         - | 6429 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6430 | ` *  Removes duplicate values from an array.` |
|         - | 6431 | ` * Parameters` |
|         - | 6432 | ` *  $array` |
|         - | 6433 | ` *   The input array.` |
|         - | 6434 | ` *  $flags` |
|         - | 6435 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6436 | ` *   behavior using these values:` |
|         - | 6437 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6438 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6439 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6440 | ` * Return` |
|         - | 6441 | ` *  The filtered array.` |
|         - | 6442 | ` */` |
|        24 | 6443 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6444 | `{` |
|         - | 6445 | `	ph7_hashmap_node *pEntry;` |
|         - | 6446 | `	ph7_value *pNeedle;` |
|         - | 6447 | `	ph7_hashmap *pSrc;` |
|         - | 6448 | `	ph7_value *pArray;` |
|         - | 6449 | `	int bStrict;` |
|         - | 6450 | `	sxi32 rc;` |
|         - | 6451 | `	sxu32 n;` |
|        28 | 6452 | `	if( nArg < 1 ){` |
|         - | 6453 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6454 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6455 | `			"ArgumentCountError",` |
|         - | 6456 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6457 | `			);` |
|         - | 6458 | `	}` |
|        25 | 6459 | `	if( nArg > 2 ){` |
|         - | 6460 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6461 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6462 | `			"ArgumentCountError",` |
|         - | 6463 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6464 | `			nArg` |
|         - | 6465 | `			);` |
|         - | 6466 | `	}` |
|         - | 6467 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6468 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6469 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6470 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6471 | `			"TypeError",` |
|         - | 6472 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6473 | `			ph7_type_name(apArg[0])` |
|         - | 6474 | `			);` |
|         - | 6475 | `	}` |
|        19 | 6476 | `	bStrict = FALSE;` |
|         - | 6477 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6478 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6479 | `	/* Create a new array */` |
|        19 | 6480 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6481 | `	if( pArray == 0 ){` |
|       ! 0 | 6482 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6483 | `		return PH7_OK;` |
|         - | 6484 | `	}` |
|         - | 6485 | `	/* Perform the requested operation */` |
|        19 | 6486 | `	pEntry = pSrc->pFirst;` |
|        83 | 6487 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6488 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6489 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6490 | `		if( pNeedle ){` |
|        65 | 6491 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6492 | `		}` |
|        65 | 6493 | `		if( rc != SXRET_OK ){` |
|         - | 6494 | `			/* Perform the insertion */` |
|        37 | 6495 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6496 | `		}` |
|         - | 6497 | `		/* Point to the next entry */` |
|        65 | 6498 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6499 | `	}` |
|         - | 6500 | `	/* Return the freshly created array */` |
|        19 | 6501 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6502 | `	return PH7_OK;` |
|        16 | 6503 | `}` |
|         - | 6504 | `/*` |
|         - | 6505 | ` * array array_flip(array $input)` |
|         - | 6506 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6507 | ` * Parameter` |
|         - | 6508 | ` *  $input` |
|         - | 6509 | ` *   Input array.` |
|         - | 6510 | ` * Return` |
|         - | 6511 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6512 | ` */` |
|        34 | 6513 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6514 | `{` |
|         - | 6515 | `	ph7_hashmap_node *pEntry;` |
|         - | 6516 | `	ph7_hashmap *pSrc;` |
|         - | 6517 | `	ph7_value *pArray;` |
|         - | 6518 | `	ph7_value *pKey;` |
|         - | 6519 | `	ph7_value sVal;` |
|         - | 6520 | `	sxu32 n;` |
|         - | 6521 |  |
|         - | 6522 | `	/* PHP requires exactly one argument */` |
|        39 | 6523 | `	if( nArg != 1 ){` |
|         - | 6524 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6525 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6526 | `			"ArgumentCountError",` |
|         - | 6527 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6528 | `			nArg` |
|         - | 6529 | `			);` |
|         - | 6530 | `	}` |
|         - | 6531 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6532 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6533 | `		/* Type mismatch -> TypeError */` |
|         8 | 6534 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6535 | `			"TypeError",` |
|         - | 6536 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6537 | `			ph7_type_name(apArg[0])` |
|         - | 6538 | `			);` |
|         - | 6539 | `	}` |
|         - | 6540 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6541 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6542 | `	/* Create a new array */` |
|        27 | 6543 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6544 | `	if( pArray == 0 ){` |
|       ! 0 | 6545 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6546 | `		return PH7_OK;` |
|         - | 6547 | `	}` |
|         - | 6548 | `	/* Start processing */` |
|        27 | 6549 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6550 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6551 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6552 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6553 | `		if( pKey ){` |
|         - | 6554 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6555 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6556 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6557 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6558 | `					);` |
|     22236 | 6559 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6560 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6561 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6562 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6563 | `				}else{` |
|         - | 6564 | `					SyString sStr;` |
|      2227 | 6565 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6566 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6567 | `				}` |
|         - | 6568 | `				/* Perform the insertion */` |
|     22227 | 6569 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6570 | `				/* Safely release the value because each inserted entry` |
|         - | 6571 | `				 * has its own private copy of the value.` |
|         - | 6572 | `				 */` |
|     22227 | 6573 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6574 | `			}else{` |
|         - | 6575 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6576 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6577 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6578 | `					);` |
|         - | 6579 | `			}` |
|     11118 | 6580 | `		}` |
|         - | 6581 | `		/* Point to the next entry */` |
|     22237 | 6582 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6583 | `	}` |
|         - | 6584 | `	/* Return the freshly created array */` |
|        27 | 6585 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6586 | `	return PH7_OK;` |
|        22 | 6587 | `}` |
|         - | 6588 | `/*` |
|         - | 6589 | ` * number array_sum(array $array )` |
|         - | 6590 | ` *  Calculate the sum of values in an array.` |
|         - | 6591 | ` * Parameters` |
|         - | 6592 | ` *  $array: The input array.` |
|         - | 6593 | ` * Return` |
|         - | 6594 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6595 | ` */` |
|        24 | 6596 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6597 | `{` |
|         - | 6598 | `	ph7_hashmap_node *pEntry;` |
|         - | 6599 | `	ph7_value *pObj;` |
|        25 | 6600 | `	double dSum = 0;` |
|         - | 6601 | `	sxu32 n;` |
|        25 | 6602 | `	pEntry = pMap->pFirst;` |
|        91 | 6603 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6604 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6605 | `		if( pObj ){` |
|        67 | 6606 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6607 | `				dSum += pObj->rVal;` |
|        53 | 6608 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6609 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6610 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6611 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6612 | `					double dv = 0;` |
|        13 | 6613 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6614 | `					dSum += dv;` |
|         7 | 6615 | `				}` |
|        12 | 6616 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6617 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6618 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6619 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6620 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6621 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6622 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6623 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6624 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6625 | `			}` |
|         - | 6626 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6627 | `		}` |
|         - | 6628 | `		/* Point to the next entry */` |
|        67 | 6629 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6630 | `	}` |
|         - | 6631 | `	/* Return sum */` |
|        25 | 6632 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6633 | `}` |
|       680 | 6634 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6635 | `{` |
|         - | 6636 | `	ph7_hashmap_node *pEntry;` |
|         - | 6637 | `	ph7_value *pObj;` |
|       682 | 6638 | `	sxi64 nSum = 0;` |
|         - | 6639 | `	sxu32 n;` |
|       682 | 6640 | `	pEntry = pMap->pFirst;` |
|      4672 | 6641 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      3992 | 6642 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      3992 | 6643 | `		if( pObj ){` |
|      3992 | 6644 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3982 | 6645 | `				nSum += pObj->x.iVal;` |
|      2001 | 6646 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6647 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6648 | `					sxi64 nv = 0;` |
|         5 | 6649 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6650 | `					nSum += nv;` |
|         3 | 6651 | `				}` |
|         8 | 6652 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6653 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6654 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6655 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6656 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6657 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6658 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6659 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6660 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6661 | `			}` |
|         - | 6662 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      1995 | 6663 | `		}` |
|         - | 6664 | `		/* Point to the next entry */` |
|      3992 | 6665 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      1997 | 6666 | `	}` |
|         - | 6667 | `	/* Return sum */` |
|       682 | 6668 | `	ph7_result_int64(pCtx,nSum);` |
|       682 | 6669 | `}` |
|         - | 6670 | `/* number array_sum(array $array )` |
|         - | 6671 | ` * (See block-coment above)` |
|         - | 6672 | ` */` |
|       718 | 6673 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6674 | `{` |
|         - | 6675 | `	ph7_hashmap_node *pEntry;` |
|         - | 6676 | `	ph7_hashmap *pMap;` |
|         - | 6677 | `	ph7_value *pObj;` |
|       723 | 6678 | `	int useDouble = 0;` |
|         - | 6679 | `	sxu32 n;` |
|         - | 6680 | `	/* PHP requires exactly one argument */` |
|       723 | 6681 | `	if( nArg != 1 ){` |
|         8 | 6682 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6683 | `			"ArgumentCountError",` |
|         - | 6684 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6685 | `			nArg` |
|         - | 6686 | `			);` |
|         - | 6687 | `	}` |
|         - | 6688 | `	/* Make sure we are dealing with a valid hashmap */` |
|       717 | 6689 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6690 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6691 | `		char zBuf[64];` |
|         8 | 6692 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6693 | `			"TypeError",` |
|         - | 6694 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6695 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6696 | `			);` |
|         - | 6697 | `	}` |
|       712 | 6698 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       712 | 6699 | `	if( pMap->nEntry < 1 ){` |
|         - | 6700 | `		/* Nothing to compute,return 0 */` |
|         7 | 6701 | `		ph7_result_int(pCtx,0);` |
|         7 | 6702 | `		return PH7_OK;` |
|         - | 6703 | `	}` |
|         - | 6704 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6705 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6706 | `	 */` |
|       706 | 6707 | `	pEntry = pMap->pFirst;` |
|      4704 | 6708 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4024 | 6709 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4024 | 6710 | `		if( pObj ){` |
|      4024 | 6711 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6712 | `				useDouble = 1;` |
|        19 | 6713 | `				break;` |
|         - | 6714 | `			}` |
|      4006 | 6715 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6716 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6717 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6718 | `				sxu32 i;` |
|        23 | 6719 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6720 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6721 | `						useDouble = 1;` |
|         7 | 6722 | `						break;` |
|         - | 6723 | `					}` |
|         6 | 6724 | `				}` |
|        13 | 6725 | `				if( useDouble ){` |
|         7 | 6726 | `					break;` |
|         - | 6727 | `				}` |
|         3 | 6728 | `			}` |
|      1999 | 6729 | `		}` |
|      4000 | 6730 | `		pEntry = pEntry->pPrev;` |
|      2001 | 6731 | `	}` |
|       706 | 6732 | `	if( useDouble ){` |
|        25 | 6733 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6734 | `	}else{` |
|       682 | 6735 | `		Int64Sum(pCtx,pMap);` |
|         - | 6736 | `	}` |
|       706 | 6737 | `	return PH7_OK;` |
|       364 | 6738 | `}` |
|         - | 6739 | `/*` |
|         - | 6740 | ` * number array_product(array $array )` |
|         - | 6741 | ` *  Calculate the product of values in an array.` |
|         - | 6742 | ` * Parameters` |
|         - | 6743 | ` *  $array: The input array.` |
|         - | 6744 | ` * Return` |
|         - | 6745 | ` *  Returns the product of values as an integer or float.` |
|         - | 6746 | ` */` |
|         2 | 6747 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6748 | `{` |
|         - | 6749 | `	ph7_hashmap_node *pEntry;` |
|         - | 6750 | `	ph7_value *pObj;` |
|         - | 6751 | `	double dProd;` |
|         - | 6752 | `	sxu32 n;` |
|         3 | 6753 | `	pEntry = pMap->pFirst;` |
|         3 | 6754 | `	dProd = 1;` |
|         7 | 6755 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6756 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6757 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6758 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6759 | `				dProd *= pObj->rVal;` |
|         4 | 6760 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6761 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6762 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6763 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6764 | `					double dv = 0;` |
|       ! 0 | 6765 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6766 | `					dProd *= dv;` |
|       ! 0 | 6767 | `				}` |
|       ! 0 | 6768 | `			}` |
|         2 | 6769 | `		}` |
|         - | 6770 | `		/* Point to the next entry */` |
|         5 | 6771 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6772 | `	}` |
|         - | 6773 | `	/* Return product */` |
|         3 | 6774 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6775 | `}` |
|         2 | 6776 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6777 | `{` |
|         - | 6778 | `	ph7_hashmap_node *pEntry;` |
|         - | 6779 | `	ph7_value *pObj;` |
|         - | 6780 | `	sxi64 nProd;` |
|         - | 6781 | `	sxu32 n;` |
|         3 | 6782 | `	pEntry = pMap->pFirst;` |
|         3 | 6783 | `	nProd = 1;` |
|         9 | 6784 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6785 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6786 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6787 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6788 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6789 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6790 | `				nProd *= pObj->x.iVal;` |
|         3 | 6791 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6792 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6793 | `					sxi64 nv = 0;` |
|       ! 0 | 6794 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6795 | `					nProd *= nv;` |
|       ! 0 | 6796 | `				}` |
|       ! 0 | 6797 | `			}` |
|         3 | 6798 | `		}` |
|         - | 6799 | `		/* Point to the next entry */` |
|         7 | 6800 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6801 | `	}` |
|         - | 6802 | `	/* Return product */` |
|         3 | 6803 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6804 | `}` |
|         - | 6805 | `/* number array_product(array $array )` |
|         - | 6806 | ` * (See block-block comment above)` |
|         - | 6807 | ` */` |
|        18 | 6808 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6809 | `{` |
|         - | 6810 | `	ph7_hashmap *pMap;` |
|         - | 6811 | `	ph7_value *pObj;` |
|        19 | 6812 | `	if( nArg < 1 ){` |
|         - | 6813 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6814 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6815 | `		return PH7_OK;` |
|         - | 6816 | `	}` |
|         - | 6817 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6818 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6819 | `		char zBuf[64];` |
|        19 | 6820 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6821 | `			"TypeError",` |
|         - | 6822 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6823 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6824 | `			);` |
|         - | 6825 | `	}` |
|         7 | 6826 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6827 | `	if( pMap->nEntry < 1 ){` |
|         - | 6828 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6829 | `		ph7_result_int(pCtx,1);` |
|         3 | 6830 | `		return PH7_OK;` |
|         - | 6831 | `	}` |
|         - | 6832 | `	/* If the first element is of type float,then perform floating` |
|         - | 6833 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6834 | `	 */` |
|         5 | 6835 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6836 | `	if( pObj == 0 ){` |
|       ! 0 | 6837 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6838 | `		return PH7_OK;` |
|         - | 6839 | `	}` |
|         5 | 6840 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6841 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6842 | `	}else{` |
|         3 | 6843 | `		Int64Prod(pCtx,pMap);` |
|         - | 6844 | `	}` |
|         5 | 6845 | `	return PH7_OK;` |
|        10 | 6846 | `}` |
|         - | 6847 | `/*` |
|         - | 6848 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6849 | ` *  Pick one or more random entries out of an array.` |
|         - | 6850 | ` * Parameters` |
|         - | 6851 | ` * $input` |
|         - | 6852 | ` *  The input array.` |
|         - | 6853 | ` * $num_req` |
|         - | 6854 | ` *  Specifies how many entries you want to pick.` |
|         - | 6855 | ` * Return` |
|         - | 6856 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6857 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6858 | ` *  NULL is returned on failure.` |
|         - | 6859 | ` */` |
|        42 | 6860 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6861 | `{` |
|         - | 6862 | `	ph7_hashmap_node *pNode;` |
|         - | 6863 | `	ph7_hashmap *pMap;` |
|        43 | 6864 | `	int nItem = 1;` |
|        43 | 6865 | `	if( nArg < 1 ){` |
|         - | 6866 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6867 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6868 | `		return PH7_OK;` |
|         - | 6869 | `	}` |
|         - | 6870 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6871 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6872 | `		char zBuf[64];` |
|        10 | 6873 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6874 | `			"TypeError",` |
|         - | 6875 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6876 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6877 | `			);` |
|         - | 6878 | `	}` |
|         - | 6879 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6880 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6881 | `	if( nArg > 1 ){` |
|        29 | 6882 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6883 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6884 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6885 | `			char zBuf[64];` |
|        10 | 6886 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6887 | `				"TypeError",` |
|         - | 6888 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6889 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6890 | `				);` |
|         - | 6891 | `		}` |
|        23 | 6892 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6893 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6894 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6895 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6896 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6897 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6898 | `			int len;` |
|         9 | 6899 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6900 | `			sxi64 iLong; double dReal;` |
|         9 | 6901 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6902 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6903 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6904 | `					"TypeError",` |
|         - | 6905 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6906 | `					);` |
|         - | 6907 | `			}` |
|         - | 6908 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6909 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6910 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6911 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6912 | `			}` |
|         3 | 6913 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6914 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6915 | `			nItem = (int)iLong;` |
|         2 | 6916 | `		}else{` |
|        15 | 6917 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6918 | `		}` |
|         8 | 6919 | `	}` |
|         - | 6920 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6921 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6922 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6923 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6924 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6925 | `			"ValueError",` |
|         - | 6926 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6927 | `			);` |
|         - | 6928 | `	}` |
|         - | 6929 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6930 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6931 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6932 | `			"ValueError",` |
|         - | 6933 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6934 | `			);` |
|         - | 6935 | `	}` |
|        13 | 6936 | `	if( nItem < 2 ){` |
|         - | 6937 | `		sxu32 nEntry;` |
|         - | 6938 | `		/* Select a random number */` |
|         9 | 6939 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6940 | `		/* Extract the desired entry.` |
|         - | 6941 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6942 | `		 */` |
|         9 | 6943 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         3 | 6944 | `			pNode = pMap->pLast;` |
|         3 | 6945 | `			nEntry = pMap->nEntry - nEntry;` |
|         3 | 6946 | `			if( nEntry > 1 ){` |
|       ! 0 | 6947 | `				for(;;){` |
|       ! 0 | 6948 | `					if( nEntry == 0 ){` |
|       ! 0 | 6949 | `						break;` |
|         - | 6950 | `					}` |
|         - | 6951 | `					/* Point to the previous entry */` |
|       ! 0 | 6952 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6953 | `					nEntry--;` |
|       ! 0 | 6954 | `				}` |
|       ! 0 | 6955 | `			}` |
|         2 | 6956 | `		}else{` |
|         7 | 6957 | `			pNode = pMap->pFirst;` |
|         4 | 6958 | `			for(;;){` |
|         9 | 6959 | `				if( nEntry == 0 ){` |
|         7 | 6960 | `					break;` |
|         - | 6961 | `				}` |
|         - | 6962 | `				/* Point to the next entry */` |
|         2 | 6963 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         2 | 6964 | `				nEntry--;` |
|       ! 0 | 6965 | `			}` |
|         - | 6966 | `		}` |
|         9 | 6967 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6968 | `			/* Int key */` |
|         7 | 6969 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6970 | `		}else{` |
|         - | 6971 | `			/* Blob key */` |
|         3 | 6972 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6973 | `		}` |
|         5 | 6974 | `	}else{` |
|         - | 6975 | `		ph7_value sKey,*pArray;` |
|         - | 6976 | `		ph7_hashmap *pDest;` |
|         - | 6977 | `		/* Create a new array */` |
|         5 | 6978 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 6979 | `		if( pArray == 0 ){` |
|       ! 0 | 6980 | `			ph7_result_null(pCtx);` |
|       ! 0 | 6981 | `			return PH7_OK;` |
|         - | 6982 | `		}` |
|         - | 6983 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 6984 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 6985 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 6986 | `		/* Copy the first n items */` |
|         5 | 6987 | `		pNode = pMap->pFirst;` |
|         5 | 6988 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 6989 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 6990 | `		}` |
|        15 | 6991 | `		while( nItem > 0){` |
|        11 | 6992 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 6993 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 6994 | `			PH7_MemObjRelease(&sKey);` |
|         - | 6995 | `			/* Point to the next entry */` |
|        11 | 6996 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 6997 | `			nItem--;` |
|         1 | 6998 | `		}` |
|         - | 6999 | `		/* Shuffle the array */` |
|         5 | 7000 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7001 | `		/* Rehash node */` |
|         5 | 7002 | `		HashmapSortRehash(pDest);` |
|         - | 7003 | `		/* Return the random array */` |
|         5 | 7004 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7005 | `	}` |
|        13 | 7006 | `	return PH7_OK;` |
|        22 | 7007 | `}` |
|         - | 7008 | `/*` |
|         - | 7009 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7010 | ` *  Split an array into chunks.` |
|         - | 7011 | ` * Parameters` |
|         - | 7012 | ` * $input` |
|         - | 7013 | ` *   The array to work on` |
|         - | 7014 | ` * $size` |
|         - | 7015 | ` *   The size of each chunk` |
|         - | 7016 | ` * $preserve_keys` |
|         - | 7017 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7018 | ` *   the chunk numerically.` |
|         - | 7019 | ` * Return` |
|         - | 7020 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7021 | ` *  zero, with each dimension containing size elements.` |
|         - | 7022 | ` */` |
|        42 | 7023 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7024 | `{` |
|         - | 7025 | `	ph7_value *pArray,*pChunk;` |
|         - | 7026 | `	ph7_hashmap_node *pEntry;` |
|         - | 7027 | `	ph7_hashmap *pMap;` |
|         - | 7028 | `	int bPreserve;` |
|         - | 7029 | `	sxu32 nChunk;` |
|         - | 7030 | `	sxu32 nSize;` |
|         - | 7031 | `	sxu32 n;` |
|         - | 7032 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 7033 | `	if( nArg < 2 ){` |
|         - | 7034 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 7035 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7036 | `			"ArgumentCountError",` |
|         - | 7037 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 7038 | `			nArg` |
|         - | 7039 | `			);` |
|         - | 7040 | `	}` |
|        45 | 7041 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7042 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7043 | `			"TypeError",` |
|         - | 7044 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7045 | `			ph7_type_name(apArg[0])` |
|         - | 7046 | `			);` |
|         - | 7047 | `	}` |
|         - | 7048 | `	/* Create a new array */` |
|        43 | 7049 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 7050 | `	if( pArray == 0 ){` |
|       ! 0 | 7051 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7052 | `		return PH7_OK;` |
|         - | 7053 | `	}` |
|         - | 7054 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 7055 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7056 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7057 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 7058 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 7059 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 7060 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7061 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7062 | `			"TypeError",` |
|         - | 7063 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7064 | `			ph7_type_name(apArg[1])` |
|         - | 7065 | `			);` |
|         - | 7066 | `	}` |
|         - | 7067 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7068 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7069 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 7070 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7071 | `		int len;` |
|         3 | 7072 | `		sxu8 bReal = FALSE;` |
|         3 | 7073 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7074 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7075 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7076 | `				"TypeError",` |
|         - | 7077 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7078 | `				);` |
|         - | 7079 | `		}` |
|       ! 0 | 7080 | `		if( bReal ){` |
|         - | 7081 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7082 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7083 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7084 | `				zStr` |
|         - | 7085 | `				);` |
|       ! 0 | 7086 | `		}` |
|       ! 0 | 7087 | `	}` |
|         - | 7088 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7089 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7090 | `	 * later via ph7_value_to_int. */` |
|        40 | 7091 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7092 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7093 | `		sxi64 i = (sxi64)d;` |
|         3 | 7094 | `		if( d != (double)i ){` |
|         4 | 7095 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7096 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7097 | `				d` |
|         - | 7098 | `				);` |
|         1 | 7099 | `		}` |
|         1 | 7100 | `	}` |
|         - | 7101 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7102 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7103 | `	{` |
|        40 | 7104 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 7105 | `		if( nSizeSigned < 1 ){` |
|         - | 7106 | `			/* size <= 0 -> ValueError */` |
|         6 | 7107 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7108 | `				"ValueError",` |
|         - | 7109 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7110 | `				);` |
|         - | 7111 | `		}` |
|        35 | 7112 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7113 | `	}` |
|        35 | 7114 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7115 | `		/* Return the whole array */` |
|         3 | 7116 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7117 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7118 | `		return PH7_OK;` |
|         - | 7119 | `	}` |
|        33 | 7120 | `	bPreserve = 0;` |
|        33 | 7121 | `	if( nArg > 2 ){` |
|         - | 7122 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7123 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7124 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7125 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7126 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7127 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7128 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7129 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7130 | `				"TypeError",` |
|         - | 7131 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7132 | `				ph7_type_name(apArg[2])` |
|         - | 7133 | `				);` |
|         - | 7134 | `		}` |
|        21 | 7135 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7136 | `	}` |
|         - | 7137 | `	/* Start processing */` |
|        27 | 7138 | `	pEntry = pMap->pFirst;` |
|        27 | 7139 | `	nChunk = 0;` |
|        27 | 7140 | `	pChunk = 0;` |
|        27 | 7141 | `	n = pMap->nEntry;` |
|        56 | 7142 | `	for( ;; ){` |
|       113 | 7143 | `		if( n < 1 ){` |
|         - | 7144 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7145 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7146 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7147 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7148 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7149 | `			 * exists. */` |
|        27 | 7150 | `			if( pChunk ){` |
|        27 | 7151 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7152 | `			}` |
|        27 | 7153 | `			break;` |
|         - | 7154 | `		}` |
|        87 | 7155 | `		if( nChunk < 1 ){` |
|        71 | 7156 | `			if( pChunk ){` |
|         - | 7157 | `				/* Put the first chunk */` |
|        45 | 7158 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7159 | `			}` |
|         - | 7160 | `			/* Create a new dimension */` |
|        71 | 7161 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7162 | `												   * will be automatically released as soon we return` |
|         - | 7163 | `												   * from this function */` |
|        71 | 7164 | `			if( pChunk == 0 ){` |
|       ! 0 | 7165 | `				break;` |
|         - | 7166 | `			}` |
|        71 | 7167 | `			nChunk = nSize;` |
|        35 | 7168 | `		}` |
|         - | 7169 | `		/* Insert the entry */` |
|        87 | 7170 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7171 | `		/* Point to the next entry */` |
|        87 | 7172 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7173 | `		nChunk--;` |
|        87 | 7174 | `		n--;` |
|         1 | 7175 | `	}` |
|         - | 7176 | `	/* Return the multidimensional array */` |
|        27 | 7177 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7178 | `	return PH7_OK;` |
|        26 | 7179 | `}` |
|         - | 7180 | `/*` |
|         - | 7181 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7182 | ` *  Pad array to the specified length with a value.` |
|         - | 7183 | ` * $input` |
|         - | 7184 | ` *   Initial array of values to pad.` |
|         - | 7185 | ` * $pad_size` |
|         - | 7186 | ` *   New size of the array.` |
|         - | 7187 | ` * $pad_value` |
|         - | 7188 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7189 | ` */` |
|         - | 7190 | `/*` |
|         - | 7191 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7192 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7193 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7194 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7195 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7196 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7197 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7198 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7199 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7200 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7201 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7202 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7203 | ` */` |
|        50 | 7204 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7205 | `	ph7_context *pCtx,` |
|         - | 7206 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7207 | `	int iArg,              /* 1-based argument position */` |
|         - | 7208 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7209 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7210 | `	)` |
|         1 | 7211 | `{` |
|        51 | 7212 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7213 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7214 | `			"ValueError",` |
|         - | 7215 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7216 | `			zFunc,iArg,zParam` |
|         - | 7217 | `			);` |
|         - | 7218 | `	}` |
|        37 | 7219 | `	return SXRET_OK;` |
|        26 | 7220 | `}` |
|        72 | 7221 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7222 | `{` |
|         - | 7223 | `	ph7_hashmap *pMap;` |
|         - | 7224 | `	ph7_value *pArray;` |
|         - | 7225 | `	sxi64 iLen,iAbs;` |
|         - | 7226 | `	int nEntry;` |
|         - | 7227 | `	sxi32 rc;` |
|        77 | 7228 | `	if( nArg != 3 ){` |
|        12 | 7229 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7230 | `			"ArgumentCountError",` |
|         - | 7231 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7232 | `			nArg` |
|         - | 7233 | `			);` |
|         - | 7234 | `	}` |
|        68 | 7235 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7236 | `		char zBuf[64];` |
|        14 | 7237 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7238 | `			"TypeError",` |
|         - | 7239 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7240 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7241 | `			);` |
|         - | 7242 | `	}` |
|         - | 7243 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7244 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7245 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7246 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        58 | 7247 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        56 | 7248 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7249 | `		char zBuf[64];` |
|         7 | 7250 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7251 | `			"TypeError",` |
|         - | 7252 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7253 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7254 | `			);` |
|         - | 7255 | `	}` |
|        55 | 7256 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7257 | `		int nStr;` |
|        11 | 7258 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7259 | `		sxi64 iLong; double dReal;` |
|        11 | 7260 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7261 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7262 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7263 | `				"TypeError",` |
|         - | 7264 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7265 | `				);` |
|         - | 7266 | `		}` |
|         7 | 7267 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7268 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7269 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7270 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7271 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7272 | `					"TypeError",` |
|         - | 7273 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7274 | `					);` |
|         - | 7275 | `			}` |
|         3 | 7276 | `			iLen = (sxi64)dReal;` |
|         3 | 7277 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7278 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7279 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7280 | `					zStr` |
|         - | 7281 | `					);` |
|       ! 0 | 7282 | `			}` |
|         2 | 7283 | `		}else{` |
|         5 | 7284 | `			iLen = iLong;` |
|         - | 7285 | `		}` |
|         4 | 7286 | `	}else{` |
|        45 | 7287 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7288 | `	}` |
|         - | 7289 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7290 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7291 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7292 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7293 | `	 * overflow). */` |
|        51 | 7294 | `	iAbs = iLen;` |
|        51 | 7295 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7296 | `		iAbs = -iAbs;` |
|         7 | 7297 | `	}` |
|        51 | 7298 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7299 | `	if( rc != SXRET_OK ){` |
|        15 | 7300 | `		return rc;` |
|         - | 7301 | `	}` |
|        37 | 7302 | `	nEntry = (int)iLen;` |
|         - | 7303 | `	/* Create a new array */` |
|        37 | 7304 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7305 | `	if( pArray == 0 ){` |
|       ! 0 | 7306 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7307 | `	}` |
|        37 | 7308 | `	if( nEntry < 0 ){` |
|        11 | 7309 | `		nEntry = -nEntry;` |
|        11 | 7310 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7311 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7312 | `			/* Insert given items first */` |
|        25 | 7313 | `			while( nEntry > 0 ){` |
|        19 | 7314 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7315 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7316 | `				}` |
|        19 | 7317 | `				nEntry--;` |
|         1 | 7318 | `			}` |
|         - | 7319 | `			/* Merge the two arrays */` |
|         7 | 7320 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7321 | `		}else{` |
|         5 | 7322 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7323 | `		}` |
|        32 | 7324 | `	}else if( nEntry > 0 ){` |
|        25 | 7325 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7326 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7327 | `			/* Merge the two arrays first */` |
|        19 | 7328 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7329 | `			/* Insert given items */` |
|       275 | 7330 | `			while( nEntry > 0 ){` |
|       257 | 7331 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7332 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7333 | `				}` |
|       257 | 7334 | `				nEntry--;` |
|         1 | 7335 | `			}` |
|        10 | 7336 | `		}else{` |
|         7 | 7337 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7338 | `		}` |
|        13 | 7339 | `	}else{` |
|         - | 7340 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7341 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7342 | `	}` |
|         - | 7343 | `	/* Return the new array */` |
|        37 | 7344 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7345 | `	return PH7_OK;` |
|        41 | 7346 | `}` |
|         - | 7347 | `/*` |
|         - | 7348 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7349 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7350 | ` * Parameters` |
|         - | 7351 | ` * $array` |
|         - | 7352 | ` *   The array in which elements are replaced.` |
|         - | 7353 | ` * $array1` |
|         - | 7354 | ` *   The array from which elements will be extracted.` |
|         - | 7355 | ` * ....` |
|         - | 7356 | ` *  More arrays from which elements will be extracted.` |
|         - | 7357 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7358 | ` * Return` |
|         - | 7359 | ` *  Returns an array.` |
|         - | 7360 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7361 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7362 | ` */` |
|        22 | 7363 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7364 | `{` |
|         - | 7365 | `	ph7_hashmap *pMap;` |
|         - | 7366 | `	ph7_value *pArray;` |
|         - | 7367 | `	int i;` |
|        26 | 7368 | `	if( nArg < 1 ){` |
|         3 | 7369 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7370 | `			"ArgumentCountError",` |
|         - | 7371 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7372 | `			);` |
|         - | 7373 | `	}` |
|        23 | 7374 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7375 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7376 | `			"TypeError",` |
|         - | 7377 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7378 | `			ph7_type_name(apArg[0])` |
|         - | 7379 | `			);` |
|         - | 7380 | `	}` |
|         - | 7381 | `	/* Create a new array */` |
|        20 | 7382 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7383 | `	if( pArray == 0 ){` |
|       ! 0 | 7384 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7385 | `		return PH7_OK;` |
|         - | 7386 | `	}` |
|         - | 7387 | `	/* Overwrite from the first array */` |
|        20 | 7388 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7389 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7390 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7391 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7392 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7393 | `			/* Type mismatch -> TypeError */` |
|         4 | 7394 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7395 | `				"TypeError",` |
|         - | 7396 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7397 | `				i + 1,` |
|         2 | 7398 | `				ph7_type_name(apArg[i])` |
|         - | 7399 | `				);` |
|         - | 7400 | `		}` |
|         - | 7401 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7402 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7403 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7404 | `	}` |
|         - | 7405 | `	/* Return the new array */` |
|        17 | 7406 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7407 | `	return PH7_OK;` |
|        15 | 7408 | `}` |
|         - | 7409 | `/*` |
|         - | 7410 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7411 | ` *  Filters elements of an array using a callback function.` |
|         - | 7412 | ` * Parameters` |
|         - | 7413 | ` *  $input` |
|         - | 7414 | ` *    The array to iterate over` |
|         - | 7415 | ` * $callback` |
|         - | 7416 | ` *    The callback function to use` |
|         - | 7417 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7418 | ` *    will be removed.` |
|         - | 7419 | ` * Return` |
|         - | 7420 | ` *  The filtered array.` |
|         - | 7421 | ` */` |
|        32 | 7422 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7423 | `{` |
|         - | 7424 | `	ph7_hashmap_node *pEntry;` |
|         - | 7425 | `	ph7_hashmap *pMap;` |
|         - | 7426 | `	ph7_value *pArray;` |
|         - | 7427 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7428 | `	ph7_value *pValue;` |
|         - | 7429 | `	sxi32 rc;` |
|         - | 7430 | `	int keep;` |
|         - | 7431 | `	sxu32 n;` |
|        34 | 7432 | `	if( nArg < 1 ){` |
|         - | 7433 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7434 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7435 | `		return PH7_OK;` |
|         - | 7436 | `	}` |
|         - | 7437 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7438 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7439 | `		char zBuf[64];` |
|        22 | 7440 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7441 | `			"TypeError",` |
|         - | 7442 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7443 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7444 | `			);` |
|         - | 7445 | `	}` |
|         - | 7446 | `	/* Create a new array */` |
|        20 | 7447 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7448 | `	if( pArray == 0 ){` |
|       ! 0 | 7449 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7450 | `		return PH7_OK;` |
|         - | 7451 | `	}` |
|         - | 7452 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7453 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7454 | `	pEntry = pMap->pFirst;` |
|        20 | 7455 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7456 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7457 | `	/* Perform the requested operation */` |
|        78 | 7458 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7459 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7460 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7461 | `		if( pValue == 0 ){` |
|         - | 7462 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7463 | `			keep = FALSE;` |
|        64 | 7464 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7465 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7466 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7467 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7468 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7469 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7470 | `					int len;` |
|         3 | 7471 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7472 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7473 | `						"TypeError",` |
|         - | 7474 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7475 | `						zName` |
|         - | 7476 | `						);` |
|       ! 0 | 7477 | `				}else{` |
|       ! 0 | 7478 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7479 | `						"TypeError",` |
|         - | 7480 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7481 | `						ph7_type_name(apArg[1])` |
|         - | 7482 | `						);` |
|         - | 7483 | `				}` |
|         - | 7484 | `			}` |
|        33 | 7485 | `			keep = FALSE;` |
|        33 | 7486 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7487 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7488 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7489 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7490 | `				return PH7_EXCEPTION;` |
|         - | 7491 | `			}` |
|        31 | 7492 | `			if( rc == SXRET_OK ){` |
|         - | 7493 | `				/* Perform a boolean cast */` |
|        31 | 7494 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7495 | `			}` |
|        31 | 7496 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7497 | `		}else{` |
|         - | 7498 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7499 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7500 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7501 | `			 */` |
|        29 | 7502 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7503 | `		}` |
|        59 | 7504 | `		if( keep ){` |
|         - | 7505 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7506 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7507 | `		}` |
|         - | 7508 | `		/* Point to the next entry */` |
|        59 | 7509 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7510 | `	}` |
|        15 | 7511 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7512 | `	return PH7_OK;` |
|        18 | 7513 | `}` |
|         - | 7514 | `/*` |
|         - | 7515 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7516 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7517 | ` * Parameters` |
|         - | 7518 | ` *  $callback` |
|         - | 7519 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7520 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7521 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7522 | ` *   are zipped together.` |
|         - | 7523 | ` *  $array` |
|         - | 7524 | ` *   The first array to run through the callback function.` |
|         - | 7525 | ` *  $arrays` |
|         - | 7526 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7527 | ` * Return` |
|         - | 7528 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7529 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7530 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7531 | ` *  padding shorter arrays with NULL.` |
|         - | 7532 | ` */` |
|        62 | 7533 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7534 | `{` |
|         - | 7535 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7536 | `	ph7_hashmap_node *pEntry;` |
|         - | 7537 | `	ph7_hashmap *pMap;` |
|         - | 7538 | `	ph7_vm *pVm;` |
|         - | 7539 | `	int bNullCallback;` |
|         - | 7540 | `	sxi32 rc;` |
|         - | 7541 | `	int i;` |
|         - | 7542 | `	sxu32 n;` |
|        67 | 7543 | `	if( nArg < 2 ){` |
|         8 | 7544 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7545 | `			"ArgumentCountError",` |
|         - | 7546 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7547 | `			nArg` |
|         - | 7548 | `			);` |
|         - | 7549 | `	}` |
|        62 | 7550 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        62 | 7551 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7552 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7553 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7554 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7555 | `				"TypeError",` |
|         - | 7556 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7557 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7558 | `				zFunc` |
|         - | 7559 | `				);` |
|         - | 7560 | `		}` |
|         3 | 7561 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7562 | `			"TypeError",` |
|         - | 7563 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7564 | `			"no array or string given"` |
|         - | 7565 | `			);` |
|         - | 7566 | `	}` |
|         - | 7567 | `	/* Every remaining argument must be an array */` |
|       121 | 7568 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7569 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7570 | `			if( i == 1 ){` |
|         4 | 7571 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7572 | `					"TypeError",` |
|         - | 7573 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7574 | `					ph7_type_name(apArg[1])` |
|         - | 7575 | `					);` |
|         - | 7576 | `			}` |
|       ! 0 | 7577 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7578 | `				"TypeError",` |
|         - | 7579 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7580 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7581 | `				);` |
|         - | 7582 | `		}` |
|        34 | 7583 | `	}` |
|        54 | 7584 | `	pVm = pCtx->pVm;` |
|         - | 7585 | `	/* Create a new array */` |
|        54 | 7586 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7587 | `	if( pArray == 0 ){` |
|       ! 0 | 7588 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7589 | `		return PH7_OK;` |
|         - | 7590 | `	}` |
|        54 | 7591 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7592 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7593 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7594 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7595 | `	if( nArg == 2 ){` |
|         - | 7596 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7597 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7598 | `		pEntry = pMap->pFirst;` |
|       134 | 7599 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7600 | `			/* Extract the node value */` |
|        96 | 7601 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7602 | `			if( pValue ){` |
|         - | 7603 | `				/* Extract the node key */` |
|        96 | 7604 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7605 | `				if( bNullCallback ){` |
|         - | 7606 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7607 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7608 | `				}else{` |
|         - | 7609 | `					/* Invoke the supplied callback */` |
|        86 | 7610 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7611 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7612 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7613 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7614 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7615 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7616 | `						return PH7_EXCEPTION;` |
|         - | 7617 | `					}` |
|         - | 7618 | `					/* Insert the callback return value */` |
|        82 | 7619 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7620 | `				}` |
|        92 | 7621 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7622 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7623 | `			}` |
|         - | 7624 | `			/* Point to the next entry */` |
|        92 | 7625 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7626 | `		}` |
|        21 | 7627 | `	}else{` |
|         - | 7628 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7629 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7630 | `		int nArrays = nArg - 1;` |
|         - | 7631 | `		ph7_hashmap_node **apCur;` |
|         - | 7632 | `		ph7_value **apCallArg;` |
|         - | 7633 | `		ph7_value sNull;` |
|        11 | 7634 | `		sxu32 nMax = 0;` |
|        11 | 7635 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7636 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7637 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7638 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7639 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7640 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7641 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7642 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7643 | `			return PH7_OK;` |
|         - | 7644 | `		}` |
|        11 | 7645 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7646 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7647 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7648 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7649 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7650 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7651 | `				nMax = pMap->nEntry;` |
|         6 | 7652 | `			}` |
|        12 | 7653 | `		}` |
|        35 | 7654 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7655 | `			ph7_value *pZip = 0;` |
|        25 | 7656 | `			if( bNullCallback ){` |
|         - | 7657 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7658 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7659 | `			}` |
|        79 | 7660 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7661 | `				ph7_value *pv = &sNull;` |
|        55 | 7662 | `				if( apCur[i] ){` |
|        53 | 7663 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7664 | `					if( pNodeVal ){` |
|        53 | 7665 | `						pv = pNodeVal;` |
|        26 | 7666 | `					}` |
|        53 | 7667 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7668 | `				}` |
|        55 | 7669 | `				if( bNullCallback ){` |
|         9 | 7670 | `					if( pZip ){` |
|         9 | 7671 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7672 | `					}` |
|         5 | 7673 | `				}else{` |
|        47 | 7674 | `					apCallArg[i] = pv;` |
|         - | 7675 | `				}` |
|        28 | 7676 | `			}` |
|        25 | 7677 | `			if( bNullCallback ){` |
|         5 | 7678 | `				if( pZip ){` |
|         5 | 7679 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7680 | `				}` |
|         3 | 7681 | `			}else{` |
|        21 | 7682 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7683 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7684 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7685 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7686 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7687 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7688 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7689 | `					return PH7_EXCEPTION;` |
|         - | 7690 | `				}` |
|        21 | 7691 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7692 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7693 | `			}` |
|        13 | 7694 | `		}` |
|        11 | 7695 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7696 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7697 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7698 | `	}` |
|        50 | 7699 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7700 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7701 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7702 | `	return PH7_OK;` |
|        36 | 7703 | `}` |
|         - | 7704 | `/*` |
|         - | 7705 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7706 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7707 | ` * Parameters` |
|         - | 7708 | ` *  $array` |
|         - | 7709 | ` *   The input array.` |
|         - | 7710 | ` *  $callback` |
|         - | 7711 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7712 | ` *  $initial` |
|         - | 7713 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7714 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7715 | ` * Return` |
|         - | 7716 | ` *  Returns the resulting value.` |
|         - | 7717 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7718 | ` */` |
|        34 | 7719 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7720 | `{` |
|         - | 7721 | `	ph7_hashmap_node *pEntry;` |
|         - | 7722 | `	ph7_hashmap *pMap;` |
|         - | 7723 | `	ph7_value *pValue;` |
|         - | 7724 | `	ph7_value sResult;` |
|         - | 7725 | `	sxi32 rc;` |
|         - | 7726 | `	sxu32 n;` |
|        39 | 7727 | `	if( nArg < 2 ){` |
|         8 | 7728 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7729 | `			"ArgumentCountError",` |
|         - | 7730 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7731 | `			nArg` |
|         - | 7732 | `			);` |
|         - | 7733 | `	}` |
|        35 | 7734 | `	if( nArg > 3 ){` |
|         4 | 7735 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7736 | `			"ArgumentCountError",` |
|         - | 7737 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7738 | `			nArg` |
|         - | 7739 | `			);` |
|         - | 7740 | `	}` |
|        33 | 7741 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7742 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7743 | `			"TypeError",` |
|         - | 7744 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7745 | `			ph7_type_name(apArg[0])` |
|         - | 7746 | `			);` |
|         - | 7747 | `	}` |
|        31 | 7748 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7749 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7750 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7751 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7752 | `				"TypeError",` |
|         - | 7753 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7754 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7755 | `				zFunc` |
|         - | 7756 | `				);` |
|         - | 7757 | `		}` |
|         9 | 7758 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7759 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7760 | `				"TypeError",` |
|         - | 7761 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7762 | `				"array callback must have exactly two members"` |
|         - | 7763 | `				);` |
|         - | 7764 | `		}` |
|         6 | 7765 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7766 | `			"TypeError",` |
|         - | 7767 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7768 | `			"no array or string given"` |
|         - | 7769 | `			);` |
|         - | 7770 | `	}` |
|         - | 7771 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7772 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7773 | `	/* Assume a NULL initial value */` |
|        19 | 7774 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7775 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7776 | `	if( nArg > 2 ){` |
|         - | 7777 | `		/* Set the initial value */` |
|        13 | 7778 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7779 | `	}` |
|         - | 7780 | `	/* Perform the requested operation */` |
|        19 | 7781 | `	pEntry = pMap->pFirst;` |
|        55 | 7782 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7783 | `		/* Extract the node value */` |
|        39 | 7784 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7785 | `		/* Invoke the supplied callback */` |
|        39 | 7786 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7787 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7788 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7789 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7790 | `			return PH7_EXCEPTION;` |
|         - | 7791 | `		}` |
|         - | 7792 | `		/* Point to the next entry */` |
|        37 | 7793 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7794 | `	}` |
|        17 | 7795 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7796 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7797 | `	return PH7_OK;` |
|        22 | 7798 | `}` |
|         - | 7799 | `/*` |
|         - | 7800 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7801 | ` *  Apply a user function to every member of an array.` |
|         - | 7802 | ` * Parameters` |
|         - | 7803 | ` *  $array` |
|         - | 7804 | ` *   The input array.` |
|         - | 7805 | ` *  $funcname` |
|         - | 7806 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7807 | ` *   the first, and the key/index second.` |
|         - | 7808 | ` * Note:` |
|         - | 7809 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7810 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7811 | ` *  be made in the original array itself.` |
|         - | 7812 | ` *  $userdata` |
|         - | 7813 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7814 | ` *   to the callback funcname.` |
|         - | 7815 | ` * Return` |
|         - | 7816 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7817 | ` */` |
|        38 | 7818 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7819 | `{` |
|         - | 7820 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7821 | `	ph7_hashmap_node *pEntry;` |
|         - | 7822 | `	ph7_hashmap *pMap;` |
|         - | 7823 | `	sxu32 n;` |
|        43 | 7824 | `	if( nArg < 2 ){` |
|         8 | 7825 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7826 | `			"ArgumentCountError",` |
|         - | 7827 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7828 | `			nArg` |
|         - | 7829 | `			);` |
|         - | 7830 | `	}` |
|        39 | 7831 | `	if( nArg > 3 ){` |
|         4 | 7832 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7833 | `			"ArgumentCountError",` |
|         - | 7834 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7835 | `			nArg` |
|         - | 7836 | `			);` |
|         - | 7837 | `	}` |
|        37 | 7838 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7839 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7840 | `			"TypeError",` |
|         - | 7841 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7842 | `			ph7_type_name(apArg[0])` |
|         - | 7843 | `			);` |
|         - | 7844 | `	}` |
|        35 | 7845 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7846 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7847 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7848 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7849 | `				"TypeError",` |
|         - | 7850 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7851 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7852 | `				zFunc` |
|         - | 7853 | `				);` |
|         - | 7854 | `		}` |
|        12 | 7855 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7856 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7857 | `				"TypeError",` |
|         - | 7858 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7859 | `				"array callback must have exactly two members"` |
|         - | 7860 | `				);` |
|         - | 7861 | `		}` |
|         6 | 7862 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7863 | `			"TypeError",` |
|         - | 7864 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7865 | `			"no array or string given"` |
|         - | 7866 | `			);` |
|         - | 7867 | `	}` |
|        21 | 7868 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7869 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7870 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7871 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7872 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7873 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7874 | `	/* Perform the desired operation */` |
|        21 | 7875 | `	pEntry = pMap->pFirst;` |
|        61 | 7876 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7877 | `		/* Extract the node value */` |
|        43 | 7878 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7879 | `		if( pValue ){` |
|         - | 7880 | `			sxi32 rcW;` |
|         - | 7881 | `			/* Extract the entry key */` |
|        43 | 7882 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7883 | `			/* Invoke the supplied callback */` |
|        43 | 7884 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7885 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7886 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7887 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7888 | `				return PH7_EXCEPTION;` |
|         - | 7889 | `			}` |
|        20 | 7890 | `		}` |
|         - | 7891 | `		/* Point to the next entry */` |
|        41 | 7892 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7893 | `	}` |
|         - | 7894 | `	/* All done, return TRUE */` |
|        19 | 7895 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7896 | `	return PH7_OK;` |
|        24 | 7897 | `}` |
|         - | 7898 | `/*` |
|         - | 7899 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7900 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7901 | ` */` |
|        22 | 7902 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7903 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7904 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7905 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7906 | `	int iNest             /* Nesting level */` |
|         - | 7907 | `	)` |
|         1 | 7908 | `{` |
|         - | 7909 | `	ph7_hashmap_node *pEntry;` |
|         - | 7910 | `	ph7_value *pValue,sKey;` |
|         - | 7911 | `	sxi32 rc;` |
|         - | 7912 | `	sxu32 n;` |
|         - | 7913 | `	/* Iterate through hashmap entries */` |
|        23 | 7914 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7915 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7916 | `	pEntry = pMap->pFirst;` |
|        59 | 7917 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7918 | `		/* Extract the node value */` |
|        37 | 7919 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7920 | `		if( pValue ){` |
|        37 | 7921 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7922 | `				if( iNest < 32 ){` |
|         - | 7923 | `					/* Recurse */` |
|        11 | 7924 | `					iNest++;` |
|        11 | 7925 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7926 | `					iNest--;` |
|        11 | 7927 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7928 | `						return PH7_EXCEPTION;` |
|         - | 7929 | `					}` |
|         5 | 7930 | `				}` |
|         6 | 7931 | `			}else{` |
|         - | 7932 | `				/* Extract the node key */` |
|        27 | 7933 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7934 | `				/* Invoke the supplied callback */` |
|        27 | 7935 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7936 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7937 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7938 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7939 | `					return PH7_EXCEPTION;` |
|         - | 7940 | `				}` |
|         - | 7941 | `			}` |
|        18 | 7942 | `		}` |
|         - | 7943 | `		/* Point to the next entry */` |
|        37 | 7944 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7945 | `	}` |
|        23 | 7946 | `	return PH7_OK;` |
|        12 | 7947 | `}` |
|         - | 7948 | `/*` |
|         - | 7949 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7950 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7951 | ` * Parameters` |
|         - | 7952 | ` *  $array` |
|         - | 7953 | ` *   The input array.` |
|         - | 7954 | ` *  $funcname` |
|         - | 7955 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7956 | ` *   the first, and the key/index second.` |
|         - | 7957 | ` * Note:` |
|         - | 7958 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7959 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7960 | ` *  be made in the original array itself.` |
|         - | 7961 | ` *  $userdata` |
|         - | 7962 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7963 | ` *   to the callback funcname.` |
|         - | 7964 | ` * Return` |
|         - | 7965 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7966 | ` */` |
|        30 | 7967 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7968 | `{` |
|         - | 7969 | `	ph7_hashmap *pMap;` |
|        35 | 7970 | `	if( nArg < 2 ){` |
|         8 | 7971 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7972 | `			"ArgumentCountError",` |
|         - | 7973 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7974 | `			nArg` |
|         - | 7975 | `			);` |
|         - | 7976 | `	}` |
|        31 | 7977 | `	if( nArg > 3 ){` |
|         4 | 7978 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7979 | `			"ArgumentCountError",` |
|         - | 7980 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 7981 | `			nArg` |
|         - | 7982 | `			);` |
|         - | 7983 | `	}` |
|        29 | 7984 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7985 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7986 | `			"TypeError",` |
|         - | 7987 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7988 | `			ph7_type_name(apArg[0])` |
|         - | 7989 | `			);` |
|         - | 7990 | `	}` |
|        27 | 7991 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7992 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7993 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7994 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7995 | `				"TypeError",` |
|         - | 7996 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7997 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7998 | `				zFunc` |
|         - | 7999 | `				);` |
|         - | 8000 | `		}` |
|        12 | 8001 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8002 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8003 | `				"TypeError",` |
|         - | 8004 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8005 | `				"array callback must have exactly two members"` |
|         - | 8006 | `				);` |
|         - | 8007 | `		}` |
|         6 | 8008 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8009 | `			"TypeError",` |
|         - | 8010 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8011 | `			"no array or string given"` |
|         - | 8012 | `			);` |
|         - | 8013 | `	}` |
|         - | 8014 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8015 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8016 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8017 | `	/* Perform the desired operation */` |
|        13 | 8018 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8019 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8020 | `		return PH7_EXCEPTION;` |
|         - | 8021 | `	}` |
|         - | 8022 | `	/* All done, return TRUE */` |
|        13 | 8023 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8024 | `	return PH7_OK;` |
|        20 | 8025 | `}` |
|         - | 8026 | `/*` |
|         - | 8027 | ` * bool array_is_list(array $array)` |
|         - | 8028 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8029 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8030 | ` * Return` |
|         - | 8031 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8032 | ` */` |
|         - | 8033 | `/*` |
|         - | 8034 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8035 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8036 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8037 | ` */` |
|       242 | 8038 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8039 | `{` |
|       243 | 8040 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       243 | 8041 | `	sxi64 iExpect = 0;` |
|         - | 8042 | `	sxu32 n;` |
|       545 | 8043 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       403 | 8044 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8045 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8046 | `			return 0;` |
|         - | 8047 | `		}` |
|       303 | 8048 | `		++iExpect;` |
|       303 | 8049 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       152 | 8050 | `	}` |
|       143 | 8051 | `	return 1;` |
|       122 | 8052 | `}` |
|        12 | 8053 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8054 | `{` |
|        13 | 8055 | `	if( nArg < 1 ){` |
|       ! 0 | 8056 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8057 | `			"ArgumentCountError",` |
|         - | 8058 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8059 | `			);` |
|         - | 8060 | `	}` |
|        13 | 8061 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8062 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8063 | `			"TypeError",` |
|         - | 8064 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8065 | `			ph7_type_name(apArg[0])` |
|         - | 8066 | `			);` |
|         - | 8067 | `	}` |
|        13 | 8068 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8069 | `	return PH7_OK;` |
|         7 | 8070 | `}` |
|         - | 8071 | `/*` |
|         - | 8072 | ` * mixed array_first(array $array)` |
|         - | 8073 | ` * mixed array_last(array $array)` |
|         - | 8074 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8075 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8076 | ` *  untouched (unlike reset()/end()).` |
|         - | 8077 | ` */` |
|        20 | 8078 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8079 | `{` |
|         - | 8080 | `	ph7_hashmap *pMap;` |
|         - | 8081 | `	ph7_hashmap_node *pNode;` |
|         - | 8082 | `	ph7_value *pVal;` |
|        21 | 8083 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 8084 | `	if( nArg < 1 ){` |
|         4 | 8085 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8086 | `			"ArgumentCountError",` |
|         - | 8087 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8088 | `			zName` |
|         - | 8089 | `			);` |
|         - | 8090 | `	}` |
|        19 | 8091 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8092 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8093 | `			"TypeError",` |
|         - | 8094 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8095 | `			zName,` |
|         1 | 8096 | `			ph7_type_name(apArg[0])` |
|         - | 8097 | `			);` |
|         - | 8098 | `	}` |
|        17 | 8099 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8100 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8101 | `	if( pNode == 0 ){` |
|         - | 8102 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8103 | `		ph7_result_null(pCtx);` |
|         5 | 8104 | `		return PH7_OK;` |
|         - | 8105 | `	}` |
|        13 | 8106 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8107 | `	if( pVal ){` |
|        13 | 8108 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8109 | `	}else{` |
|       ! 0 | 8110 | `		ph7_result_null(pCtx);` |
|         - | 8111 | `	}` |
|        13 | 8112 | `	return PH7_OK;` |
|        11 | 8113 | `}` |
|        10 | 8114 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8115 | `{` |
|        11 | 8116 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8117 | `}` |
|        10 | 8118 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8119 | `{` |
|        11 | 8120 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8121 | `}` |
|         - | 8122 | `/*` |
|         - | 8123 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8124 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8125 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8126 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8127 | ` *  untouched.` |
|         - | 8128 | ` */` |
|        24 | 8129 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8130 | `{` |
|         - | 8131 | `	ph7_hashmap *pMap;` |
|         - | 8132 | `	ph7_hashmap_node *pNode;` |
|        25 | 8133 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        25 | 8134 | `	if( nArg < 1 ){` |
|         4 | 8135 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8136 | `			"ArgumentCountError",` |
|         - | 8137 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8138 | `			zName` |
|         - | 8139 | `			);` |
|         - | 8140 | `	}` |
|        23 | 8141 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8142 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8143 | `			"TypeError",` |
|         - | 8144 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8145 | `			zName,` |
|         1 | 8146 | `			ph7_type_name(apArg[0])` |
|         - | 8147 | `			);` |
|         - | 8148 | `	}` |
|        21 | 8149 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8150 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8151 | `	if( pNode == 0 ){` |
|         - | 8152 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8153 | `		ph7_result_null(pCtx);` |
|         5 | 8154 | `		return PH7_OK;` |
|         - | 8155 | `	}` |
|        17 | 8156 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8157 | `	return PH7_OK;` |
|        13 | 8158 | `}` |
|        12 | 8159 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8160 | `{` |
|        13 | 8161 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8162 | `}` |
|        12 | 8163 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8164 | `{` |
|        13 | 8165 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8166 | `}` |
|         - | 8167 | `/*` |
|         - | 8168 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8169 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8170 | ` * array_column() for both the column value and the index key.` |
|         - | 8171 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8172 | ` * container or the key is absent.` |
|         - | 8173 | ` */` |
|        32 | 8174 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8175 | `{` |
|        33 | 8176 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8177 | `		ph7_hashmap_node *pNode;` |
|        25 | 8178 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8179 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8180 | `		}` |
|        11 | 8181 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8182 | `		ph7_value sName;` |
|         - | 8183 | `		const char *zName;` |
|         - | 8184 | `		ph7_value *pAttr;` |
|         - | 8185 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8186 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8187 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8188 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8189 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8190 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8191 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8192 | `		return pAttr;` |
|         - | 8193 | `	}` |
|         5 | 8194 | `	return 0;` |
|        17 | 8195 | `}` |
|         - | 8196 | `/*` |
|         - | 8197 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8198 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8199 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8200 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8201 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8202 | ` *  Each row may be an array or an object.` |
|         - | 8203 | ` */` |
|        12 | 8204 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8205 | `{` |
|         - | 8206 | `	ph7_hashmap_node *pNode;` |
|         - | 8207 | `	ph7_hashmap *pMap;` |
|         - | 8208 | `	ph7_value *pArray;` |
|         - | 8209 | `	ph7_value *pRow;` |
|         - | 8210 | `	ph7_value *pCol;` |
|         - | 8211 | `	ph7_value *pIdx;` |
|         - | 8212 | `	int bWantCol;` |
|         - | 8213 | `	int bWantIdx;` |
|         - | 8214 | `	sxu32 n;` |
|        13 | 8215 | `	if( nArg < 2 ){` |
|       ! 0 | 8216 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8217 | `			"ArgumentCountError",` |
|         - | 8218 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8219 | `			nArg` |
|         - | 8220 | `			);` |
|         - | 8221 | `	}` |
|        13 | 8222 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8224 | `			"TypeError",` |
|         - | 8225 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8226 | `			ph7_type_name(apArg[0])` |
|         - | 8227 | `			);` |
|         - | 8228 | `	}` |
|        13 | 8229 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8230 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8231 | `	if( pArray == 0 ){` |
|       ! 0 | 8232 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8233 | `		return PH7_OK;` |
|         - | 8234 | `	}` |
|         - | 8235 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8236 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8237 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8238 | `	pNode = pMap->pFirst;` |
|        33 | 8239 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8240 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8241 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8242 | `		if( pRow == 0 ){` |
|       ! 0 | 8243 | `			continue;` |
|         - | 8244 | `		}` |
|        21 | 8245 | `		if( bWantCol ){` |
|        19 | 8246 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8247 | `			if( pCol == 0 ){` |
|         - | 8248 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8249 | `				continue;` |
|         - | 8250 | `			}` |
|         9 | 8251 | `		}else{` |
|         3 | 8252 | `			pCol = pRow;` |
|         - | 8253 | `		}` |
|        19 | 8254 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8255 | `		if( pIdx ){` |
|        13 | 8256 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8257 | `		}else{` |
|         7 | 8258 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8259 | `		}` |
|        10 | 8260 | `	}` |
|        13 | 8261 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8262 | `	return PH7_OK;` |
|         7 | 8263 | `}` |
|         - | 8264 | `/*` |
|         - | 8265 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8266 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8267 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8268 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8269 | ` */` |
|        28 | 8270 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8271 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8272 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8273 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8274 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8275 | `	)` |
|         1 | 8276 | `{` |
|         - | 8277 | `	ph7_hashmap_node *pEntry;` |
|         - | 8278 | `	ph7_hashmap *pMap;` |
|         - | 8279 | `	ph7_value *pValue;` |
|         - | 8280 | `	ph7_value *apCbArg[2];` |
|         - | 8281 | `	ph7_value sKey;` |
|         - | 8282 | `	ph7_value sResult;` |
|         - | 8283 | `	sxi32 rc;` |
|         - | 8284 | `	sxu32 n;` |
|        29 | 8285 | `	*ppMatch = 0;` |
|        29 | 8286 | `	if( nArg < 2 ){` |
|       ! 0 | 8287 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8288 | `			"ArgumentCountError",` |
|         - | 8289 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8290 | `			zName,nArg` |
|         - | 8291 | `			);` |
|         - | 8292 | `	}` |
|        29 | 8293 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8294 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8295 | `			"TypeError",` |
|         - | 8296 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8297 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8298 | `			);` |
|         - | 8299 | `	}` |
|        29 | 8300 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8301 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8302 | `			"TypeError",` |
|         - | 8303 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8304 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8305 | `			);` |
|         - | 8306 | `	}` |
|        29 | 8307 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8308 | `	pEntry = pMap->pFirst;` |
|        29 | 8309 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8310 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8311 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8312 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8313 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8314 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8315 | `		if( pValue ){` |
|         - | 8316 | `			/* The callback receives ($value, $key). */` |
|        59 | 8317 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8318 | `			apCbArg[0] = pValue;` |
|        59 | 8319 | `			apCbArg[1] = &sKey;` |
|        59 | 8320 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8321 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8322 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8323 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8324 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8325 | `				return PH7_EXCEPTION;` |
|         - | 8326 | `			}` |
|        59 | 8327 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8328 | `				*ppMatch = pEntry;` |
|        15 | 8329 | `				break;` |
|         - | 8330 | `			}` |
|        22 | 8331 | `		}` |
|        45 | 8332 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8333 | `	}` |
|        29 | 8334 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8335 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8336 | `	return PH7_OK;` |
|        15 | 8337 | `}` |
|         - | 8338 | `/*` |
|         - | 8339 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8340 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8341 | ` *  is truthy, or NULL if none match.` |
|         - | 8342 | ` */` |
|         6 | 8343 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8344 | `{` |
|         - | 8345 | `	ph7_hashmap_node *pMatch;` |
|         - | 8346 | `	ph7_value *pVal;` |
|         - | 8347 | `	sxi32 rc;` |
|         7 | 8348 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8349 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8350 | `		return rc;` |
|         - | 8351 | `	}` |
|         7 | 8352 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8353 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8354 | `	}else{` |
|         3 | 8355 | `		ph7_result_null(pCtx);` |
|         - | 8356 | `	}` |
|         7 | 8357 | `	return PH7_OK;` |
|         4 | 8358 | `}` |
|         - | 8359 | `/*` |
|         - | 8360 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8361 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8362 | ` *  is truthy, or NULL if none match.` |
|         - | 8363 | ` */` |
|         6 | 8364 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8365 | `{` |
|         - | 8366 | `	ph7_hashmap_node *pMatch;` |
|         - | 8367 | `	sxi32 rc;` |
|         7 | 8368 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8369 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8370 | `		return rc;` |
|         - | 8371 | `	}` |
|         7 | 8372 | `	if( pMatch == 0 ){` |
|         3 | 8373 | `		ph7_result_null(pCtx);` |
|         6 | 8374 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8375 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8376 | `	}else{` |
|         4 | 8377 | `		ph7_result_string(pCtx,` |
|         2 | 8378 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8379 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8380 | `	}` |
|         7 | 8381 | `	return PH7_OK;` |
|         4 | 8382 | `}` |
|         - | 8383 | `/*` |
|         - | 8384 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8385 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8386 | ` *  FALSE for an empty array.` |
|         - | 8387 | ` */` |
|         8 | 8388 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8389 | `{` |
|         - | 8390 | `	ph7_hashmap_node *pMatch;` |
|         - | 8391 | `	sxi32 rc;` |
|         9 | 8392 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8393 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8394 | `		return rc;` |
|         - | 8395 | `	}` |
|         9 | 8396 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8397 | `	return PH7_OK;` |
|         5 | 8398 | `}` |
|         - | 8399 | `/*` |
|         - | 8400 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8401 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8402 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8403 | ` */` |
|         8 | 8404 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8405 | `{` |
|         - | 8406 | `	ph7_hashmap_node *pMatch;` |
|         - | 8407 | `	sxi32 rc;` |
|         9 | 8408 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8409 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8410 | `		return rc;` |
|         - | 8411 | `	}` |
|         9 | 8412 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8413 | `	return PH7_OK;` |
|         5 | 8414 | `}` |
|         - | 8415 | `/*` |
|         - | 8416 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8417 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8418 | ` */` |
|         - | 8419 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8420 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        70 | 8421 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8422 | `{` |
|        74 | 8423 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        35 | 8424 | `	(void)pVm;` |
|        74 | 8425 | `	p->nCount++;` |
|        74 | 8426 | `	if( p->pArray ){` |
|         - | 8427 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8428 | `		 * otherwise append with an auto-assigned int index. */` |
|        66 | 8429 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        31 | 8430 | `	}` |
|        74 | 8431 | `	return SXRET_OK;` |
|         4 | 8432 | `}` |
|         - | 8433 | `/*` |
|         - | 8434 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8435 | ` */` |
|        26 | 8436 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8437 | `{` |
|         - | 8438 | `	struct IterCollect sCol;` |
|         - | 8439 | `	ph7_value *pArray;` |
|         - | 8440 | `	sxi32 rc;` |
|        30 | 8441 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8442 | `	pArray = ph7_context_new_array(pCtx);` |
|        30 | 8443 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8444 | `	sCol.pArray = pArray;` |
|        30 | 8445 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        30 | 8446 | `	sCol.nCount = 0;` |
|        30 | 8447 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8448 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8449 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8450 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8451 | `		sxu32 n;` |
|         9 | 8452 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8453 | `			ph7_value sKey, *pVal;` |
|         7 | 8454 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8455 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8456 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8457 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8458 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8459 | `			pEntry = pEntry->pPrev;` |
|         4 | 8460 | `		}` |
|         3 | 8461 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8462 | `		return PH7_OK;` |
|         - | 8463 | `	}` |
|        28 | 8464 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        28 | 8465 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        26 | 8466 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8467 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8468 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8469 | `			ph7_type_name(apArg[0]));` |
|         - | 8470 | `	}` |
|        26 | 8471 | `	ph7_result_value(pCtx,pArray);` |
|        26 | 8472 | `	return PH7_OK;` |
|        17 | 8473 | `}` |
|         - | 8474 | `/*` |
|         - | 8475 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8476 | ` */` |
|         6 | 8477 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8478 | `{` |
|         - | 8479 | `	struct IterCollect sCol;` |
|         - | 8480 | `	sxi32 rc;` |
|         7 | 8481 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         7 | 8482 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8483 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8484 | `		return PH7_OK;` |
|         - | 8485 | `	}` |
|         5 | 8486 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         5 | 8487 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         5 | 8488 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         5 | 8489 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8490 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8491 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8492 | `			ph7_type_name(apArg[0]));` |
|         - | 8493 | `	}` |
|         5 | 8494 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         5 | 8495 | `	return PH7_OK;` |
|         4 | 8496 | `}` |
|         - | 8497 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8498 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8499 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8500 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        24 | 8501 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8502 | `{` |
|        25 | 8503 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8504 | `	ph7_value sResult;` |
|         - | 8505 | `	SySet aArg;` |
|         - | 8506 | `	sxi32 rc;` |
|         - | 8507 | `	int bContinue;` |
|        12 | 8508 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        25 | 8509 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        25 | 8510 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8511 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8512 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8513 | `		sxu32 n;` |
|        17 | 8514 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8515 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8516 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8517 | `			pEntry = pEntry->pPrev;` |
|         5 | 8518 | `		}` |
|         4 | 8519 | `	}` |
|        25 | 8520 | `	PH7_MemObjInit(pVm,&sResult);` |
|        37 | 8521 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        24 | 8522 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        25 | 8523 | `	SySetRelease(&aArg);` |
|        25 | 8524 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        23 | 8525 | `	p->nCount++;` |
|        23 | 8526 | `	PH7_MemObjToBool(&sResult);` |
|        23 | 8527 | `	bContinue = (sResult.x.iVal != 0);` |
|        23 | 8528 | `	PH7_MemObjRelease(&sResult);` |
|        23 | 8529 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        13 | 8530 | `}` |
|         - | 8531 | `/*` |
|         - | 8532 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8533 | ` */` |
|         8 | 8534 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8535 | `{` |
|         - | 8536 | `	struct IterApply sApp;` |
|         - | 8537 | `	sxi32 rc;` |
|         9 | 8538 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8539 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8540 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8541 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8542 | `	}` |
|         9 | 8543 | `	sApp.pCallback = apArg[1];` |
|         9 | 8544 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|         9 | 8545 | `	sApp.nCount = 0;` |
|         9 | 8546 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|         9 | 8547 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8548 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8549 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8550 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8551 | `			ph7_type_name(apArg[0]));` |
|         - | 8552 | `	}` |
|         7 | 8553 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|         7 | 8554 | `	return PH7_OK;` |
|         5 | 8555 | `}` |
|         - | 8556 | `/*` |
|         - | 8557 | ` * Table of hashmap functions.` |
|         - | 8558 | ` */` |
|         - | 8559 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8560 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8561 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8562 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8563 | `	{"count",             ph7_hashmap_count },` |
|         - | 8564 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8565 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8566 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8567 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8568 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8569 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8570 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8571 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8572 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8573 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8574 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8575 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8576 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8577 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8578 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8579 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8580 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8581 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8582 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8583 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8584 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8585 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8586 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8587 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8588 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8589 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8590 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8591 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8592 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8593 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8594 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8595 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8596 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8597 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8598 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8599 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8600 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8601 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8602 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8603 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8604 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8605 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8606 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8607 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8608 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8609 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8610 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8611 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8612 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8613 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8614 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8615 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8616 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8617 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8618 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8619 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8620 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8621 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8622 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8623 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8624 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8625 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8626 | `	{"current",           ph7_hashmap_current },` |
|         - | 8627 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8628 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8629 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8630 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8631 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8632 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8633 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8634 | `};` |
|         - | 8635 | `/*` |
|         - | 8636 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8637 | ` */` |
|      3504 | 8638 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8639 | `{` |
|         - | 8640 | `	sxu32 n;` |
|    262805 | 8641 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    259301 | 8642 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    129653 | 8643 | `	}` |
|      3509 | 8644 | `}` |
|         - | 8645 | `/*` |
|         - | 8646 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8647 | ` * the BLOB given as the first argument.` |
|         - | 8648 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8649 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8650 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8651 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8652 | ` */` |
|         - | 8653 | `/*` |
|         - | 8654 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8655 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8656 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8657 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8658 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8659 | ` */` |
|        26 | 8660 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8661 | `{` |
|        29 | 8662 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8663 | `	ph7_value *pObj;` |
|        29 | 8664 | `	sxu32 n = 0;` |
|         - | 8665 | `	int isRef;` |
|        29 | 8666 | `	sxi32 rc = SXRET_OK;` |
|         - | 8667 | `	int i;` |
|        44 | 8668 | `	for(;;){` |
|        91 | 8669 | `		if( n >= pMap->nEntry ){` |
|        29 | 8670 | `			break;` |
|         - | 8671 | `		}` |
|       127 | 8672 | `		for( i = 0 ; i < nTab ; i++ ){` |
|        65 | 8673 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        34 | 8674 | `		}` |
|         - | 8675 | `		/* Dump key */` |
|        65 | 8676 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|        33 | 8677 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|        17 | 8678 | `		}else{` |
|        48 | 8679 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|        15 | 8680 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8681 | `		}` |
|         - | 8682 | `#ifdef __WINNT__` |
|         3 | 8683 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8684 | `#else` |
|        62 | 8685 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8686 | `#endif` |
|         - | 8687 | `		/* Dump node value */` |
|        65 | 8688 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        65 | 8689 | `		isRef = 0;` |
|        65 | 8690 | `		if( pObj ){` |
|        65 | 8691 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 8692 | `				/* Referenced object */` |
|       ! 0 | 8693 | `				isRef = 1;` |
|       ! 0 | 8694 | `			}` |
|        65 | 8695 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|        65 | 8696 | `			if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8697 | `				break;` |
|         - | 8698 | `			}` |
|        31 | 8699 | `		}` |
|         - | 8700 | `		/* Point to the next entry */` |
|        65 | 8701 | `		n++;` |
|        65 | 8702 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8703 | `	}` |
|        29 | 8704 | `	return rc;` |
|         3 | 8705 | `}` |
|        22 | 8706 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8707 | `{` |
|         - | 8708 | `	sxi32 rc;` |
|         - | 8709 | `	int i;` |
|        24 | 8710 | `	if( nDepth > 31 ){` |
|         - | 8711 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8712 | `		/* Nesting limit reached */` |
|       ! 0 | 8713 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8714 | `		if( ShowType ){` |
|       ! 0 | 8715 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       ! 0 | 8716 | `		}` |
|       ! 0 | 8717 | `		return SXERR_LIMIT;` |
|         - | 8718 | `	}` |
|        24 | 8719 | `	if( !ShowType ){` |
|        11 | 8720 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|         5 | 8721 | `	}` |
|         - | 8722 | `	/* Total entries */` |
|        24 | 8723 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|         - | 8724 | `#ifdef __WINNT__` |
|         2 | 8725 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8726 | `#else` |
|        22 | 8727 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8728 | `#endif` |
|        24 | 8729 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|        46 | 8730 | `	for( i = 0 ; i < nTab ; i++ ){` |
|        24 | 8731 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        13 | 8732 | `	}` |
|        24 | 8733 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        24 | 8734 | `	return rc;` |
|        13 | 8735 | `}` |
|         - | 8736 | `/*` |
|         - | 8737 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8738 | ` * retrieved entry.` |
|         - | 8739 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8740 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8741 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8742 | ` * a value different from PH7_OK.` |
|         - | 8743 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8744 | ` */` |
|     33874 | 8745 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8746 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8747 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8748 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8749 | `	)` |
|         5 | 8750 | `{` |
|         - | 8751 | `	ph7_hashmap_node *pEntry;` |
|         - | 8752 | `	ph7_value sKey,sValue;` |
|         - | 8753 | `	sxi32 rc;` |
|         - | 8754 | `	sxu32 n;` |
|         - | 8755 | `	/* Initialize walker parameter */` |
|     33879 | 8756 | `	rc = SXRET_OK;` |
|     33879 | 8757 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     33879 | 8758 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     33879 | 8759 | `	n = pMap->nEntry;` |
|     33879 | 8760 | `	pEntry = pMap->pFirst;` |
|         - | 8761 | `	/* Start the iteration process */` |
|     90192 | 8762 | `	for(;;){` |
|    180389 | 8763 | `		if( n < 1 ){` |
|     33879 | 8764 | `			break;` |
|         - | 8765 | `		}` |
|         - | 8766 | `		/* Extract a copy of the key and a copy the current value */` |
|    146515 | 8767 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    146515 | 8768 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8769 | `		/* Invoke the user callback */` |
|    146515 | 8770 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8771 | `		/* Release the copy of the key and the value */` |
|    146515 | 8772 | `		PH7_MemObjRelease(&sKey);` |
|    146515 | 8773 | `		PH7_MemObjRelease(&sValue);` |
|    146515 | 8774 | `		if( rc != PH7_OK ){` |
|         - | 8775 | `			/* Callback request an operation abort */` |
|       ! 0 | 8776 | `			return SXERR_ABORT;` |
|         - | 8777 | `		}` |
|         - | 8778 | `		/* Point to the next entry */` |
|    146515 | 8779 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    146515 | 8780 | `		n--;` |
|         5 | 8781 | `	}` |
|         - | 8782 | `	/* All done */` |
|     33879 | 8783 | `	return SXRET_OK;` |
|     16942 | 8784 | `}` |
|         - | 8785 |  |
