# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4022/4505 lines (89.28%)

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
|   7960430 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7960435 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7960435 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    520769 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    520774 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    520774 |   35 | `	sxu32 nH = 5381;` |
|    520774 |   36 | `	zEnd = &zIn[nLen];` |
|    597583 |   37 | `	for(;;){` |
|   1195172 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1020886 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    919297 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    795009 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    520774 |   43 | `	return nH;` |
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
|   3651878 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3651883 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3651883 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3651883 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3651883 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3651883 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3651883 |  112 | `	pNode->nHash = nHash;` |
|   3651883 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3651883 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3651883 |  115 | `	return pNode;` |
|   1825944 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    200123 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    200128 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    200128 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    200128 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    200128 |  133 | `	pNode->pMap  = &(*pMap);` |
|    200128 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    200128 |  135 | `	pNode->nHash = nHash;` |
|    200128 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    200128 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    200128 |  138 | `	pNode->nValIdx = nValIdx;` |
|    200128 |  139 | `	return pNode;` |
|    100066 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3852001 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3852006 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   3391977 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   3391977 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1695986 |  150 | `	}` |
|   3852006 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3852006 |  153 | `	if( pMap->pFirst == 0 ){` |
|     88802 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     88802 |  156 | `		pMap->pCur = pNode;` |
|     44403 |  157 | `	}else{` |
|   3763209 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3852006 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3852006 |  174 | `	++pMap->nEntry;` |
|   3852006 |  175 | `}` |
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
|      5357 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2677 |  192 | `	}` |
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
|   3852001 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3852006 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     94302 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     94302 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     94302 |  245 | `		if( nNew < 1 ){` |
|     88802 |  246 | `			nNew = 16;` |
|     44398 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     94302 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     94302 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     94302 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     94302 |  260 | `		pMap->apBucket = apNew;` |
|     94302 |  261 | `		pMap->nSize = nNew;` |
|     94302 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     88802 |  264 | `			return SXRET_OK;` |
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
|   3763209 |  289 | `	return SXRET_OK;` |
|   1926005 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3651878 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3651883 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3651841 |  310 | `		if( pValue ){` |
|   3651835 |  311 | `			sSafeVal = *pValue;` |
|   3651835 |  312 | `			pValue = &sSafeVal;` |
|   1825915 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3651841 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3651841 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3651841 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3651835 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1825915 |  322 | `		}` |
|   3651841 |  323 | `		nIdx = pObj->nIdx;` |
|   1825923 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3651883 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3651883 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3651883 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3651883 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3651883 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3651883 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3651883 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3651883 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3651883 |  349 | `	return SXRET_OK;` |
|   1825944 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    200123 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    200128 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    153152 |  370 | `		if( pValue ){` |
|    152842 |  371 | `			sSafeVal = *pValue;` |
|    152842 |  372 | `			pValue = &sSafeVal;` |
|     76418 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    153152 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    153152 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    153152 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    152842 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|     76418 |  382 | `		}` |
|    153152 |  383 | `		nIdx = pObj->nIdx;` |
|     76578 |  384 | `	}else{` |
|     46981 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    200128 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    200128 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    200128 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    200128 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     46981 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23488 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    200128 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    200128 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    200128 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    200128 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    200128 |  409 | `	return SXRET_OK;` |
|    100066 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4292272 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4292277 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       833 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4291449 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4291449 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110567698 |  433 | `	for(;;){` |
| 221135401 |  434 | `		if( pNode == 0 ){` |
|   4284687 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216850714 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216847698 |  438 | `			&& pNode->nHash == nHash` |
| 108425727 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      6767 |  441 | `				if( ppNode ){` |
|      6745 |  442 | `					*ppNode = pNode;` |
|      3370 |  443 | `				}` |
|      6767 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216843954 |  447 | `		pNode = pNode->pNextCollide;` |
|         2 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4284687 |  450 | `	return SXERR_NOTFOUND;` |
|   2146141 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    349125 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    349130 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     28484 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    320651 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    320651 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    259821 |  475 | `	for(;;){` |
|    519647 |  476 | `		if( pNode == 0 ){` |
|    252587 |  477 | `			break;` |
|         - |  478 | `		}` |
|    267060 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    265546 |  480 | `			&& pNode->nHash == nHash` |
|    166099 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     68171 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     68069 |  484 | `				if( ppNode ){` |
|     68041 |  485 | `					*ppNode = pNode;` |
|     34018 |  486 | `				}` |
|     68069 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    199001 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    252587 |  493 | `	return SXERR_NOTFOUND;` |
|    174567 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    349321 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    349326 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    349326 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    349326 |  504 | `	int isNeg = FALSE, nDigit;` |
|    349326 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    349304 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    349300 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    349300 |  516 | `	zDigit = zIn;` |
|    175114 |  517 | `	for(;;){` |
|    350234 |  518 | `		if( zIn >= zEnd ){` |
|       315 |  519 | `			break;` |
|         - |  520 | `		}` |
|    349920 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    348986 |  523 | `			return FALSE;` |
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
|    174665 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    155862 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    155867 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    155867 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    149203 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    149203 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    149127 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    149127 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|        38 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      6745 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        89 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        44 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      6745 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     77931 |  575 | `result:` |
|    155867 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     74021 |  578 | `		if( ppNode ){` |
|     73969 |  579 | `			*ppNode = pNode;` |
|     36982 |  580 | `		}` |
|     74021 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     81851 |  584 | `	return SXERR_NOTFOUND;` |
|     77936 |  585 | `}` |
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
|   1505286 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1505291 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1505285 |  623 | `	return FALSE;` |
|    752648 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3801103 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3801108 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3801108 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3801108 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    153140 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    153140 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       231 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    229362 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     76452 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|    152422 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    152286 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    152286 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1823984 |  683 | `IntKey:` |
|   3648203 |  684 | `	if( pKey ){` |
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
|   1505257 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1505255 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1505249 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1505249 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1505247 |  726 | `			++pMap->iNextIdx;` |
|    752621 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3647997 |  730 | `	return rc;` |
|   1900556 |  731 | `}` |
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
|     47028 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     47033 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     47033 |  766 | `	sxi32 rc = SXRET_OK;` |
|     47033 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46993 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     46993 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     70484 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23493 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     46981 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46981 |  790 | `		return rc;` |
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
|     23519 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1560817 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1560822 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1560822 |  832 | `	return pObj;` |
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
|     79563 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     79568 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     79568 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     79568 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     79568 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     79568 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     79568 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     79568 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     79568 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     79568 |  921 | `	return rc;` |
|     39659 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     17108 |  927 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     17113 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     17113 |  932 | `	if( pEntry->pPrevCollide ){` |
|     12114 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5952 |  934 | `	}else{` |
|      5004 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     17113 |  937 | `	if( pEntry->pNextCollide ){` |
|      1082 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       564 |  939 | `	}` |
|     17113 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     17113 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     17113 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     17113 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     17113 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17113 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     12445 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      6124 |  949 | `	}` |
|     17113 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17113 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     17113 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     17113 |  956 | `		pMap->iNextIdx++;` |
|      8554 |  957 | `	}` |
|     17113 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     34022 |  966 | `static int HashmapFindValue(` |
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
|     34027 |  979 | `	pEntry = pMap->pFirst;` |
|     34027 |  980 | `	n = pMap->nEntry;` |
|     34027 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     34027 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     81191 |  983 | `	for(;;){` |
|    162386 |  984 | `		if( n < 1 ){` |
|        79 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    162308 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    162308 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    162308 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    162308 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    162308 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    162308 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    162308 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    162308 | 1001 | `			if( rc == 0 ){` |
|     33949 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     33949 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     64180 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    128364 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    128364 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|        79 | 1014 | `	return SXERR_NOTFOUND;` |
|     17016 | 1015 | `}` |
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
|    728254 | 1237 | `static sxi32 HashmapDuplicateNode(` |
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
|    728254 | 1248 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    728256 | 1249 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
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
|    728251 | 1274 | `	sSafeVal = *pVal;` |
|         - | 1275 |  |
|    728251 | 1276 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1277 | `		/* Blob key insertion */` |
|      4049 | 1278 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4049 | 1279 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4049 | 1280 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4049 | 1281 | `		PH7_MemObjRelease(&sKey);` |
|      2027 | 1282 | `	}else{` |
|         - | 1283 | `		/* Int key */` |
|    724207 | 1284 | `		if( iAction == 0 ){ /* Merge */` |
|    720521 | 1285 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    363949 | 1286 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1287 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1288 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1289 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1290 | `		}else{ /* Dup */` |
|      3661 | 1291 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1292 | `		}` |
|         - | 1293 | `	}` |
|    728251 | 1294 | `	return rc;` |
|    364132 | 1295 | `}` |
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
|    723491 | 1323 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1324 | `		/* Extract the node value */` |
|    720581 | 1325 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    720581 | 1326 | `		if( pVal ){` |
|         - | 1327 | `			/* Make a local copy of the value.` |
|         - | 1328 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1329 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1330 | `			 * to the old pool.` |
|         - | 1331 | `			 */` |
|    720581 | 1332 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    360293 | 1333 | `		}else{` |
|       ! 0 | 1334 | `			rc = SXRET_OK;` |
|         - | 1335 | `		}` |
|    720581 | 1336 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1337 | `			return rc;` |
|         - | 1338 | `		}` |
|         - | 1339 | `		/* Point to the next entry */` |
|    720581 | 1340 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    360293 | 1341 | `	}` |
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
|    249314 | 1499 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1500 | `{` |
|    249319 | 1501 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1502 | `	ph7_hashmap *pNew;` |
|         - | 1503 | `	ph7_value *pBacking;` |
|         - | 1504 | `	sxu32 nValIdx;` |
|         - | 1505 | `	int bValueInPool;` |
|    249319 | 1506 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    249319 | 1507 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1508 | `		/* Sole owner, no separation needed */` |
|    246505 | 1509 | `		return pMap;` |
|         - | 1510 | `	}` |
|      2819 | 1511 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1512 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1513 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1514 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       131 | 1515 | `		return pMap;` |
|         - | 1516 | `	}` |
|         - | 1517 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1518 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1519 | `	 * frame is popped. */` |
|      2689 | 1520 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2689 | 1521 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2684 | 1522 | `		if( pBacking && pBacking != pValue` |
|      2659 | 1523 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2639 | 1524 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1525 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2639 | 1526 | `			pMap->iRef--;` |
|      2639 | 1527 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1528 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2589 | 1529 | `				pMap->iRef++;` |
|      2589 | 1530 | `				return pMap;` |
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
|    124662 | 1593 | `}` |
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
|    143532 | 1685 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1686 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1687 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1688 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1689 | `	)` |
|         5 | 1690 | `{` |
|         - | 1691 | `	ph7_hashmap *pMap;` |
|         - | 1692 | `	/* Allocate a new instance */` |
|    143537 | 1693 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    143537 | 1694 | `	if( pMap == 0 ){` |
|       ! 0 | 1695 | `		return 0;` |
|         - | 1696 | `	}` |
|         - | 1697 | `	/* Zero the structure */` |
|    143537 | 1698 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1699 | `	/* Fill in the structure */` |
|    143537 | 1700 | `	pMap->pVm = &(*pVm);` |
|    143537 | 1701 | `	pMap->iRef = 1;` |
|         - | 1702 | `	/* Default hash functions */` |
|    143537 | 1703 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    143537 | 1704 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    143537 | 1705 | `	return pMap;` |
|     71771 | 1706 | `}` |
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
|     95504 | 1798 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1799 | `{` |
|         - | 1800 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     95509 | 1801 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1802 | `	sxu32 n;` |
|     95509 | 1803 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1804 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1805 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1806 | `		return SXRET_OK;` |
|         - | 1807 | `	}` |
|     95509 | 1808 | `	if( pMap->pActiveSteps ){` |
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
|     95509 | 1821 | `	n = 0;` |
|     95509 | 1822 | `	pEntry = pMap->pFirst;` |
|   1918899 | 1823 | `	for(;;){` |
|   3837804 | 1824 | `		if( n >= pMap->nEntry ){` |
|     95509 | 1825 | `			break;` |
|         - | 1826 | `		}` |
|   3742300 | 1827 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1828 | `		/* Remove the reference from the foreign table */` |
|   3742300 | 1829 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3742300 | 1830 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1831 | `			/* Restore the ph7_value to the free list */` |
|   3742240 | 1832 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1871117 | 1833 | `		}` |
|         - | 1834 | `		/* Release the node */` |
|   3742300 | 1835 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    109900 | 1836 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     54947 | 1837 | `		}` |
|   3742300 | 1838 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1839 | `		/* Point to the next entry */` |
|   3742300 | 1840 | `		pEntry = pNext;` |
|   3742300 | 1841 | `		n++;` |
|         5 | 1842 | `	}` |
|     95509 | 1843 | `	if( pMap->nEntry > 0 ){` |
|         - | 1844 | `		/* Release the hash bucket */` |
|     69198 | 1845 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     34596 | 1846 | `	}` |
|     95509 | 1847 | `	if( FreeDS ){` |
|         - | 1848 | `		/* Free the whole instance */` |
|     95483 | 1849 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     47744 | 1850 | `	}else{` |
|         - | 1851 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1852 | `		pMap->apBucket = 0;` |
|        28 | 1853 | `		pMap->iNextIdx = 0;` |
|        28 | 1854 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1855 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1856 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1857 | `	}` |
|     95509 | 1858 | `	return SXRET_OK;` |
|     47757 | 1859 | `}` |
|         - | 1860 | `/*` |
|         - | 1861 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1862 | ` * If the count reaches zero which mean no more variables` |
|         - | 1863 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1864 | ` */` |
|    887860 | 1865 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1866 | `{` |
|    887865 | 1867 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1868 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    887865 | 1869 | `	pMap->iRef--;` |
|    887865 | 1870 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     95463 | 1871 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     47729 | 1872 | `	}` |
|    887865 | 1873 | `}` |
|         - | 1874 | `/*` |
|         - | 1875 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1876 | ` * Write a pointer to the target node on success.` |
|         - | 1877 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1878 | ` */` |
|    156082 | 1879 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1880 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1881 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1882 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1883 | `	)` |
|         5 | 1884 | `{` |
|         - | 1885 | `	sxi32 rc;` |
|    156087 | 1886 | `	if( pMap->nEntry < 1 ){` |
|         - | 1887 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1888 | `		 */` |
|       225 | 1889 | `		return SXERR_NOTFOUND;` |
|         - | 1890 | `	}` |
|    155867 | 1891 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    155867 | 1892 | `	return rc;` |
|     78046 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1896 | ` * hashmap.` |
|         - | 1897 | ` * If a node with the given key already exists in the database` |
|         - | 1898 | ` * then this function overwrite the old value.` |
|         - | 1899 | ` */` |
|   3080253 | 1900 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   3080258 | 1911 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   3080258 | 1912 | `	return rc;` |
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
|     47018 | 1951 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1952 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1953 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1954 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1955 | `	)` |
|         5 | 1956 | `{` |
|         - | 1957 | `	sxi32 rc;` |
|     47023 | 1958 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1959 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1960 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1961 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1962 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1963 | `		return PH7_ABORT;` |
|         - | 1964 | `	}` |
|     47023 | 1965 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     47023 | 1966 | `	return rc;` |
|     23514 | 1967 | `}` |
|         - | 1968 | `/*` |
|         - | 1969 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1970 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1971 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1972 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1973 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1974 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1975 | ` */` |
|     23558 | 1976 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1977 | `{` |
|     23563 | 1978 | `	pStep->pCursor = pMap->pFirst;` |
|     23563 | 1979 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     23563 | 1980 | `	pMap->pActiveSteps = pStep;` |
|     23563 | 1981 | `}` |
|         - | 1982 | `/*` |
|         - | 1983 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1984 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1985 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1986 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1987 | ` */` |
|     23382 | 1988 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1989 | `{` |
|     23387 | 1990 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     23387 | 1991 | `	while( *ppLink ){` |
|     23387 | 1992 | `		if( *ppLink == pStep ){` |
|     23387 | 1993 | `			*ppLink = pStep->pNextActive;` |
|     23387 | 1994 | `			pStep->pNextActive = 0;` |
|     23387 | 1995 | `			return;` |
|         - | 1996 | `		}` |
|       ! 0 | 1997 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1998 | `	}` |
|     11696 | 1999 | `}` |
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
|    631348 | 2020 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2021 | `{` |
|    631353 | 2022 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    631353 | 2023 | `	if( pEntry ){` |
|    631353 | 2024 | `		if( bStore ){` |
|    243301 | 2025 | `			PH7_MemObjStore(pEntry,pValue);` |
|    121653 | 2026 | `		}else{` |
|    388057 | 2027 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2028 | `		}` |
|    315424 | 2029 | `	}else{` |
|       ! 0 | 2030 | `		PH7_MemObjRelease(pValue);` |
|         - | 2031 | `	}` |
|    631353 | 2032 | `}` |
|         - | 2033 | `/*` |
|         - | 2034 | ` * Extract a node key.` |
|         - | 2035 | ` */` |
|    168558 | 2036 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2037 | `{` |
|         - | 2038 | `	/* Fill with the current key */` |
|    168563 | 2039 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    162733 | 2040 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2041 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2042 | `		}` |
|    162733 | 2043 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    162733 | 2044 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     81369 | 2045 | `	}else{` |
|      5835 | 2046 | `		SyBlobReset(&pKey->sBlob);` |
|      5835 | 2047 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5835 | 2048 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2049 | `	}` |
|    168563 | 2050 | `}` |
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
|     52486 | 2101 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2102 | `{` |
|         - | 2103 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2104 | `    /* Prevent compiler warning */` |
|     52491 | 2105 | `	result.pNext = result.pPrev = 0;` |
|     52491 | 2106 | `	pTail = &result;` |
|    132348 | 2107 | `	while( pA && pB ){` |
|     79862 | 2108 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     62606 | 2109 | `			pTail->pPrev = pA;` |
|     62606 | 2110 | `			pA->pNext = pTail;` |
|     62606 | 2111 | `			pTail = pA;` |
|     62606 | 2112 | `			pA = pA->pPrev;` |
|     31499 | 2113 | `		}else{` |
|     17261 | 2114 | `			pTail->pPrev = pB;` |
|     17261 | 2115 | `			pB->pNext = pTail;` |
|     17261 | 2116 | `			pTail = pB;` |
|     17261 | 2117 | `			pB = pB->pPrev;` |
|         - | 2118 | `		}` |
|         5 | 2119 | `	}` |
|     52491 | 2120 | `	if( pA ){` |
|      4409 | 2121 | `		pTail->pPrev = pA;` |
|      4409 | 2122 | `		pA->pNext = pTail;` |
|     50120 | 2123 | `	}else if( pB ){` |
|     47783 | 2124 | `		pTail->pPrev = pB;` |
|     47783 | 2125 | `		pB->pNext = pTail;` |
|     24063 | 2126 | `	}else{` |
|       309 | 2127 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2128 | `	}` |
|     52491 | 2129 | `	return result.pPrev;` |
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
|      1216 | 2143 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2144 | `{` |
|         - | 2145 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2146 | `	sxu32 i;` |
|      1221 | 2147 | `	SyZero(a,sizeof(a));` |
|         - | 2148 | `	/* Point to the first inserted entry */` |
|      1221 | 2149 | `	pIn = pMap->pFirst;` |
|     18389 | 2150 | `	while( pIn ){` |
|     17173 | 2151 | `		p = pIn;` |
|     17173 | 2152 | `		pIn = p->pPrev;` |
|     17173 | 2153 | `		p->pPrev = 0;` |
|     31963 | 2154 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     31963 | 2155 | `			if( a[i]==0 ){` |
|     17173 | 2156 | `				a[i] = p;` |
|     17173 | 2157 | `				break;` |
|       ! 0 | 2158 | `			}else{` |
|     14795 | 2159 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     14795 | 2160 | `				a[i] = 0;` |
|         - | 2161 | `			}` |
|      7400 | 2162 | `		}` |
|     17173 | 2163 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2164 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2165 | `			 * But that is impossible.` |
|         - | 2166 | `			 */` |
|       ! 0 | 2167 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2168 | `		}` |
|         5 | 2169 | `	}` |
|      1221 | 2170 | `	p = a[0];` |
|     38917 | 2171 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|         - | 2172 | `		/* Higher-index buckets hold EARLIER-inserted (and larger) runs, so the` |
|         - | 2173 | `		 * bucket must be the LEFT operand: HashmapNodeMerge favors its left arg on` |
|         - | 2174 | `		 * a tie (cmp <= 0), and php's sorts are stable (PHP 8.0+) — equal elements` |
|         - | 2175 | `		 * keep their original order. Passing p (the later elements) on the left` |
|         - | 2176 | `		 * reversed equal runs (e.g. usort of five tie-keyed items moved the last to` |
|         - | 2177 | `		 * the front). */` |
|     37701 | 2178 | `		p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     18853 | 2179 | `	}` |
|      1221 | 2180 | `	p->pNext = 0;` |
|         - | 2181 | `	/* Reflect the change */` |
|      1221 | 2182 | `	pMap->pFirst = p;` |
|         - | 2183 | `	/* Reset the loop cursor */` |
|      1221 | 2184 | `	pMap->pCur = pMap->pFirst;` |
|      1221 | 2185 | `	return SXRET_OK;` |
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
|     79479 | 2283 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2284 | `{` |
|     79484 | 2285 | `	if( pCmpData == 0 ){` |
|         - | 2286 | `		/* SORT_REGULAR fast path */` |
|     79396 | 2287 | `		return HashmapNodeCmp(pA,pB,FALSE);` |
|         - | 2288 | `	}` |
|        89 | 2289 | `	return HashmapFlagValueCmp(pA,pB,SX_PTR_TO_INT(pCmpData));` |
|     39617 | 2290 | `}` |
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
|        22 | 2536 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2537 | `{` |
|         - | 2538 | `	sxu32 n;` |
|        12 | 2539 | `	SXUNUSED(pB); /* cc warning */` |
|        12 | 2540 | `	SXUNUSED(pCmpData);` |
|         - | 2541 | `	/* Grab a random number */` |
|        23 | 2542 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2543 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2544 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2545 | `	 */` |
|        23 | 2546 | `	return n&1 ? 1 : -1;` |
|         1 | 2547 | `}` |
|         - | 2548 | `/*` |
|         - | 2549 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2550 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2551 | ` */` |
|      1132 | 2552 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2553 | `{` |
|         - | 2554 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2555 | `	sxu32 i;` |
|         - | 2556 | `	/* Rehash all entries */` |
|      1137 | 2557 | `	pLast = p = pMap->pFirst;` |
|      1137 | 2558 | `	pMap->iNextIdx = 0;` |
|      1137 | 2559 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      1137 | 2560 | `	i = 0;` |
|      9008 | 2561 | `	for( ;; ){` |
|     18021 | 2562 | `		if( i >= pMap->nEntry ){` |
|      1137 | 2563 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|      1137 | 2564 | `			break;` |
|         - | 2565 | `		}` |
|     16889 | 2566 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2567 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2568 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2569 | `			/* Change key type */` |
|         5 | 2570 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2571 | `		}` |
|     16889 | 2572 | `		HashmapRehashIntNode(p);` |
|         - | 2573 | `		/* Point to the next entry */` |
|     16889 | 2574 | `		i++;` |
|     16889 | 2575 | `		pLast = p;` |
|     16889 | 2576 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2577 | `	}` |
|      1137 | 2578 | `}` |
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
|      1100 | 2600 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2601 | `{` |
|         - | 2602 | `	ph7_hashmap *pMap;` |
|         - | 2603 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1105 | 2604 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2605 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2606 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2607 | `		return PH7_OK;` |
|         - | 2608 | `	}` |
|         - | 2609 | `	/* Point to the internal representation of the input hashmap */` |
|      1105 | 2610 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1105 | 2611 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1105 | 2612 | `	if( pMap->nEntry > 1 ){` |
|      1103 | 2613 | `		sxi32 iCmpFlags = 0;` |
|      1103 | 2614 | `		if( nArg > 1 ){` |
|         - | 2615 | `			/* Extract comparison flags */` |
|        15 | 2616 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         7 | 2617 | `		}` |
|         - | 2618 | `		/* Do the merge sort */` |
|      1103 | 2619 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2620 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      1103 | 2621 | `		HashmapSortRehash(pMap);` |
|       549 | 2622 | `	}` |
|         - | 2623 | `	/* All done,return TRUE */` |
|      1105 | 2624 | `	ph7_result_bool(pCtx,1);` |
|      1105 | 2625 | `	return PH7_OK;` |
|       555 | 2626 | `}` |
|         - | 2627 | `/*` |
|         - | 2628 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2629 | ` *  Sort an array and maintain index association.` |
|         - | 2630 | ` * Parameters` |
|         - | 2631 | ` *  $array` |
|         - | 2632 | ` *   The input array.` |
|         - | 2633 | ` * $sort_flags` |
|         - | 2634 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2635 | ` *  Sorting type flags:` |
|         - | 2636 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2637 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2638 | ` *   SORT_STRING - compare items as strings` |
|         - | 2639 | ` * Return` |
|         - | 2640 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2641 | ` */` |
|        34 | 2642 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2643 | `{` |
|         - | 2644 | `	ph7_hashmap *pMap;` |
|         - | 2645 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        39 | 2646 | `	if( nArg < 1 ){` |
|       ! 0 | 2647 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2648 | `			"ArgumentCountError",` |
|         - | 2649 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2650 | `			);` |
|         - | 2651 | `	}` |
|         - | 2652 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        39 | 2653 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2654 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2655 | `			"TypeError",` |
|         - | 2656 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2657 | `			ph7_type_name(apArg[0])` |
|         - | 2658 | `			);` |
|         - | 2659 | `	}` |
|         - | 2660 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 2661 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        27 | 2662 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        27 | 2663 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2664 | `		sxi32 iCmpFlags = 0;` |
|        23 | 2665 | `		if( nArg > 1 ){` |
|         - | 2666 | `			/* Extract comparison flags */` |
|         7 | 2667 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2668 | `		}` |
|         - | 2669 | `		/* Do the merge sort */` |
|        23 | 2670 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2671 | `		/* Fix the last link broken by the merge */` |
|        55 | 2672 | `		while(pMap->pLast->pPrev){` |
|        33 | 2673 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2674 | `		}` |
|        11 | 2675 | `	}` |
|         - | 2676 | `	/* All done,return TRUE */` |
|        27 | 2677 | `	ph7_result_bool(pCtx,1);` |
|        27 | 2678 | `	return PH7_OK;` |
|        22 | 2679 | `}` |
|         - | 2680 | `/*` |
|         - | 2681 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2682 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2683 | ` * Parameters` |
|         - | 2684 | ` *  $array` |
|         - | 2685 | ` *   The input array.` |
|         - | 2686 | ` * $sort_flags` |
|         - | 2687 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2688 | ` *  Sorting type flags:` |
|         - | 2689 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2690 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2691 | ` *   SORT_STRING - compare items as strings` |
|         - | 2692 | ` * Return` |
|         - | 2693 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2694 | ` */` |
|        32 | 2695 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2696 | `{` |
|         - | 2697 | `	ph7_hashmap *pMap;` |
|         - | 2698 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2699 | `	if( nArg < 1 ){` |
|       ! 0 | 2700 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2701 | `			"ArgumentCountError",` |
|         - | 2702 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2703 | `			);` |
|         - | 2704 | `	}` |
|         - | 2705 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2706 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2707 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2708 | `			"TypeError",` |
|         - | 2709 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2710 | `			ph7_type_name(apArg[0])` |
|         - | 2711 | `			);` |
|         - | 2712 | `	}` |
|         - | 2713 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2714 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2715 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2716 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2717 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2718 | `		if( nArg > 1 ){` |
|         - | 2719 | `			/* Extract comparison flags */` |
|         7 | 2720 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2721 | `		}` |
|         - | 2722 | `		/* Do the merge sort */` |
|        21 | 2723 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2724 | `		/* Fix the last link broken by the merge */` |
|        37 | 2725 | `		while(pMap->pLast->pPrev){` |
|        17 | 2726 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2727 | `		}` |
|        10 | 2728 | `	}` |
|         - | 2729 | `	/* All done,return TRUE */` |
|        25 | 2730 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2731 | `	return PH7_OK;` |
|        21 | 2732 | `}` |
|         - | 2733 | `/*` |
|         - | 2734 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2735 | ` *  Sort an array by key.` |
|         - | 2736 | ` * Parameters` |
|         - | 2737 | ` *  $array` |
|         - | 2738 | ` *   The input array.` |
|         - | 2739 | ` * $sort_flags` |
|         - | 2740 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2741 | ` *  Sorting type flags:` |
|         - | 2742 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2743 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2744 | ` *   SORT_STRING - compare items as strings` |
|         - | 2745 | ` * Return` |
|         - | 2746 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2747 | ` */` |
|        20 | 2748 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2749 | `{` |
|         - | 2750 | `	ph7_hashmap *pMap;` |
|         - | 2751 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 2752 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2753 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2754 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2755 | `		return PH7_OK;` |
|         - | 2756 | `	}` |
|         - | 2757 | `	/* Point to the internal representation of the input hashmap */` |
|        22 | 2758 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        22 | 2759 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        22 | 2760 | `	if( pMap->nEntry > 1 ){` |
|        22 | 2761 | `		sxi32 iCmpFlags = 0;` |
|        22 | 2762 | `		if( nArg > 1 ){` |
|         - | 2763 | `			/* Extract comparison flags */` |
|         5 | 2764 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2765 | `		}` |
|         - | 2766 | `		/* Do the merge sort */` |
|        22 | 2767 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2768 | `		/* Fix the last link broken by the merge */` |
|        56 | 2769 | `		while(pMap->pLast->pPrev){` |
|        35 | 2770 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2771 | `		}` |
|        10 | 2772 | `	}` |
|         - | 2773 | `	/* All done,return TRUE */` |
|        22 | 2774 | `	ph7_result_bool(pCtx,1);` |
|        22 | 2775 | `	return PH7_OK;` |
|        12 | 2776 | `}` |
|         - | 2777 | `/*` |
|         - | 2778 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2779 | ` *  Sort an array by key in reverse order.` |
|         - | 2780 | ` * Parameters` |
|         - | 2781 | ` *  $array` |
|         - | 2782 | ` *   The input array.` |
|         - | 2783 | ` * $sort_flags` |
|         - | 2784 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2785 | ` *  Sorting type flags:` |
|         - | 2786 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2787 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2788 | ` *   SORT_STRING - compare items as strings` |
|         - | 2789 | ` * Return` |
|         - | 2790 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2791 | ` */` |
|         6 | 2792 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2793 | `{` |
|         - | 2794 | `	ph7_hashmap *pMap;` |
|         - | 2795 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2796 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2797 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2798 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2799 | `		return PH7_OK;` |
|         - | 2800 | `	}` |
|         - | 2801 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2802 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2803 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2804 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2805 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2806 | `		if( nArg > 1 ){` |
|         - | 2807 | `			/* Extract comparison flags */` |
|         3 | 2808 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         1 | 2809 | `		}` |
|         - | 2810 | `		/* Do the merge sort */` |
|         7 | 2811 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2812 | `		/* Fix the last link broken by the merge */` |
|        23 | 2813 | `		while(pMap->pLast->pPrev){` |
|        17 | 2814 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2815 | `		}` |
|         3 | 2816 | `	}` |
|         - | 2817 | `	/* All done,return TRUE */` |
|         7 | 2818 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2819 | `	return PH7_OK;` |
|         4 | 2820 | `}` |
|         - | 2821 | `/*` |
|         - | 2822 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2823 | ` * Sort an array in reverse order.` |
|         - | 2824 | ` * Parameters` |
|         - | 2825 | ` *  $array` |
|         - | 2826 | ` *   The input array.` |
|         - | 2827 | ` * $sort_flags` |
|         - | 2828 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2829 | ` *  Sorting type flags:` |
|         - | 2830 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2831 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2832 | ` *   SORT_STRING - compare items as strings` |
|         - | 2833 | ` * Return` |
|         - | 2834 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2835 | ` */` |
|         6 | 2836 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2837 | `{` |
|         - | 2838 | `	ph7_hashmap *pMap;` |
|         - | 2839 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2840 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2841 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2842 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2843 | `		return PH7_OK;` |
|         - | 2844 | `	}` |
|         - | 2845 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2846 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2847 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2848 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2849 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2850 | `		if( nArg > 1 ){` |
|         - | 2851 | `			/* Extract comparison flags */` |
|         5 | 2852 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         2 | 2853 | `		}` |
|         - | 2854 | `		/* Do the merge sort */` |
|         7 | 2855 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2856 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         7 | 2857 | `		HashmapSortRehash(pMap);` |
|         3 | 2858 | `	}` |
|         - | 2859 | `	/* All done,return TRUE */` |
|         7 | 2860 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2861 | `	return PH7_OK;` |
|         4 | 2862 | `}` |
|         - | 2863 | `/*` |
|         - | 2864 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2865 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2866 | ` * Parameters` |
|         - | 2867 | ` *  $array` |
|         - | 2868 | ` *   The input array.` |
|         - | 2869 | ` * $cmp_function` |
|         - | 2870 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2871 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2872 | ` *  to, or greater than the second.` |
|         - | 2873 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2874 | ` * Return` |
|         - | 2875 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2876 | ` */` |
|        26 | 2877 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2878 | `{` |
|         - | 2879 | `	ph7_hashmap *pMap;` |
|         - | 2880 | `	/* Make sure we are dealing with a valid hashmap */` |
|        29 | 2881 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2882 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2883 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2884 | `		return PH7_OK;` |
|         - | 2885 | `	}` |
|        29 | 2886 | `	if( nArg > 1 ){` |
|         - | 2887 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2888 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2889 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        29 | 2890 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        29 | 2891 | `		if( rcCb != PH7_OK ){` |
|         3 | 2892 | `			return rcCb;` |
|         - | 2893 | `		}` |
|        12 | 2894 | `	}` |
|         - | 2895 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 2896 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        27 | 2897 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        27 | 2898 | `	if( pMap->nEntry > 1 ){` |
|        27 | 2899 | `		ph7_value *pCallback = 0;` |
|         - | 2900 | `		ProcNodeCmp xCmp;` |
|        27 | 2901 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        27 | 2902 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2903 | `			/* Point to the desired callback */` |
|        27 | 2904 | `			pCallback = apArg[1];` |
|        15 | 2905 | `		}else{` |
|         - | 2906 | `			/* Use the default comparison function */` |
|       ! 0 | 2907 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2908 | `		}` |
|         - | 2909 | `		/* Do the merge sort */` |
|        27 | 2910 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        27 | 2911 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2912 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        27 | 2913 | `		HashmapSortRehash(pMap);` |
|        27 | 2914 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2915 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2916 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2917 | `			return PH7_EXCEPTION;` |
|         - | 2918 | `		}` |
|         8 | 2919 | `	}` |
|         - | 2920 | `	/* All done,return TRUE */` |
|        18 | 2921 | `	ph7_result_bool(pCtx,1);` |
|        18 | 2922 | `	return PH7_OK;` |
|        16 | 2923 | `}` |
|         - | 2924 | `/*` |
|         - | 2925 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2926 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2927 | ` *  and maintain index association.` |
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
|        14 | 2939 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2940 | `{` |
|         - | 2941 | `	ph7_hashmap *pMap;` |
|         - | 2942 | `	/* Make sure we are dealing with a valid hashmap */` |
|        15 | 2943 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2944 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2945 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2946 | `		return PH7_OK;` |
|         - | 2947 | `	}` |
|        15 | 2948 | `	if( nArg > 1 ){` |
|         - | 2949 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2950 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2951 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        15 | 2952 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        15 | 2953 | `		if( rcCb != PH7_OK ){` |
|         3 | 2954 | `			return rcCb;` |
|         - | 2955 | `		}` |
|         6 | 2956 | `	}` |
|         - | 2957 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 2958 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 2959 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 2960 | `	if( pMap->nEntry > 1 ){` |
|        13 | 2961 | `		ph7_value *pCallback = 0;` |
|         - | 2962 | `		ProcNodeCmp xCmp;` |
|        13 | 2963 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        13 | 2964 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2965 | `			/* Point to the desired callback */` |
|        13 | 2966 | `			pCallback = apArg[1];` |
|         7 | 2967 | `		}else{` |
|         - | 2968 | `			/* Use the default comparison function */` |
|       ! 0 | 2969 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2970 | `		}` |
|         - | 2971 | `		/* Do the merge sort */` |
|        13 | 2972 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        13 | 2973 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2974 | `		/* Fix the last link broken by the merge */` |
|        31 | 2975 | `		while(pMap->pLast->pPrev){` |
|        19 | 2976 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2977 | `		}` |
|        13 | 2978 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2979 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2980 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2981 | `			return PH7_EXCEPTION;` |
|         - | 2982 | `		}` |
|         6 | 2983 | `	}` |
|         - | 2984 | `	/* All done,return TRUE */` |
|        13 | 2985 | `	ph7_result_bool(pCtx,1);` |
|        13 | 2986 | `	return PH7_OK;` |
|         8 | 2987 | `}` |
|         - | 2988 | `/*` |
|         - | 2989 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2990 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2991 | ` *  function and maintain index association.` |
|         - | 2992 | ` * Parameters` |
|         - | 2993 | ` *  $array` |
|         - | 2994 | ` *   The input array.` |
|         - | 2995 | ` * $cmp_function` |
|         - | 2996 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2997 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2998 | ` *  to, or greater than the second.` |
|         - | 2999 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 3000 | ` * Return` |
|         - | 3001 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3002 | ` */` |
|         4 | 3003 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3004 | `{` |
|         - | 3005 | `	ph7_hashmap *pMap;` |
|         - | 3006 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3007 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3008 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3009 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3010 | `		return PH7_OK;` |
|         - | 3011 | `	}` |
|         5 | 3012 | `	if( nArg > 1 ){` |
|         - | 3013 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 3014 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 3015 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|         5 | 3016 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|         5 | 3017 | `		if( rcCb != PH7_OK ){` |
|         3 | 3018 | `			return rcCb;` |
|         - | 3019 | `		}` |
|         1 | 3020 | `	}` |
|         - | 3021 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3022 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3023 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3024 | `	if( pMap->nEntry > 1 ){` |
|         3 | 3025 | `		ph7_value *pCallback = 0;` |
|         - | 3026 | `		ProcNodeCmp xCmp;` |
|         3 | 3027 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 3028 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 3029 | `			/* Point to the desired callback */` |
|         3 | 3030 | `			pCallback = apArg[1];` |
|         2 | 3031 | `		}else{` |
|         - | 3032 | `			/* Use the default comparison function */` |
|       ! 0 | 3033 | `			xCmp = HashmapCmpCallback2;` |
|         - | 3034 | `		}` |
|         - | 3035 | `		/* Do the merge sort */` |
|         3 | 3036 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 3037 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 3038 | `		/* Fix the last link broken by the merge */` |
|         3 | 3039 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 3040 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 3041 | `		}` |
|         3 | 3042 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 3043 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 3044 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 3045 | `			return PH7_EXCEPTION;` |
|         - | 3046 | `		}` |
|         1 | 3047 | `	}` |
|         - | 3048 | `	/* All done,return TRUE */` |
|         3 | 3049 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3050 | `	return PH7_OK;` |
|         3 | 3051 | `}` |
|         - | 3052 | `/*` |
|         - | 3053 | ` * bool shuffle(array &$array)` |
|         - | 3054 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 3055 | ` * Parameters` |
|         - | 3056 | ` *  $array` |
|         - | 3057 | ` *   The input array.` |
|         - | 3058 | ` * Return` |
|         - | 3059 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3060 | ` *` |
|         - | 3061 | ` */` |
|         2 | 3062 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3063 | `{` |
|         - | 3064 | `	ph7_hashmap *pMap;` |
|         - | 3065 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3066 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 3067 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 3068 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3069 | `		return PH7_OK;` |
|         - | 3070 | `	}` |
|         - | 3071 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3072 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 3073 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 3074 | `	if( pMap->nEntry > 1 ){` |
|         - | 3075 | `		/* Do the merge sort */` |
|         3 | 3076 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 3077 | `		/* Fix the last link broken by the merge */` |
|         6 | 3078 | `		while(pMap->pLast->pPrev){` |
|         4 | 3079 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 3080 | `		}` |
|         1 | 3081 | `	}` |
|         - | 3082 | `	/* All done,return TRUE */` |
|         3 | 3083 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3084 | `	return PH7_OK;` |
|         2 | 3085 | `}` |
|         - | 3086 | `/*` |
|         - | 3087 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 3088 | ` *   Count all elements in an array, or something in an object.` |
|         - | 3089 | ` * Parameters` |
|         - | 3090 | ` *  $var` |
|         - | 3091 | ` *   The array or the object.` |
|         - | 3092 | ` * $mode` |
|         - | 3093 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 3094 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 3095 | ` *  all the elements of a multidimensional array.` |
|         - | 3096 | ` * Return` |
|         - | 3097 | ` *  Returns the number of elements in the array.` |
|         - | 3098 | ` */` |
|      2216 | 3099 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3100 | `{` |
|      2221 | 3101 | `	int bRecursive = FALSE;` |
|      2221 | 3102 | `	int bCycleDetected = FALSE;` |
|         - | 3103 | `	sxi64 iCount;` |
|      2221 | 3104 | `	if( nArg < 1 ){` |
|       ! 0 | 3105 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3106 | `			"ArgumentCountError",` |
|         - | 3107 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 3108 | `			);` |
|         - | 3109 | `	}` |
|      2221 | 3110 | `	if( nArg > 2 ){` |
|         4 | 3111 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3112 | `			"ArgumentCountError",` |
|         - | 3113 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3114 | `			nArg` |
|         - | 3115 | `			);` |
|         - | 3116 | `	}` |
|         - | 3117 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3118 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3119 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      2219 | 3120 | `	if( nArg > 1 ){` |
|        44 | 3121 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 3122 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 3123 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3124 | `				"ValueError",` |
|         - | 3125 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3126 | `				);` |
|         - | 3127 | `		}` |
|        34 | 3128 | `		bRecursive = iMode == 1;` |
|        16 | 3129 | `	}` |
|      2211 | 3130 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3131 | `		/* Countable object: dispatch to ->count() */` |
|        75 | 3132 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        63 | 3133 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        63 | 3134 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        63 | 3135 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        61 | 3136 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3137 | `					"count",sizeof("count")-1);` |
|        61 | 3138 | `				if( pMeth ){` |
|         - | 3139 | `					ph7_value sResult;` |
|        61 | 3140 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        61 | 3141 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        61 | 3142 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        61 | 3143 | `					PH7_MemObjRelease(&sResult);` |
|        61 | 3144 | `					return PH7_OK;` |
|         - | 3145 | `				}` |
|       ! 0 | 3146 | `			}` |
|         1 | 3147 | `		}` |
|        22 | 3148 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3149 | `			"TypeError",` |
|         - | 3150 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3151 | `			ph7_type_name(apArg[0])` |
|         - | 3152 | `			);` |
|         - | 3153 | `	}` |
|         - | 3154 | `	/* Count */` |
|      2141 | 3155 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      2141 | 3156 | `	if( bCycleDetected ){` |
|         3 | 3157 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3158 | `	}` |
|      2141 | 3159 | `	ph7_result_int64(pCtx,iCount);` |
|      2141 | 3160 | `	return PH7_OK;` |
|      1113 | 3161 | `}` |
|         - | 3162 | `/*` |
|         - | 3163 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3164 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3165 | ` * Parameters` |
|         - | 3166 | ` * $key` |
|         - | 3167 | ` *   Value to check.` |
|         - | 3168 | ` * $search` |
|         - | 3169 | ` *  An array with keys to check.` |
|         - | 3170 | ` * Return` |
|         - | 3171 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3172 | ` */` |
|        94 | 3173 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3174 | `{` |
|         - | 3175 | `	sxi32 rc;` |
|        99 | 3176 | `	if( nArg != 2 ){` |
|         - | 3177 | `		/* PHP requires exactly two arguments */` |
|         4 | 3178 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3179 | `			"ArgumentCountError",` |
|         - | 3180 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3181 | `			nArg` |
|         - | 3182 | `			);` |
|         - | 3183 | `	}` |
|         - | 3184 | `	/* Make sure we are dealing with a valid hashmap */` |
|        97 | 3185 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3186 | `		/* Type mismatch -> TypeError */` |
|         8 | 3187 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3188 | `			"TypeError",` |
|         - | 3189 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3190 | `			ph7_type_name(apArg[1])` |
|         - | 3191 | `			);` |
|         - | 3192 | `	}` |
|         - | 3193 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        92 | 3194 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3195 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3196 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3197 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3198 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3199 | `			"use an empty string instead"` |
|         - | 3200 | `			);` |
|        91 | 3201 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3202 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3203 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3204 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3205 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3206 | `				,rVal` |
|         - | 3207 | `				);` |
|         1 | 3208 | `		}` |
|         1 | 3209 | `	}` |
|         - | 3210 | `	/* Perform the lookup */` |
|        92 | 3211 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3212 | `	/* lookup result */` |
|        92 | 3213 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        92 | 3214 | `	return PH7_OK;` |
|        52 | 3215 | `}` |
|         - | 3216 | `/*` |
|         - | 3217 | ` * value array_pop(array $array)` |
|         - | 3218 | ` *   POP the last inserted element from the array.` |
|         - | 3219 | ` * Parameter` |
|         - | 3220 | ` *  The array to get the value from.` |
|         - | 3221 | ` * Return` |
|         - | 3222 | ` *  Poped value or NULL on failure.` |
|         - | 3223 | ` */` |
|       108 | 3224 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3225 | `{` |
|         - | 3226 | `	ph7_hashmap *pMap;` |
|         - | 3227 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       112 | 3228 | `	if( nArg != 1 ){` |
|         4 | 3229 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3230 | `			"ArgumentCountError",` |
|         - | 3231 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3232 | `			nArg` |
|         - | 3233 | `			);` |
|         - | 3234 | `	}` |
|         - | 3235 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3236 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       110 | 3237 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3238 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3239 | `			"Error",` |
|         - | 3240 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3241 | `			);` |
|         - | 3242 | `	}` |
|         - | 3243 | `	/* Make sure we are dealing with a valid hashmap */` |
|       104 | 3244 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3245 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3246 | `			"TypeError",` |
|         - | 3247 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3248 | `			ph7_type_name(apArg[0])` |
|         - | 3249 | `			);` |
|         - | 3250 | `	}` |
|       101 | 3251 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       101 | 3252 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       101 | 3253 | `	if( pMap->nEntry < 1 ){` |
|         - | 3254 | `		/* Nothing to pop,return NULL */` |
|         3 | 3255 | `		ph7_result_null(pCtx);` |
|         2 | 3256 | `	}else{` |
|        99 | 3257 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3258 | `		ph7_value *pObj;` |
|        99 | 3259 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        99 | 3260 | `		if( pObj ){` |
|         - | 3261 | `			/* Node value */` |
|        99 | 3262 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3263 | `			/* Unlink the node */` |
|        99 | 3264 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        50 | 3265 | `		}else{` |
|       ! 0 | 3266 | `			ph7_result_null(pCtx);` |
|         - | 3267 | `		}` |
|         - | 3268 | `		/* Reset the cursor */` |
|        99 | 3269 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3270 | `	}` |
|       101 | 3271 | `	return PH7_OK;` |
|        58 | 3272 | `}` |
|         - | 3273 | `/*` |
|         - | 3274 | ` * int array_push($array,$var,...)` |
|         - | 3275 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3276 | ` * Parameters` |
|         - | 3277 | ` *  array` |
|         - | 3278 | ` *    The input array.` |
|         - | 3279 | ` *  var` |
|         - | 3280 | ` *   On or more value to push.` |
|         - | 3281 | ` * Return` |
|         - | 3282 | ` *  New array count (including old items).` |
|         - | 3283 | ` */` |
|        22 | 3284 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3285 | `{` |
|         - | 3286 | `	ph7_hashmap *pMap;` |
|         - | 3287 | `	sxi32 rc;` |
|         - | 3288 | `	int i;` |
|        26 | 3289 | `	if( nArg < 1 ){` |
|       ! 0 | 3290 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3291 | `			"ArgumentCountError",` |
|         - | 3292 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3293 | `			nArg` |
|         - | 3294 | `			);` |
|         - | 3295 | `	}` |
|         - | 3296 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3297 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3298 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3299 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3300 | `			"Error",` |
|         - | 3301 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3302 | `			);` |
|         - | 3303 | `	}` |
|         - | 3304 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3305 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3306 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3307 | `			"TypeError",` |
|         - | 3308 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3309 | `			ph7_type_name(apArg[0])` |
|         - | 3310 | `			);` |
|         - | 3311 | `	}` |
|         - | 3312 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3313 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3314 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3315 | `	/* Start pushing given values */` |
|        34 | 3316 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3317 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3318 | `		if( rc != SXRET_OK ){` |
|         3 | 3319 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3320 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3321 | `				return rc;` |
|         - | 3322 | `			}` |
|       ! 0 | 3323 | `			break;` |
|         - | 3324 | `		}` |
|         9 | 3325 | `	}` |
|         - | 3326 | `	/* Return the new count */` |
|        15 | 3327 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3328 | `	return PH7_OK;` |
|        15 | 3329 | `}` |
|         - | 3330 | `/*` |
|         - | 3331 | ` * value array_shift(array $array)` |
|         - | 3332 | ` *   Shift an element off the beginning of array.` |
|         - | 3333 | ` * Parameter` |
|         - | 3334 | ` *  The array to get the value from.` |
|         - | 3335 | ` * Return` |
|         - | 3336 | ` *  Shifted value or NULL on failure.` |
|         - | 3337 | ` */` |
|        44 | 3338 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3339 | `{` |
|         - | 3340 | `	ph7_hashmap *pMap;` |
|         - | 3341 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3342 | `	if( nArg != 1 ){` |
|         4 | 3343 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3344 | `			"ArgumentCountError",` |
|         - | 3345 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3346 | `			nArg` |
|         - | 3347 | `			);` |
|         - | 3348 | `	}` |
|         - | 3349 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3350 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3351 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3352 | `			"Error",` |
|         - | 3353 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3354 | `			);` |
|         - | 3355 | `	}` |
|         - | 3356 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3357 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3358 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3359 | `			"TypeError",` |
|         - | 3360 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3361 | `			ph7_type_name(apArg[0])` |
|         - | 3362 | `			);` |
|         - | 3363 | `	}` |
|         - | 3364 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3365 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3366 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3367 | `	if( pMap->nEntry < 1 ){` |
|         - | 3368 | `		/* Empty hashmap,return NULL */` |
|         3 | 3369 | `		ph7_result_null(pCtx);` |
|         2 | 3370 | `	}else{` |
|        39 | 3371 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3372 | `		ph7_value *pObj;` |
|         - | 3373 | `		sxu32 n;` |
|        39 | 3374 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3375 | `		if( pObj ){` |
|         - | 3376 | `			/* Node value */` |
|        39 | 3377 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3378 | `			/* Unlink the first node */` |
|        39 | 3379 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3380 | `		}else{` |
|       ! 0 | 3381 | `			ph7_result_null(pCtx);` |
|         - | 3382 | `		}` |
|         - | 3383 | `		/* Rehash all int keys */` |
|        39 | 3384 | `		n = pMap->nEntry;` |
|        39 | 3385 | `		pEntry = pMap->pFirst;` |
|        39 | 3386 | `		pMap->iNextIdx = 0;` |
|        39 | 3387 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3388 | `		for(;;){` |
|        99 | 3389 | `			if( n < 1 ){` |
|        39 | 3390 | `				break;` |
|         - | 3391 | `			}` |
|        65 | 3392 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3393 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3394 | `			}` |
|         - | 3395 | `			/* Point to the next entry */` |
|        65 | 3396 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3397 | `			n--;` |
|         5 | 3398 | `		}` |
|         - | 3399 | `		/* Reset the cursor */` |
|        39 | 3400 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3401 | `	}` |
|        41 | 3402 | `	return PH7_OK;` |
|        27 | 3403 | `}` |
|         - | 3404 | `/*` |
|         - | 3405 | ` * Extract the node cursor value.` |
|         - | 3406 | ` */` |
|      1232 | 3407 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3408 | `{` |
|      1233 | 3409 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3410 | `	ph7_value *pVal;` |
|      1233 | 3411 | `	if( pCur == 0 ){` |
|         - | 3412 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3413 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3414 | `		return PH7_OK;` |
|         - | 3415 | `	}` |
|      1195 | 3416 | `	if( iDirection != 0 ){` |
|       227 | 3417 | `		if( iDirection > 0 ){` |
|         - | 3418 | `			/* Point to the next entry */` |
|       225 | 3419 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       225 | 3420 | `			pCur = pMap->pCur;` |
|       113 | 3421 | `		}else{` |
|         - | 3422 | `			/* Point to the previous entry */` |
|         3 | 3423 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3424 | `			pCur = pMap->pCur;` |
|         - | 3425 | `		}` |
|       227 | 3426 | `		if( pCur == 0 ){` |
|         - | 3427 | `			/* End of input reached,return FALSE */` |
|        91 | 3428 | `			ph7_result_bool(pCtx,0);` |
|        91 | 3429 | `			return PH7_OK;` |
|         - | 3430 | `		}` |
|        68 | 3431 | `	}` |
|         - | 3432 | `	/* Point to the desired element */` |
|      1105 | 3433 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      1105 | 3434 | `	if( pVal ){` |
|      1105 | 3435 | `		ph7_result_value(pCtx,pVal);` |
|       553 | 3436 | `	}else{` |
|       ! 0 | 3437 | `		ph7_result_bool(pCtx,0);` |
|         - | 3438 | `	}` |
|      1105 | 3439 | `	return PH7_OK;` |
|       617 | 3440 | `}` |
|         - | 3441 | `/*` |
|         - | 3442 | ` * value current(array $array)` |
|         - | 3443 | ` *  Return the current element in an array.` |
|         - | 3444 | ` * Parameter` |
|         - | 3445 | ` *  $input: The input array.` |
|         - | 3446 | ` * Return` |
|         - | 3447 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3448 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3449 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3450 | ` *  is empty, current() returns FALSE.` |
|         - | 3451 | ` */` |
|       356 | 3452 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3453 | `{` |
|       357 | 3454 | `	if( nArg < 1 ){` |
|         - | 3455 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3456 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3457 | `		return PH7_OK;` |
|         - | 3458 | `	}` |
|         - | 3459 | `	/* Make sure we are dealing with a valid hashmap */` |
|       357 | 3460 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3461 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3462 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3463 | `		return PH7_OK;` |
|         - | 3464 | `	}` |
|       357 | 3465 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       357 | 3466 | `	return PH7_OK;` |
|       179 | 3467 | `}` |
|         - | 3468 | `/*` |
|         - | 3469 | ` * value next(array $input)` |
|         - | 3470 | ` *  Advance the internal array pointer of an array.` |
|         - | 3471 | ` * Parameter` |
|         - | 3472 | ` *  $input: The input array.` |
|         - | 3473 | ` * Return` |
|         - | 3474 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3475 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3476 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3477 | ` */` |
|       224 | 3478 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3479 | `{` |
|       225 | 3480 | `	if( nArg < 1 ){` |
|         - | 3481 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3482 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3483 | `		return PH7_OK;` |
|         - | 3484 | `	}` |
|         - | 3485 | `	/* Make sure we are dealing with a valid hashmap */` |
|       225 | 3486 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3487 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3488 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3489 | `		return PH7_OK;` |
|         - | 3490 | `	}` |
|       225 | 3491 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       225 | 3492 | `	return PH7_OK;` |
|       113 | 3493 | `}` |
|         - | 3494 | `/*` |
|         - | 3495 | ` * value prev(array $input)` |
|         - | 3496 | ` *  Rewind the internal array pointer.` |
|         - | 3497 | ` * Parameter` |
|         - | 3498 | ` *  $input: The input array.` |
|         - | 3499 | ` * Return` |
|         - | 3500 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3501 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3502 | ` *  elements.` |
|         - | 3503 | ` */` |
|         2 | 3504 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3505 | `{` |
|         3 | 3506 | `	if( nArg < 1 ){` |
|         - | 3507 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3508 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3509 | `		return PH7_OK;` |
|         - | 3510 | `	}` |
|         - | 3511 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3512 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3513 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3514 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3515 | `		return PH7_OK;` |
|         - | 3516 | `	}` |
|         3 | 3517 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3518 | `	return PH7_OK;` |
|         2 | 3519 | `}` |
|         - | 3520 | `/*` |
|         - | 3521 | ` * value end(array $input)` |
|         - | 3522 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3523 | ` * Parameter` |
|         - | 3524 | ` *  $input: The input array.` |
|         - | 3525 | ` * Return` |
|         - | 3526 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3527 | ` */` |
|       390 | 3528 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3529 | `{` |
|         - | 3530 | `	ph7_hashmap *pMap;` |
|       391 | 3531 | `	if( nArg < 1 ){` |
|         - | 3532 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3533 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3534 | `		return PH7_OK;` |
|         - | 3535 | `	}` |
|         - | 3536 | `	/* Make sure we are dealing with a valid hashmap */` |
|       391 | 3537 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3538 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3539 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3540 | `		return PH7_OK;` |
|         - | 3541 | `	}` |
|         - | 3542 | `	/* Point to the internal representation of the input hashmap */` |
|       391 | 3543 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3544 | `	/* Point to the last node */` |
|       391 | 3545 | `	pMap->pCur = pMap->pLast;` |
|         - | 3546 | `	/* Return the last node value */` |
|       391 | 3547 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       391 | 3548 | `	return PH7_OK;` |
|       196 | 3549 | `}` |
|         - | 3550 | `/*` |
|         - | 3551 | ` * value reset(array $array )` |
|         - | 3552 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3553 | ` * Parameter` |
|         - | 3554 | ` *  $input: The input array.` |
|         - | 3555 | ` * Return` |
|         - | 3556 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3557 | ` */` |
|       260 | 3558 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3559 | `{` |
|         - | 3560 | `	ph7_hashmap *pMap;` |
|       261 | 3561 | `	if( nArg < 1 ){` |
|         - | 3562 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3563 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3564 | `		return PH7_OK;` |
|         - | 3565 | `	}` |
|         - | 3566 | `	/* Make sure we are dealing with a valid hashmap */` |
|       261 | 3567 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3568 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3569 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3570 | `		return PH7_OK;` |
|         - | 3571 | `	}` |
|         - | 3572 | `	/* Point to the internal representation of the input hashmap */` |
|       261 | 3573 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3574 | `	/* Point to the first node */` |
|       261 | 3575 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3576 | `	/* Return the last node value if available */` |
|       261 | 3577 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       261 | 3578 | `	return PH7_OK;` |
|       131 | 3579 | `}` |
|         - | 3580 | `/*` |
|         - | 3581 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3582 | ` * array_key_first() and array_key_last().` |
|         - | 3583 | ` */` |
|       772 | 3584 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3585 | `{` |
|       773 | 3586 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3587 | `		/* Key is integer */` |
|       383 | 3588 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       192 | 3589 | `	}else{` |
|         - | 3590 | `		/* Key is blob */` |
|       586 | 3591 | `		ph7_result_string(pCtx,` |
|       390 | 3592 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3593 | `	}` |
|       773 | 3594 | `}` |
|         - | 3595 | `/*` |
|         - | 3596 | ` * value key(array $array)` |
|         - | 3597 | ` *   Fetch a key from an array` |
|         - | 3598 | ` * Parameter` |
|         - | 3599 | ` *  $input` |
|         - | 3600 | ` *   The input array.` |
|         - | 3601 | ` * Return` |
|         - | 3602 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3603 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3604 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3605 | ` *  is empty, key() returns NULL.` |
|         - | 3606 | ` */` |
|       892 | 3607 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3608 | `{` |
|         - | 3609 | `	ph7_hashmap_node *pCur;` |
|         - | 3610 | `	ph7_hashmap *pMap;` |
|       893 | 3611 | `	if( nArg < 1 ){` |
|         - | 3612 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3613 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3614 | `		return PH7_OK;` |
|         - | 3615 | `	}` |
|         - | 3616 | `	/* Make sure we are dealing with a valid hashmap */` |
|       893 | 3617 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3618 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3619 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3620 | `		return PH7_OK;` |
|         - | 3621 | `	}` |
|       893 | 3622 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       893 | 3623 | `	pCur = pMap->pCur;` |
|       893 | 3624 | `	if( pCur == 0 ){` |
|         - | 3625 | `		/* Cursor does not point to anything,return NULL */` |
|       137 | 3626 | `		ph7_result_null(pCtx);` |
|       137 | 3627 | `		return PH7_OK;` |
|         - | 3628 | `	}` |
|       757 | 3629 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       757 | 3630 | `	return PH7_OK;` |
|       447 | 3631 | `}` |
|         - | 3632 | `/*` |
|         - | 3633 | ` * array each(array $input)` |
|         - | 3634 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3635 | ` * Parameter` |
|         - | 3636 | ` *  $input` |
|         - | 3637 | ` *    The input array.` |
|         - | 3638 | ` * Return` |
|         - | 3639 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3640 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3641 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3642 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3643 | ` *  each() returns FALSE.` |
|         - | 3644 | ` */` |
|        22 | 3645 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3646 | `{` |
|         - | 3647 | `	ph7_hashmap_node *pCur;` |
|         - | 3648 | `	ph7_hashmap *pMap;` |
|         - | 3649 | `	ph7_value *pArray;` |
|         - | 3650 | `	ph7_value *pVal;` |
|         - | 3651 | `	ph7_value sKey;` |
|        23 | 3652 | `	if( nArg < 1 ){` |
|         - | 3653 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3654 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3655 | `		return PH7_OK;` |
|         - | 3656 | `	}` |
|         - | 3657 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3658 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3659 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3660 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3661 | `		return PH7_OK;` |
|         - | 3662 | `	}` |
|         - | 3663 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3664 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3665 | `	if( pMap->pCur == 0 ){` |
|         - | 3666 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3667 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3668 | `		return PH7_OK;` |
|         - | 3669 | `	}` |
|        15 | 3670 | `	pCur = pMap->pCur;` |
|         - | 3671 | `	/* Create a new array */` |
|        15 | 3672 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3673 | `	if( pArray == 0 ){` |
|       ! 0 | 3674 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3675 | `		return PH7_OK;` |
|         - | 3676 | `	}` |
|        15 | 3677 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3678 | `	/* Insert the current value */` |
|        15 | 3679 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3680 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3681 | `	/* Make the key */` |
|        15 | 3682 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3683 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3684 | `	}else{` |
|         9 | 3685 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3686 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3687 | `	}` |
|         - | 3688 | `	/* Insert the current key */` |
|        15 | 3689 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3690 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3691 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3692 | `	/* Advance the cursor */` |
|        15 | 3693 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3694 | `	/* Return the current entry */` |
|        15 | 3695 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3696 | `	return PH7_OK;` |
|        12 | 3697 | `}` |
|         - | 3698 | `/*` |
|         - | 3699 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3700 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3701 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3702 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3703 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3704 | ` */` |
|         - | 3705 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3706 | `/*` |
|         - | 3707 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3708 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3709 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3710 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3711 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3712 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3713 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3714 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3715 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3716 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3717 | ` */` |
|         - | 3718 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3719 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3720 | `/*` |
|         - | 3721 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3722 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3723 | ` */` |
|       ! 0 | 3724 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3725 | `{` |
|       ! 0 | 3726 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3727 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3728 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3729 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3730 | `		zBuf[n] = 0;` |
|       ! 0 | 3731 | `		return zBuf;` |
|         - | 3732 | `	}` |
|       ! 0 | 3733 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3734 | `}` |
|         - | 3735 | `/*` |
|         - | 3736 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3737 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3738 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3739 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3740 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3741 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3742 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3743 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3744 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3745 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3746 | ` */` |
|       156 | 3747 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3748 | `{` |
|       157 | 3749 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3750 | `	sxu64 uVal = 0;` |
|       157 | 3751 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3752 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3753 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3754 | `		bNeg = (z[0] == '-');` |
|         3 | 3755 | `		z++;` |
|         1 | 3756 | `	}` |
|       237 | 3757 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3758 | `		int d = z[0] - '0';` |
|         - | 3759 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3760 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3761 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3762 | `			bOverflow = 1;` |
|       ! 0 | 3763 | `		}else{` |
|        81 | 3764 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3765 | `		}` |
|        81 | 3766 | `		bDigit = 1;` |
|        81 | 3767 | `		z++;` |
|         1 | 3768 | `	}` |
|       157 | 3769 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3770 | `		bReal = 1;` |
|         3 | 3771 | `		z++;` |
|         5 | 3772 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3773 | `			bDigit = 1;` |
|         3 | 3774 | `			z++;` |
|         1 | 3775 | `		}` |
|         1 | 3776 | `	}` |
|         - | 3777 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3778 | `	if( !bDigit ){` |
|        61 | 3779 | `		return RANGE_IN_ERROR;` |
|         - | 3780 | `	}` |
|         - | 3781 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3782 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3783 | `		z++;` |
|         9 | 3784 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3785 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3786 | `			return RANGE_IN_ERROR;` |
|         - | 3787 | `		}` |
|         9 | 3788 | `		bReal = 1;` |
|        17 | 3789 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3790 | `	}` |
|         - | 3791 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3792 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3793 | `	if( z != zEnd ){` |
|        13 | 3794 | `		return RANGE_IN_ERROR;` |
|         - | 3795 | `	}` |
|        84 | 3796 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3797 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3798 | `		bReal = 1;` |
|        84 | 3799 | `	}` |
|        43 | 3800 | `	if( bReal ){` |
|        11 | 3801 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3802 | `		return RANGE_IN_DOUBLE;` |
|         - | 3803 | `	}` |
|         - | 3804 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3805 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3806 | `	return RANGE_IN_LONG;` |
|        58 | 3807 | `}` |
|         - | 3808 | `/*` |
|         - | 3809 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3810 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3811 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3812 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3813 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3814 | ` */` |
|       332 | 3815 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3816 | `{` |
|         - | 3817 | `	char zMsg[160];` |
|       333 | 3818 | `	*pRc = PH7_OK;` |
|       333 | 3819 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3820 | `		char zType[80];` |
|       ! 0 | 3821 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3822 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3823 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3824 | `		return FALSE;` |
|         - | 3825 | `	}` |
|       333 | 3826 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3827 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3828 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3829 | `			iArg,zName);` |
|         5 | 3830 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3831 | `		*pbNullCoerced = TRUE;` |
|         2 | 3832 | `	}` |
|       333 | 3833 | `	return TRUE;` |
|       167 | 3834 | `}` |
|         - | 3835 | `/*` |
|         - | 3836 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3837 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3838 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3839 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3840 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3841 | ` */` |
|        60 | 3842 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3843 | `{` |
|        61 | 3844 | `	*pRc = PH7_OK;` |
|        61 | 3845 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3846 | `		char zType[80];` |
|       ! 0 | 3847 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3848 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3849 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3850 | `		return RANGE_IN_ERROR;` |
|         - | 3851 | `	}` |
|        61 | 3852 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3853 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3854 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3855 | `		*pLong = 0;` |
|         3 | 3856 | `		return RANGE_IN_LONG;` |
|         - | 3857 | `	}` |
|        59 | 3858 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3859 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3860 | `		return RANGE_IN_DOUBLE;` |
|         - | 3861 | `	}` |
|        35 | 3862 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3863 | `		const char *zStr;` |
|         - | 3864 | `		int nLen;` |
|         - | 3865 | `		sxu8 iKind;` |
|         3 | 3866 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3867 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3868 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3869 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3870 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3871 | `		}` |
|         3 | 3872 | `		return iKind;` |
|         - | 3873 | `	}` |
|         - | 3874 | `	/* int / bool */` |
|        33 | 3875 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3876 | `	return RANGE_IN_LONG;` |
|        31 | 3877 | `}` |
|         - | 3878 | `/*` |
|         - | 3879 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3880 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3881 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3882 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3883 | ` */` |
|       300 | 3884 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3885 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3886 | `{` |
|         - | 3887 | `	char zMsg[160];` |
|         - | 3888 | `	double r;` |
|       301 | 3889 | `	*pRc = PH7_OK;` |
|       301 | 3890 | `	if( bNullCoerced ){` |
|         - | 3891 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3892 | `		*pLong = 0;` |
|         5 | 3893 | `		*pDouble = 0.0;` |
|         5 | 3894 | `		return RANGE_IN_LONG;` |
|         - | 3895 | `	}` |
|       297 | 3896 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3897 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3898 | `check_dval:` |
|        25 | 3899 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3900 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3901 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3902 | `			return RANGE_IN_ERROR;` |
|         - | 3903 | `		}` |
|        21 | 3904 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3905 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3906 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3907 | `			return RANGE_IN_ERROR;` |
|         - | 3908 | `		}` |
|        17 | 3909 | `		*pDouble = r;` |
|        17 | 3910 | `		return RANGE_IN_DOUBLE;` |
|         - | 3911 | `	}` |
|       277 | 3912 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3913 | `		const char *zStr;` |
|         - | 3914 | `		int nLen;` |
|         - | 3915 | `		sxu8 iKind;` |
|        81 | 3916 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3917 | `		if( nLen == 0 ){` |
|         7 | 3918 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3919 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3920 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3921 | `			*pLong = 0;` |
|         5 | 3922 | `			*pDouble = 0.0;` |
|        41 | 3923 | `			return RANGE_IN_LONG;` |
|         - | 3924 | `		}` |
|        77 | 3925 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3926 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3927 | `			r = *pDouble;` |
|         5 | 3928 | `			goto check_dval;` |
|         - | 3929 | `		}` |
|        73 | 3930 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3931 | `			*pDouble = (double)*pLong;` |
|        23 | 3932 | `			if( nLen == 1 ){` |
|         - | 3933 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3934 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3935 | `				return RANGE_IN_DIGIT;` |
|         - | 3936 | `			}` |
|        15 | 3937 | `			return RANGE_IN_LONG;` |
|         - | 3938 | `		}` |
|        51 | 3939 | `		if( nLen != 1 ){` |
|        10 | 3940 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3941 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3942 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3943 | `		}` |
|        51 | 3944 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3945 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3946 | `		*pLong = 0;` |
|        51 | 3947 | `		*pDouble = 0.0;` |
|        51 | 3948 | `		return RANGE_IN_STRING;` |
|         - | 3949 | `	}` |
|         - | 3950 | `	/* int / bool */` |
|       197 | 3951 | `	*pLong = ph7_value_to_int64(pIn);` |
|       197 | 3952 | `	*pDouble = (double)*pLong;` |
|       197 | 3953 | `	return RANGE_IN_LONG;` |
|       151 | 3954 | `}` |
|         - | 3955 | `/*` |
|         - | 3956 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3957 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3958 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3959 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3960 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3961 | ` * exactly like php's two macros.` |
|         - | 3962 | ` */` |
|         6 | 3963 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3964 | `{` |
|        10 | 3965 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3966 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3967 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3968 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3969 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3970 | `}` |
|         6 | 3971 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3972 | `{` |
|         - | 3973 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3974 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3975 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3976 | `	const unsigned int nBuf = 1500;` |
|         7 | 3977 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3978 | `	if( zMsg == 0 ){` |
|       ! 0 | 3979 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3980 | `	}` |
|         7 | 3981 | `	snprintf(zMsg,nBuf,` |
|         - | 3982 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3983 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3984 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3985 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3986 | `}` |
|         - | 3987 | `/*` |
|         - | 3988 | ` * Set the element container to the next range element and append it to the` |
|         - | 3989 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3990 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3991 | ` * below stay one line per iteration.` |
|         - | 3992 | ` */` |
|    401680 | 3993 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3994 | `{` |
|    401681 | 3995 | `	ph7_value_int64(pValue,iVal);` |
|    401681 | 3996 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3997 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3998 | `	}` |
|    401681 | 3999 | `	return PH7_OK;` |
|    200841 | 4000 | `}` |
|        70 | 4001 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 4002 | `{` |
|        71 | 4003 | `	ph7_value_double(pValue,rVal);` |
|        71 | 4004 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 4005 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4006 | `	}` |
|        71 | 4007 | `	return PH7_OK;` |
|        36 | 4008 | `}` |
|       168 | 4009 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 4010 | `{` |
|       169 | 4011 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 4012 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 4013 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4014 | `	}` |
|       169 | 4015 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 4016 | `	return PH7_OK;` |
|        85 | 4017 | `}` |
|         - | 4018 | `/*` |
|         - | 4019 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 4020 | ` *  Create an array containing a range of elements.` |
|         - | 4021 | ` * Return` |
|         - | 4022 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 4023 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 4024 | ` */` |
|       168 | 4025 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4026 | `{` |
|         - | 4027 | `	ph7_value *pValue,*pArray;` |
|       169 | 4028 | `	sxi32 rc = PH7_OK;` |
|       169 | 4029 | `	int is_step_double = 0,is_step_negative = 0;` |
|       169 | 4030 | `	double step_double = 1.0;` |
|       169 | 4031 | `	sxi64 step = 1;` |
|         - | 4032 | `	sxu8 start_type,end_type;` |
|       169 | 4033 | `	sxi64 start_long = 0,end_long = 0;` |
|       169 | 4034 | `	double start_double = 0.0,end_double = 0.0;` |
|       169 | 4035 | `	unsigned char cStart = 0,cEnd = 0;` |
|       169 | 4036 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 4037 | `	sxu32 i,size;` |
|         - | 4038 |  |
|         - | 4039 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       169 | 4040 | `	if( nArg > 3 ){` |
|         4 | 4041 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 4042 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 4043 | `	}` |
|       167 | 4044 | `	if( nArg < 2 ){` |
|         - | 4045 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 4046 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 4047 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 4048 | `	}` |
|         - | 4049 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 4050 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       167 | 4051 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 4052 | `		return rc;` |
|         - | 4053 | `	}` |
|       167 | 4054 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 4055 | `		return rc;` |
|         - | 4056 | `	}` |
|       167 | 4057 | `	if( nArg > 2 ){` |
|        61 | 4058 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 4059 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 4060 | `			return rc;` |
|         - | 4061 | `		}` |
|        59 | 4062 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 4063 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 4064 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4065 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 4066 | `			}` |
|        23 | 4067 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 4068 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4069 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 4070 | `			}` |
|         - | 4071 | `			/* We only want positive step values. */` |
|        21 | 4072 | `			if( step_double < 0.0 ){` |
|       ! 0 | 4073 | `				is_step_negative = 1;` |
|       ! 0 | 4074 | `				step_double *= -1;` |
|       ! 0 | 4075 | `			}` |
|         - | 4076 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 4077 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 4078 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 4079 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 4080 | `				step = (sxi64)step_double;` |
|        19 | 4081 | `				if( (double)step != step_double ){` |
|        17 | 4082 | `					is_step_double = 1;` |
|         8 | 4083 | `				}` |
|        10 | 4084 | `			}else{` |
|         - | 4085 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 4086 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 4087 | `				is_step_double = 1;` |
|         - | 4088 | `			}` |
|        11 | 4089 | `		}else{` |
|         - | 4090 | `			/* We only want positive step values. */` |
|        35 | 4091 | `			if( step < 0 ){` |
|        11 | 4092 | `				if( step == SMALLEST_INT64 ){` |
|         - | 4093 | `					/* -step would overflow */` |
|         4 | 4094 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 4095 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 4096 | `				}` |
|         9 | 4097 | `				is_step_negative = 1;` |
|         9 | 4098 | `				step = -step;` |
|         4 | 4099 | `			}` |
|        33 | 4100 | `			step_double = (double)step;` |
|         - | 4101 | `		}` |
|        53 | 4102 | `		if( step_double == 0.0 ){` |
|         7 | 4103 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4104 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 4105 | `		}` |
|        23 | 4106 | `	}` |
|       153 | 4107 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       153 | 4108 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 4109 | `		return rc;` |
|         - | 4110 | `	}` |
|       149 | 4111 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       149 | 4112 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 4113 | `		return rc;` |
|         - | 4114 | `	}` |
|         - | 4115 | `	/* Element container + result array */` |
|       145 | 4116 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       145 | 4117 | `	pArray = ph7_context_new_array(pCtx);` |
|       145 | 4118 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4119 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4120 | `	}` |
|         - | 4121 | `	/* If the range is given as strings, generate an array of characters. */` |
|       145 | 4122 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4123 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4124 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4125 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4126 | `			 * and the range is numeric. */` |
|        15 | 4127 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4128 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4129 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4130 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4131 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4132 | `				}` |
|         7 | 4133 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4134 | `			}else{` |
|         9 | 4135 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4136 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4137 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4138 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4139 | `				}` |
|         9 | 4140 | `				start_type = RANGE_IN_LONG;` |
|         - | 4141 | `			}` |
|        15 | 4142 | `			goto handle_numeric_inputs;` |
|         - | 4143 | `		}` |
|        23 | 4144 | `		if( is_step_double ){` |
|         - | 4145 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4146 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4147 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4148 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4149 | `					" of characters, inputs converted to 0");` |
|         1 | 4150 | `			}` |
|         5 | 4151 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4152 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4153 | `			goto handle_numeric_inputs;` |
|         - | 4154 | `		}` |
|         - | 4155 | `		/* Generate an array of characters */` |
|        19 | 4156 | `		if( cStart > cEnd ){` |
|         - | 4157 | `			/* Decreasing char range */` |
|         - | 4158 | `			int iCur;` |
|         3 | 4159 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4160 | `				goto boundary_error;` |
|         - | 4161 | `			}` |
|        17 | 4162 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4163 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4164 | `					return rc;` |
|         - | 4165 | `				}` |
|         8 | 4166 | `			}` |
|        18 | 4167 | `		}else if( cEnd > cStart ){` |
|         - | 4168 | `			/* Increasing char range */` |
|         - | 4169 | `			int iCur;` |
|        15 | 4170 | `			if( is_step_negative ){` |
|         3 | 4171 | `				goto negative_step_error;` |
|         - | 4172 | `			}` |
|        13 | 4173 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4174 | `				goto boundary_error;` |
|         - | 4175 | `			}` |
|       163 | 4176 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4177 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4178 | `					return rc;` |
|         - | 4179 | `				}` |
|        77 | 4180 | `			}` |
|         6 | 4181 | `		}else{` |
|         3 | 4182 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4183 | `				return rc;` |
|         - | 4184 | `			}` |
|         - | 4185 | `		}` |
|        15 | 4186 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4187 | `		return PH7_OK;` |
|         - | 4188 | `	}` |
|        54 | 4189 | `handle_numeric_inputs:` |
|       135 | 4190 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4191 | `		/* Float range */` |
|         - | 4192 | `		double elem,calc;` |
|        25 | 4193 | `		if( start_double > end_double ){` |
|         - | 4194 | `			/* Decreasing float range */` |
|         7 | 4195 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4196 | `				goto boundary_error;` |
|         - | 4197 | `			}` |
|         7 | 4198 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4199 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4200 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4201 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4202 | `			}` |
|         5 | 4203 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4204 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4205 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4206 | `					return rc;` |
|         - | 4207 | `				}` |
|         8 | 4208 | `			}` |
|        21 | 4209 | `		}else if( end_double > start_double ){` |
|         - | 4210 | `			/* Increasing float range */` |
|        17 | 4211 | `			if( is_step_negative ){` |
|       ! 0 | 4212 | `				goto negative_step_error;` |
|         - | 4213 | `			}` |
|        17 | 4214 | `			if( end_double - start_double < step_double ){` |
|         3 | 4215 | `				goto boundary_error;` |
|         - | 4216 | `			}` |
|        15 | 4217 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4218 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4219 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4220 | `			}` |
|        11 | 4221 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4222 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4223 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4224 | `					return rc;` |
|         - | 4225 | `				}` |
|        28 | 4226 | `			}` |
|         6 | 4227 | `		}else{` |
|         3 | 4228 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4229 | `				return rc;` |
|         - | 4230 | `			}` |
|         - | 4231 | `		}` |
|         9 | 4232 | `	}else{` |
|         - | 4233 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4234 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4235 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       103 | 4236 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4237 | `		sxu64 calc;` |
|       103 | 4238 | `		if( start_long > end_long ){` |
|         - | 4239 | `			/* Decreasing int range */` |
|        19 | 4240 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4241 | `				goto boundary_error;` |
|         - | 4242 | `			}` |
|        17 | 4243 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4244 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4245 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4246 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4247 | `			}` |
|        15 | 4248 | `			size = (sxu32)(calc + 1);` |
|       101 | 4249 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4250 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4251 | `					return rc;` |
|         - | 4252 | `				}` |
|        44 | 4253 | `			}` |
|        92 | 4254 | `		}else if( end_long > start_long ){` |
|         - | 4255 | `			/* Increasing int range */` |
|        79 | 4256 | `			if( is_step_negative ){` |
|         3 | 4257 | `				goto negative_step_error;` |
|         - | 4258 | `			}` |
|        77 | 4259 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4260 | `				goto boundary_error;` |
|         - | 4261 | `			}` |
|        75 | 4262 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        75 | 4263 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4264 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4265 | `			}` |
|        71 | 4266 | `			size = (sxu32)(calc + 1);` |
|    401659 | 4267 | `			for( i = 0 ; i < size ; ++i ){` |
|    401589 | 4268 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4269 | `					return rc;` |
|         - | 4270 | `				}` |
|    200795 | 4271 | `			}` |
|        36 | 4272 | `		}else{` |
|         7 | 4273 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4274 | `				return rc;` |
|         - | 4275 | `			}` |
|         - | 4276 | `		}` |
|         - | 4277 | `	}` |
|         - | 4278 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4279 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       107 | 4280 | `	ph7_result_value(pCtx,pArray);` |
|       107 | 4281 | `	return PH7_OK;` |
|         2 | 4282 | `negative_step_error:` |
|         5 | 4283 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4284 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4285 | `boundary_error:` |
|         9 | 4286 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4287 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        85 | 4288 | `}` |
|         - | 4289 | `/*` |
|         - | 4290 | ` * array array_values(array $array)` |
|         - | 4291 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4292 | ` * Parameters` |
|         - | 4293 | ` *  $array` |
|         - | 4294 | ` *   The input array.` |
|         - | 4295 | ` * Return` |
|         - | 4296 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4297 | ` */` |
|        48 | 4298 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4299 | `{` |
|         - | 4300 | `	ph7_hashmap_node *pNode;` |
|         - | 4301 | `	ph7_hashmap *pMap;` |
|         - | 4302 | `	ph7_value *pArray;` |
|         - | 4303 | `	ph7_value *pObj;` |
|         - | 4304 | `	sxu32 n;` |
|        51 | 4305 | `	if( nArg != 1 ){` |
|         - | 4306 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4307 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4308 | `			"ArgumentCountError",` |
|         - | 4309 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4310 | `			nArg` |
|         - | 4311 | `			);` |
|         - | 4312 | `	}` |
|         - | 4313 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4314 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4315 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4316 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4317 | `			"TypeError",` |
|         - | 4318 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4319 | `			ph7_type_name(apArg[0])` |
|         - | 4320 | `			);` |
|         - | 4321 | `	}` |
|         - | 4322 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4323 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4324 | `	/* Create a new array */` |
|        46 | 4325 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4326 | `	if( pArray == 0 ){` |
|       ! 0 | 4327 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4328 | `		return PH7_OK;` |
|         - | 4329 | `	}` |
|         - | 4330 | `	/* Perform the requested operation */` |
|        46 | 4331 | `	pNode = pMap->pFirst;` |
|       144 | 4332 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4333 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4334 | `		if( pObj ){` |
|         - | 4335 | `			/* perform the insertion */` |
|       100 | 4336 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4337 | `		}` |
|         - | 4338 | `		/* Point to the next entry */` |
|       100 | 4339 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4340 | `	}` |
|         - | 4341 | `	/* return the new array */` |
|        46 | 4342 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4343 | `	return PH7_OK;` |
|        27 | 4344 | `}` |
|         - | 4345 | `/*` |
|         - | 4346 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4347 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4348 | ` * Parameters` |
|         - | 4349 | ` *  $input` |
|         - | 4350 | ` *   An array containing keys to return.` |
|         - | 4351 | ` * $search_value` |
|         - | 4352 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4353 | ` * $strict` |
|         - | 4354 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4355 | ` * Return` |
|         - | 4356 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4357 | ` */` |
|       174 | 4358 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4359 | `{` |
|         - | 4360 | `	ph7_hashmap_node *pNode;` |
|         - | 4361 | `	ph7_hashmap *pMap;` |
|         - | 4362 | `	ph7_value *pArray;` |
|         - | 4363 | `	ph7_value sObj;` |
|         - | 4364 | `	ph7_value sVal;` |
|         - | 4365 | `	SyString sKey;` |
|         - | 4366 | `	int bStrict;` |
|         - | 4367 | `	sxi32 rc;` |
|         - | 4368 | `	sxu32 n;` |
|       178 | 4369 | `	if( nArg < 1 ){` |
|         - | 4370 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4371 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4372 | `			"ArgumentCountError",` |
|         - | 4373 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4374 | `			);` |
|         - | 4375 | `	}` |
|         - | 4376 | `	/* Make sure we are dealing with a valid hashmap */` |
|       178 | 4377 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4378 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4379 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4380 | `			"TypeError",` |
|         - | 4381 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4382 | `			ph7_type_name(apArg[0])` |
|         - | 4383 | `			);` |
|         - | 4384 | `	}` |
|         - | 4385 | `	/* Point to the internal representation of the input hashmap */` |
|       175 | 4386 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4387 | `	/* Create a new array */` |
|       175 | 4388 | `	pArray = ph7_context_new_array(pCtx);` |
|       175 | 4389 | `	if( pArray == 0 ){` |
|       ! 0 | 4390 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4391 | `		return PH7_OK;` |
|         - | 4392 | `	}` |
|       175 | 4393 | `	bStrict = FALSE;` |
|       175 | 4394 | `	if( nArg > 2 ){` |
|         - | 4395 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4396 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4397 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4398 | `				"TypeError",` |
|         - | 4399 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4400 | `				ph7_type_name(apArg[2])` |
|         - | 4401 | `				);` |
|         - | 4402 | `		}` |
|         9 | 4403 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4404 | `	}` |
|         - | 4405 | `	/* Perform the requested operation */` |
|       175 | 4406 | `	pNode = pMap->pFirst;` |
|       175 | 4407 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1533 | 4408 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1361 | 4409 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       199 | 4410 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|       101 | 4411 | `		}else{` |
|      1164 | 4412 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1164 | 4413 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4414 | `		}` |
|      1361 | 4415 | `		rc = 0;` |
|      1361 | 4416 | `		if( nArg > 1 ){` |
|        65 | 4417 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4418 | `			if( pValue ){` |
|         - | 4419 | `				ph7_value sNeedle;` |
|        65 | 4420 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4421 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4422 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4423 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4424 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4425 | `				 * corrupt every later comparison. */` |
|        65 | 4426 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4427 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4428 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4429 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4430 | `			}` |
|        32 | 4431 | `		}` |
|      1361 | 4432 | `		if( rc == 0 ){` |
|         - | 4433 | `			/* Perform the insertion */` |
|      1329 | 4434 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       663 | 4435 | `		}` |
|      1361 | 4436 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4437 | `		/* Point to the next entry */` |
|      1361 | 4438 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       682 | 4439 | `	}` |
|         - | 4440 | `	/* return the new array */` |
|       175 | 4441 | `	ph7_result_value(pCtx,pArray);` |
|       175 | 4442 | `	return PH7_OK;` |
|        91 | 4443 | `}` |
|         - | 4444 | `/*` |
|         - | 4445 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4446 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4447 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4448 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4449 | ` * Parameters` |
|         - | 4450 | ` *  $arr1` |
|         - | 4451 | ` *   First array` |
|         - | 4452 | ` *  $arr2` |
|         - | 4453 | ` *   Second array` |
|         - | 4454 | ` * Return` |
|         - | 4455 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4456 | ` * Note` |
|         - | 4457 | ` *  This function is a symisc eXtension.` |
|         - | 4458 | ` */` |
|         4 | 4459 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4460 | `{` |
|         - | 4461 | `	ph7_hashmap *p1,*p2;` |
|         - | 4462 | `	int rc;` |
|         5 | 4463 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4464 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4465 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4466 | `		return PH7_OK;` |
|         - | 4467 | `	}` |
|         - | 4468 | `	/* Point to the hashmaps */` |
|         5 | 4469 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4470 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4471 | `	rc = (p1 == p2);` |
|         - | 4472 | `	/* Same instance? */` |
|         5 | 4473 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4474 | `	return PH7_OK;` |
|         3 | 4475 | `}` |
|         - | 4476 | `/*` |
|         - | 4477 | ` * array array_merge(array ...$arrays)` |
|         - | 4478 | ` *  Merge one or more arrays.` |
|         - | 4479 | ` * Parameters` |
|         - | 4480 | ` *  ...$arrays` |
|         - | 4481 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4482 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4483 | ` * Return` |
|         - | 4484 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4485 | ` *  with no arguments.` |
|         - | 4486 | ` */` |
|      1126 | 4487 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4488 | `{` |
|         - | 4489 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4490 | `	ph7_value *pArray;` |
|         - | 4491 | `	int i;` |
|         - | 4492 | `	/* Create a new array */` |
|      1131 | 4493 | `	pArray = ph7_context_new_array(pCtx);` |
|      1131 | 4494 | `	if( pArray == 0 ){` |
|       ! 0 | 4495 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4496 | `		return PH7_OK;` |
|         - | 4497 | `	}` |
|         - | 4498 | `	/* Point to the internal representation of the hashmap */` |
|      1131 | 4499 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4500 | `	/* Start merging */` |
|      3359 | 4501 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4502 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2237 | 4503 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4504 | `			/* Type mismatch -> TypeError */` |
|         8 | 4505 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4506 | `				"TypeError",` |
|         - | 4507 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4508 | `				i + 1,` |
|         4 | 4509 | `				ph7_type_name(apArg[i])` |
|         - | 4510 | `				);` |
|       ! 0 | 4511 | `		}else{` |
|      2233 | 4512 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4513 | `			/* Merge the two hashmaps */` |
|      2233 | 4514 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4515 | `		}` |
|      1119 | 4516 | `	}` |
|         - | 4517 | `	/* Return the freshly created array */` |
|      1127 | 4518 | `	ph7_result_value(pCtx,pArray);` |
|      1127 | 4519 | `	return PH7_OK;` |
|       568 | 4520 | `}` |
|         - | 4521 | `/*` |
|         - | 4522 | ` * array array_copy(array $source)` |
|         - | 4523 | ` *  Make a blind copy of the target array.` |
|         - | 4524 | ` * Parameters` |
|         - | 4525 | ` *  $source` |
|         - | 4526 | ` *   Target array` |
|         - | 4527 | ` * Return` |
|         - | 4528 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4529 | ` * Note` |
|         - | 4530 | ` *  This function is a symisc eXtension.` |
|         - | 4531 | ` */` |
|        18 | 4532 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4533 | `{` |
|         - | 4534 | `	ph7_hashmap *pMap;` |
|         - | 4535 | `	ph7_value *pArray;` |
|        19 | 4536 | `	if( nArg < 1 ){` |
|         - | 4537 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4538 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4539 | `		return PH7_OK;` |
|         - | 4540 | `	}` |
|         - | 4541 | `	/* Create a new array */` |
|        19 | 4542 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4543 | `	if( pArray == 0 ){` |
|       ! 0 | 4544 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4545 | `		return PH7_OK;` |
|         - | 4546 | `	}` |
|         - | 4547 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4548 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4549 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4550 | `		/* Point to the internal representation of the source */` |
|        19 | 4551 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4552 | `		/* Perform the copy */` |
|        19 | 4553 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4554 | `	}else{` |
|         - | 4555 | `		/* Simple insertion */` |
|       ! 0 | 4556 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4557 | `	}` |
|         - | 4558 | `	/* Return the duplicated array */` |
|        19 | 4559 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4560 | `	return PH7_OK;` |
|        10 | 4561 | `}` |
|         - | 4562 | `/*` |
|         - | 4563 | ` * bool array_erase(array $source)` |
|         - | 4564 | ` *  Remove all elements from a given array.` |
|         - | 4565 | ` * Parameters` |
|         - | 4566 | ` *  $source` |
|         - | 4567 | ` *   Target array` |
|         - | 4568 | ` * Return` |
|         - | 4569 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4570 | ` * Note` |
|         - | 4571 | ` *  This function is a symisc eXtension.` |
|         - | 4572 | ` */` |
|        26 | 4573 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4574 | `{` |
|         - | 4575 | `	ph7_hashmap *pMap;` |
|        28 | 4576 | `	if( nArg < 1 ){` |
|         - | 4577 | `		/* Missing arguments */` |
|       ! 0 | 4578 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4579 | `		return PH7_OK;` |
|         - | 4580 | `	}` |
|         - | 4581 | `	/* Point to the target hashmap */` |
|        28 | 4582 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4583 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4584 | `	/* Erase */` |
|        28 | 4585 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4586 | `	return PH7_OK;` |
|        15 | 4587 | `}` |
|         - | 4588 | `/*` |
|         - | 4589 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4590 | ` *  Extract a slice of the array.` |
|         - | 4591 | ` * Parameters` |
|         - | 4592 | ` *  $array` |
|         - | 4593 | ` *    The input array.` |
|         - | 4594 | ` * $offset` |
|         - | 4595 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4596 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4597 | ` * $length (optional, nullable)` |
|         - | 4598 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4599 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4600 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4601 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4602 | ` * $preserve_keys (optional)` |
|         - | 4603 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4604 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4605 | ` * Return` |
|         - | 4606 | ` *   The new slice.` |
|         - | 4607 | ` */` |
|        66 | 4608 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4609 | `{` |
|         - | 4610 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4611 | `	ph7_hashmap_node *pCur;` |
|         - | 4612 | `	ph7_value *pArray;` |
|         - | 4613 | `	int iLength,iOfft;` |
|         - | 4614 | `	int bPreserve;` |
|         - | 4615 | `	sxi32 rc;` |
|        71 | 4616 | `	if( nArg < 2 ){` |
|       ! 0 | 4617 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4618 | `			"ArgumentCountError",` |
|         - | 4619 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4620 | `			nArg` |
|         - | 4621 | `			);` |
|         - | 4622 | `	}` |
|        71 | 4623 | `	if( nArg > 4 ){` |
|         4 | 4624 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4625 | `			"ArgumentCountError",` |
|         - | 4626 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4627 | `			nArg` |
|         - | 4628 | `			);` |
|         - | 4629 | `	}` |
|        69 | 4630 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4631 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4632 | `			"TypeError",` |
|         - | 4633 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4634 | `			ph7_type_name(apArg[0])` |
|         - | 4635 | `			);` |
|         - | 4636 | `	}` |
|         - | 4637 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        92 | 4638 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        95 | 4639 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4640 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4641 | `			"TypeError",` |
|         - | 4642 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4643 | `			ph7_type_name(apArg[1])` |
|         - | 4644 | `			);` |
|         - | 4645 | `	}` |
|         - | 4646 | `	/* Validate $length type if provided: nullable int */` |
|        65 | 4647 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        56 | 4648 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        56 | 4649 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4650 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4651 | `				"TypeError",` |
|         - | 4652 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4653 | `				ph7_type_name(apArg[2])` |
|         - | 4654 | `				);` |
|         - | 4655 | `		}` |
|        18 | 4656 | `	}` |
|         - | 4657 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        63 | 4658 | `	if( nArg > 3 ){` |
|         7 | 4659 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4660 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4661 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4662 | `				"TypeError",` |
|         - | 4663 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4664 | `				ph7_type_name(apArg[3])` |
|         - | 4665 | `				);` |
|         - | 4666 | `		}` |
|         2 | 4667 | `	}` |
|         - | 4668 | `	/* Point the internal representation of the target array */` |
|        63 | 4669 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        63 | 4670 | `	bPreserve = FALSE;` |
|         - | 4671 | `	/* Get the offset */` |
|         - | 4672 | `	{` |
|        63 | 4673 | `		sxi64 iTmp = 0;` |
|        63 | 4674 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        63 | 4675 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4676 | `			return rcArg;` |
|         - | 4677 | `		}` |
|        63 | 4678 | `		iOfft = (int)iTmp;` |
|         - | 4679 | `	}` |
|        63 | 4680 | `	if( iOfft < 0 ){` |
|         5 | 4681 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4682 | `		if( iOfft < 0 ){` |
|         3 | 4683 | `			iOfft = 0;` |
|         1 | 4684 | `		}` |
|         2 | 4685 | `	}` |
|        63 | 4686 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4687 | `		/* Offset past end of array, return empty array */` |
|         5 | 4688 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4689 | `		if( pArray == 0 ){` |
|       ! 0 | 4690 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4691 | `			return PH7_OK;` |
|         - | 4692 | `		}` |
|         5 | 4693 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4694 | `		return PH7_OK;` |
|         - | 4695 | `	}` |
|         - | 4696 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        59 | 4697 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        59 | 4698 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        37 | 4699 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        37 | 4700 | `		if( iLength < 0 ){` |
|         5 | 4701 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4702 | `		}` |
|        37 | 4703 | `		if( iLength < 0 ){` |
|         3 | 4704 | `			iLength = 0;` |
|         1 | 4705 | `		}` |
|        37 | 4706 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4707 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4708 | `		}` |
|        18 | 4709 | `	}` |
|        59 | 4710 | `	if( nArg > 3 ){` |
|         5 | 4711 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4712 | `	}` |
|         - | 4713 | `	/* Create a new array */` |
|        59 | 4714 | `	pArray = ph7_context_new_array(pCtx);` |
|        59 | 4715 | `	if( pArray == 0 ){` |
|       ! 0 | 4716 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4717 | `		return PH7_OK;` |
|         - | 4718 | `	}` |
|        59 | 4719 | `	if( iLength < 1 ){` |
|         - | 4720 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4721 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4722 | `		return PH7_OK;` |
|         - | 4723 | `	}` |
|         - | 4724 | `	/* Point to the desired entry */` |
|        55 | 4725 | `	pCur = pSrc->pFirst;` |
|        54 | 4726 | `	for(;;){` |
|       113 | 4727 | `		if( iOfft < 1 ){` |
|        55 | 4728 | `			break;` |
|         - | 4729 | `		}` |
|         - | 4730 | `		/* Point to the next entry */` |
|        63 | 4731 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        63 | 4732 | `		iOfft--;` |
|         5 | 4733 | `	}` |
|         - | 4734 | `	/* Point to the internal representation of the hashmap */` |
|        55 | 4735 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       106 | 4736 | `	for(;;){` |
|       217 | 4737 | `		if( iLength < 1 ){` |
|        55 | 4738 | `			break;` |
|         - | 4739 | `		}` |
|         - | 4740 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4741 | `		{` |
|       167 | 4742 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|       167 | 4743 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4744 | `		}` |
|       167 | 4745 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4746 | `			break;` |
|         - | 4747 | `		}` |
|         - | 4748 | `		/* Point to the next entry */` |
|       167 | 4749 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       167 | 4750 | `		iLength--;` |
|         5 | 4751 | `	}` |
|         - | 4752 | `	/* Return the freshly created array */` |
|        55 | 4753 | `	ph7_result_value(pCtx,pArray);` |
|        55 | 4754 | `	return PH7_OK;` |
|        38 | 4755 | `}` |
|         - | 4756 | `/*` |
|         - | 4757 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4758 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4759 | ` * beginning (becomes the new pFirst).` |
|         - | 4760 | ` */` |
|        38 | 4761 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4762 | `{` |
|         - | 4763 | `	ph7_hashmap_node *pNode;` |
|         - | 4764 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4765 | `	pNode = pMap->pLast;` |
|        39 | 4766 | `	if( pNode == 0 ){` |
|       ! 0 | 4767 | `		return;` |
|         - | 4768 | `	}` |
|        39 | 4769 | `	if( pNode->pNext == 0 ){` |
|         - | 4770 | `		/* Only node in the list, nothing to move */` |
|         5 | 4771 | `		return;` |
|         - | 4772 | `	}` |
|        35 | 4773 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4774 | `		/* Already in the correct position */` |
|         9 | 4775 | `		return;` |
|         - | 4776 | `	}` |
|         - | 4777 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4778 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4779 | `	pMap->pLast->pPrev = 0;` |
|         - | 4780 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4781 | `	if( pAfter == 0 ){` |
|         - | 4782 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4783 | `		pNode->pNext = 0;` |
|         3 | 4784 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4785 | `		if( pMap->pFirst ){` |
|         3 | 4786 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4787 | `		}` |
|         3 | 4788 | `		pMap->pFirst = pNode;` |
|         2 | 4789 | `	}else{` |
|        25 | 4790 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4791 | `		pNode->pPrev = pOldNext;` |
|        25 | 4792 | `		pNode->pNext = pAfter;` |
|        25 | 4793 | `		pAfter->pPrev = pNode;` |
|        25 | 4794 | `		if( pOldNext ){` |
|        25 | 4795 | `			pOldNext->pNext = pNode;` |
|        13 | 4796 | `		}else{` |
|       ! 0 | 4797 | `			pMap->pLast = pNode;` |
|         - | 4798 | `		}` |
|         - | 4799 | `	}` |
|        20 | 4800 | `}` |
|         - | 4801 | `/*` |
|         - | 4802 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4803 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4804 | ` * Parameters` |
|         - | 4805 | ` *  $array` |
|         - | 4806 | ` *    The input array.` |
|         - | 4807 | ` *  $offset` |
|         - | 4808 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4809 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4810 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4811 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4812 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4813 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4814 | ` *  $length (optional)` |
|         - | 4815 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4816 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4817 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4818 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4819 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4820 | ` *  $replacement (optional)` |
|         - | 4821 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4822 | ` *    with elements from this array.` |
|         - | 4823 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4824 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4825 | ` *    offset.` |
|         - | 4826 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4827 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4828 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4829 | ` * Return` |
|         - | 4830 | ` *   A new array consisting of the extracted elements.` |
|         - | 4831 | ` */` |
|        64 | 4832 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4833 | `{` |
|         - | 4834 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4835 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4836 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4837 | `	int iLength,iOfft,i;` |
|         - | 4838 | `	sxi32 rc;` |
|        66 | 4839 | `	if( nArg < 2 ){` |
|       ! 0 | 4840 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4841 | `			"ArgumentCountError",` |
|         - | 4842 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4843 | `			nArg` |
|         - | 4844 | `			);` |
|         - | 4845 | `	}` |
|        66 | 4846 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4847 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4848 | `			"TypeError",` |
|         - | 4849 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4850 | `			ph7_type_name(apArg[0])` |
|         - | 4851 | `			);` |
|         - | 4852 | `	}` |
|         - | 4853 | `	/* Point to the internal representation of the target array */` |
|        63 | 4854 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4855 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4856 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4857 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4858 | `	if( iOfft < 0 ){` |
|         9 | 4859 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4860 | `		if( iOfft < 0 ){` |
|         3 | 4861 | `			iOfft = 0;` |
|         2 | 4862 | `		}` |
|        59 | 4863 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4864 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4865 | `	}` |
|         - | 4866 | `	/* Get the length and clamp to valid range.` |
|         - | 4867 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4868 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4869 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4870 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4871 | `		if( iLength < 0 ){` |
|         7 | 4872 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4873 | `			if( iLength < 0 ){` |
|         3 | 4874 | `				iLength = 0;` |
|         1 | 4875 | `			}` |
|         3 | 4876 | `		}` |
|        45 | 4877 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4878 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4879 | `		}` |
|        22 | 4880 | `	}` |
|         - | 4881 | `	/* Create the result array for removed elements */` |
|        63 | 4882 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4883 | `	if( pArray == 0 ){` |
|       ! 0 | 4884 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4885 | `		return PH7_OK;` |
|         - | 4886 | `	}` |
|         - | 4887 | `	/* Get replacement array if provided */` |
|        63 | 4888 | `	pRep = 0;` |
|        63 | 4889 | `	if( nArg > 3 ){` |
|        27 | 4890 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4891 | `			/* Perform an array cast */` |
|         3 | 4892 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4893 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4894 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4895 | `			}` |
|         2 | 4896 | `		}else{` |
|        25 | 4897 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4898 | `		}` |
|        27 | 4899 | `		if( pRep ){` |
|         - | 4900 | `			/* Reset the loop cursor */` |
|        27 | 4901 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4902 | `		}` |
|        13 | 4903 | `	}` |
|         - | 4904 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4905 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4906 | `	/* Navigate to the offset position */` |
|        63 | 4907 | `	pCur = pSrc->pFirst;` |
|       131 | 4908 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4909 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4910 | `	}` |
|         - | 4911 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4912 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4913 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4914 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4915 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4916 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4917 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4918 | `		pPrev = pCur->pPrev;` |
|        79 | 4919 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4920 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4921 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4922 | `			break;` |
|         - | 4923 | `		}` |
|        79 | 4924 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4925 | `	}` |
|         - | 4926 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4927 | `	if( pRep ){` |
|         - | 4928 | `		ph7_value sSafeVal;` |
|        78 | 4929 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4930 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4931 | `			if( pRvalue ){` |
|         - | 4932 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4933 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4934 | `				 * since it points into that same pool. */` |
|        39 | 4935 | `				sSafeVal = *pRvalue;` |
|        39 | 4936 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4937 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4938 | `					pNewNode = pSrc->pLast;` |
|        39 | 4939 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4940 | `					pInsertAfter = pNewNode;` |
|        19 | 4941 | `				}` |
|        19 | 4942 | `			}` |
|         1 | 4943 | `		}` |
|        13 | 4944 | `	}` |
|         - | 4945 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4946 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4947 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4948 | `	 * and removals left gaps. */` |
|         - | 4949 | `	{` |
|        63 | 4950 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4951 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4952 | `		pSrc->iNextIdx = 0;` |
|       233 | 4953 | `		while( n > 0 ){` |
|       171 | 4954 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4955 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4956 | `			}` |
|       171 | 4957 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4958 | `			n--;` |
|         1 | 4959 | `		}` |
|        63 | 4960 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4961 | `	}` |
|         - | 4962 | `	/* Return the freshly created array */` |
|        63 | 4963 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4964 | `	return PH7_OK;` |
|        34 | 4965 | `}` |
|         - | 4966 | `/*` |
|         - | 4967 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4968 | ` *  Checks if a value exists in an array.` |
|         - | 4969 | ` * Parameters` |
|         - | 4970 | ` *  $needle` |
|         - | 4971 | ` *   The searched value.` |
|         - | 4972 | ` *   Note:` |
|         - | 4973 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4974 | ` * $haystack` |
|         - | 4975 | ` *  The target array.` |
|         - | 4976 | ` * $strict` |
|         - | 4977 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4978 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4979 | ` */` |
|     33894 | 4980 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4981 | `{` |
|         - | 4982 | `	ph7_value *pNeedle;` |
|         - | 4983 | `	int bStrict;` |
|         - | 4984 | `	int rc;` |
|     33899 | 4985 | `	if( nArg < 2 ){` |
|         - | 4986 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4987 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4988 | `		return PH7_OK;` |
|         - | 4989 | `	}` |
|     33899 | 4990 | `	pNeedle = apArg[0];` |
|     33899 | 4991 | `	bStrict = 0;` |
|     33899 | 4992 | `	if( nArg > 2 ){` |
|        58 | 4993 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        28 | 4994 | `	}` |
|     33899 | 4995 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4996 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4997 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4998 | `		/* Set the comparison result */` |
|       ! 0 | 4999 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 5000 | `		return PH7_OK;` |
|         - | 5001 | `	}` |
|         - | 5002 | `	/* Perform the lookup */` |
|     33899 | 5003 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 5004 | `	/* Lookup result */` |
|     33899 | 5005 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33899 | 5006 | `	return PH7_OK;` |
|     16952 | 5007 | `}` |
|         - | 5008 | `/*` |
|         - | 5009 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 5010 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 5011 | ` * Parameters` |
|         - | 5012 | ` * $needle` |
|         - | 5013 | ` *   The searched value.` |
|         - | 5014 | ` * $haystack` |
|         - | 5015 | ` *   The array.` |
|         - | 5016 | ` * $strict` |
|         - | 5017 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 5018 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 5019 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 5020 | ` * Return` |
|         - | 5021 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 5022 | ` */` |
|        28 | 5023 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 5024 | `{` |
|         - | 5025 | `	ph7_hashmap_node *pEntry;` |
|         - | 5026 | `	ph7_value *pVal,sNeedle;` |
|         - | 5027 | `	ph7_hashmap *pMap;` |
|         - | 5028 | `	ph7_value sVal;` |
|         - | 5029 | `	int bStrict;` |
|         - | 5030 | `	sxu32 n;` |
|         - | 5031 | `	int rc;` |
|        30 | 5032 | `	if( nArg < 2 ){` |
|         - | 5033 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 5034 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5035 | `			"ArgumentCountError",` |
|         - | 5036 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 5037 | `			nArg` |
|         - | 5038 | `			);` |
|         - | 5039 | `	}` |
|        30 | 5040 | `	bStrict = FALSE;` |
|        30 | 5041 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 5042 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 5043 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5044 | `			"TypeError",` |
|         - | 5045 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 5046 | `			ph7_type_name(apArg[1])` |
|         - | 5047 | `			);` |
|         - | 5048 | `	}` |
|        27 | 5049 | `	if( nArg > 2 ){` |
|         - | 5050 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        13 | 5051 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 5052 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5053 | `				"TypeError",` |
|         - | 5054 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 5055 | `				ph7_type_name(apArg[2])` |
|         - | 5056 | `				);` |
|         - | 5057 | `		}` |
|        13 | 5058 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         6 | 5059 | `	}` |
|         - | 5060 | `	/* Point to the internal representation of the internal hashmap */` |
|        27 | 5061 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 5062 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        27 | 5063 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        27 | 5064 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        27 | 5065 | `	pEntry = pMap->pFirst;` |
|        27 | 5066 | `	n = pMap->nEntry;` |
|        29 | 5067 | `	for(;;){` |
|        59 | 5068 | `		if( !n ){` |
|         9 | 5069 | `			break;` |
|         - | 5070 | `		}` |
|         - | 5071 | `		/* Extract node value */` |
|        51 | 5072 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        51 | 5073 | `		if( pVal ){` |
|         - | 5074 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 5075 | `			 * can change their type.` |
|         - | 5076 | `			 */` |
|        51 | 5077 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        51 | 5078 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        51 | 5079 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        51 | 5080 | `			PH7_MemObjRelease(&sVal);` |
|        51 | 5081 | `			PH7_MemObjRelease(&sNeedle);` |
|        51 | 5082 | `			if( rc == 0 ){` |
|         - | 5083 | `				/* Match found,return key */` |
|        19 | 5084 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 5085 | `					/* INT key */` |
|        13 | 5086 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         7 | 5087 | `				}else{` |
|         7 | 5088 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5089 | `					/* Blob key */` |
|         7 | 5090 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 5091 | `				}` |
|        19 | 5092 | `				return PH7_OK;` |
|         - | 5093 | `			}` |
|        16 | 5094 | `		}` |
|         - | 5095 | `		/* Point to the next entry */` |
|        33 | 5096 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5097 | `		n--;` |
|         1 | 5098 | `	}` |
|         - | 5099 | `	/* No such value,return FALSE */` |
|         9 | 5100 | `	ph7_result_bool(pCtx,0);` |
|         9 | 5101 | `	return PH7_OK;` |
|        16 | 5102 | `}` |
|         - | 5103 | `/*` |
|         - | 5104 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 5105 | ` *  Computes the difference of arrays.` |
|         - | 5106 | ` * Parameters` |
|         - | 5107 | ` *  $array1` |
|         - | 5108 | ` *    The array to compare from` |
|         - | 5109 | ` *  $array2` |
|         - | 5110 | ` *    An array to compare against` |
|         - | 5111 | ` *  $...` |
|         - | 5112 | ` *   More arrays to compare against` |
|         - | 5113 | ` * Return` |
|         - | 5114 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5115 | ` *  are not present in any of the other arrays.` |
|         - | 5116 | ` */` |
|        20 | 5117 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5118 | `{` |
|         - | 5119 | `	ph7_hashmap_node *pEntry;` |
|         - | 5120 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5121 | `	ph7_value *pArray;` |
|         - | 5122 | `	ph7_value *pVal;` |
|         - | 5123 | `	sxi32 rc;` |
|         - | 5124 | `	sxu32 n;` |
|         - | 5125 | `	int i;` |
|         - | 5126 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5127 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5128 | `	 * debugging difficult. */` |
|        23 | 5129 | `	if( nArg < 1 ){` |
|       ! 0 | 5130 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5131 | `			"ArgumentCountError",` |
|         - | 5132 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5133 | `			nArg` |
|         - | 5134 | `			);` |
|         - | 5135 | `	}` |
|        23 | 5136 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5137 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5138 | `			"TypeError",` |
|         - | 5139 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5140 | `			ph7_type_name(apArg[0])` |
|         - | 5141 | `			);` |
|         - | 5142 | `	}` |
|        36 | 5143 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5144 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5145 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5146 | `				"TypeError",` |
|         - | 5147 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5148 | `				i + 1,` |
|         2 | 5149 | `				ph7_type_name(apArg[i])` |
|         - | 5150 | `				);` |
|         - | 5151 | `		}` |
|         9 | 5152 | `	}` |
|        17 | 5153 | `	if( nArg == 1 ){` |
|         - | 5154 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5155 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5156 | `		return PH7_OK;` |
|         - | 5157 | `	}` |
|         - | 5158 | `	/* Create a new array */` |
|        15 | 5159 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5160 | `	if( pArray == 0 ){` |
|       ! 0 | 5161 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5162 | `		return PH7_OK;` |
|         - | 5163 | `	}` |
|         - | 5164 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5165 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5166 | `	/* Perform the diff */` |
|        15 | 5167 | `	pEntry = pSrc->pFirst;` |
|        15 | 5168 | `	n = pSrc->nEntry;` |
|        27 | 5169 | `	for(;;){` |
|        55 | 5170 | `		if( n < 1 ){` |
|        15 | 5171 | `			break;` |
|         - | 5172 | `		}` |
|         - | 5173 | `		/* Extract the node value */` |
|        41 | 5174 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5175 | `		if( pVal ){` |
|        69 | 5176 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5177 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5178 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5179 | `				/* Perform the lookup */` |
|        45 | 5180 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5181 | `				if( rc == SXRET_OK ){` |
|         - | 5182 | `					/* Value exist */` |
|        17 | 5183 | `					break;` |
|         - | 5184 | `				}` |
|        15 | 5185 | `			}` |
|        41 | 5186 | `			if( i >= nArg ){` |
|         - | 5187 | `				/* Perform the insertion */` |
|        25 | 5188 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5189 | `			}` |
|        20 | 5190 | `		}` |
|         - | 5191 | `		/* Point to the next entry */` |
|        41 | 5192 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5193 | `		n--;` |
|         1 | 5194 | `	}` |
|         - | 5195 | `	/* Return the freshly created array */` |
|        15 | 5196 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5197 | `	return PH7_OK;` |
|        13 | 5198 | `}` |
|         - | 5199 | `/*` |
|         - | 5200 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5201 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5202 | ` * Parameters` |
|         - | 5203 | ` *  $array1` |
|         - | 5204 | ` *    The array to compare from` |
|         - | 5205 | ` *  $array2` |
|         - | 5206 | ` *    An array to compare against` |
|         - | 5207 | ` *  $...` |
|         - | 5208 | ` *   More arrays to compare against.` |
|         - | 5209 | ` * $callback` |
|         - | 5210 | ` *  The callback comparison function.` |
|         - | 5211 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5212 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5213 | ` *  than the second.` |
|         - | 5214 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5215 | ` * Return` |
|         - | 5216 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5217 | ` *  are not present in any of the other arrays.` |
|         - | 5218 | ` */` |
|        20 | 5219 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5220 | `{` |
|         - | 5221 | `	ph7_hashmap_node *pEntry;` |
|         - | 5222 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5223 | `	ph7_value *pCallback;` |
|         - | 5224 | `	ph7_value *pArray;` |
|         - | 5225 | `	ph7_value *pVal;` |
|         - | 5226 | `	sxi32 rc;` |
|         - | 5227 | `	sxu32 n;` |
|         - | 5228 | `	int i;` |
|         - | 5229 |  |
|         - | 5230 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5231 | `	if( nArg < 2 ){` |
|       ! 0 | 5232 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5233 | `			"ArgumentCountError",` |
|         - | 5234 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5235 | `			nArg` |
|         - | 5236 | `			);` |
|         - | 5237 | `	}` |
|        25 | 5238 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5239 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5240 | `			"TypeError",` |
|         - | 5241 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5242 | `			ph7_type_name(apArg[0])` |
|         - | 5243 | `			);` |
|         - | 5244 | `	}` |
|         - | 5245 |  |
|        23 | 5246 | `	if( nArg == 2 ){` |
|         - | 5247 | `		/* Only the original array and the callback were provided. */` |
|         - | 5248 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5249 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5250 | `		 * validation order.` |
|         - | 5251 | `		 */` |
|         4 | 5252 | `	} else {` |
|         - | 5253 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5254 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5255 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5256 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5257 | `					"TypeError",` |
|         - | 5258 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5259 | `					i + 1,` |
|         6 | 5260 | `					ph7_type_name(apArg[i])` |
|         - | 5261 | `					);` |
|         - | 5262 | `			}` |
|         7 | 5263 | `		}` |
|         - | 5264 | `	}` |
|         - | 5265 |  |
|         - | 5266 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5267 | `	pCallback = apArg[nArg - 1];` |
|         - | 5268 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5269 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5270 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5271 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5272 | `				"TypeError",` |
|         - | 5273 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5274 | `				nArg` |
|         - | 5275 | `				);` |
|         - | 5276 | `		}` |
|         6 | 5277 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5278 | `			int len;` |
|         3 | 5279 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5280 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5281 | `				"TypeError",` |
|         - | 5282 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5283 | `				nArg,` |
|         1 | 5284 | `				zName` |
|         - | 5285 | `				);` |
|         - | 5286 | `		}` |
|         4 | 5287 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5288 | `			"TypeError",` |
|         - | 5289 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5290 | `			nArg` |
|         - | 5291 | `			);` |
|         - | 5292 | `	}` |
|         - | 5293 |  |
|         7 | 5294 | `	if( nArg == 2 ){` |
|         - | 5295 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5296 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5297 | `		return PH7_OK;` |
|         - | 5298 | `	}` |
|         - | 5299 |  |
|         - | 5300 | `	/* Create a new array */` |
|         5 | 5301 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5302 | `	if( pArray == 0 ){` |
|       ! 0 | 5303 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5304 | `		return PH7_OK;` |
|         - | 5305 | `	}` |
|         - | 5306 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5307 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5308 | `	/* Perform the diff */` |
|         5 | 5309 | `	pEntry = pSrc->pFirst;` |
|         5 | 5310 | `	n = pSrc->nEntry;` |
|         5 | 5311 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5312 | `	for(;;){` |
|        11 | 5313 | `		if( n < 1 ){` |
|         3 | 5314 | `			break;` |
|         - | 5315 | `		}` |
|         - | 5316 | `		/* Extract the node value */` |
|         9 | 5317 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5318 | `		if( pVal ){` |
|        15 | 5319 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5320 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5321 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5322 | `				/* Perform the lookup */` |
|         9 | 5323 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5324 | `				if( rc == SXRET_OK ){` |
|         - | 5325 | `					/* Value exist */` |
|         3 | 5326 | `					break;` |
|         - | 5327 | `				}` |
|         4 | 5328 | `			}` |
|         9 | 5329 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5330 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5331 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5332 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5333 | `				return PH7_EXCEPTION;` |
|         - | 5334 | `			}` |
|         7 | 5335 | `			if( i >= (nArg - 1)){` |
|         - | 5336 | `				/* Perform the insertion */` |
|         5 | 5337 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5338 | `			}` |
|         3 | 5339 | `		}` |
|         - | 5340 | `		/* Point to the next entry */` |
|         7 | 5341 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5342 | `		n--;` |
|         1 | 5343 | `	}` |
|         - | 5344 | `	/* Return the freshly created array */` |
|         3 | 5345 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5346 | `	return PH7_OK;` |
|        15 | 5347 | `}` |
|         - | 5348 | `/*` |
|         - | 5349 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5350 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5351 | ` * Parameters` |
|         - | 5352 | ` *  $array1` |
|         - | 5353 | ` *    The array to compare from` |
|         - | 5354 | ` *  $array2` |
|         - | 5355 | ` *    An array to compare against` |
|         - | 5356 | ` *  $...` |
|         - | 5357 | ` *   More arrays to compare against` |
|         - | 5358 | ` * Return` |
|         - | 5359 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5360 | ` *  are not present in any of the other arrays.` |
|         - | 5361 | ` */` |
|        20 | 5362 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5363 | `{` |
|         - | 5364 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5365 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5366 | `	ph7_value *pArray;` |
|         - | 5367 | `	ph7_value *pVal;` |
|         - | 5368 | `	sxi32 rc;` |
|         - | 5369 | `	sxu32 n;` |
|         - | 5370 | `	int i;` |
|         - | 5371 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5372 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5373 | `	 * accompanying integration tests to pass. */` |
|        24 | 5374 | `	if( nArg < 1 ){` |
|       ! 0 | 5375 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5376 | `			"ArgumentCountError",` |
|         - | 5377 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5378 | `			nArg` |
|         - | 5379 | `			);` |
|         - | 5380 | `	}` |
|        24 | 5381 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5382 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5383 | `			"TypeError",` |
|         - | 5384 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5385 | `			ph7_type_name(apArg[0])` |
|         - | 5386 | `			);` |
|         - | 5387 | `	}` |
|        37 | 5388 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5389 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5390 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5391 | `				"TypeError",` |
|         - | 5392 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5393 | `				i + 1,` |
|         4 | 5394 | `				ph7_type_name(apArg[i])` |
|         - | 5395 | `				);` |
|         - | 5396 | `		}` |
|        10 | 5397 | `	}` |
|        15 | 5398 | `	if( nArg == 1 ){` |
|         - | 5399 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5400 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5401 | `		return PH7_OK;` |
|         - | 5402 | `	}` |
|         - | 5403 | `	/* Create a new array */` |
|        13 | 5404 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5405 | `	if( pArray == 0 ){` |
|       ! 0 | 5406 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5407 | `		return PH7_OK;` |
|         - | 5408 | `	}` |
|         - | 5409 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5410 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5411 | `	/* Perform the diff */` |
|        13 | 5412 | `	pEntry = pSrc->pFirst;` |
|        13 | 5413 | `	n = pSrc->nEntry;` |
|        13 | 5414 | `	pN1 = pN2 = 0;` |
|        34 | 5415 | `	for(;;){` |
|         - | 5416 | `		int keep;` |
|        41 | 5417 | `		if( n < 1 ){` |
|        13 | 5418 | `			break;` |
|         - | 5419 | `		}` |
|         - | 5420 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5421 | `		keep = 1;` |
|        47 | 5422 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5423 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5424 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5425 | `			/* Perform a key lookup first */` |
|        33 | 5426 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5427 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5428 | `			}else{` |
|        21 | 5429 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5430 | `			}` |
|        33 | 5431 | `			if( rc != SXRET_OK ){` |
|         - | 5432 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5433 | `				continue;` |
|         - | 5434 | `			}` |
|         - | 5435 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5436 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5437 | `			if( pVal ){` |
|         - | 5438 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5439 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5440 | `				if( pVal2 ){` |
|         - | 5441 | `					ph7_value sV1,sV2;` |
|         - | 5442 | `					sxi32 cmp;` |
|         - | 5443 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5444 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5445 | `					 * null element used to come back bool(false) in the` |
|         - | 5446 | `					 * caller's array). */` |
|        17 | 5447 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5448 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5449 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5450 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5451 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5452 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5453 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5454 | `					if( cmp == 0 ){` |
|         - | 5455 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5456 | `						keep = 0;` |
|        15 | 5457 | `						break;` |
|         - | 5458 | `					}` |
|         1 | 5459 | `				}` |
|         1 | 5460 | `			}` |
|         2 | 5461 | `		}` |
|        29 | 5462 | `		if( keep ){` |
|         - | 5463 | `			/* Perform the insertion */` |
|        15 | 5464 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5465 | `		}` |
|         - | 5466 | `		/* Point to the next entry */` |
|        29 | 5467 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5468 | `		n--;` |
|         1 | 5469 | `	}` |
|         - | 5470 | `	/* Return the freshly created array */` |
|        13 | 5471 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5472 | `	return PH7_OK;` |
|        14 | 5473 | `}` |
|         - | 5474 | `/*` |
|         - | 5475 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5476 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5477 | ` *  by a user supplied callback function.` |
|         - | 5478 | ` * Parameters` |
|         - | 5479 | ` *  $array1` |
|         - | 5480 | ` *    The array to compare from` |
|         - | 5481 | ` *  $array2` |
|         - | 5482 | ` *    An array to compare against` |
|         - | 5483 | ` *  $...` |
|         - | 5484 | ` *   More arrays to compare against.` |
|         - | 5485 | ` *  $key_compare_func` |
|         - | 5486 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5487 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5488 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5489 | ` * Return` |
|         - | 5490 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5491 | ` *  are not present in any of the other arrays.` |
|         - | 5492 | ` */` |
|        22 | 5493 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5494 | `{` |
|         - | 5495 | `	ph7_hashmap_node *pEntry;` |
|         - | 5496 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5497 | `	ph7_value *pCallback;` |
|         - | 5498 | `	ph7_value *pArray;` |
|         - | 5499 | `	sxi32 rc;` |
|         - | 5500 | `	sxu32 n;` |
|         - | 5501 | `	int i;` |
|         - | 5502 |  |
|         - | 5503 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5504 | `	if( nArg < 2 ){` |
|       ! 0 | 5505 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5506 | `			"ArgumentCountError",` |
|         - | 5507 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5508 | `			nArg` |
|         - | 5509 | `			);` |
|         - | 5510 | `	}` |
|        26 | 5511 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5512 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5513 | `			"TypeError",` |
|         - | 5514 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5515 | `			ph7_type_name(apArg[0])` |
|         - | 5516 | `			);` |
|         - | 5517 | `	}` |
|         - | 5518 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5519 | `	 * expected to be a callback. */` |
|        38 | 5520 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5521 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5522 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5523 | `				"TypeError",` |
|         - | 5524 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5525 | `				i + 1,` |
|         2 | 5526 | `				ph7_type_name(apArg[i])` |
|         - | 5527 | `				);` |
|         - | 5528 | `		}` |
|         9 | 5529 | `	}` |
|         - | 5530 | `	/* Point to the callback value */` |
|        22 | 5531 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5532 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5533 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5534 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5535 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5536 | `		 * string given" which we also reproduce. */` |
|         9 | 5537 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5538 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5539 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5540 | `				"TypeError",` |
|         - | 5541 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5542 | `				nArg` |
|         - | 5543 | `				);` |
|         - | 5544 | `		}` |
|         6 | 5545 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5546 | `			/* neither array nor string */` |
|         8 | 5547 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5548 | `				"TypeError",` |
|         - | 5549 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5550 | `				nArg` |
|         - | 5551 | `				);` |
|         - | 5552 | `		}` |
|         - | 5553 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5554 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5555 | `			"TypeError",` |
|         - | 5556 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5557 | `			nArg,` |
|       ! 0 | 5558 | `			ph7_type_name(pCallback)` |
|         - | 5559 | `			);` |
|         - | 5560 | `	}` |
|        13 | 5561 | `	if( nArg == 2 ){` |
|         - | 5562 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5563 | `		 * input array. */` |
|         3 | 5564 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5565 | `		return PH7_OK;` |
|         - | 5566 | `	}` |
|         - | 5567 | `	/* Create a new array */` |
|        11 | 5568 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5569 | `	if( pArray == 0 ){` |
|       ! 0 | 5570 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5571 | `		return PH7_OK;` |
|         - | 5572 | `	}` |
|         - | 5573 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5574 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5575 | `	/* Perform the diff */` |
|        11 | 5576 | `	pEntry = pSrc->pFirst;` |
|        11 | 5577 | `	n = pSrc->nEntry;` |
|        21 | 5578 | `	for(;;){` |
|         - | 5579 | `		int keep;` |
|        27 | 5580 | `		if( n < 1 ){` |
|         9 | 5581 | `			break;` |
|         - | 5582 | `		}` |
|        19 | 5583 | `		keep = 1;` |
|        31 | 5584 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5585 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5586 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5587 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5588 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5589 | `			while( pIt ){` |
|         - | 5590 | `				/* build temporary key values for callback */` |
|         - | 5591 | `				ph7_value key1, key2, result;` |
|         - | 5592 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5593 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5594 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5595 | `				}else{` |
|         - | 5596 | `					SyString sStr;` |
|        33 | 5597 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5598 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5599 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5600 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5601 | `				}` |
|        33 | 5602 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5603 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5604 | `				}else{` |
|         - | 5605 | `					SyString sStr;` |
|        33 | 5606 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5607 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5608 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5609 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5610 | `				}` |
|        33 | 5611 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5612 | `				/* call user callback with (key1, key2) */` |
|         - | 5613 | `				{` |
|         - | 5614 | `					ph7_value *apK[2];` |
|        33 | 5615 | `					apK[0] = &key1;` |
|        33 | 5616 | `					apK[1] = &key2;` |
|        33 | 5617 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5618 | `				}` |
|        33 | 5619 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5620 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5621 | `					 * array_uintersect (which signal back from` |
|         - | 5622 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5623 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5624 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5625 | `					PH7_MemObjRelease(&result);` |
|         3 | 5626 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5627 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5628 | `					return PH7_EXCEPTION;` |
|         - | 5629 | `				}` |
|        31 | 5630 | `				if( rc == SXRET_OK ){` |
|        31 | 5631 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5632 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5633 | `					}` |
|        31 | 5634 | `					if( result.x.iVal == 0 ){` |
|         - | 5635 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5636 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5637 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5638 | `						if( pVal1 && pVal2 ){` |
|         - | 5639 | `							ph7_value sV1,sV2;` |
|         - | 5640 | `							sxi32 cmp;` |
|         - | 5641 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5642 | `							 * place and these are LIVE array elements. */` |
|        13 | 5643 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5644 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5645 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5646 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5647 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5648 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5649 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5650 | `							if( cmp == 0 ){` |
|         9 | 5651 | `								keep = 0;` |
|         9 | 5652 | `								PH7_MemObjRelease(&result);` |
|         - | 5653 | `								/* release keys too before breaking */` |
|         9 | 5654 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5655 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5656 | `								break;` |
|         - | 5657 | `							}` |
|         2 | 5658 | `						}` |
|         2 | 5659 | `					}` |
|        11 | 5660 | `				}` |
|        23 | 5661 | `				PH7_MemObjRelease(&result);` |
|        23 | 5662 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5663 | `				PH7_MemObjRelease(&key2);` |
|         - | 5664 | `				/* move to next node */` |
|        23 | 5665 | `				pIt = pIt->pPrev;` |
|        23 | 5666 | `				if( keep == 0 ) break;` |
|         1 | 5667 | `			}` |
|        21 | 5668 | `			if( keep == 0 ) break;` |
|         7 | 5669 | `		}` |
|        17 | 5670 | `		if( keep ){` |
|         - | 5671 | `			/* Perform the insertion */` |
|         9 | 5672 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5673 | `		}` |
|         - | 5674 | `		/* Point to the next entry */` |
|        17 | 5675 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5676 | `		n--;` |
|         1 | 5677 | `	}` |
|         - | 5678 | `	/* Return the freshly created array */` |
|         9 | 5679 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5680 | `	return PH7_OK;` |
|        15 | 5681 | `}` |
|         - | 5682 | `/*` |
|         - | 5683 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5684 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5685 | ` * Parameters` |
|         - | 5686 | ` *  $array1` |
|         - | 5687 | ` *    The array to compare from` |
|         - | 5688 | ` *  $array2` |
|         - | 5689 | ` *    An array to compare against` |
|         - | 5690 | ` *  $...` |
|         - | 5691 | ` *   More arrays to compare against` |
|         - | 5692 | ` * Return` |
|         - | 5693 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5694 | ` *  in any of the other arrays.` |
|         - | 5695 | ` * Note that NULL is returned on failure.` |
|         - | 5696 | ` */` |
|        12 | 5697 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5698 | `{` |
|         - | 5699 | `	ph7_hashmap_node *pEntry;` |
|         - | 5700 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5701 | `	ph7_value *pArray;` |
|         - | 5702 | `	sxi32 rc;` |
|         - | 5703 | `	sxu32 n;` |
|         - | 5704 | `	int i;` |
|         - | 5705 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5706 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5707 | `	 * helpers. */` |
|        15 | 5708 | `	if( nArg < 1 ){` |
|       ! 0 | 5709 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5710 | `			"ArgumentCountError",` |
|         - | 5711 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5712 | `			nArg` |
|         - | 5713 | `			);` |
|         - | 5714 | `	}` |
|        15 | 5715 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5716 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5717 | `			"TypeError",` |
|         - | 5718 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5719 | `			ph7_type_name(apArg[0])` |
|         - | 5720 | `			);` |
|         - | 5721 | `	}` |
|        20 | 5722 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5723 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5724 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5725 | `				"TypeError",` |
|         - | 5726 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5727 | `				i + 1,` |
|         2 | 5728 | `				ph7_type_name(apArg[i])` |
|         - | 5729 | `				);` |
|         - | 5730 | `		}` |
|         5 | 5731 | `	}` |
|         9 | 5732 | `	if( nArg == 1 ){` |
|         - | 5733 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5734 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5735 | `		return PH7_OK;` |
|         - | 5736 | `	}` |
|         - | 5737 | `	/* Create a new array */` |
|         7 | 5738 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5739 | `	if( pArray == 0 ){` |
|       ! 0 | 5740 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5741 | `		return PH7_OK;` |
|         - | 5742 | `	}` |
|         - | 5743 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5744 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5745 | `	/* Perfrom the diff */` |
|         7 | 5746 | `	pEntry = pSrc->pFirst;` |
|         7 | 5747 | `	n = pSrc->nEntry;` |
|        12 | 5748 | `	for(;;){` |
|        25 | 5749 | `		if( n < 1 ){` |
|         7 | 5750 | `			break;` |
|         - | 5751 | `		}` |
|        31 | 5752 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5753 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5754 | `				/* ignore */` |
|       ! 0 | 5755 | `				continue;` |
|         - | 5756 | `			}` |
|        23 | 5757 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5758 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5759 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5760 | `				/* Blob lookup */` |
|        17 | 5761 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5762 | `			}else{` |
|         - | 5763 | `				/* Int lookup */` |
|         7 | 5764 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5765 | `			}` |
|        23 | 5766 | `			if( rc == SXRET_OK ){` |
|         - | 5767 | `				/* Key exists,break immediately */` |
|        11 | 5768 | `				break;` |
|         - | 5769 | `			}` |
|         7 | 5770 | `		}` |
|        19 | 5771 | `		if( i >= nArg ){` |
|         - | 5772 | `			/* Perform the insertion */` |
|         9 | 5773 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5774 | `		}` |
|         - | 5775 | `		/* Point to the next entry */` |
|        19 | 5776 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5777 | `		n--;` |
|         1 | 5778 | `	}` |
|         - | 5779 | `	/* Return the freshly created array */` |
|         7 | 5780 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5781 | `	return PH7_OK;` |
|         9 | 5782 | `}` |
|         - | 5783 | `/*` |
|         - | 5784 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5785 | ` *  Computes the intersection of arrays.` |
|         - | 5786 | ` * Parameters` |
|         - | 5787 | ` *  $array1` |
|         - | 5788 | ` *    The array to compare from` |
|         - | 5789 | ` *  $array2` |
|         - | 5790 | ` *    An array to compare against` |
|         - | 5791 | ` *  $...` |
|         - | 5792 | ` *   More arrays to compare against` |
|         - | 5793 | ` * Return` |
|         - | 5794 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5795 | ` *  in all of the parameters.` |
|         - | 5796 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5797 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5798 | ` */` |
|        20 | 5799 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5800 | `{` |
|         - | 5801 | `	ph7_hashmap_node *pEntry;` |
|         - | 5802 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5803 | `	ph7_value *pArray;` |
|         - | 5804 | `	ph7_value *pVal;` |
|         - | 5805 | `	sxi32 rc;` |
|         - | 5806 | `	sxu32 n;` |
|         - | 5807 | `	int i;` |
|        23 | 5808 | `	if( nArg < 1 ){` |
|       ! 0 | 5809 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5810 | `			"ArgumentCountError",` |
|         - | 5811 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5812 | `			nArg` |
|         - | 5813 | `			);` |
|         - | 5814 | `	}` |
|        23 | 5815 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5816 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5817 | `			"TypeError",` |
|         - | 5818 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5819 | `			ph7_type_name(apArg[0])` |
|         - | 5820 | `			);` |
|         - | 5821 | `	}` |
|        36 | 5822 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5823 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5824 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5825 | `				"TypeError",` |
|         - | 5826 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5827 | `				i + 1,` |
|         2 | 5828 | `				ph7_type_name(apArg[i])` |
|         - | 5829 | `				);` |
|         - | 5830 | `		}` |
|         9 | 5831 | `	}` |
|        17 | 5832 | `	if( nArg == 1 ){` |
|         - | 5833 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5834 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5835 | `		return PH7_OK;` |
|         - | 5836 | `	}` |
|         - | 5837 | `	/* Create a new array */` |
|        15 | 5838 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5839 | `	if( pArray == 0 ){` |
|       ! 0 | 5840 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5841 | `		return PH7_OK;` |
|         - | 5842 | `	}` |
|         - | 5843 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5844 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5845 | `	/* Perform the intersection */` |
|        15 | 5846 | `	pEntry = pSrc->pFirst;` |
|        15 | 5847 | `	n = pSrc->nEntry;` |
|        31 | 5848 | `	for(;;){` |
|        63 | 5849 | `		if( n < 1 ){` |
|        15 | 5850 | `			break;` |
|         - | 5851 | `		}` |
|         - | 5852 | `		/* Extract the node value */` |
|        49 | 5853 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5854 | `		if( pVal ){` |
|        79 | 5855 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5856 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5857 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5858 | `				/* Perform the lookup */` |
|        55 | 5859 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5860 | `				if( rc != SXRET_OK ){` |
|         - | 5861 | `					/* Value does not exist */` |
|        25 | 5862 | `					break;` |
|         - | 5863 | `				}` |
|        16 | 5864 | `			}` |
|        49 | 5865 | `			if( i >= nArg ){` |
|         - | 5866 | `				/* Perform the insertion */` |
|        25 | 5867 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5868 | `			}` |
|        24 | 5869 | `		}` |
|         - | 5870 | `		/* Point to the next entry */` |
|        49 | 5871 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5872 | `		n--;` |
|         1 | 5873 | `	}` |
|         - | 5874 | `	/* Return the freshly created array */` |
|        15 | 5875 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5876 | `	return PH7_OK;` |
|        13 | 5877 | `}` |
|         - | 5878 | `/*` |
|         - | 5879 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5880 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5881 | ` * Parameters` |
|         - | 5882 | ` *  $array1` |
|         - | 5883 | ` *    The array to compare from` |
|         - | 5884 | ` *  $array2` |
|         - | 5885 | ` *    An array to compare against` |
|         - | 5886 | ` *  $...` |
|         - | 5887 | ` *   More arrays to compare against` |
|         - | 5888 | ` * Return` |
|         - | 5889 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5890 | ` *  in all the arguments, with matching keys.` |
|         - | 5891 | ` */` |
|        20 | 5892 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5893 | `{` |
|         - | 5894 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5895 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5896 | `	ph7_value *pArray;` |
|         - | 5897 | `	ph7_value *pVal;` |
|         - | 5898 | `	sxi32 rc;` |
|         - | 5899 | `	sxu32 n;` |
|         - | 5900 | `	int i;` |
|        23 | 5901 | `	if( nArg < 1 ){` |
|       ! 0 | 5902 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5903 | `			"ArgumentCountError",` |
|         - | 5904 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5905 | `			nArg` |
|         - | 5906 | `			);` |
|         - | 5907 | `	}` |
|        23 | 5908 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5909 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5910 | `			"TypeError",` |
|         - | 5911 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5912 | `			ph7_type_name(apArg[0])` |
|         - | 5913 | `			);` |
|         - | 5914 | `	}` |
|        36 | 5915 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5916 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5917 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5918 | `				"TypeError",` |
|         - | 5919 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5920 | `				i + 1,` |
|         2 | 5921 | `				ph7_type_name(apArg[i])` |
|         - | 5922 | `				);` |
|         - | 5923 | `		}` |
|         9 | 5924 | `	}` |
|        17 | 5925 | `	if( nArg == 1 ){` |
|         - | 5926 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5927 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5928 | `		return PH7_OK;` |
|         - | 5929 | `	}` |
|         - | 5930 | `	/* Create a new array */` |
|        15 | 5931 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5932 | `	if( pArray == 0 ){` |
|       ! 0 | 5933 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5934 | `		return PH7_OK;` |
|         - | 5935 | `	}` |
|         - | 5936 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5937 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5938 | `	/* Perform the intersection */` |
|        15 | 5939 | `	pEntry = pSrc->pFirst;` |
|        15 | 5940 | `	n = pSrc->nEntry;` |
|        15 | 5941 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5942 | `	for(;;){` |
|        47 | 5943 | `		if( n < 1 ){` |
|        15 | 5944 | `			break;` |
|         - | 5945 | `		}` |
|         - | 5946 | `		/* Extract the node value */` |
|        33 | 5947 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5948 | `		if( pVal ){` |
|        53 | 5949 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5950 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5951 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5952 | `				/* Perform a key lookup first */` |
|        37 | 5953 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5954 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5955 | `				}else{` |
|        23 | 5956 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5957 | `				}` |
|        37 | 5958 | `				if( rc != SXRET_OK ){` |
|         - | 5959 | `					/* No such key,break immediately */` |
|         7 | 5960 | `					break;` |
|         - | 5961 | `				}` |
|         - | 5962 | `				/* Perform the lookup */` |
|        31 | 5963 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5964 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5965 | `					/* Value does not exist */` |
|         6 | 5966 | `					break;` |
|         - | 5967 | `				}` |
|        11 | 5968 | `			}` |
|        33 | 5969 | `			if( i >= nArg ){` |
|         - | 5970 | `				/* Perform the insertion */` |
|        17 | 5971 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5972 | `			}` |
|        16 | 5973 | `		}` |
|         - | 5974 | `		/* Point to the next entry */` |
|        33 | 5975 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5976 | `		n--;` |
|         1 | 5977 | `	}` |
|         - | 5978 | `	/* Return the freshly created array */` |
|        15 | 5979 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5980 | `	return PH7_OK;` |
|        13 | 5981 | `}` |
|         - | 5982 | `/*` |
|         - | 5983 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5984 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5985 | ` * Parameters` |
|         - | 5986 | ` *  $array1` |
|         - | 5987 | ` *    The array to compare from` |
|         - | 5988 | ` *  $...` |
|         - | 5989 | ` *   More arrays to compare against` |
|         - | 5990 | ` * Return` |
|         - | 5991 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5992 | ` *  have keys that are present in all arguments.` |
|         - | 5993 | ` * Note that NULL is returned on failure.` |
|         - | 5994 | ` */` |
|        20 | 5995 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5996 | `{` |
|         - | 5997 | `	ph7_hashmap_node *pEntry;` |
|         - | 5998 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5999 | `	ph7_value *pArray;` |
|         - | 6000 | `	sxi32 rc;` |
|         - | 6001 | `	sxu32 n;` |
|         - | 6002 | `	int i;` |
|        23 | 6003 | `	if( nArg < 1 ){` |
|       ! 0 | 6004 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6005 | `			"ArgumentCountError",` |
|         - | 6006 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 6007 | `			nArg` |
|         - | 6008 | `			);` |
|         - | 6009 | `	}` |
|        23 | 6010 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6011 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6012 | `			"TypeError",` |
|         - | 6013 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6014 | `			ph7_type_name(apArg[0])` |
|         - | 6015 | `			);` |
|         - | 6016 | `	}` |
|        36 | 6017 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 6018 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6019 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6020 | `				"TypeError",` |
|         - | 6021 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 6022 | `				i + 1,` |
|         2 | 6023 | `				ph7_type_name(apArg[i])` |
|         - | 6024 | `				);` |
|         - | 6025 | `		}` |
|         9 | 6026 | `	}` |
|        17 | 6027 | `	if( nArg == 1 ){` |
|         - | 6028 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 6029 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 6030 | `		return PH7_OK;` |
|         - | 6031 | `	}` |
|         - | 6032 | `	/* Create a new array */` |
|        15 | 6033 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 6034 | `	if( pArray == 0 ){` |
|       ! 0 | 6035 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6036 | `		return PH7_OK;` |
|         - | 6037 | `	}` |
|         - | 6038 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 6039 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6040 | `	/* Perform the intersection */` |
|        15 | 6041 | `	pEntry = pSrc->pFirst;` |
|        15 | 6042 | `	n = pSrc->nEntry;` |
|        24 | 6043 | `	for(;;){` |
|        49 | 6044 | `		if( n < 1 ){` |
|        15 | 6045 | `			break;` |
|         - | 6046 | `		}` |
|        57 | 6047 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 6048 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 6049 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 6050 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 6051 | `				/* Blob lookup */` |
|        27 | 6052 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 6053 | `			}else{` |
|         - | 6054 | `				/* Int key */` |
|        13 | 6055 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 6056 | `			}` |
|        39 | 6057 | `			if( rc != SXRET_OK ){` |
|         - | 6058 | `				/* Key does not exist, break immediately */` |
|        17 | 6059 | `				break;` |
|         - | 6060 | `			}` |
|        12 | 6061 | `		}` |
|        35 | 6062 | `		if( i >= nArg ){` |
|         - | 6063 | `			/* Perform the insertion */` |
|        19 | 6064 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 6065 | `		}` |
|         - | 6066 | `		/* Point to the next entry */` |
|        35 | 6067 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6068 | `		n--;` |
|         1 | 6069 | `	}` |
|         - | 6070 | `	/* Return the freshly created array */` |
|        15 | 6071 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 6072 | `	return PH7_OK;` |
|        13 | 6073 | `}` |
|         - | 6074 | `/*` |
|         - | 6075 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 6076 | ` *  Computes the intersection of arrays.` |
|         - | 6077 | ` * Parameters` |
|         - | 6078 | ` *  $array1` |
|         - | 6079 | ` *    The array to compare from` |
|         - | 6080 | ` *  $array2` |
|         - | 6081 | ` *    An array to compare against` |
|         - | 6082 | ` *  $...` |
|         - | 6083 | ` *   More arrays to compare against` |
|         - | 6084 | ` * $callback` |
|         - | 6085 | ` *  The callback comparison function.` |
|         - | 6086 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 6087 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 6088 | ` *  than the second.` |
|         - | 6089 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 6090 | ` * Return` |
|         - | 6091 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 6092 | ` *  in all of the parameters. .` |
|         - | 6093 | ` * Note that NULL is returned on failure.` |
|         - | 6094 | ` */` |
|        24 | 6095 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6096 | `{` |
|         - | 6097 | `	ph7_hashmap_node *pEntry;` |
|         - | 6098 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 6099 | `	ph7_value *pCallback;` |
|         - | 6100 | `	ph7_value *pArray;` |
|         - | 6101 | `	ph7_value *pVal;` |
|         - | 6102 | `	sxi32 rc;` |
|         - | 6103 | `	sxu32 n;` |
|         - | 6104 | `	int i;` |
|         - | 6105 |  |
|         - | 6106 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 6107 | `	if( nArg < 2 ){` |
|       ! 0 | 6108 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6109 | `			"ArgumentCountError",` |
|         - | 6110 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 6111 | `			nArg` |
|         - | 6112 | `			);` |
|         - | 6113 | `	}` |
|        29 | 6114 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6115 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6116 | `			"TypeError",` |
|         - | 6117 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6118 | `			ph7_type_name(apArg[0])` |
|         - | 6119 | `			);` |
|         - | 6120 | `	}` |
|         - | 6121 |  |
|        27 | 6122 | `	if( nArg == 2 ){` |
|         - | 6123 | `		/* Only the original array and the callback were provided. */` |
|         - | 6124 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6125 | `		 * validation ordering. */` |
|         3 | 6126 | `	} else {` |
|         - | 6127 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6128 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6129 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6130 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6131 | `					"TypeError",` |
|         - | 6132 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6133 | `					i + 1,` |
|         2 | 6134 | `					ph7_type_name(apArg[i])` |
|         - | 6135 | `					);` |
|         - | 6136 | `			}` |
|        13 | 6137 | `		}` |
|         - | 6138 | `	}` |
|         - | 6139 |  |
|         - | 6140 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6141 | `	pCallback = apArg[nArg - 1];` |
|         - | 6142 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6143 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6144 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6145 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6146 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6147 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6148 | `			 */` |
|         9 | 6149 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6150 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6151 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6152 | `					"TypeError",` |
|         - | 6153 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6154 | `					nArg` |
|         - | 6155 | `					);` |
|         - | 6156 | `			}` |
|         - | 6157 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6158 | `			{` |
|         6 | 6159 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6160 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6161 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6162 | `					int nMethodLen;` |
|         6 | 6163 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6164 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6165 | `					if( pClass ){` |
|         - | 6166 | `						/* Class exists but method is missing. */` |
|         4 | 6167 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6168 | `							"TypeError",` |
|         - | 6169 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6170 | `							nArg,` |
|         1 | 6171 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6172 | `							zMethod` |
|         - | 6173 | `							);` |
|         - | 6174 | `					}` |
|         - | 6175 | `					/* Class not found */` |
|         - | 6176 | `					{` |
|         - | 6177 | `						int nName;` |
|         3 | 6178 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6179 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6180 | `							"TypeError",` |
|         - | 6181 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6182 | `							nArg,` |
|         1 | 6183 | `							zName` |
|         - | 6184 | `							);` |
|         - | 6185 | `					}` |
|         - | 6186 | `				}` |
|         - | 6187 | `			}` |
|         - | 6188 | `			/* Fallback message */` |
|       ! 0 | 6189 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6190 | `				"TypeError",` |
|         - | 6191 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6192 | `				nArg` |
|         - | 6193 | `				);` |
|         - | 6194 | `		}` |
|         6 | 6195 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6196 | `			int len;` |
|         3 | 6197 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6198 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6199 | `				"TypeError",` |
|         - | 6200 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6201 | `				nArg,` |
|         1 | 6202 | `				zName` |
|         - | 6203 | `				);` |
|         - | 6204 | `		}` |
|         4 | 6205 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6206 | `			"TypeError",` |
|         - | 6207 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6208 | `			nArg` |
|         - | 6209 | `			);` |
|         - | 6210 | `	}` |
|         - | 6211 |  |
|        11 | 6212 | `	if( nArg == 2 ){` |
|         - | 6213 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6214 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6215 | `		return PH7_OK;` |
|         - | 6216 | `	}` |
|         - | 6217 |  |
|         - | 6218 | `	/* Create a new array */` |
|         7 | 6219 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6220 | `	if( pArray == 0 ){` |
|       ! 0 | 6221 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6222 | `		return PH7_OK;` |
|         - | 6223 | `	}` |
|         - | 6224 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6225 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6226 | `	/* Perform the intersection */` |
|         7 | 6227 | `	pEntry = pSrc->pFirst;` |
|         7 | 6228 | `	n = pSrc->nEntry;` |
|         7 | 6229 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6230 | `	for(;;){` |
|        19 | 6231 | `		if( n < 1 ){` |
|         5 | 6232 | `			break;` |
|         - | 6233 | `		}` |
|         - | 6234 | `		/* Extract the node value */` |
|        15 | 6235 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6236 | `		if( pVal ){` |
|        23 | 6237 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6238 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6239 | `					/* ignore */` |
|       ! 0 | 6240 | `					continue;` |
|         - | 6241 | `				}` |
|         - | 6242 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6243 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6244 | `				/* Perform the lookup */` |
|        15 | 6245 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6246 | `				if( rc != SXRET_OK ){` |
|         - | 6247 | `					/* Value does not exist */` |
|         7 | 6248 | `					break;` |
|         - | 6249 | `				}` |
|         5 | 6250 | `			}` |
|        15 | 6251 | `			if( i >= (nArg-1) ){` |
|         - | 6252 | `				/* Perform the insertion */` |
|         9 | 6253 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6254 | `			}` |
|         7 | 6255 | `		}` |
|        15 | 6256 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6257 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6258 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6259 | `			return PH7_EXCEPTION;` |
|         - | 6260 | `		}` |
|         - | 6261 | `		/* Point to the next entry */` |
|        13 | 6262 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6263 | `		n--;` |
|         1 | 6264 | `	}` |
|         - | 6265 | `	/* Return the freshly created array */` |
|         5 | 6266 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6267 | `	return PH7_OK;` |
|        17 | 6268 | `}` |
|         - | 6269 | `/*` |
|         - | 6270 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6271 | ` *  Fill an array with values.` |
|         - | 6272 | ` * Parameters` |
|         - | 6273 | ` *  $start_index` |
|         - | 6274 | ` *    The first index of the returned array.` |
|         - | 6275 | ` *  $num` |
|         - | 6276 | ` *   Number of elements to insert.` |
|         - | 6277 | ` *  $value` |
|         - | 6278 | ` *    Value to use for filling.` |
|         - | 6279 | ` * Return` |
|         - | 6280 | ` *  The filled array or null on failure.` |
|         - | 6281 | ` */` |
|       240 | 6282 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6283 | `{` |
|         - | 6284 | `	ph7_value *pArray;` |
|         - | 6285 | `	int i,nEntry;` |
|         - | 6286 |  |
|         - | 6287 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6288 | `	if( nArg != 3 ){` |
|         - | 6289 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6290 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6291 | `			"ArgumentCountError",` |
|         - | 6292 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6293 | `			nArg` |
|         - | 6294 | `			);` |
|         - | 6295 | `	}` |
|         - | 6296 |  |
|         - | 6297 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6298 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6299 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6300 | `	 * and NULLs are rejected outright. */` |
|       357 | 6301 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6302 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6303 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6304 | `			"TypeError",` |
|         - | 6305 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6306 | `			ph7_type_name(apArg[0])` |
|         - | 6307 | `			);` |
|         - | 6308 | `	}` |
|       242 | 6309 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6310 | `		int len;` |
|         8 | 6311 | `		sxu8 bReal = FALSE;` |
|         8 | 6312 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6313 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6314 | `			/* Non‑numeric string is an error. */` |
|         3 | 6315 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6316 | `				"TypeError",` |
|         - | 6317 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6318 | `				);` |
|         - | 6319 | `		}` |
|         5 | 6320 | `		if( bReal ){` |
|         - | 6321 | `			/* float-string -> deprecation warning */` |
|         4 | 6322 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6323 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6324 | `				zStr` |
|         - | 6325 | `				);` |
|         1 | 6326 | `		}` |
|         2 | 6327 | `	}` |
|         - | 6328 |  |
|         - | 6329 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6330 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6331 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6332 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6333 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6334 | `			"TypeError",` |
|         - | 6335 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6336 | `			ph7_type_name(apArg[1])` |
|         - | 6337 | `			);` |
|         - | 6338 | `	}` |
|       239 | 6339 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6340 | `		int len;` |
|         3 | 6341 | `		sxu8 bReal = FALSE;` |
|         3 | 6342 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6343 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6344 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6345 | `				"TypeError",` |
|         - | 6346 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6347 | `				);` |
|         - | 6348 | `		}` |
|       ! 0 | 6349 | `	}` |
|         - | 6350 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6351 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6352 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6353 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6354 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6355 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6356 | `		if( d != (double)i64 ){` |
|         7 | 6357 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6358 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6359 | `				d` |
|         - | 6360 | `				);` |
|         2 | 6361 | `		}` |
|         2 | 6362 | `	}` |
|         - | 6363 |  |
|         - | 6364 | `	/* Total number of entries to insert */` |
|       236 | 6365 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6366 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6367 | `	if( nEntry < 0 ){` |
|         3 | 6368 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6369 | `			"ValueError",` |
|         - | 6370 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6371 | `			);` |
|         - | 6372 | `	}` |
|         - | 6373 |  |
|         - | 6374 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6375 | `	if( nEntry == 0 ){` |
|         7 | 6376 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6377 | `		return PH7_OK;` |
|         - | 6378 | `	}` |
|         - | 6379 |  |
|         - | 6380 | `	/* Create a new array */` |
|       227 | 6381 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6382 | `	if( pArray == 0 ){` |
|       ! 0 | 6383 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6384 | `	}` |
|         - | 6385 |  |
|         - | 6386 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6387 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6388 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6389 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6390 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6391 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6392 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6393 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6394 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6395 | `		}` |
|   1058803 | 6396 | `	}` |
|         - | 6397 | `	/* Return the filled array */` |
|       227 | 6398 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6399 | `	return PH7_OK;` |
|       124 | 6400 | `}` |
|         - | 6401 | `/*` |
|         - | 6402 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6403 | ` *  Fill an array with values, specifying keys.` |
|         - | 6404 | ` * Parameters` |
|         - | 6405 | ` *  $input` |
|         - | 6406 | ` *   Array of values that will be used as key.` |
|         - | 6407 | ` *  $value` |
|         - | 6408 | ` *    Value to use for filling.` |
|         - | 6409 | ` * Return` |
|         - | 6410 | ` *  The filled array.` |
|         - | 6411 | ` * Throws` |
|         - | 6412 | ` *  ValueError if $input is not an array.` |
|         - | 6413 | ` */` |
|        22 | 6414 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6415 | `{` |
|         - | 6416 | `	ph7_hashmap_node *pEntry;` |
|         - | 6417 | `	ph7_hashmap *pSrc;` |
|         - | 6418 | `	ph7_value *pArray;` |
|         - | 6419 | `	sxu32 n;` |
|         - | 6420 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6421 | `	if( nArg != 2 ){` |
|         4 | 6422 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6423 | `			"ArgumentCountError",` |
|         - | 6424 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6425 | `			nArg` |
|         - | 6426 | `			);` |
|         - | 6427 | `	}` |
|         - | 6428 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6429 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6430 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6431 | `			"TypeError",` |
|         - | 6432 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6433 | `			ph7_type_name(apArg[0])` |
|         - | 6434 | `			);` |
|         - | 6435 | `	}` |
|         - | 6436 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6437 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6438 | `	/* Create a new array */` |
|        17 | 6439 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6440 | `	if( pArray == 0 ){` |
|       ! 0 | 6441 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6442 | `		return PH7_OK;` |
|         - | 6443 | `	}` |
|         - | 6444 | `	/* Perform the requested operation */` |
|        17 | 6445 | `	pEntry = pSrc->pFirst;` |
|        45 | 6446 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6447 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6448 | `		/* Point to the next entry */` |
|        29 | 6449 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6450 | `	}` |
|         - | 6451 | `	/* Return the filled array */` |
|        17 | 6452 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6453 | `	return PH7_OK;` |
|        14 | 6454 | `}` |
|         - | 6455 | `/*` |
|         - | 6456 | ` * array array_combine(array $keys,array $values)` |
|         - | 6457 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6458 | ` * Parameters` |
|         - | 6459 | ` *  $keys` |
|         - | 6460 | ` *    Array of keys to be used.` |
|         - | 6461 | ` * $values` |
|         - | 6462 | ` *   Array of values to be used.` |
|         - | 6463 | ` * Return` |
|         - | 6464 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6465 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6466 | ` *  not an array.` |
|         - | 6467 | ` */` |
|        16 | 6468 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6469 | `{` |
|         - | 6470 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6471 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6472 | `	ph7_value *pArray;` |
|         - | 6473 | `	sxu32 n;` |
|         - | 6474 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6475 | `	if( nArg != 2 ){` |
|         - | 6476 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6477 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6478 | `			"ArgumentCountError",` |
|         - | 6479 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6480 | `			nArg` |
|         - | 6481 | `			);` |
|         - | 6482 | `	}` |
|         - | 6483 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6484 | `	 * argument index in the error message. */` |
|        20 | 6485 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6486 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6487 | `			"TypeError",` |
|         - | 6488 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6489 | `			ph7_type_name(apArg[0])` |
|         - | 6490 | `			);` |
|         - | 6491 | `	}` |
|        17 | 6492 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6493 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6494 | `			"TypeError",` |
|         - | 6495 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6496 | `			ph7_type_name(apArg[1])` |
|         - | 6497 | `			);` |
|         - | 6498 | `	}` |
|         - | 6499 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6500 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6501 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6502 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6503 | `		/* Length mismatch -> ValueError */` |
|         3 | 6504 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6505 | `			"ValueError",` |
|         - | 6506 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6507 | `			);` |
|         - | 6508 | `	}` |
|         - | 6509 | `	/* Create a new array */` |
|        11 | 6510 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6511 | `	if( pArray == 0 ){` |
|       ! 0 | 6512 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6513 | `		return PH7_OK;` |
|         - | 6514 | `	}` |
|         - | 6515 | `	/* Perform the requested operation */` |
|        11 | 6516 | `	pKe = pKey->pFirst;` |
|        11 | 6517 | `	pVe = pValue->pFirst;` |
|        33 | 6518 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6519 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6520 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6521 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6522 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6523 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6524 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6525 | `		 * original array must not be mutated. */` |
|        23 | 6526 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6527 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6528 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6529 | `			if( pTmpKey ){` |
|         5 | 6530 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6531 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6532 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6533 | `				pKeyCopy = pTmpKey;` |
|         2 | 6534 | `			}` |
|         2 | 6535 | `		}` |
|        23 | 6536 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6537 | `		/* Point to the next entry */` |
|        23 | 6538 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6539 | `		pVe = pVe->pPrev;` |
|        12 | 6540 | `	}` |
|         - | 6541 | `	/* Return the filled array */` |
|        11 | 6542 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6543 | `	return PH7_OK;` |
|        12 | 6544 | `}` |
|         - | 6545 | `/*` |
|         - | 6546 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6547 | ` *  Return an array with elements in reverse order.` |
|         - | 6548 | ` * Parameters` |
|         - | 6549 | ` *  $array` |
|         - | 6550 | ` *   The input array.` |
|         - | 6551 | ` *  $preserve_keys (optional)` |
|         - | 6552 | ` *   If set to TRUE keys are preserved.` |
|         - | 6553 | ` * Return` |
|         - | 6554 | ` *  The reversed array.` |
|         - | 6555 | ` */` |
|        18 | 6556 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6557 | `{` |
|         - | 6558 | `	ph7_hashmap_node *pEntry;` |
|         - | 6559 | `	ph7_hashmap *pSrc;` |
|         - | 6560 | `	ph7_value *pArray;` |
|         - | 6561 | `	int bPreserve;` |
|         - | 6562 | `	sxu32 n;` |
|        20 | 6563 | `	if( nArg < 1 ){` |
|       ! 0 | 6564 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6565 | `			"ArgumentCountError",` |
|         - | 6566 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6567 | `			nArg` |
|         - | 6568 | `			);` |
|         - | 6569 | `	}` |
|         - | 6570 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6571 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6572 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6573 | `			"TypeError",` |
|         - | 6574 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6575 | `			ph7_type_name(apArg[0])` |
|         - | 6576 | `			);` |
|         - | 6577 | `	}` |
|        17 | 6578 | `	bPreserve = FALSE;` |
|        17 | 6579 | `	if( nArg > 1 ){` |
|         7 | 6580 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6581 | `	}` |
|         - | 6582 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6583 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6584 | `	/* Create a new array */` |
|        17 | 6585 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6586 | `	if( pArray == 0 ){` |
|       ! 0 | 6587 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6588 | `		return PH7_OK;` |
|         - | 6589 | `	}` |
|         - | 6590 | `	/* Perform the requested operation */` |
|        17 | 6591 | `	pEntry = pSrc->pLast;` |
|        55 | 6592 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6593 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6594 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6595 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6596 | `		/* Point to the previous entry */` |
|        39 | 6597 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6598 | `	}` |
|        17 | 6599 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6600 | `	return PH7_OK;` |
|        11 | 6601 | `}` |
|         - | 6602 | `/*` |
|         - | 6603 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6604 | ` *  Removes duplicate values from an array.` |
|         - | 6605 | ` * Parameters` |
|         - | 6606 | ` *  $array` |
|         - | 6607 | ` *   The input array.` |
|         - | 6608 | ` *  $flags` |
|         - | 6609 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6610 | ` *   behavior using these values:` |
|         - | 6611 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6612 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6613 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6614 | ` * Return` |
|         - | 6615 | ` *  The filtered array.` |
|         - | 6616 | ` */` |
|        36 | 6617 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6618 | `{` |
|         - | 6619 | `	ph7_hashmap_node *pEntry;` |
|         - | 6620 | `	ph7_value *pNeedle;` |
|         - | 6621 | `	ph7_hashmap *pSrc;` |
|         - | 6622 | `	ph7_value *pArray;` |
|         - | 6623 | `	int iFlags,base,bFold;` |
|         - | 6624 | `	sxu32 n;` |
|        39 | 6625 | `	if( nArg < 1 ){` |
|         - | 6626 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6627 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6628 | `			"ArgumentCountError",` |
|         - | 6629 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6630 | `			);` |
|         - | 6631 | `	}` |
|        39 | 6632 | `	if( nArg > 2 ){` |
|         - | 6633 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6634 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6635 | `			"ArgumentCountError",` |
|         - | 6636 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6637 | `			nArg` |
|         - | 6638 | `			);` |
|         - | 6639 | `	}` |
|         - | 6640 | `	/* Make sure we are dealing with a valid hashmap */` |
|        36 | 6641 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6642 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6643 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6644 | `			"TypeError",` |
|         - | 6645 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6646 | `			ph7_type_name(apArg[0])` |
|         - | 6647 | `			);` |
|         - | 6648 | `	}` |
|         - | 6649 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|         - | 6650 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|        33 | 6651 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|        33 | 6652 | `	base = iFlags & ~8;` |
|        33 | 6653 | `	bFold = (iFlags & 8) != 0;` |
|         - | 6654 | `	/* Point to the internal representation of the input hashmap */` |
|        33 | 6655 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6656 | `	/* Create a new array */` |
|        33 | 6657 | `	pArray = ph7_context_new_array(pCtx);` |
|        33 | 6658 | `	if( pArray == 0 ){` |
|       ! 0 | 6659 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6660 | `		return PH7_OK;` |
|         - | 6661 | `	}` |
|         - | 6662 | `	/* Perform the requested operation */` |
|        33 | 6663 | `	pEntry = pSrc->pFirst;` |
|       145 | 6664 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       113 | 6665 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|       113 | 6666 | `		if( pNeedle ){` |
|         - | 6667 | `			/* Keep this element unless a flag-equal one is already present. */` |
|       113 | 6668 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|       113 | 6669 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|       113 | 6670 | `			int bDup = 0;` |
|         - | 6671 | `			sxu32 i;` |
|         - | 6672 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|         - | 6673 | `			 * loop over pSrc). */` |
|       177 | 6674 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|       117 | 6675 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|       117 | 6676 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|        53 | 6677 | `					bDup = 1;` |
|        53 | 6678 | `					break;` |
|         - | 6679 | `				}` |
|        65 | 6680 | `				pK = pK->pPrev;` |
|        33 | 6681 | `			}` |
|       113 | 6682 | `			if( !bDup ){` |
|        61 | 6683 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|        30 | 6684 | `			}` |
|        56 | 6685 | `		}` |
|         - | 6686 | `		/* Point to the next entry */` |
|       113 | 6687 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        57 | 6688 | `	}` |
|         - | 6689 | `	/* Return the freshly created array */` |
|        33 | 6690 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 6691 | `	return PH7_OK;` |
|        21 | 6692 | `}` |
|         - | 6693 | `/*` |
|         - | 6694 | ` * array array_flip(array $input)` |
|         - | 6695 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6696 | ` * Parameter` |
|         - | 6697 | ` *  $input` |
|         - | 6698 | ` *   Input array.` |
|         - | 6699 | ` * Return` |
|         - | 6700 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6701 | ` */` |
|        30 | 6702 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6703 | `{` |
|         - | 6704 | `	ph7_hashmap_node *pEntry;` |
|         - | 6705 | `	ph7_hashmap *pSrc;` |
|         - | 6706 | `	ph7_value *pArray;` |
|         - | 6707 | `	ph7_value *pKey;` |
|         - | 6708 | `	ph7_value sVal;` |
|         - | 6709 | `	sxu32 n;` |
|         - | 6710 |  |
|         - | 6711 | `	/* PHP requires exactly one argument */` |
|        33 | 6712 | `	if( nArg != 1 ){` |
|         - | 6713 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6714 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6715 | `			"ArgumentCountError",` |
|         - | 6716 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6717 | `			nArg` |
|         - | 6718 | `			);` |
|         - | 6719 | `	}` |
|         - | 6720 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6721 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6722 | `		/* Type mismatch -> TypeError */` |
|         4 | 6723 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6724 | `			"TypeError",` |
|         - | 6725 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6726 | `			ph7_type_name(apArg[0])` |
|         - | 6727 | `			);` |
|         - | 6728 | `	}` |
|         - | 6729 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6730 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6731 | `	/* Create a new array */` |
|        27 | 6732 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6733 | `	if( pArray == 0 ){` |
|       ! 0 | 6734 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6735 | `		return PH7_OK;` |
|         - | 6736 | `	}` |
|         - | 6737 | `	/* Start processing */` |
|        27 | 6738 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6739 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6740 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6741 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6742 | `		if( pKey ){` |
|         - | 6743 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6744 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6745 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6746 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6747 | `					);` |
|     22236 | 6748 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6749 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6750 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6751 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6752 | `				}else{` |
|         - | 6753 | `					SyString sStr;` |
|      2227 | 6754 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6755 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6756 | `				}` |
|         - | 6757 | `				/* Perform the insertion */` |
|     22227 | 6758 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6759 | `				/* Safely release the value because each inserted entry` |
|         - | 6760 | `				 * has its own private copy of the value.` |
|         - | 6761 | `				 */` |
|     22227 | 6762 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6763 | `			}else{` |
|         - | 6764 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6765 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6766 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6767 | `					);` |
|         - | 6768 | `			}` |
|     11118 | 6769 | `		}` |
|         - | 6770 | `		/* Point to the next entry */` |
|     22237 | 6771 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6772 | `	}` |
|         - | 6773 | `	/* Return the freshly created array */` |
|        27 | 6774 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6775 | `	return PH7_OK;` |
|        18 | 6776 | `}` |
|         - | 6777 | `/*` |
|         - | 6778 | ` * number array_sum(array $array )` |
|         - | 6779 | ` *  Calculate the sum of values in an array.` |
|         - | 6780 | ` * Parameters` |
|         - | 6781 | ` *  $array: The input array.` |
|         - | 6782 | ` * Return` |
|         - | 6783 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6784 | ` */` |
|        24 | 6785 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6786 | `{` |
|         - | 6787 | `	ph7_hashmap_node *pEntry;` |
|         - | 6788 | `	ph7_value *pObj;` |
|        26 | 6789 | `	double dSum = 0;` |
|         - | 6790 | `	sxu32 n;` |
|        26 | 6791 | `	pEntry = pMap->pFirst;` |
|        92 | 6792 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6793 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6794 | `		if( pObj ){` |
|        68 | 6795 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6796 | `				dSum += pObj->rVal;` |
|        54 | 6797 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6798 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6799 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6800 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6801 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6802 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6803 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6804 | `						"Addition is not supported on type string");` |
|        14 | 6805 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6806 | `					double dv = 0;` |
|        13 | 6807 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6808 | `					dSum += dv;` |
|         8 | 6809 | `				}` |
|        12 | 6810 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6811 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6812 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6813 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6814 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6815 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6816 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6817 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6818 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6819 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6820 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6821 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6822 | `			}` |
|         - | 6823 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6824 | `		}` |
|         - | 6825 | `		/* Point to the next entry */` |
|        68 | 6826 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6827 | `	}` |
|         - | 6828 | `	/* Return sum */` |
|        26 | 6829 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6830 | `}` |
|       690 | 6831 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         3 | 6832 | `{` |
|         - | 6833 | `	ph7_hashmap_node *pEntry;` |
|         - | 6834 | `	ph7_value *pObj;` |
|       693 | 6835 | `	sxi64 nSum = 0;` |
|         - | 6836 | `	sxu32 n;` |
|       693 | 6837 | `	pEntry = pMap->pFirst;` |
|      6705 | 6838 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      6015 | 6839 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      6015 | 6840 | `		if( pObj ){` |
|      6015 | 6841 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      5995 | 6842 | `				nSum += pObj->x.iVal;` |
|      3018 | 6843 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6844 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6845 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6846 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6847 | `						"Addition is not supported on type string");` |
|        10 | 6848 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6849 | `					sxi64 nv = 0;` |
|         8 | 6850 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6851 | `					nSum += nv;` |
|         5 | 6852 | `				}` |
|        17 | 6853 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6854 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6855 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6856 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6857 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6858 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6859 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6860 | `					"Addition is not supported on type %s",` |
|         2 | 6861 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6862 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6863 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6864 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6865 | `			}` |
|         - | 6866 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      3006 | 6867 | `		}` |
|         - | 6868 | `		/* Point to the next entry */` |
|      6015 | 6869 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3009 | 6870 | `	}` |
|         - | 6871 | `	/* Return sum */` |
|       693 | 6872 | `	ph7_result_int64(pCtx,nSum);` |
|       693 | 6873 | `}` |
|         - | 6874 | `/* number array_sum(array $array )` |
|         - | 6875 | ` * (See block-coment above)` |
|         - | 6876 | ` */` |
|       726 | 6877 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6878 | `{` |
|         - | 6879 | `	ph7_hashmap_node *pEntry;` |
|         - | 6880 | `	ph7_hashmap *pMap;` |
|         - | 6881 | `	ph7_value *pObj;` |
|       730 | 6882 | `	int useDouble = 0;` |
|         - | 6883 | `	sxu32 n;` |
|         - | 6884 | `	/* PHP requires exactly one argument */` |
|       730 | 6885 | `	if( nArg != 1 ){` |
|         4 | 6886 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6887 | `			"ArgumentCountError",` |
|         - | 6888 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6889 | `			nArg` |
|         - | 6890 | `			);` |
|         - | 6891 | `	}` |
|         - | 6892 | `	/* Make sure we are dealing with a valid hashmap */` |
|       728 | 6893 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6894 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6895 | `		char zBuf[64];` |
|         8 | 6896 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6897 | `			"TypeError",` |
|         - | 6898 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6899 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6900 | `			);` |
|         - | 6901 | `	}` |
|       723 | 6902 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       723 | 6903 | `	if( pMap->nEntry < 1 ){` |
|         - | 6904 | `		/* Nothing to compute,return 0 */` |
|         7 | 6905 | `		ph7_result_int(pCtx,0);` |
|         7 | 6906 | `		return PH7_OK;` |
|         - | 6907 | `	}` |
|         - | 6908 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6909 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6910 | `	 */` |
|       717 | 6911 | `	pEntry = pMap->pFirst;` |
|      6737 | 6912 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      6047 | 6913 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      6047 | 6914 | `		if( pObj ){` |
|      6047 | 6915 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6916 | `				useDouble = 1;` |
|        20 | 6917 | `				break;` |
|         - | 6918 | `			}` |
|      6029 | 6919 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6920 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6921 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6922 | `				sxu32 i;` |
|        32 | 6923 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6924 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6925 | `						useDouble = 1;` |
|         7 | 6926 | `						break;` |
|         - | 6927 | `					}` |
|         9 | 6928 | `				}` |
|        18 | 6929 | `				if( useDouble ){` |
|         7 | 6930 | `					break;` |
|         - | 6931 | `				}` |
|         5 | 6932 | `			}` |
|      3010 | 6933 | `		}` |
|      6023 | 6934 | `		pEntry = pEntry->pPrev;` |
|      3013 | 6935 | `	}` |
|       717 | 6936 | `	if( useDouble ){` |
|        26 | 6937 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6938 | `	}else{` |
|       693 | 6939 | `		Int64Sum(pCtx,pMap);` |
|         - | 6940 | `	}` |
|       717 | 6941 | `	return PH7_OK;` |
|       367 | 6942 | `}` |
|         - | 6943 | `/*` |
|         - | 6944 | ` * number array_product(array $array )` |
|         - | 6945 | ` *  Calculate the product of values in an array.` |
|         - | 6946 | ` * Parameters` |
|         - | 6947 | ` *  $array: The input array.` |
|         - | 6948 | ` * Return` |
|         - | 6949 | ` *  Returns the product of values as an integer or float.` |
|         - | 6950 | ` */` |
|         2 | 6951 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6952 | `{` |
|         - | 6953 | `	ph7_hashmap_node *pEntry;` |
|         - | 6954 | `	ph7_value *pObj;` |
|         - | 6955 | `	double dProd;` |
|         - | 6956 | `	sxu32 n;` |
|         3 | 6957 | `	pEntry = pMap->pFirst;` |
|         3 | 6958 | `	dProd = 1;` |
|         7 | 6959 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6960 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6961 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6962 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6963 | `				dProd *= pObj->rVal;` |
|         4 | 6964 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6965 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6966 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6967 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6968 | `					double dv = 0;` |
|       ! 0 | 6969 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6970 | `					dProd *= dv;` |
|       ! 0 | 6971 | `				}` |
|       ! 0 | 6972 | `			}` |
|         2 | 6973 | `		}` |
|         - | 6974 | `		/* Point to the next entry */` |
|         5 | 6975 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6976 | `	}` |
|         - | 6977 | `	/* Return product */` |
|         3 | 6978 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6979 | `}` |
|         2 | 6980 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6981 | `{` |
|         - | 6982 | `	ph7_hashmap_node *pEntry;` |
|         - | 6983 | `	ph7_value *pObj;` |
|         - | 6984 | `	sxi64 nProd;` |
|         - | 6985 | `	sxu32 n;` |
|         3 | 6986 | `	pEntry = pMap->pFirst;` |
|         3 | 6987 | `	nProd = 1;` |
|         9 | 6988 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6989 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6990 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6991 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6992 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6993 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6994 | `				nProd *= pObj->x.iVal;` |
|         3 | 6995 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6996 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6997 | `					sxi64 nv = 0;` |
|       ! 0 | 6998 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6999 | `					nProd *= nv;` |
|       ! 0 | 7000 | `				}` |
|       ! 0 | 7001 | `			}` |
|         3 | 7002 | `		}` |
|         - | 7003 | `		/* Point to the next entry */` |
|         7 | 7004 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 7005 | `	}` |
|         - | 7006 | `	/* Return product */` |
|         3 | 7007 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 7008 | `}` |
|         - | 7009 | `/* number array_product(array $array )` |
|         - | 7010 | ` * (See block-block comment above)` |
|         - | 7011 | ` */` |
|        16 | 7012 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7013 | `{` |
|         - | 7014 | `	ph7_hashmap *pMap;` |
|         - | 7015 | `	ph7_value *pObj;` |
|        17 | 7016 | `	if( nArg < 1 ){` |
|         - | 7017 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 7018 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 7019 | `		return PH7_OK;` |
|         - | 7020 | `	}` |
|         - | 7021 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 7022 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7023 | `		char zBuf[64];` |
|        16 | 7024 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7025 | `			"TypeError",` |
|         - | 7026 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 7027 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7028 | `			);` |
|         - | 7029 | `	}` |
|         7 | 7030 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 7031 | `	if( pMap->nEntry < 1 ){` |
|         - | 7032 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 7033 | `		ph7_result_int(pCtx,1);` |
|         3 | 7034 | `		return PH7_OK;` |
|         - | 7035 | `	}` |
|         - | 7036 | `	/* If the first element is of type float,then perform floating` |
|         - | 7037 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 7038 | `	 */` |
|         5 | 7039 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 7040 | `	if( pObj == 0 ){` |
|       ! 0 | 7041 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 7042 | `		return PH7_OK;` |
|         - | 7043 | `	}` |
|         5 | 7044 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 7045 | `		DoubleProd(pCtx,pMap);` |
|         2 | 7046 | `	}else{` |
|         3 | 7047 | `		Int64Prod(pCtx,pMap);` |
|         - | 7048 | `	}` |
|         5 | 7049 | `	return PH7_OK;` |
|         9 | 7050 | `}` |
|         - | 7051 | `/*` |
|         - | 7052 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 7053 | ` *  Pick one or more random entries out of an array.` |
|         - | 7054 | ` * Parameters` |
|         - | 7055 | ` * $input` |
|         - | 7056 | ` *  The input array.` |
|         - | 7057 | ` * $num_req` |
|         - | 7058 | ` *  Specifies how many entries you want to pick.` |
|         - | 7059 | ` * Return` |
|         - | 7060 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 7061 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 7062 | ` *  NULL is returned on failure.` |
|         - | 7063 | ` */` |
|        36 | 7064 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7065 | `{` |
|         - | 7066 | `	ph7_hashmap_node *pNode;` |
|         - | 7067 | `	ph7_hashmap *pMap;` |
|        37 | 7068 | `	int nItem = 1;` |
|        37 | 7069 | `	if( nArg < 1 ){` |
|         - | 7070 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7071 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7072 | `		return PH7_OK;` |
|         - | 7073 | `	}` |
|         - | 7074 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 7075 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7076 | `		char zBuf[64];` |
|        10 | 7077 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7078 | `			"TypeError",` |
|         - | 7079 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7080 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7081 | `			);` |
|         - | 7082 | `	}` |
|         - | 7083 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 7084 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 7085 | `	if( nArg > 1 ){` |
|        23 | 7086 | `		ph7_value *pNum = apArg[1];` |
|        22 | 7087 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 7088 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 7089 | `			char zBuf[64];` |
|       ! 0 | 7090 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7091 | `				"TypeError",` |
|         - | 7092 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 7093 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 7094 | `				);` |
|         - | 7095 | `		}` |
|        23 | 7096 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 7097 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 7098 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 7099 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 7100 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 7101 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 7102 | `			int len;` |
|         9 | 7103 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 7104 | `			sxi64 iLong; double dReal;` |
|         9 | 7105 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 7106 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 7107 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7108 | `					"TypeError",` |
|         - | 7109 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 7110 | `					);` |
|         - | 7111 | `			}` |
|         - | 7112 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 7113 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 7114 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 7115 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 7116 | `			}` |
|         3 | 7117 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 7118 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 7119 | `			nItem = (int)iLong;` |
|         2 | 7120 | `		}else{` |
|        15 | 7121 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 7122 | `		}` |
|         8 | 7123 | `	}` |
|         - | 7124 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 7125 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7126 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 7127 | `	if( pMap->nEntry < 1 ){` |
|         5 | 7128 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7129 | `			"ValueError",` |
|         - | 7130 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7131 | `			);` |
|         - | 7132 | `	}` |
|         - | 7133 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7134 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7135 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7136 | `			"ValueError",` |
|         - | 7137 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7138 | `			);` |
|         - | 7139 | `	}` |
|        13 | 7140 | `	if( nItem < 2 ){` |
|         - | 7141 | `		sxu32 nEntry;` |
|         - | 7142 | `		/* Select a random number */` |
|         9 | 7143 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7144 | `		/* Extract the desired entry.` |
|         - | 7145 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7146 | `		 */` |
|         9 | 7147 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         5 | 7148 | `			pNode = pMap->pLast;` |
|         5 | 7149 | `			nEntry = pMap->nEntry - nEntry;` |
|         5 | 7150 | `			if( nEntry > 1 ){` |
|       ! 0 | 7151 | `				for(;;){` |
|       ! 0 | 7152 | `					if( nEntry == 0 ){` |
|       ! 0 | 7153 | `						break;` |
|         - | 7154 | `					}` |
|         - | 7155 | `					/* Point to the previous entry */` |
|       ! 0 | 7156 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7157 | `					nEntry--;` |
|       ! 0 | 7158 | `				}` |
|       ! 0 | 7159 | `			}` |
|         2 | 7160 | `		}else{` |
|         5 | 7161 | `			pNode = pMap->pFirst;` |
|         5 | 7162 | `			for(;;){` |
|         7 | 7163 | `				if( nEntry == 0 ){` |
|         5 | 7164 | `					break;` |
|         - | 7165 | `				}` |
|         - | 7166 | `				/* Point to the next entry */` |
|         3 | 7167 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         3 | 7168 | `				nEntry--;` |
|         1 | 7169 | `			}` |
|         - | 7170 | `		}` |
|         9 | 7171 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7172 | `			/* Int key */` |
|         7 | 7173 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7174 | `		}else{` |
|         - | 7175 | `			/* Blob key */` |
|         3 | 7176 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7177 | `		}` |
|         5 | 7178 | `	}else{` |
|         - | 7179 | `		ph7_value sKey,*pArray;` |
|         - | 7180 | `		ph7_hashmap *pDest;` |
|         - | 7181 | `		/* Create a new array */` |
|         5 | 7182 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7183 | `		if( pArray == 0 ){` |
|       ! 0 | 7184 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7185 | `			return PH7_OK;` |
|         - | 7186 | `		}` |
|         - | 7187 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7188 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7189 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7190 | `		/* Copy the first n items */` |
|         5 | 7191 | `		pNode = pMap->pFirst;` |
|         5 | 7192 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7193 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7194 | `		}` |
|        15 | 7195 | `		while( nItem > 0){` |
|        11 | 7196 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7197 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7198 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7199 | `			/* Point to the next entry */` |
|        11 | 7200 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7201 | `			nItem--;` |
|         1 | 7202 | `		}` |
|         - | 7203 | `		/* Shuffle the array */` |
|         5 | 7204 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7205 | `		/* Rehash node */` |
|         5 | 7206 | `		HashmapSortRehash(pDest);` |
|         - | 7207 | `		/* Return the random array */` |
|         5 | 7208 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7209 | `	}` |
|        13 | 7210 | `	return PH7_OK;` |
|        19 | 7211 | `}` |
|         - | 7212 | `/*` |
|         - | 7213 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7214 | ` *  Split an array into chunks.` |
|         - | 7215 | ` * Parameters` |
|         - | 7216 | ` * $input` |
|         - | 7217 | ` *   The array to work on` |
|         - | 7218 | ` * $size` |
|         - | 7219 | ` *   The size of each chunk` |
|         - | 7220 | ` * $preserve_keys` |
|         - | 7221 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7222 | ` *   the chunk numerically.` |
|         - | 7223 | ` * Return` |
|         - | 7224 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7225 | ` *  zero, with each dimension containing size elements.` |
|         - | 7226 | ` */` |
|        36 | 7227 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7228 | `{` |
|         - | 7229 | `	ph7_value *pArray,*pChunk;` |
|         - | 7230 | `	ph7_hashmap_node *pEntry;` |
|         - | 7231 | `	ph7_hashmap *pMap;` |
|         - | 7232 | `	int bPreserve;` |
|         - | 7233 | `	sxu32 nChunk;` |
|         - | 7234 | `	sxu32 nSize;` |
|         - | 7235 | `	sxu32 n;` |
|         - | 7236 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7237 | `	if( nArg < 2 ){` |
|         - | 7238 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7239 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7240 | `			"ArgumentCountError",` |
|         - | 7241 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7242 | `			nArg` |
|         - | 7243 | `			);` |
|         - | 7244 | `	}` |
|        41 | 7245 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7246 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7247 | `			"TypeError",` |
|         - | 7248 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7249 | `			ph7_type_name(apArg[0])` |
|         - | 7250 | `			);` |
|         - | 7251 | `	}` |
|         - | 7252 | `	/* Create a new array */` |
|        38 | 7253 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7254 | `	if( pArray == 0 ){` |
|       ! 0 | 7255 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7256 | `		return PH7_OK;` |
|         - | 7257 | `	}` |
|         - | 7258 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7259 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7260 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7261 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7262 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7263 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7264 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7265 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7266 | `			"TypeError",` |
|         - | 7267 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7268 | `			ph7_type_name(apArg[1])` |
|         - | 7269 | `			);` |
|         - | 7270 | `	}` |
|         - | 7271 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7272 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7273 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7274 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7275 | `		int len;` |
|         3 | 7276 | `		sxu8 bReal = FALSE;` |
|         3 | 7277 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7278 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7279 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7280 | `				"TypeError",` |
|         - | 7281 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7282 | `				);` |
|         - | 7283 | `		}` |
|       ! 0 | 7284 | `		if( bReal ){` |
|         - | 7285 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7286 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7287 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7288 | `				zStr` |
|         - | 7289 | `				);` |
|       ! 0 | 7290 | `		}` |
|       ! 0 | 7291 | `	}` |
|         - | 7292 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7293 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7294 | `	 * later via ph7_value_to_int. */` |
|        35 | 7295 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7296 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7297 | `		sxi64 i = (sxi64)d;` |
|         3 | 7298 | `		if( d != (double)i ){` |
|         4 | 7299 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7300 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7301 | `				d` |
|         - | 7302 | `				);` |
|         1 | 7303 | `		}` |
|         1 | 7304 | `	}` |
|         - | 7305 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7306 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7307 | `	{` |
|        35 | 7308 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7309 | `		if( nSizeSigned < 1 ){` |
|         - | 7310 | `			/* size <= 0 -> ValueError */` |
|         6 | 7311 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7312 | `				"ValueError",` |
|         - | 7313 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7314 | `				);` |
|         - | 7315 | `		}` |
|        29 | 7316 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7317 | `	}` |
|        29 | 7318 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7319 | `		/* Return the whole array */` |
|         3 | 7320 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7321 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7322 | `		return PH7_OK;` |
|         - | 7323 | `	}` |
|        27 | 7324 | `	bPreserve = 0;` |
|        27 | 7325 | `	if( nArg > 2 ){` |
|         - | 7326 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7327 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7328 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7329 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7330 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7331 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7332 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7333 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7334 | `				"TypeError",` |
|         - | 7335 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7336 | `				ph7_type_name(apArg[2])` |
|         - | 7337 | `				);` |
|         - | 7338 | `		}` |
|        21 | 7339 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7340 | `	}` |
|         - | 7341 | `	/* Start processing */` |
|        27 | 7342 | `	pEntry = pMap->pFirst;` |
|        27 | 7343 | `	nChunk = 0;` |
|        27 | 7344 | `	pChunk = 0;` |
|        27 | 7345 | `	n = pMap->nEntry;` |
|        56 | 7346 | `	for( ;; ){` |
|       113 | 7347 | `		if( n < 1 ){` |
|         - | 7348 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7349 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7350 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7351 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7352 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7353 | `			 * exists. */` |
|        27 | 7354 | `			if( pChunk ){` |
|        27 | 7355 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7356 | `			}` |
|        27 | 7357 | `			break;` |
|         - | 7358 | `		}` |
|        87 | 7359 | `		if( nChunk < 1 ){` |
|        71 | 7360 | `			if( pChunk ){` |
|         - | 7361 | `				/* Put the first chunk */` |
|        45 | 7362 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7363 | `			}` |
|         - | 7364 | `			/* Create a new dimension */` |
|        71 | 7365 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7366 | `												   * will be automatically released as soon we return` |
|         - | 7367 | `												   * from this function */` |
|        71 | 7368 | `			if( pChunk == 0 ){` |
|       ! 0 | 7369 | `				break;` |
|         - | 7370 | `			}` |
|        71 | 7371 | `			nChunk = nSize;` |
|        35 | 7372 | `		}` |
|         - | 7373 | `		/* Insert the entry */` |
|        87 | 7374 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7375 | `		/* Point to the next entry */` |
|        87 | 7376 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7377 | `		nChunk--;` |
|        87 | 7378 | `		n--;` |
|         1 | 7379 | `	}` |
|         - | 7380 | `	/* Return the multidimensional array */` |
|        27 | 7381 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7382 | `	return PH7_OK;` |
|        23 | 7383 | `}` |
|         - | 7384 | `/*` |
|         - | 7385 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7386 | ` *  Pad array to the specified length with a value.` |
|         - | 7387 | ` * $input` |
|         - | 7388 | ` *   Initial array of values to pad.` |
|         - | 7389 | ` * $pad_size` |
|         - | 7390 | ` *   New size of the array.` |
|         - | 7391 | ` * $pad_value` |
|         - | 7392 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7393 | ` */` |
|         - | 7394 | `/*` |
|         - | 7395 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7396 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7397 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7398 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7399 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7400 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7401 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7402 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7403 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7404 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7405 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7406 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7407 | ` */` |
|        50 | 7408 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7409 | `	ph7_context *pCtx,` |
|         - | 7410 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7411 | `	int iArg,              /* 1-based argument position */` |
|         - | 7412 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7413 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7414 | `	)` |
|         1 | 7415 | `{` |
|        51 | 7416 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7417 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7418 | `			"ValueError",` |
|         - | 7419 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7420 | `			zFunc,iArg,zParam` |
|         - | 7421 | `			);` |
|         - | 7422 | `	}` |
|        37 | 7423 | `	return SXRET_OK;` |
|        26 | 7424 | `}` |
|        62 | 7425 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7426 | `{` |
|         - | 7427 | `	ph7_hashmap *pMap;` |
|         - | 7428 | `	ph7_value *pArray;` |
|         - | 7429 | `	sxi64 iLen,iAbs;` |
|         - | 7430 | `	int nEntry;` |
|         - | 7431 | `	sxi32 rc;` |
|        65 | 7432 | `	if( nArg != 3 ){` |
|         4 | 7433 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7434 | `			"ArgumentCountError",` |
|         - | 7435 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7436 | `			nArg` |
|         - | 7437 | `			);` |
|         - | 7438 | `	}` |
|        62 | 7439 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7440 | `		char zBuf[64];` |
|        11 | 7441 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7442 | `			"TypeError",` |
|         - | 7443 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7444 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7445 | `			);` |
|         - | 7446 | `	}` |
|         - | 7447 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7448 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7449 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7450 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7451 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7452 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7453 | `		char zBuf[64];` |
|       ! 0 | 7454 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7455 | `			"TypeError",` |
|         - | 7456 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7457 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7458 | `			);` |
|         - | 7459 | `	}` |
|        55 | 7460 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7461 | `		int nStr;` |
|        11 | 7462 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7463 | `		sxi64 iLong; double dReal;` |
|        11 | 7464 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7465 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7466 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7467 | `				"TypeError",` |
|         - | 7468 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7469 | `				);` |
|         - | 7470 | `		}` |
|         7 | 7471 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7472 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7473 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7474 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7475 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7476 | `					"TypeError",` |
|         - | 7477 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7478 | `					);` |
|         - | 7479 | `			}` |
|         3 | 7480 | `			iLen = (sxi64)dReal;` |
|         3 | 7481 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7482 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7483 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7484 | `					zStr` |
|         - | 7485 | `					);` |
|       ! 0 | 7486 | `			}` |
|         2 | 7487 | `		}else{` |
|         5 | 7488 | `			iLen = iLong;` |
|         - | 7489 | `		}` |
|         4 | 7490 | `	}else{` |
|        45 | 7491 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7492 | `	}` |
|         - | 7493 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7494 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7495 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7496 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7497 | `	 * overflow). */` |
|        51 | 7498 | `	iAbs = iLen;` |
|        51 | 7499 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7500 | `		iAbs = -iAbs;` |
|         7 | 7501 | `	}` |
|        51 | 7502 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7503 | `	if( rc != SXRET_OK ){` |
|        15 | 7504 | `		return rc;` |
|         - | 7505 | `	}` |
|        37 | 7506 | `	nEntry = (int)iLen;` |
|         - | 7507 | `	/* Create a new array */` |
|        37 | 7508 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7509 | `	if( pArray == 0 ){` |
|       ! 0 | 7510 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7511 | `	}` |
|        37 | 7512 | `	if( nEntry < 0 ){` |
|        11 | 7513 | `		nEntry = -nEntry;` |
|        11 | 7514 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7515 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7516 | `			/* Insert given items first */` |
|        25 | 7517 | `			while( nEntry > 0 ){` |
|        19 | 7518 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7519 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7520 | `				}` |
|        19 | 7521 | `				nEntry--;` |
|         1 | 7522 | `			}` |
|         - | 7523 | `			/* Merge the two arrays */` |
|         7 | 7524 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7525 | `		}else{` |
|         5 | 7526 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7527 | `		}` |
|        32 | 7528 | `	}else if( nEntry > 0 ){` |
|        25 | 7529 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7530 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7531 | `			/* Merge the two arrays first */` |
|        19 | 7532 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7533 | `			/* Insert given items */` |
|       275 | 7534 | `			while( nEntry > 0 ){` |
|       257 | 7535 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7536 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7537 | `				}` |
|       257 | 7538 | `				nEntry--;` |
|         1 | 7539 | `			}` |
|        10 | 7540 | `		}else{` |
|         7 | 7541 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7542 | `		}` |
|        13 | 7543 | `	}else{` |
|         - | 7544 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7545 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7546 | `	}` |
|         - | 7547 | `	/* Return the new array */` |
|        37 | 7548 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7549 | `	return PH7_OK;` |
|        34 | 7550 | `}` |
|         - | 7551 | `/*` |
|         - | 7552 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7553 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7554 | ` * Parameters` |
|         - | 7555 | ` * $array` |
|         - | 7556 | ` *   The array in which elements are replaced.` |
|         - | 7557 | ` * $array1` |
|         - | 7558 | ` *   The array from which elements will be extracted.` |
|         - | 7559 | ` * ....` |
|         - | 7560 | ` *  More arrays from which elements will be extracted.` |
|         - | 7561 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7562 | ` * Return` |
|         - | 7563 | ` *  Returns an array.` |
|         - | 7564 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7565 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7566 | ` */` |
|        20 | 7567 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7568 | `{` |
|         - | 7569 | `	ph7_hashmap *pMap;` |
|         - | 7570 | `	ph7_value *pArray;` |
|         - | 7571 | `	int i;` |
|        23 | 7572 | `	if( nArg < 1 ){` |
|       ! 0 | 7573 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7574 | `			"ArgumentCountError",` |
|         - | 7575 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7576 | `			);` |
|         - | 7577 | `	}` |
|        23 | 7578 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7579 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7580 | `			"TypeError",` |
|         - | 7581 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7582 | `			ph7_type_name(apArg[0])` |
|         - | 7583 | `			);` |
|         - | 7584 | `	}` |
|         - | 7585 | `	/* Create a new array */` |
|        20 | 7586 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7587 | `	if( pArray == 0 ){` |
|       ! 0 | 7588 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7589 | `		return PH7_OK;` |
|         - | 7590 | `	}` |
|         - | 7591 | `	/* Overwrite from the first array */` |
|        20 | 7592 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7593 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7594 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7595 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7596 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7597 | `			/* Type mismatch -> TypeError */` |
|         4 | 7598 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7599 | `				"TypeError",` |
|         - | 7600 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7601 | `				i + 1,` |
|         2 | 7602 | `				ph7_type_name(apArg[i])` |
|         - | 7603 | `				);` |
|         - | 7604 | `		}` |
|         - | 7605 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7606 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7607 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7608 | `	}` |
|         - | 7609 | `	/* Return the new array */` |
|        17 | 7610 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7611 | `	return PH7_OK;` |
|        13 | 7612 | `}` |
|         - | 7613 | `/*` |
|         - | 7614 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7615 | ` *  Filters elements of an array using a callback function.` |
|         - | 7616 | ` * Parameters` |
|         - | 7617 | ` *  $input` |
|         - | 7618 | ` *    The array to iterate over` |
|         - | 7619 | ` * $callback` |
|         - | 7620 | ` *    The callback function to use` |
|         - | 7621 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7622 | ` *    will be removed.` |
|         - | 7623 | ` * Return` |
|         - | 7624 | ` *  The filtered array.` |
|         - | 7625 | ` */` |
|        30 | 7626 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7627 | `{` |
|         - | 7628 | `	ph7_hashmap_node *pEntry;` |
|         - | 7629 | `	ph7_hashmap *pMap;` |
|         - | 7630 | `	ph7_value *pArray;` |
|         - | 7631 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7632 | `	ph7_value *pValue;` |
|         - | 7633 | `	sxi32 rc;` |
|         - | 7634 | `	int keep;` |
|         - | 7635 | `	sxu32 n;` |
|        32 | 7636 | `	if( nArg < 1 ){` |
|         - | 7637 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7638 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7639 | `		return PH7_OK;` |
|         - | 7640 | `	}` |
|         - | 7641 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7642 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7643 | `		char zBuf[64];` |
|        19 | 7644 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7645 | `			"TypeError",` |
|         - | 7646 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7647 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7648 | `			);` |
|         - | 7649 | `	}` |
|         - | 7650 | `	/* Create a new array */` |
|        20 | 7651 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7652 | `	if( pArray == 0 ){` |
|       ! 0 | 7653 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7654 | `		return PH7_OK;` |
|         - | 7655 | `	}` |
|         - | 7656 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7657 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7658 | `	pEntry = pMap->pFirst;` |
|        20 | 7659 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7660 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7661 | `	/* Perform the requested operation */` |
|        78 | 7662 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7663 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7664 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7665 | `		if( pValue == 0 ){` |
|         - | 7666 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7667 | `			keep = FALSE;` |
|        64 | 7668 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7669 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7670 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7671 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7672 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7673 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7674 | `					int len;` |
|         3 | 7675 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7676 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7677 | `						"TypeError",` |
|         - | 7678 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7679 | `						zName` |
|         - | 7680 | `						);` |
|       ! 0 | 7681 | `				}else{` |
|       ! 0 | 7682 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7683 | `						"TypeError",` |
|         - | 7684 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7685 | `						ph7_type_name(apArg[1])` |
|         - | 7686 | `						);` |
|         - | 7687 | `				}` |
|         - | 7688 | `			}` |
|        33 | 7689 | `			keep = FALSE;` |
|        33 | 7690 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7691 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7692 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7693 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7694 | `				return PH7_EXCEPTION;` |
|         - | 7695 | `			}` |
|        31 | 7696 | `			if( rc == SXRET_OK ){` |
|         - | 7697 | `				/* Perform a boolean cast */` |
|        31 | 7698 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7699 | `			}` |
|        31 | 7700 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7701 | `		}else{` |
|         - | 7702 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7703 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7704 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7705 | `			 */` |
|        29 | 7706 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7707 | `		}` |
|        59 | 7708 | `		if( keep ){` |
|         - | 7709 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7710 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7711 | `		}` |
|         - | 7712 | `		/* Point to the next entry */` |
|        59 | 7713 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7714 | `	}` |
|        15 | 7715 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7716 | `	return PH7_OK;` |
|        17 | 7717 | `}` |
|         - | 7718 | `/*` |
|         - | 7719 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7720 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7721 | ` * Parameters` |
|         - | 7722 | ` *  $callback` |
|         - | 7723 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7724 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7725 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7726 | ` *   are zipped together.` |
|         - | 7727 | ` *  $array` |
|         - | 7728 | ` *   The first array to run through the callback function.` |
|         - | 7729 | ` *  $arrays` |
|         - | 7730 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7731 | ` * Return` |
|         - | 7732 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7733 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7734 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7735 | ` *  padding shorter arrays with NULL.` |
|         - | 7736 | ` */` |
|        84 | 7737 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7738 | `{` |
|         - | 7739 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7740 | `	ph7_hashmap_node *pEntry;` |
|         - | 7741 | `	ph7_hashmap *pMap;` |
|         - | 7742 | `	ph7_vm *pVm;` |
|         - | 7743 | `	int bNullCallback;` |
|         - | 7744 | `	sxi32 rc;` |
|         - | 7745 | `	int i;` |
|         - | 7746 | `	sxu32 n;` |
|        89 | 7747 | `	if( nArg < 2 ){` |
|       ! 0 | 7748 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7749 | `			"ArgumentCountError",` |
|         - | 7750 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7751 | `			nArg` |
|         - | 7752 | `			);` |
|         - | 7753 | `	}` |
|        89 | 7754 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        89 | 7755 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         8 | 7756 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         6 | 7757 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         8 | 7758 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7759 | `				"TypeError",` |
|         - | 7760 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7761 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7762 | `				zFunc` |
|         - | 7763 | `				);` |
|         - | 7764 | `		}` |
|         3 | 7765 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7766 | `			"TypeError",` |
|         - | 7767 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7768 | `			"no array or string given"` |
|         - | 7769 | `			);` |
|         - | 7770 | `	}` |
|         - | 7771 | `	/* Every remaining argument must be an array */` |
|       170 | 7772 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        94 | 7773 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7774 | `			if( i == 1 ){` |
|         4 | 7775 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7776 | `					"TypeError",` |
|         - | 7777 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7778 | `					ph7_type_name(apArg[1])` |
|         - | 7779 | `					);` |
|         - | 7780 | `			}` |
|       ! 0 | 7781 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7782 | `				"TypeError",` |
|         - | 7783 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7784 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7785 | `				);` |
|         - | 7786 | `		}` |
|        48 | 7787 | `	}` |
|        80 | 7788 | `	pVm = pCtx->pVm;` |
|         - | 7789 | `	/* Create a new array */` |
|        80 | 7790 | `	pArray = ph7_context_new_array(pCtx);` |
|        80 | 7791 | `	if( pArray == 0 ){` |
|       ! 0 | 7792 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7793 | `		return PH7_OK;` |
|         - | 7794 | `	}` |
|        80 | 7795 | `	PH7_MemObjInit(pVm,&sResult);` |
|        80 | 7796 | `	PH7_MemObjInit(pVm,&sKey);` |
|        80 | 7797 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        80 | 7798 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        80 | 7799 | `	if( nArg == 2 ){` |
|         - | 7800 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        70 | 7801 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        70 | 7802 | `		pEntry = pMap->pFirst;` |
|       240 | 7803 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7804 | `			/* Extract the node value */` |
|       178 | 7805 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|       178 | 7806 | `			if( pValue ){` |
|         - | 7807 | `				/* Extract the node key */` |
|       178 | 7808 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       178 | 7809 | `				if( bNullCallback ){` |
|         - | 7810 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7811 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7812 | `				}else{` |
|         - | 7813 | `					/* Invoke the supplied callback */` |
|       168 | 7814 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|       168 | 7815 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7816 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7817 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7818 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7819 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7820 | `						return PH7_EXCEPTION;` |
|         - | 7821 | `					}` |
|         - | 7822 | `					/* Insert the callback return value */` |
|       164 | 7823 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7824 | `				}` |
|       174 | 7825 | `				PH7_MemObjRelease(&sKey);` |
|       174 | 7826 | `				PH7_MemObjRelease(&sResult);` |
|        85 | 7827 | `			}` |
|         - | 7828 | `			/* Point to the next entry */` |
|       174 | 7829 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        89 | 7830 | `		}` |
|        35 | 7831 | `	}else{` |
|         - | 7832 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7833 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7834 | `		int nArrays = nArg - 1;` |
|         - | 7835 | `		ph7_hashmap_node **apCur;` |
|         - | 7836 | `		ph7_value **apCallArg;` |
|         - | 7837 | `		ph7_value sNull;` |
|        11 | 7838 | `		sxu32 nMax = 0;` |
|        11 | 7839 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7840 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7841 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7842 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7843 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7844 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7845 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7846 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7847 | `			return PH7_OK;` |
|         - | 7848 | `		}` |
|        11 | 7849 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7850 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7851 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7852 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7853 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7854 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7855 | `				nMax = pMap->nEntry;` |
|         6 | 7856 | `			}` |
|        12 | 7857 | `		}` |
|        35 | 7858 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7859 | `			ph7_value *pZip = 0;` |
|        25 | 7860 | `			if( bNullCallback ){` |
|         - | 7861 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7862 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7863 | `			}` |
|        79 | 7864 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7865 | `				ph7_value *pv = &sNull;` |
|        55 | 7866 | `				if( apCur[i] ){` |
|        53 | 7867 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7868 | `					if( pNodeVal ){` |
|        53 | 7869 | `						pv = pNodeVal;` |
|        26 | 7870 | `					}` |
|        53 | 7871 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7872 | `				}` |
|        55 | 7873 | `				if( bNullCallback ){` |
|         9 | 7874 | `					if( pZip ){` |
|         9 | 7875 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7876 | `					}` |
|         5 | 7877 | `				}else{` |
|        47 | 7878 | `					apCallArg[i] = pv;` |
|         - | 7879 | `				}` |
|        28 | 7880 | `			}` |
|        25 | 7881 | `			if( bNullCallback ){` |
|         5 | 7882 | `				if( pZip ){` |
|         5 | 7883 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7884 | `				}` |
|         3 | 7885 | `			}else{` |
|        21 | 7886 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7887 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7888 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7889 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7890 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7891 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7892 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7893 | `					return PH7_EXCEPTION;` |
|         - | 7894 | `				}` |
|        21 | 7895 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7896 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7897 | `			}` |
|        13 | 7898 | `		}` |
|        11 | 7899 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7900 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7901 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7902 | `	}` |
|        76 | 7903 | `	PH7_MemObjRelease(&sKey);` |
|        76 | 7904 | `	PH7_MemObjRelease(&sResult);` |
|        76 | 7905 | `	ph7_result_value(pCtx,pArray);` |
|        76 | 7906 | `	return PH7_OK;` |
|        47 | 7907 | `}` |
|         - | 7908 | `/*` |
|         - | 7909 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7910 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7911 | ` * Parameters` |
|         - | 7912 | ` *  $array` |
|         - | 7913 | ` *   The input array.` |
|         - | 7914 | ` *  $callback` |
|         - | 7915 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7916 | ` *  $initial` |
|         - | 7917 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7918 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7919 | ` * Return` |
|         - | 7920 | ` *  Returns the resulting value.` |
|         - | 7921 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7922 | ` */` |
|        30 | 7923 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7924 | `{` |
|         - | 7925 | `	ph7_hashmap_node *pEntry;` |
|         - | 7926 | `	ph7_hashmap *pMap;` |
|         - | 7927 | `	ph7_value *pValue;` |
|         - | 7928 | `	ph7_value sResult;` |
|         - | 7929 | `	sxi32 rc;` |
|         - | 7930 | `	sxu32 n;` |
|        35 | 7931 | `	if( nArg < 2 ){` |
|       ! 0 | 7932 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7933 | `			"ArgumentCountError",` |
|         - | 7934 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7935 | `			nArg` |
|         - | 7936 | `			);` |
|         - | 7937 | `	}` |
|        35 | 7938 | `	if( nArg > 3 ){` |
|         4 | 7939 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7940 | `			"ArgumentCountError",` |
|         - | 7941 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7942 | `			nArg` |
|         - | 7943 | `			);` |
|         - | 7944 | `	}` |
|        33 | 7945 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7946 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7947 | `			"TypeError",` |
|         - | 7948 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7949 | `			ph7_type_name(apArg[0])` |
|         - | 7950 | `			);` |
|         - | 7951 | `	}` |
|        31 | 7952 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7953 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7954 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7955 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7956 | `				"TypeError",` |
|         - | 7957 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7958 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7959 | `				zFunc` |
|         - | 7960 | `				);` |
|         - | 7961 | `		}` |
|         9 | 7962 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7963 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7964 | `				"TypeError",` |
|         - | 7965 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7966 | `				"array callback must have exactly two members"` |
|         - | 7967 | `				);` |
|         - | 7968 | `		}` |
|         6 | 7969 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7970 | `			"TypeError",` |
|         - | 7971 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7972 | `			"no array or string given"` |
|         - | 7973 | `			);` |
|         - | 7974 | `	}` |
|         - | 7975 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7976 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7977 | `	/* Assume a NULL initial value */` |
|        19 | 7978 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7979 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7980 | `	if( nArg > 2 ){` |
|         - | 7981 | `		/* Set the initial value */` |
|        13 | 7982 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7983 | `	}` |
|         - | 7984 | `	/* Perform the requested operation */` |
|        19 | 7985 | `	pEntry = pMap->pFirst;` |
|        55 | 7986 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7987 | `		/* Extract the node value */` |
|        39 | 7988 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7989 | `		/* Invoke the supplied callback */` |
|        39 | 7990 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7991 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7992 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7993 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7994 | `			return PH7_EXCEPTION;` |
|         - | 7995 | `		}` |
|         - | 7996 | `		/* Point to the next entry */` |
|        37 | 7997 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7998 | `	}` |
|        17 | 7999 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 8000 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 8001 | `	return PH7_OK;` |
|        20 | 8002 | `}` |
|         - | 8003 | `/*` |
|         - | 8004 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8005 | ` *  Apply a user function to every member of an array.` |
|         - | 8006 | ` * Parameters` |
|         - | 8007 | ` *  $array` |
|         - | 8008 | ` *   The input array.` |
|         - | 8009 | ` *  $funcname` |
|         - | 8010 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8011 | ` *   the first, and the key/index second.` |
|         - | 8012 | ` * Note:` |
|         - | 8013 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8014 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8015 | ` *  be made in the original array itself.` |
|         - | 8016 | ` *  $userdata` |
|         - | 8017 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8018 | ` *   to the callback funcname.` |
|         - | 8019 | ` * Return` |
|         - | 8020 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8021 | ` */` |
|        36 | 8022 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8023 | `{` |
|         - | 8024 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 8025 | `	ph7_hashmap_node *pEntry;` |
|         - | 8026 | `	ph7_hashmap *pMap;` |
|         - | 8027 | `	sxu32 n;` |
|        41 | 8028 | `	if( nArg < 2 ){` |
|       ! 0 | 8029 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8030 | `			"ArgumentCountError",` |
|         - | 8031 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 8032 | `			nArg` |
|         - | 8033 | `			);` |
|         - | 8034 | `	}` |
|        41 | 8035 | `	if( nArg > 3 ){` |
|         4 | 8036 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8037 | `			"ArgumentCountError",` |
|         - | 8038 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 8039 | `			nArg` |
|         - | 8040 | `			);` |
|         - | 8041 | `	}` |
|        39 | 8042 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8043 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8044 | `			"TypeError",` |
|         - | 8045 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8046 | `			ph7_type_name(apArg[0])` |
|         - | 8047 | `			);` |
|         - | 8048 | `	}` |
|        37 | 8049 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        17 | 8050 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         6 | 8051 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         8 | 8052 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8053 | `				"TypeError",` |
|         - | 8054 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8055 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 8056 | `				zFunc` |
|         - | 8057 | `				);` |
|         - | 8058 | `		}` |
|        12 | 8059 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8060 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8061 | `				"TypeError",` |
|         - | 8062 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8063 | `				"array callback must have exactly two members"` |
|         - | 8064 | `				);` |
|         - | 8065 | `		}` |
|         6 | 8066 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8067 | `			"TypeError",` |
|         - | 8068 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8069 | `			"no array or string given"` |
|         - | 8070 | `			);` |
|         - | 8071 | `	}` |
|        21 | 8072 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 8073 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 8074 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 8075 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8076 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 8077 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 8078 | `	/* Perform the desired operation */` |
|        21 | 8079 | `	pEntry = pMap->pFirst;` |
|        61 | 8080 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8081 | `		/* Extract the node value */` |
|        43 | 8082 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 8083 | `		if( pValue ){` |
|         - | 8084 | `			sxi32 rcW;` |
|         - | 8085 | `			/* Extract the entry key */` |
|        43 | 8086 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8087 | `			/* Invoke the supplied callback */` |
|        43 | 8088 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 8089 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 8090 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 8091 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 8092 | `				return PH7_EXCEPTION;` |
|         - | 8093 | `			}` |
|        20 | 8094 | `		}` |
|         - | 8095 | `		/* Point to the next entry */` |
|        41 | 8096 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 8097 | `	}` |
|         - | 8098 | `	/* All done, return TRUE */` |
|        19 | 8099 | `	ph7_result_bool(pCtx,1);` |
|        19 | 8100 | `	return PH7_OK;` |
|        23 | 8101 | `}` |
|         - | 8102 | `/*` |
|         - | 8103 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 8104 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 8105 | ` */` |
|        22 | 8106 | `static sxi32 HashmapWalkRecursive(` |
|         - | 8107 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 8108 | `	ph7_value *pCallback, /* User callback */` |
|         - | 8109 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 8110 | `	int iNest             /* Nesting level */` |
|         - | 8111 | `	)` |
|         1 | 8112 | `{` |
|         - | 8113 | `	ph7_hashmap_node *pEntry;` |
|         - | 8114 | `	ph7_value *pValue,sKey;` |
|         - | 8115 | `	sxi32 rc;` |
|         - | 8116 | `	sxu32 n;` |
|         - | 8117 | `	/* Iterate through hashmap entries */` |
|        23 | 8118 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 8119 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 8120 | `	pEntry = pMap->pFirst;` |
|        59 | 8121 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8122 | `		/* Extract the node value */` |
|        37 | 8123 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 8124 | `		if( pValue ){` |
|        37 | 8125 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 8126 | `				if( iNest < 32 ){` |
|         - | 8127 | `					/* Recurse */` |
|        11 | 8128 | `					iNest++;` |
|        11 | 8129 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8130 | `					iNest--;` |
|        11 | 8131 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8132 | `						return PH7_EXCEPTION;` |
|         - | 8133 | `					}` |
|         5 | 8134 | `				}` |
|         6 | 8135 | `			}else{` |
|         - | 8136 | `				/* Extract the node key */` |
|        27 | 8137 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8138 | `				/* Invoke the supplied callback */` |
|        27 | 8139 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8140 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8141 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8142 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8143 | `					return PH7_EXCEPTION;` |
|         - | 8144 | `				}` |
|         - | 8145 | `			}` |
|        18 | 8146 | `		}` |
|         - | 8147 | `		/* Point to the next entry */` |
|        37 | 8148 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8149 | `	}` |
|        23 | 8150 | `	return PH7_OK;` |
|        12 | 8151 | `}` |
|         - | 8152 | `/*` |
|         - | 8153 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8154 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8155 | ` * Parameters` |
|         - | 8156 | ` *  $array` |
|         - | 8157 | ` *   The input array.` |
|         - | 8158 | ` *  $funcname` |
|         - | 8159 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8160 | ` *   the first, and the key/index second.` |
|         - | 8161 | ` * Note:` |
|         - | 8162 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8163 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8164 | ` *  be made in the original array itself.` |
|         - | 8165 | ` *  $userdata` |
|         - | 8166 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8167 | ` *   to the callback funcname.` |
|         - | 8168 | ` * Return` |
|         - | 8169 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8170 | ` */` |
|        26 | 8171 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8172 | `{` |
|         - | 8173 | `	ph7_hashmap *pMap;` |
|        31 | 8174 | `	if( nArg < 2 ){` |
|       ! 0 | 8175 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8176 | `			"ArgumentCountError",` |
|         - | 8177 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8178 | `			nArg` |
|         - | 8179 | `			);` |
|         - | 8180 | `	}` |
|        31 | 8181 | `	if( nArg > 3 ){` |
|         4 | 8182 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8183 | `			"ArgumentCountError",` |
|         - | 8184 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8185 | `			nArg` |
|         - | 8186 | `			);` |
|         - | 8187 | `	}` |
|        29 | 8188 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8189 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8190 | `			"TypeError",` |
|         - | 8191 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8192 | `			ph7_type_name(apArg[0])` |
|         - | 8193 | `			);` |
|         - | 8194 | `	}` |
|        27 | 8195 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8196 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8197 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8198 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8199 | `				"TypeError",` |
|         - | 8200 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8201 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8202 | `				zFunc` |
|         - | 8203 | `				);` |
|         - | 8204 | `		}` |
|        12 | 8205 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8206 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8207 | `				"TypeError",` |
|         - | 8208 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8209 | `				"array callback must have exactly two members"` |
|         - | 8210 | `				);` |
|         - | 8211 | `		}` |
|         6 | 8212 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8213 | `			"TypeError",` |
|         - | 8214 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8215 | `			"no array or string given"` |
|         - | 8216 | `			);` |
|         - | 8217 | `	}` |
|         - | 8218 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8219 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8220 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8221 | `	/* Perform the desired operation */` |
|        13 | 8222 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8223 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8224 | `		return PH7_EXCEPTION;` |
|         - | 8225 | `	}` |
|         - | 8226 | `	/* All done, return TRUE */` |
|        13 | 8227 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8228 | `	return PH7_OK;` |
|        18 | 8229 | `}` |
|         - | 8230 | `/*` |
|         - | 8231 | ` * bool array_is_list(array $array)` |
|         - | 8232 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8233 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8234 | ` * Return` |
|         - | 8235 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8236 | ` */` |
|         - | 8237 | `/*` |
|         - | 8238 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8239 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8240 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8241 | ` */` |
|       336 | 8242 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         3 | 8243 | `{` |
|       339 | 8244 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       339 | 8245 | `	sxi64 iExpect = 0;` |
|         - | 8246 | `	sxu32 n;` |
|       777 | 8247 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       583 | 8248 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8249 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       145 | 8250 | `			return 0;` |
|         - | 8251 | `		}` |
|       441 | 8252 | `		++iExpect;` |
|       441 | 8253 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       222 | 8254 | `	}` |
|       197 | 8255 | `	return 1;` |
|       171 | 8256 | `}` |
|        12 | 8257 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8258 | `{` |
|        13 | 8259 | `	if( nArg < 1 ){` |
|       ! 0 | 8260 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8261 | `			"ArgumentCountError",` |
|         - | 8262 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8263 | `			);` |
|         - | 8264 | `	}` |
|        13 | 8265 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8266 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8267 | `			"TypeError",` |
|         - | 8268 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8269 | `			ph7_type_name(apArg[0])` |
|         - | 8270 | `			);` |
|         - | 8271 | `	}` |
|        13 | 8272 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8273 | `	return PH7_OK;` |
|         7 | 8274 | `}` |
|         - | 8275 | `/*` |
|         - | 8276 | ` * mixed array_first(array $array)` |
|         - | 8277 | ` * mixed array_last(array $array)` |
|         - | 8278 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8279 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8280 | ` *  untouched (unlike reset()/end()).` |
|         - | 8281 | ` */` |
|        18 | 8282 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8283 | `{` |
|         - | 8284 | `	ph7_hashmap *pMap;` |
|         - | 8285 | `	ph7_hashmap_node *pNode;` |
|         - | 8286 | `	ph7_value *pVal;` |
|        19 | 8287 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8288 | `	if( nArg < 1 ){` |
|       ! 0 | 8289 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8290 | `			"ArgumentCountError",` |
|         - | 8291 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8292 | `			zName` |
|         - | 8293 | `			);` |
|         - | 8294 | `	}` |
|        19 | 8295 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8296 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8297 | `			"TypeError",` |
|         - | 8298 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8299 | `			zName,` |
|         1 | 8300 | `			ph7_type_name(apArg[0])` |
|         - | 8301 | `			);` |
|         - | 8302 | `	}` |
|        17 | 8303 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8304 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8305 | `	if( pNode == 0 ){` |
|         - | 8306 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8307 | `		ph7_result_null(pCtx);` |
|         5 | 8308 | `		return PH7_OK;` |
|         - | 8309 | `	}` |
|        13 | 8310 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8311 | `	if( pVal ){` |
|        13 | 8312 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8313 | `	}else{` |
|       ! 0 | 8314 | `		ph7_result_null(pCtx);` |
|         - | 8315 | `	}` |
|        13 | 8316 | `	return PH7_OK;` |
|        10 | 8317 | `}` |
|         8 | 8318 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8319 | `{` |
|         9 | 8320 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8321 | `}` |
|        10 | 8322 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8323 | `{` |
|        11 | 8324 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8325 | `}` |
|         - | 8326 | `/*` |
|         - | 8327 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8328 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8329 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8330 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8331 | ` *  untouched.` |
|         - | 8332 | ` */` |
|        22 | 8333 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8334 | `{` |
|         - | 8335 | `	ph7_hashmap *pMap;` |
|         - | 8336 | `	ph7_hashmap_node *pNode;` |
|        23 | 8337 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8338 | `	if( nArg < 1 ){` |
|       ! 0 | 8339 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8340 | `			"ArgumentCountError",` |
|         - | 8341 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8342 | `			zName` |
|         - | 8343 | `			);` |
|         - | 8344 | `	}` |
|        23 | 8345 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8346 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8347 | `			"TypeError",` |
|         - | 8348 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8349 | `			zName,` |
|         1 | 8350 | `			ph7_type_name(apArg[0])` |
|         - | 8351 | `			);` |
|         - | 8352 | `	}` |
|        21 | 8353 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8354 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8355 | `	if( pNode == 0 ){` |
|         - | 8356 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8357 | `		ph7_result_null(pCtx);` |
|         5 | 8358 | `		return PH7_OK;` |
|         - | 8359 | `	}` |
|        17 | 8360 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8361 | `	return PH7_OK;` |
|        12 | 8362 | `}` |
|        10 | 8363 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8364 | `{` |
|        11 | 8365 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8366 | `}` |
|        12 | 8367 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8368 | `{` |
|        13 | 8369 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8370 | `}` |
|         - | 8371 | `/*` |
|         - | 8372 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8373 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8374 | ` * array_column() for both the column value and the index key.` |
|         - | 8375 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8376 | ` * container or the key is absent.` |
|         - | 8377 | ` */` |
|        32 | 8378 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8379 | `{` |
|        33 | 8380 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8381 | `		ph7_hashmap_node *pNode;` |
|        25 | 8382 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8383 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8384 | `		}` |
|        11 | 8385 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8386 | `		ph7_value sName;` |
|         - | 8387 | `		const char *zName;` |
|         - | 8388 | `		ph7_value *pAttr;` |
|         - | 8389 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8390 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8391 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8392 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8393 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8394 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8395 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8396 | `		return pAttr;` |
|         - | 8397 | `	}` |
|         5 | 8398 | `	return 0;` |
|        17 | 8399 | `}` |
|         - | 8400 | `/*` |
|         - | 8401 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8402 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8403 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8404 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8405 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8406 | ` *  Each row may be an array or an object.` |
|         - | 8407 | ` */` |
|        12 | 8408 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8409 | `{` |
|         - | 8410 | `	ph7_hashmap_node *pNode;` |
|         - | 8411 | `	ph7_hashmap *pMap;` |
|         - | 8412 | `	ph7_value *pArray;` |
|         - | 8413 | `	ph7_value *pRow;` |
|         - | 8414 | `	ph7_value *pCol;` |
|         - | 8415 | `	ph7_value *pIdx;` |
|         - | 8416 | `	int bWantCol;` |
|         - | 8417 | `	int bWantIdx;` |
|         - | 8418 | `	sxu32 n;` |
|        13 | 8419 | `	if( nArg < 2 ){` |
|       ! 0 | 8420 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8421 | `			"ArgumentCountError",` |
|         - | 8422 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8423 | `			nArg` |
|         - | 8424 | `			);` |
|         - | 8425 | `	}` |
|        13 | 8426 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8427 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8428 | `			"TypeError",` |
|         - | 8429 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8430 | `			ph7_type_name(apArg[0])` |
|         - | 8431 | `			);` |
|         - | 8432 | `	}` |
|        13 | 8433 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8434 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8435 | `	if( pArray == 0 ){` |
|       ! 0 | 8436 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8437 | `		return PH7_OK;` |
|         - | 8438 | `	}` |
|         - | 8439 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8440 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8441 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8442 | `	pNode = pMap->pFirst;` |
|        33 | 8443 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8444 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8445 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8446 | `		if( pRow == 0 ){` |
|       ! 0 | 8447 | `			continue;` |
|         - | 8448 | `		}` |
|        21 | 8449 | `		if( bWantCol ){` |
|        19 | 8450 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8451 | `			if( pCol == 0 ){` |
|         - | 8452 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8453 | `				continue;` |
|         - | 8454 | `			}` |
|         9 | 8455 | `		}else{` |
|         3 | 8456 | `			pCol = pRow;` |
|         - | 8457 | `		}` |
|        19 | 8458 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8459 | `		if( pIdx ){` |
|        13 | 8460 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8461 | `		}else{` |
|         7 | 8462 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8463 | `		}` |
|        10 | 8464 | `	}` |
|        13 | 8465 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8466 | `	return PH7_OK;` |
|         7 | 8467 | `}` |
|         - | 8468 | `/*` |
|         - | 8469 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8470 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8471 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8472 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8473 | ` */` |
|        28 | 8474 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8475 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8476 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8477 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8478 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8479 | `	)` |
|         1 | 8480 | `{` |
|         - | 8481 | `	ph7_hashmap_node *pEntry;` |
|         - | 8482 | `	ph7_hashmap *pMap;` |
|         - | 8483 | `	ph7_value *pValue;` |
|         - | 8484 | `	ph7_value *apCbArg[2];` |
|         - | 8485 | `	ph7_value sKey;` |
|         - | 8486 | `	ph7_value sResult;` |
|         - | 8487 | `	sxi32 rc;` |
|         - | 8488 | `	sxu32 n;` |
|        29 | 8489 | `	*ppMatch = 0;` |
|        29 | 8490 | `	if( nArg < 2 ){` |
|       ! 0 | 8491 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8492 | `			"ArgumentCountError",` |
|         - | 8493 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8494 | `			zName,nArg` |
|         - | 8495 | `			);` |
|         - | 8496 | `	}` |
|        29 | 8497 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8498 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8499 | `			"TypeError",` |
|         - | 8500 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8501 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8502 | `			);` |
|         - | 8503 | `	}` |
|        29 | 8504 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8505 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8506 | `			"TypeError",` |
|         - | 8507 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8508 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8509 | `			);` |
|         - | 8510 | `	}` |
|        29 | 8511 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8512 | `	pEntry = pMap->pFirst;` |
|        29 | 8513 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8514 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8515 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8516 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8517 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8518 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8519 | `		if( pValue ){` |
|         - | 8520 | `			/* The callback receives ($value, $key). */` |
|        59 | 8521 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8522 | `			apCbArg[0] = pValue;` |
|        59 | 8523 | `			apCbArg[1] = &sKey;` |
|        59 | 8524 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8525 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8526 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8527 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8528 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8529 | `				return PH7_EXCEPTION;` |
|         - | 8530 | `			}` |
|        59 | 8531 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8532 | `				*ppMatch = pEntry;` |
|        15 | 8533 | `				break;` |
|         - | 8534 | `			}` |
|        22 | 8535 | `		}` |
|        45 | 8536 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8537 | `	}` |
|        29 | 8538 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8539 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8540 | `	return PH7_OK;` |
|        15 | 8541 | `}` |
|         - | 8542 | `/*` |
|         - | 8543 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8544 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8545 | ` *  is truthy, or NULL if none match.` |
|         - | 8546 | ` */` |
|         6 | 8547 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8548 | `{` |
|         - | 8549 | `	ph7_hashmap_node *pMatch;` |
|         - | 8550 | `	ph7_value *pVal;` |
|         - | 8551 | `	sxi32 rc;` |
|         7 | 8552 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8553 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8554 | `		return rc;` |
|         - | 8555 | `	}` |
|         7 | 8556 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8557 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8558 | `	}else{` |
|         3 | 8559 | `		ph7_result_null(pCtx);` |
|         - | 8560 | `	}` |
|         7 | 8561 | `	return PH7_OK;` |
|         4 | 8562 | `}` |
|         - | 8563 | `/*` |
|         - | 8564 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8565 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8566 | ` *  is truthy, or NULL if none match.` |
|         - | 8567 | ` */` |
|         6 | 8568 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8569 | `{` |
|         - | 8570 | `	ph7_hashmap_node *pMatch;` |
|         - | 8571 | `	sxi32 rc;` |
|         7 | 8572 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8573 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8574 | `		return rc;` |
|         - | 8575 | `	}` |
|         7 | 8576 | `	if( pMatch == 0 ){` |
|         3 | 8577 | `		ph7_result_null(pCtx);` |
|         6 | 8578 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8579 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8580 | `	}else{` |
|         4 | 8581 | `		ph7_result_string(pCtx,` |
|         2 | 8582 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8583 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8584 | `	}` |
|         7 | 8585 | `	return PH7_OK;` |
|         4 | 8586 | `}` |
|         - | 8587 | `/*` |
|         - | 8588 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8589 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8590 | ` *  FALSE for an empty array.` |
|         - | 8591 | ` */` |
|         8 | 8592 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8593 | `{` |
|         - | 8594 | `	ph7_hashmap_node *pMatch;` |
|         - | 8595 | `	sxi32 rc;` |
|         9 | 8596 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8597 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8598 | `		return rc;` |
|         - | 8599 | `	}` |
|         9 | 8600 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8601 | `	return PH7_OK;` |
|         5 | 8602 | `}` |
|         - | 8603 | `/*` |
|         - | 8604 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8605 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8606 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8607 | ` */` |
|         8 | 8608 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8609 | `{` |
|         - | 8610 | `	ph7_hashmap_node *pMatch;` |
|         - | 8611 | `	sxi32 rc;` |
|         9 | 8612 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8613 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8614 | `		return rc;` |
|         - | 8615 | `	}` |
|         9 | 8616 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8617 | `	return PH7_OK;` |
|         5 | 8618 | `}` |
|         - | 8619 | `/*` |
|         - | 8620 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8621 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8622 | ` */` |
|         - | 8623 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8624 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8625 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8626 | `{` |
|        84 | 8627 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8628 | `	(void)pVm;` |
|        84 | 8629 | `	p->nCount++;` |
|        84 | 8630 | `	if( p->pArray ){` |
|         - | 8631 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8632 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8633 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8634 | `	}` |
|        84 | 8635 | `	return SXRET_OK;` |
|         4 | 8636 | `}` |
|         - | 8637 | `/*` |
|         - | 8638 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8639 | ` */` |
|        30 | 8640 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8641 | `{` |
|         - | 8642 | `	struct IterCollect sCol;` |
|         - | 8643 | `	ph7_value *pArray;` |
|         - | 8644 | `	sxi32 rc;` |
|        34 | 8645 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8646 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8647 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8648 | `	sCol.pArray = pArray;` |
|        34 | 8649 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8650 | `	sCol.nCount = 0;` |
|        34 | 8651 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8652 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8653 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8654 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8655 | `		sxu32 n;` |
|         9 | 8656 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8657 | `			ph7_value sKey, *pVal;` |
|         7 | 8658 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8659 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8660 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8661 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8662 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8663 | `			pEntry = pEntry->pPrev;` |
|         4 | 8664 | `		}` |
|         3 | 8665 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8666 | `		return PH7_OK;` |
|         - | 8667 | `	}` |
|        32 | 8668 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8669 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8670 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8671 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8672 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8673 | `			ph7_type_name(apArg[0]));` |
|         - | 8674 | `	}` |
|        30 | 8675 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8676 | `	return PH7_OK;` |
|        19 | 8677 | `}` |
|         - | 8678 | `/*` |
|         - | 8679 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8680 | ` */` |
|         8 | 8681 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8682 | `{` |
|         - | 8683 | `	struct IterCollect sCol;` |
|         - | 8684 | `	sxi32 rc;` |
|         9 | 8685 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8686 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8687 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8688 | `		return PH7_OK;` |
|         - | 8689 | `	}` |
|         7 | 8690 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8691 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8692 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8693 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8694 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8695 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8696 | `			ph7_type_name(apArg[0]));` |
|         - | 8697 | `	}` |
|         7 | 8698 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8699 | `	return PH7_OK;` |
|         5 | 8700 | `}` |
|         - | 8701 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8702 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8703 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8704 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8705 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8706 | `{` |
|        33 | 8707 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8708 | `	ph7_value sResult;` |
|         - | 8709 | `	SySet aArg;` |
|         - | 8710 | `	sxi32 rc;` |
|         - | 8711 | `	int bContinue;` |
|        16 | 8712 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8713 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8714 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8715 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8716 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8717 | `		sxu32 n;` |
|        17 | 8718 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8719 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8720 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8721 | `			pEntry = pEntry->pPrev;` |
|         5 | 8722 | `		}` |
|         4 | 8723 | `	}` |
|        33 | 8724 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8725 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8726 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8727 | `	SySetRelease(&aArg);` |
|        33 | 8728 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8729 | `	p->nCount++;` |
|        31 | 8730 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8731 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8732 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8733 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8734 | `}` |
|         - | 8735 | `/*` |
|         - | 8736 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8737 | ` */` |
|        12 | 8738 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8739 | `{` |
|         - | 8740 | `	struct IterApply sApp;` |
|         - | 8741 | `	sxi32 rc;` |
|        13 | 8742 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8743 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8744 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8745 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8746 | `	}` |
|        13 | 8747 | `	sApp.pCallback = apArg[1];` |
|        13 | 8748 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8749 | `	sApp.nCount = 0;` |
|        13 | 8750 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8751 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8752 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8753 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8754 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8755 | `			ph7_type_name(apArg[0]));` |
|         - | 8756 | `	}` |
|        11 | 8757 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8758 | `	return PH7_OK;` |
|         7 | 8759 | `}` |
|         - | 8760 | `/*` |
|         - | 8761 | ` * Table of hashmap functions.` |
|         - | 8762 | ` */` |
|         - | 8763 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8764 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8765 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8766 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8767 | `	{"count",             ph7_hashmap_count },` |
|         - | 8768 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8769 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8770 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8771 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8772 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8773 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8774 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8775 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8776 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8777 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8778 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8779 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8780 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8781 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8782 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8783 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8784 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8785 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8786 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8787 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8788 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8789 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8790 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8791 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8792 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8793 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8794 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8795 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8796 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8797 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8798 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8799 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8800 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8801 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8802 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8803 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8804 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8805 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8806 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8807 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8808 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8809 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8810 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8811 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8812 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8813 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8814 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8815 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8816 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8817 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8818 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8819 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8820 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8821 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8822 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8823 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8824 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8825 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8826 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8827 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8828 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8829 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8830 | `	{"current",           ph7_hashmap_current },` |
|         - | 8831 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8832 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8833 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8834 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8835 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8836 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8837 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8838 | `};` |
|         - | 8839 | `/*` |
|         - | 8840 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8841 | ` */` |
|      3428 | 8842 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8843 | `{` |
|         - | 8844 | `	sxu32 n;` |
|    257105 | 8845 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    253677 | 8846 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    126841 | 8847 | `	}` |
|      3433 | 8848 | `}` |
|         - | 8849 | `/*` |
|         - | 8850 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8851 | ` * the BLOB given as the first argument.` |
|         - | 8852 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8853 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8854 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8855 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8856 | ` */` |
|         - | 8857 | `/*` |
|         - | 8858 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8859 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8860 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8861 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8862 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8863 | ` */` |
|       130 | 8864 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8865 | `{` |
|       132 | 8866 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8867 | `	ph7_value *pObj;` |
|       132 | 8868 | `	sxu32 n = 0;` |
|         - | 8869 | `	int isRef;` |
|       132 | 8870 | `	sxi32 rc = SXRET_OK;` |
|         - | 8871 | `	int i;` |
|       207 | 8872 | `	for(;;){` |
|       416 | 8873 | `		if( n >= pMap->nEntry ){` |
|       132 | 8874 | `			break;` |
|         - | 8875 | `		}` |
|       286 | 8876 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8877 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8878 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       568 | 8879 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       284 | 8880 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       286 | 8881 | `		if( ShowType ){` |
|         - | 8882 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8883 | `			 * on the next line at the same indent (php). */` |
|       146 | 8884 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        98 | 8885 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        50 | 8886 | `			}` |
|        50 | 8887 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        37 | 8888 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        19 | 8889 | `			}else{` |
|        20 | 8890 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8891 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8892 | `			}` |
|        50 | 8893 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        50 | 8894 | `			if( pObj ){` |
|        50 | 8895 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        50 | 8896 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8897 | `					break;` |
|         - | 8898 | `				}` |
|        24 | 8899 | `			}` |
|        26 | 8900 | `		}else{` |
|         - | 8901 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8902 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8903 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8904 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8905 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8906 | `			}` |
|       238 | 8907 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8908 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8909 | `			}else{` |
|       170 | 8910 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8911 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8912 | `			}` |
|       236 | 8913 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8914 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8915 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8916 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8917 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8918 | `					break;` |
|         - | 8919 | `				}` |
|        13 | 8920 | `			}else{` |
|       214 | 8921 | `				if( pObj ){` |
|       214 | 8922 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8923 | `				}` |
|       214 | 8924 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8925 | `			}` |
|         - | 8926 | `		}` |
|         - | 8927 | `		/* Point to the next entry */` |
|       286 | 8928 | `		n++;` |
|       286 | 8929 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8930 | `	}` |
|       132 | 8931 | `	return rc;` |
|         2 | 8932 | `}` |
|       126 | 8933 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8934 | `{` |
|         - | 8935 | `	sxi32 rc;` |
|         - | 8936 | `	int i;` |
|       128 | 8937 | `	if( nDepth > 31 ){` |
|         - | 8938 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8939 | `		/* Nesting limit reached */` |
|       ! 0 | 8940 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8941 | `		return SXERR_LIMIT;` |
|         - | 8942 | `	}` |
|       128 | 8943 | `	if( ShowType ){` |
|         - | 8944 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8945 | `		 * newline (a nested array is itself an entry value line). */` |
|        24 | 8946 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        24 | 8947 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        24 | 8948 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        24 | 8949 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8950 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8951 | `		}` |
|        24 | 8952 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        24 | 8953 | `		return rc;` |
|         - | 8954 | `	}` |
|         - | 8955 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8956 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8957 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8958 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8959 | `	}` |
|       105 | 8960 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8961 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8962 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8963 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8964 | `	}` |
|       105 | 8965 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8966 | `	return rc;` |
|        65 | 8967 | `}` |
|         - | 8968 | `/*` |
|         - | 8969 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8970 | ` * retrieved entry.` |
|         - | 8971 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8972 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8973 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8974 | ` * a value different from PH7_OK.` |
|         - | 8975 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8976 | ` */` |
|     35166 | 8977 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8978 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8979 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8980 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8981 | `	)` |
|         5 | 8982 | `{` |
|         - | 8983 | `	ph7_hashmap_node *pEntry;` |
|         - | 8984 | `	ph7_value sKey,sValue;` |
|         - | 8985 | `	sxi32 rc;` |
|         - | 8986 | `	sxu32 n;` |
|         - | 8987 | `	/* Initialize walker parameter */` |
|     35171 | 8988 | `	rc = SXRET_OK;` |
|     35171 | 8989 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     35171 | 8990 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     35171 | 8991 | `	n = pMap->nEntry;` |
|     35171 | 8992 | `	pEntry = pMap->pFirst;` |
|         - | 8993 | `	/* Start the iteration process */` |
|     98284 | 8994 | `	for(;;){` |
|    196573 | 8995 | `		if( n < 1 ){` |
|     35169 | 8996 | `			break;` |
|         - | 8997 | `		}` |
|         - | 8998 | `		/* Extract a copy of the key and a copy the current value */` |
|    161409 | 8999 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    161409 | 9000 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 9001 | `		/* Invoke the user callback */` |
|    161409 | 9002 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 9003 | `		/* Release the copy of the key and the value */` |
|    161409 | 9004 | `		PH7_MemObjRelease(&sKey);` |
|    161409 | 9005 | `		PH7_MemObjRelease(&sValue);` |
|    161409 | 9006 | `		if( rc != PH7_OK ){` |
|         - | 9007 | `			/* Callback request an operation abort */` |
|         3 | 9008 | `			return SXERR_ABORT;` |
|         - | 9009 | `		}` |
|         - | 9010 | `		/* Point to the next entry */` |
|    161407 | 9011 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    161407 | 9012 | `		n--;` |
|         5 | 9013 | `	}` |
|         - | 9014 | `	/* All done */` |
|     35169 | 9015 | `	return SXRET_OK;` |
|     17588 | 9016 | `}` |
|         - | 9017 |  |
