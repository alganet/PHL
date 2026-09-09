# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3959/4466 lines (88.65%)

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
|   7489472 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7489477 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7489477 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    634092 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    634097 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    634097 |   35 | `	sxu32 nH = 5381;` |
|    634097 |   36 | `	zEnd = &zIn[nLen];` |
|    717062 |   37 | `	for(;;){` |
|   1434129 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1227531 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1099637 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    957687 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    634097 |   43 | `	return nH;` |
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
|   3185810 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3185815 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3185815 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3185815 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3185815 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3185815 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3185815 |  112 | `	pNode->nHash = nHash;` |
|   3185815 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3185815 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3185815 |  115 | `	return pNode;` |
|   1592910 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    262820 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    262825 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    262825 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    262825 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    262825 |  133 | `	pNode->pMap  = &(*pMap);` |
|    262825 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    262825 |  135 | `	pNode->nHash = nHash;` |
|    262825 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    262825 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    262825 |  138 | `	pNode->nValIdx = nValIdx;` |
|    262825 |  139 | `	return pNode;` |
|    131415 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3448630 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3448635 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2973939 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2973939 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1486967 |  150 | `	}` |
|   3448635 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3448635 |  153 | `	if( pMap->pFirst == 0 ){` |
|     90075 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     90075 |  156 | `		pMap->pCur = pNode;` |
|     45040 |  157 | `	}else{` |
|   3358565 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3448635 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3448635 |  174 | `	++pMap->nEntry;` |
|   3448635 |  175 | `}` |
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
|   3448630 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3448635 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     95331 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     95331 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     95331 |  245 | `		if( nNew < 1 ){` |
|     90075 |  246 | `			nNew = 16;` |
|     45035 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     95331 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     95331 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     95331 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     95331 |  260 | `		pMap->apBucket = apNew;` |
|     95331 |  261 | `		pMap->nSize = nNew;` |
|     95331 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     90075 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5261 |  267 | `		pEntry = pMap->pFirst;` |
|      5261 |  268 | `		n = 0;` |
|   2127492 |  269 | `		for( ;; ){` |
|   4254989 |  270 | `			if( n >= pMap->nEntry ){` |
|      5261 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4249733 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4249733 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4249733 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3611873 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3611873 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1805934 |  280 | `			}` |
|   4249733 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4249733 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4249733 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5261 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2628 |  288 | `	}` |
|   3358565 |  289 | `	return SXRET_OK;` |
|   1724320 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3185810 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3185815 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3185773 |  310 | `		if( pValue ){` |
|   3185767 |  311 | `			sSafeVal = *pValue;` |
|   3185767 |  312 | `			pValue = &sSafeVal;` |
|   1592881 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3185773 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3185773 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3185773 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3185767 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1592881 |  322 | `		}` |
|   3185773 |  323 | `		nIdx = pObj->nIdx;` |
|   1592889 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3185815 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3185815 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3185815 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3185815 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3185815 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3185815 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3185815 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3185815 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3185815 |  349 | `	return SXRET_OK;` |
|   1592910 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    262820 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    262825 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    216879 |  370 | `		if( pValue ){` |
|    216569 |  371 | `			sSafeVal = *pValue;` |
|    216569 |  372 | `			pValue = &sSafeVal;` |
|    108282 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    216879 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    216879 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    216879 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    216569 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    108282 |  382 | `		}` |
|    216879 |  383 | `		nIdx = pObj->nIdx;` |
|    108442 |  384 | `	}else{` |
|     45951 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    262825 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    262825 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    262825 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    262825 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     45951 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     22973 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    262825 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    262825 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    262825 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    262825 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    262825 |  409 | `	return SXRET_OK;` |
|    131415 |  410 | `}` |
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
|    405868 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    405873 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     34601 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    371277 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    371277 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    305614 |  475 | `	for(;;){` |
|    611233 |  476 | `		if( pNode == 0 ){` |
|    306617 |  477 | `			break;` |
|         - |  478 | `		}` |
|    304616 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    303105 |  480 | `			&& pNode->nHash == nHash` |
|    183177 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     64765 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     64665 |  484 | `				if( ppNode ){` |
|     64637 |  485 | `					*ppNode = pNode;` |
|     32316 |  486 | `				}` |
|     64665 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    239961 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    306617 |  493 | `	return SXERR_NOTFOUND;` |
|    202939 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    406000 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    406005 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    406005 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    406005 |  504 | `	int isNeg = FALSE, nDigit;` |
|    406005 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    405983 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    405979 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    405979 |  516 | `	zDigit = zIn;` |
|    203421 |  517 | `	for(;;){` |
|    406847 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    406597 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    405729 |  523 | `			return FALSE;` |
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
|    203005 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    148258 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    148263 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    148263 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    143171 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    143171 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    143157 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    143157 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|         7 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      5111 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        27 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        13 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      5111 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     74129 |  575 | `result:` |
|    148263 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     69001 |  578 | `		if( ppNode ){` |
|     68949 |  579 | `			*ppNode = pNode;` |
|     34472 |  580 | `		}` |
|     69001 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     79267 |  584 | `	return SXERR_NOTFOUND;` |
|     74134 |  585 | `}` |
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
|   1042872 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1042877 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1042871 |  623 | `	return FALSE;` |
|    521441 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3402240 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3402245 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3402245 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3402245 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    216881 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    216881 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       229 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    324977 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    108324 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|    216167 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    216037 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    216037 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1592682 |  683 | `IntKey:` |
|   3185597 |  684 | `	if( pKey ){` |
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
|   1042843 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1042841 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1042835 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1042835 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1042833 |  726 | `			++pMap->iNextIdx;` |
|    521414 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3185393 |  730 | `	return rc;` |
|   1701125 |  731 | `}` |
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
|   1475499 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1475504 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1475504 |  832 | `	return pObj;` |
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
|     72170 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     72175 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     72175 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     72175 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     72175 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     72175 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     72175 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     72175 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     72175 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     72175 |  921 | `	return rc;` |
|     36056 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     14086 |  927 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     14091 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     14091 |  932 | `	if( pEntry->pPrevCollide ){` |
|     11477 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5745 |  934 | `	}else{` |
|      2619 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     14091 |  937 | `	if( pEntry->pNextCollide ){` |
|      1142 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       589 |  939 | `	}` |
|     14091 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     14091 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     14091 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     14091 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     14091 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14091 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     11797 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5901 |  949 | `	}` |
|     14091 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14091 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     14091 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     14091 |  956 | `		pMap->iNextIdx++;` |
|      7043 |  957 | `	}` |
|     14091 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     33172 |  966 | `static int HashmapFindValue(` |
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
|     33177 |  979 | `	pEntry = pMap->pFirst;` |
|     33177 |  980 | `	n = pMap->nEntry;` |
|     33177 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33177 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     79088 |  983 | `	for(;;){` |
|    158178 |  984 | `		if( n < 1 ){` |
|       115 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    158064 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    158064 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    158064 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    158064 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    158064 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    158064 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    158064 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    158064 | 1001 | `			if( rc == 0 ){` |
|     33063 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     33063 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     62502 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    125006 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    125006 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|       115 | 1014 | `	return SXERR_NOTFOUND;` |
|     16591 | 1015 | `}` |
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
|    686628 | 1201 | `static sxi32 HashmapDuplicateNode(` |
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
|    686628 | 1212 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    686630 | 1213 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
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
|    686625 | 1238 | `	sSafeVal = *pVal;` |
|         - | 1239 |  |
|    686625 | 1240 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1241 | `		/* Blob key insertion */` |
|      3987 | 1242 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      3987 | 1243 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      3987 | 1244 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      3987 | 1245 | `		PH7_MemObjRelease(&sKey);` |
|      1996 | 1246 | `	}else{` |
|         - | 1247 | `		/* Int key */` |
|    682643 | 1248 | `		if( iAction == 0 ){ /* Merge */` |
|    682397 | 1249 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    341446 | 1250 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1251 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1252 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1253 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1254 | `		}else{ /* Dup */` |
|       220 | 1255 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1256 | `		}` |
|         - | 1257 | `	}` |
|    686625 | 1258 | `	return rc;` |
|    343319 | 1259 | `}` |
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
|      2824 | 1272 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1273 | `{` |
|         - | 1274 | `	ph7_hashmap_node *pEntry;` |
|         - | 1275 | `	ph7_value *pVal;` |
|         - | 1276 | `	sxi32 rc;` |
|         - | 1277 | `	sxu32 n;` |
|      2829 | 1278 | `	if( pSrc == pDest ){` |
|         - | 1279 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1280 | `		 * Unlike the zend engine.` |
|         - | 1281 | `		 */` |
|       ! 0 | 1282 | `		return SXRET_OK;` |
|         - | 1283 | `	}` |
|         - | 1284 | `	/* Point to the first inserted entry in the source */` |
|      2829 | 1285 | `	pEntry = pSrc->pFirst;` |
|         - | 1286 | `	/* Perform the merge */` |
|    685281 | 1287 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1288 | `		/* Extract the node value */` |
|    682457 | 1289 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    682457 | 1290 | `		if( pVal ){` |
|         - | 1291 | `			/* Make a local copy of the value.` |
|         - | 1292 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1293 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1294 | `			 * to the old pool.` |
|         - | 1295 | `			 */` |
|    682457 | 1296 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    341231 | 1297 | `		}else{` |
|       ! 0 | 1298 | `			rc = SXRET_OK;` |
|         - | 1299 | `		}` |
|    682457 | 1300 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1301 | `			return rc;` |
|         - | 1302 | `		}` |
|         - | 1303 | `		/* Point to the next entry */` |
|    682457 | 1304 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    341231 | 1305 | `	}` |
|      2829 | 1306 | `	return SXRET_OK;` |
|      1417 | 1307 | `}` |
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
|    236486 | 1463 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1464 | `{` |
|    236491 | 1465 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1466 | `	ph7_hashmap *pNew;` |
|         - | 1467 | `	ph7_value *pBacking;` |
|         - | 1468 | `	sxu32 nValIdx;` |
|         - | 1469 | `	int bValueInPool;` |
|    236491 | 1470 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    236491 | 1471 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1472 | `		/* Sole owner, no separation needed */` |
|    233777 | 1473 | `		return pMap;` |
|         - | 1474 | `	}` |
|      2719 | 1475 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1476 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1477 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1478 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1479 | `		return pMap;` |
|         - | 1480 | `	}` |
|         - | 1481 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1482 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1483 | `	 * frame is popped. */` |
|      2593 | 1484 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2593 | 1485 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2588 | 1486 | `		if( pBacking && pBacking != pValue` |
|      2563 | 1487 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2543 | 1488 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1489 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2543 | 1490 | `			pMap->iRef--;` |
|      2543 | 1491 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1492 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2497 | 1493 | `				pMap->iRef++;` |
|      2497 | 1494 | `				return pMap;` |
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
|    118248 | 1557 | `}` |
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
|    144334 | 1649 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1650 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1651 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1652 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1653 | `	)` |
|         5 | 1654 | `{` |
|         - | 1655 | `	ph7_hashmap *pMap;` |
|         - | 1656 | `	/* Allocate a new instance */` |
|    144339 | 1657 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    144339 | 1658 | `	if( pMap == 0 ){` |
|       ! 0 | 1659 | `		return 0;` |
|         - | 1660 | `	}` |
|         - | 1661 | `	/* Zero the structure */` |
|    144339 | 1662 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1663 | `	/* Fill in the structure */` |
|    144339 | 1664 | `	pMap->pVm = &(*pVm);` |
|    144339 | 1665 | `	pMap->iRef = 1;` |
|         - | 1666 | `	/* Default hash functions */` |
|    144339 | 1667 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    144339 | 1668 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    144339 | 1669 | `	return pMap;` |
|     72172 | 1670 | `}` |
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
|    102406 | 1762 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1763 | `{` |
|         - | 1764 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    102411 | 1765 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1766 | `	sxu32 n;` |
|    102411 | 1767 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1768 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1769 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1770 | `		return SXRET_OK;` |
|         - | 1771 | `	}` |
|    102411 | 1772 | `	if( pMap->pActiveSteps ){` |
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
|    102411 | 1785 | `	n = 0;` |
|    102411 | 1786 | `	pEntry = pMap->pFirst;` |
|   1733131 | 1787 | `	for(;;){` |
|   3466267 | 1788 | `		if( n >= pMap->nEntry ){` |
|    102411 | 1789 | `			break;` |
|         - | 1790 | `		}` |
|   3363861 | 1791 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1792 | `		/* Remove the reference from the foreign table */` |
|   3363861 | 1793 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3363861 | 1794 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1795 | `			/* Restore the ph7_value to the free list */` |
|   3363801 | 1796 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1681898 | 1797 | `		}` |
|         - | 1798 | `		/* Release the node */` |
|   3363861 | 1799 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    189359 | 1800 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     94677 | 1801 | `		}` |
|   3363861 | 1802 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1803 | `		/* Point to the next entry */` |
|   3363861 | 1804 | `		pEntry = pNext;` |
|   3363861 | 1805 | `		n++;` |
|         5 | 1806 | `	}` |
|    102411 | 1807 | `	if( pMap->nEntry > 0 ){` |
|         - | 1808 | `		/* Release the hash bucket */` |
|     75139 | 1809 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     37567 | 1810 | `	}` |
|    102411 | 1811 | `	if( FreeDS ){` |
|         - | 1812 | `		/* Free the whole instance */` |
|    102385 | 1813 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     51195 | 1814 | `	}else{` |
|         - | 1815 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1816 | `		pMap->apBucket = 0;` |
|        28 | 1817 | `		pMap->iNextIdx = 0;` |
|        28 | 1818 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1819 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1820 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1821 | `	}` |
|    102411 | 1822 | `	return SXRET_OK;` |
|     51208 | 1823 | `}` |
|         - | 1824 | `/*` |
|         - | 1825 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1826 | ` * If the count reaches zero which mean no more variables` |
|         - | 1827 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1828 | ` */` |
|    842194 | 1829 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1830 | `{` |
|    842199 | 1831 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1832 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    842199 | 1833 | `	pMap->iRef--;` |
|    842199 | 1834 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    102365 | 1835 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     51180 | 1836 | `	}` |
|    842199 | 1837 | `}` |
|         - | 1838 | `/*` |
|         - | 1839 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1840 | ` * Write a pointer to the target node on success.` |
|         - | 1841 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1842 | ` */` |
|    148446 | 1843 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1844 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1845 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1846 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1847 | `	)` |
|         5 | 1848 | `{` |
|         - | 1849 | `	sxi32 rc;` |
|    148451 | 1850 | `	if( pMap->nEntry < 1 ){` |
|         - | 1851 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1852 | `		 */` |
|       193 | 1853 | `		return SXERR_NOTFOUND;` |
|         - | 1854 | `	}` |
|    148263 | 1855 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    148263 | 1856 | `	return rc;` |
|     74228 | 1857 | `}` |
|         - | 1858 | `/*` |
|         - | 1859 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1860 | ` * hashmap.` |
|         - | 1861 | ` * If a node with the given key already exists in the database` |
|         - | 1862 | ` * then this function overwrite the old value.` |
|         - | 1863 | ` */` |
|   2719514 | 1864 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2719519 | 1875 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2719519 | 1876 | `	return rc;` |
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
|     19124 | 1940 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1941 | `{` |
|     19129 | 1942 | `	pStep->pCursor = pMap->pFirst;` |
|     19129 | 1943 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     19129 | 1944 | `	pMap->pActiveSteps = pStep;` |
|     19129 | 1945 | `}` |
|         - | 1946 | `/*` |
|         - | 1947 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1948 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1949 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1950 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1951 | ` */` |
|     19022 | 1952 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1953 | `{` |
|     19027 | 1954 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     19027 | 1955 | `	while( *ppLink ){` |
|     19027 | 1956 | `		if( *ppLink == pStep ){` |
|     19027 | 1957 | `			*ppLink = pStep->pNextActive;` |
|     19027 | 1958 | `			pStep->pNextActive = 0;` |
|     19027 | 1959 | `			return;` |
|         - | 1960 | `		}` |
|       ! 0 | 1961 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1962 | `	}` |
|      9516 | 1963 | `}` |
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
|    596434 | 1984 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1985 | `{` |
|    596439 | 1986 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    596439 | 1987 | `	if( pEntry ){` |
|    596439 | 1988 | `		if( bStore ){` |
|    237501 | 1989 | `			PH7_MemObjStore(pEntry,pValue);` |
|    118753 | 1990 | `		}else{` |
|    358943 | 1991 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1992 | `		}` |
|    298154 | 1993 | `	}else{` |
|       ! 0 | 1994 | `		PH7_MemObjRelease(pValue);` |
|         - | 1995 | `	}` |
|    596439 | 1996 | `}` |
|         - | 1997 | `/*` |
|         - | 1998 | ` * Extract a node key.` |
|         - | 1999 | ` */` |
|    158644 | 2000 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2001 | `{` |
|         - | 2002 | `	/* Fill with the current key */` |
|    158649 | 2003 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    153355 | 2004 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2005 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2006 | `		}` |
|    153355 | 2007 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    153355 | 2008 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     76680 | 2009 | `	}else{` |
|      5299 | 2010 | `		SyBlobReset(&pKey->sBlob);` |
|      5299 | 2011 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5299 | 2012 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2013 | `	}` |
|    158649 | 2014 | `}` |
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
|     36210 | 2065 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2066 | `{` |
|         - | 2067 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2068 | `    /* Prevent compiler warning */` |
|     36215 | 2069 | `	result.pNext = result.pPrev = 0;` |
|     36215 | 2070 | `	pTail = &result;` |
|    108541 | 2071 | `	while( pA && pB ){` |
|     72331 | 2072 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47857 | 2073 | `			pTail->pPrev = pA;` |
|     47857 | 2074 | `			pA->pNext = pTail;` |
|     47857 | 2075 | `			pTail = pA;` |
|     47857 | 2076 | `			pA = pA->pPrev;` |
|     23868 | 2077 | `		}else{` |
|     24479 | 2078 | `			pTail->pPrev = pB;` |
|     24479 | 2079 | `			pB->pNext = pTail;` |
|     24479 | 2080 | `			pTail = pB;` |
|     24479 | 2081 | `			pB = pB->pPrev;` |
|         - | 2082 | `		}` |
|         5 | 2083 | `	}` |
|     36215 | 2084 | `	if( pA ){` |
|     25608 | 2085 | `		pTail->pPrev = pA;` |
|     25608 | 2086 | `		pA->pNext = pTail;` |
|     23460 | 2087 | `	}else if( pB ){` |
|     10372 | 2088 | `		pTail->pPrev = pB;` |
|     10372 | 2089 | `		pB->pNext = pTail;` |
|      5142 | 2090 | `	}else{` |
|       245 | 2091 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2092 | `	}` |
|     36215 | 2093 | `	return result.pPrev;` |
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
|       756 | 2107 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2108 | `{` |
|         - | 2109 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2110 | `	sxu32 i;` |
|       761 | 2111 | `	SyZero(a,sizeof(a));` |
|         - | 2112 | `	/* Point to the first inserted entry */` |
|       761 | 2113 | `	pIn = pMap->pFirst;` |
|     14859 | 2114 | `	while( pIn ){` |
|     14103 | 2115 | `		p = pIn;` |
|     14103 | 2116 | `		pIn = p->pPrev;` |
|     14103 | 2117 | `		p->pPrev = 0;` |
|     26877 | 2118 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26877 | 2119 | `			if( a[i]==0 ){` |
|     14103 | 2120 | `				a[i] = p;` |
|     14103 | 2121 | `				break;` |
|       ! 0 | 2122 | `			}else{` |
|     12779 | 2123 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12779 | 2124 | `				a[i] = 0;` |
|         - | 2125 | `			}` |
|      6392 | 2126 | `		}` |
|     14103 | 2127 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2128 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2129 | `			 * But that is impossible.` |
|         - | 2130 | `			 */` |
|       ! 0 | 2131 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2132 | `		}` |
|         5 | 2133 | `	}` |
|       761 | 2134 | `	p = a[0];` |
|     24197 | 2135 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     23441 | 2136 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11723 | 2137 | `	}` |
|       761 | 2138 | `	p->pNext = 0;` |
|         - | 2139 | `	/* Reflect the change */` |
|       761 | 2140 | `	pMap->pFirst = p;` |
|         - | 2141 | `	/* Reset the loop cursor */` |
|       761 | 2142 | `	pMap->pCur = pMap->pFirst;` |
|       761 | 2143 | `	return SXRET_OK;` |
|         5 | 2144 | `}` |
|         - | 2145 | `/* SPDX-SnippetEnd */` |
|         - | 2146 | `/*` |
|         - | 2147 | ` * Node comparison callback.` |
|         - | 2148 | ` * used-by: [sort(),asort(),...]` |
|         - | 2149 | ` */` |
|     72042 | 2150 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2151 | `{` |
|         - | 2152 | `	ph7_value sA,sB;` |
|         - | 2153 | `	sxi32 iFlags;` |
|         - | 2154 | `	int rc;` |
|     72047 | 2155 | `	if( pCmpData == 0 ){` |
|         - | 2156 | `		/* Perform a standard comparison */` |
|     72021 | 2157 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     72021 | 2158 | `		return rc;` |
|         - | 2159 | `	}` |
|        27 | 2160 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2161 | `	/* Duplicate node values */` |
|        27 | 2162 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        27 | 2163 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        27 | 2164 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        27 | 2165 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        27 | 2166 | `	if( iFlags == 5 ){` |
|         - | 2167 | `		/* String cast */` |
|         - | 2168 | `		const char *zA,*zB;` |
|         - | 2169 | `		sxu32 nA,nB,nMin;` |
|        17 | 2170 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2171 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2172 | `		}` |
|        17 | 2173 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2174 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2175 | `		}` |
|         - | 2176 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        17 | 2177 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        17 | 2178 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        17 | 2179 | `		nA = SyBlobLength(&sA.sBlob);` |
|        17 | 2180 | `		nB = SyBlobLength(&sB.sBlob);` |
|        17 | 2181 | `		nMin = nA < nB ? nA : nB;` |
|        17 | 2182 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        17 | 2183 | `		if( rc == 0 ){` |
|         5 | 2184 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2185 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2186 | `		}` |
|         9 | 2187 | `	}else{` |
|         - | 2188 | `		/* Numeric cast */` |
|        11 | 2189 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2190 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2191 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2192 | `	}` |
|        27 | 2193 | `	PH7_MemObjRelease(&sA);` |
|        27 | 2194 | `	PH7_MemObjRelease(&sB);` |
|        27 | 2195 | `	return rc;` |
|     35992 | 2196 | `}` |
|         - | 2197 | `/*` |
|         - | 2198 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2199 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2200 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2201 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2202 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2203 | ` */` |
|         - | 2204 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2205 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2206 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        36 | 2207 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2208 | `{` |
|        38 | 2209 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        38 | 2210 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        38 | 2211 | `	if( rc == 0 ){` |
|       ! 0 | 2212 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2213 | `	}` |
|        38 | 2214 | `	return rc;` |
|         2 | 2215 | `}` |
|        58 | 2216 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2217 | `{` |
|         - | 2218 | `	sxi32 rc;` |
|        60 | 2219 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2220 | `		/* Perform a string comparison */` |
|        32 | 2221 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        20 | 2222 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        12 | 2223 | `	}else{` |
|         - | 2224 | `		SyString sStr;` |
|        39 | 2225 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2226 | `		int bNum = 1;` |
|        39 | 2227 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2228 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2229 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2230 | `				bNum = 0;` |
|         6 | 2231 | `			}else{` |
|       ! 0 | 2232 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2233 | `			}` |
|         6 | 2234 | `		}else{` |
|        29 | 2235 | `			iA = pA->xKey.iKey;` |
|         - | 2236 | `		}` |
|        39 | 2237 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2238 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2239 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2240 | `				bNum = 0;` |
|         4 | 2241 | `			}else{` |
|       ! 0 | 2242 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2243 | `			}` |
|         4 | 2244 | `		}else{` |
|        33 | 2245 | `			iB = pB->xKey.iKey;` |
|         - | 2246 | `		}` |
|        39 | 2247 | `		if( bNum ){` |
|        23 | 2248 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2249 | `		}else{` |
|         - | 2250 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2251 | `			char zNumA[24],zNumB[24];` |
|         - | 2252 | `			SyString sA,sB;` |
|        17 | 2253 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2254 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2255 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2256 | `			}else{` |
|        11 | 2257 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2258 | `			}` |
|        17 | 2259 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2260 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2261 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2262 | `			}else{` |
|         7 | 2263 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2264 | `			}` |
|        17 | 2265 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2266 | `		}` |
|         - | 2267 | `	}` |
|        60 | 2268 | `	return rc;` |
|         2 | 2269 | `}` |
|         - | 2270 | `/*` |
|         - | 2271 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2272 | ` * used-by: [ksort()]` |
|         - | 2273 | ` */` |
|        44 | 2274 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2275 | `{` |
|        22 | 2276 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        46 | 2277 | `	return HashmapKeyNodeCmp(pA,pB);` |
|         2 | 2278 | `}` |
|         - | 2279 | `/*` |
|         - | 2280 | ` * Node comparison callback.` |
|         - | 2281 | ` * Used by: [rsort(),arsort()];` |
|         - | 2282 | ` */` |
|        80 | 2283 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2284 | `{` |
|         - | 2285 | `	ph7_value sA,sB;` |
|         - | 2286 | `	sxi32 iFlags;` |
|         - | 2287 | `	int rc;` |
|        81 | 2288 | `	if( pCmpData == 0 ){` |
|         - | 2289 | `		/* Perform a standard comparison */` |
|        59 | 2290 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2291 | `		return -rc;` |
|         - | 2292 | `	}` |
|        23 | 2293 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2294 | `	/* Duplicate node values */` |
|        23 | 2295 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        23 | 2296 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        23 | 2297 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        23 | 2298 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        23 | 2299 | `	if( iFlags == 5 ){` |
|         - | 2300 | `		/* String cast */` |
|         - | 2301 | `		const char *zA,*zB;` |
|         - | 2302 | `		sxu32 nA,nB,nMin;` |
|        11 | 2303 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2304 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2305 | `		}` |
|        11 | 2306 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2307 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2308 | `		}` |
|         - | 2309 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2310 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2311 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2312 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2313 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2314 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2315 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2316 | `		if( rc == 0 ){` |
|         3 | 2317 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2318 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2319 | `		}` |
|         6 | 2320 | `	}else{` |
|         - | 2321 | `		/* Numeric cast */` |
|        13 | 2322 | `		PH7_MemObjToNumeric(&sA);` |
|        13 | 2323 | `		PH7_MemObjToNumeric(&sB);` |
|        13 | 2324 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2325 | `	}` |
|        23 | 2326 | `	PH7_MemObjRelease(&sA);` |
|        23 | 2327 | `	PH7_MemObjRelease(&sB);` |
|        23 | 2328 | `	return -rc;` |
|        41 | 2329 | `}` |
|         - | 2330 | `/*` |
|         - | 2331 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2332 | ` * used-by: [usort(),uasort()]` |
|         - | 2333 | ` */` |
|       116 | 2334 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2335 | `{` |
|         - | 2336 | `	ph7_value sResult,*pCallback;` |
|         - | 2337 | `	ph7_value *pV1,*pV2;` |
|         - | 2338 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2339 | `	sxi32 rc;` |
|         - | 2340 | `	/* Point to the desired callback */` |
|       119 | 2341 | `	pCallback = (ph7_value *)pCmpData;` |
|       119 | 2342 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2343 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2344 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2345 | `		return 0;` |
|         - | 2346 | `	}` |
|         - | 2347 | `	/* initialize the result value */` |
|       113 | 2348 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2349 | `	/* Extract nodes values */` |
|       113 | 2350 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       113 | 2351 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       113 | 2352 | `	apArg[0] = pV1;` |
|       113 | 2353 | `	apArg[1] = pV2;` |
|         - | 2354 | `	/* Invoke the callback */` |
|       113 | 2355 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       113 | 2356 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2357 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2358 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2359 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2360 | `		rc = 0;` |
|       108 | 2361 | `	}else if( rc != SXRET_OK ){` |
|         - | 2362 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2363 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2364 | `	}else{` |
|         - | 2365 | `		/* Extract callback result */` |
|       104 | 2366 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2367 | `			/* Perform an int cast */` |
|       ! 0 | 2368 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2369 | `		}` |
|       104 | 2370 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2371 | `	}` |
|       113 | 2372 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2373 | `	/* Callback result */` |
|       113 | 2374 | `	return rc;` |
|        61 | 2375 | `}` |
|         - | 2376 | `/*` |
|         - | 2377 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2378 | ` * used-by: [krsort()]` |
|         - | 2379 | ` */` |
|        14 | 2380 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2381 | `{` |
|         7 | 2382 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        15 | 2383 | `	return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         1 | 2384 | `}` |
|         - | 2385 | `/*` |
|         - | 2386 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2387 | ` * used-by: [uksort()]` |
|         - | 2388 | ` */` |
|         6 | 2389 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2390 | `{` |
|         - | 2391 | `	ph7_value sResult,*pCallback;` |
|         - | 2392 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2393 | `	ph7_value sK1,sK2;` |
|         - | 2394 | `	sxi32 rc;` |
|         - | 2395 | `	/* Point to the desired callback */` |
|         7 | 2396 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2397 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2398 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2399 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2400 | `		return 0;` |
|         - | 2401 | `	}` |
|         - | 2402 | `	/* initialize the result value */` |
|         7 | 2403 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2404 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2405 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2406 | `	/* Extract nodes keys */` |
|         7 | 2407 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2408 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2409 | `	apArg[0] = &sK1;` |
|         7 | 2410 | `	apArg[1] = &sK2;` |
|         - | 2411 | `	/* Mark keys as constants */` |
|         7 | 2412 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2413 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2414 | `	/* Invoke the callback */` |
|         7 | 2415 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2416 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2417 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2418 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2419 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2420 | `		rc = 0;` |
|         7 | 2421 | `	}else if( rc != SXRET_OK ){` |
|         - | 2422 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2423 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2424 | `	}else{` |
|         - | 2425 | `		/* Extract callback result */` |
|         7 | 2426 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2427 | `			/* Perform an int cast */` |
|       ! 0 | 2428 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2429 | `		}` |
|         7 | 2430 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2431 | `	}` |
|         7 | 2432 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2433 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2434 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2435 | `	/* Callback result */` |
|         7 | 2436 | `	return rc;` |
|         4 | 2437 | `}` |
|         - | 2438 | `/*` |
|         - | 2439 | ` * Node comparison callback: Random node comparison.` |
|         - | 2440 | ` * used-by: [shuffle()]` |
|         - | 2441 | ` */` |
|        24 | 2442 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2443 | `{` |
|         - | 2444 | `	sxu32 n;` |
|        13 | 2445 | `	SXUNUSED(pB); /* cc warning */` |
|        13 | 2446 | `	SXUNUSED(pCmpData);` |
|         - | 2447 | `	/* Grab a random number */` |
|        25 | 2448 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2449 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2450 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2451 | `	 */` |
|        25 | 2452 | `	return n&1 ? 1 : -1;` |
|         1 | 2453 | `}` |
|         - | 2454 | `/*` |
|         - | 2455 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2456 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2457 | ` */` |
|       686 | 2458 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2459 | `{` |
|         - | 2460 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2461 | `	sxu32 i;` |
|         - | 2462 | `	/* Rehash all entries */` |
|       691 | 2463 | `	pLast = p = pMap->pFirst;` |
|       691 | 2464 | `	pMap->iNextIdx = 0;` |
|       691 | 2465 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|       691 | 2466 | `	i = 0;` |
|      7274 | 2467 | `	for( ;; ){` |
|     14553 | 2468 | `		if( i >= pMap->nEntry ){` |
|       691 | 2469 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       691 | 2470 | `			break;` |
|         - | 2471 | `		}` |
|     13867 | 2472 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2473 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2474 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2475 | `			/* Change key type */` |
|         5 | 2476 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2477 | `		}` |
|     13867 | 2478 | `		HashmapRehashIntNode(p);` |
|         - | 2479 | `		/* Point to the next entry */` |
|     13867 | 2480 | `		i++;` |
|     13867 | 2481 | `		pLast = p;` |
|     13867 | 2482 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2483 | `	}` |
|       691 | 2484 | `}` |
|         - | 2485 | `/*` |
|         - | 2486 | ` * Array functions implementation.` |
|         - | 2487 | ` * Status:` |
|         - | 2488 | ` *  Stable.` |
|         - | 2489 | ` */` |
|         - | 2490 | `/*` |
|         - | 2491 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2492 | ` * Sort an array.` |
|         - | 2493 | ` * Parameters` |
|         - | 2494 | ` *  $array` |
|         - | 2495 | ` *   The input array.` |
|         - | 2496 | ` * $sort_flags` |
|         - | 2497 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2498 | ` *  Sorting type flags:` |
|         - | 2499 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2500 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2501 | ` *   SORT_STRING - compare items as strings` |
|         - | 2502 | ` * Return` |
|         - | 2503 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2504 | ` *` |
|         - | 2505 | ` */` |
|      1040 | 2506 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2507 | `{` |
|         - | 2508 | `	ph7_hashmap *pMap;` |
|         - | 2509 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1045 | 2510 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2511 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2512 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2513 | `		return PH7_OK;` |
|         - | 2514 | `	}` |
|         - | 2515 | `	/* Point to the internal representation of the input hashmap */` |
|      1045 | 2516 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1045 | 2517 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1045 | 2518 | `	if( pMap->nEntry > 1 ){` |
|       663 | 2519 | `		sxi32 iCmpFlags = 0;` |
|       663 | 2520 | `		if( nArg > 1 ){` |
|         - | 2521 | `			/* Extract comparison flags */` |
|         5 | 2522 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2523 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2524 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2525 | `			}` |
|         2 | 2526 | `		}` |
|         - | 2527 | `		/* Do the merge sort */` |
|       663 | 2528 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2529 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       663 | 2530 | `		HashmapSortRehash(pMap);` |
|       329 | 2531 | `	}` |
|         - | 2532 | `	/* All done,return TRUE */` |
|      1045 | 2533 | `	ph7_result_bool(pCtx,1);` |
|      1045 | 2534 | `	return PH7_OK;` |
|       525 | 2535 | `}` |
|         - | 2536 | `/*` |
|         - | 2537 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2538 | ` *  Sort an array and maintain index association.` |
|         - | 2539 | ` * Parameters` |
|         - | 2540 | ` *  $array` |
|         - | 2541 | ` *   The input array.` |
|         - | 2542 | ` * $sort_flags` |
|         - | 2543 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2544 | ` *  Sorting type flags:` |
|         - | 2545 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2546 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2547 | ` *   SORT_STRING - compare items as strings` |
|         - | 2548 | ` * Return` |
|         - | 2549 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2550 | ` */` |
|        32 | 2551 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2552 | `{` |
|         - | 2553 | `	ph7_hashmap *pMap;` |
|         - | 2554 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2555 | `	if( nArg < 1 ){` |
|       ! 0 | 2556 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2557 | `			"ArgumentCountError",` |
|         - | 2558 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2559 | `			);` |
|         - | 2560 | `	}` |
|         - | 2561 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2562 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2563 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2564 | `			"TypeError",` |
|         - | 2565 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2566 | `			ph7_type_name(apArg[0])` |
|         - | 2567 | `			);` |
|         - | 2568 | `	}` |
|         - | 2569 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2570 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2571 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2572 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2573 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2574 | `		if( nArg > 1 ){` |
|         - | 2575 | `			/* Extract comparison flags */` |
|         5 | 2576 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2577 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2578 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2579 | `			}` |
|         2 | 2580 | `		}` |
|         - | 2581 | `		/* Do the merge sort */` |
|        21 | 2582 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2583 | `		/* Fix the last link broken by the merge */` |
|        49 | 2584 | `		while(pMap->pLast->pPrev){` |
|        29 | 2585 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2586 | `		}` |
|        10 | 2587 | `	}` |
|         - | 2588 | `	/* All done,return TRUE */` |
|        25 | 2589 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2590 | `	return PH7_OK;` |
|        21 | 2591 | `}` |
|         - | 2592 | `/*` |
|         - | 2593 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2594 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2595 | ` * Parameters` |
|         - | 2596 | ` *  $array` |
|         - | 2597 | ` *   The input array.` |
|         - | 2598 | ` * $sort_flags` |
|         - | 2599 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2600 | ` *  Sorting type flags:` |
|         - | 2601 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2602 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2603 | ` *   SORT_STRING - compare items as strings` |
|         - | 2604 | ` * Return` |
|         - | 2605 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2606 | ` */` |
|        30 | 2607 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2608 | `{` |
|         - | 2609 | `	ph7_hashmap *pMap;` |
|         - | 2610 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        35 | 2611 | `	if( nArg < 1 ){` |
|       ! 0 | 2612 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2613 | `			"ArgumentCountError",` |
|         - | 2614 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2615 | `			);` |
|         - | 2616 | `	}` |
|         - | 2617 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2618 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2619 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2620 | `			"TypeError",` |
|         - | 2621 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2622 | `			ph7_type_name(apArg[0])` |
|         - | 2623 | `			);` |
|         - | 2624 | `	}` |
|         - | 2625 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2626 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2627 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2628 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2629 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2630 | `		if( nArg > 1 ){` |
|         - | 2631 | `			/* Extract comparison flags */` |
|         5 | 2632 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2633 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2634 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2635 | `			}` |
|         2 | 2636 | `		}` |
|         - | 2637 | `		/* Do the merge sort */` |
|        19 | 2638 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2639 | `		/* Fix the last link broken by the merge */` |
|        35 | 2640 | `		while(pMap->pLast->pPrev){` |
|        17 | 2641 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2642 | `		}` |
|         9 | 2643 | `	}` |
|         - | 2644 | `	/* All done,return TRUE */` |
|        23 | 2645 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2646 | `	return PH7_OK;` |
|        20 | 2647 | `}` |
|         - | 2648 | `/*` |
|         - | 2649 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2650 | ` *  Sort an array by key.` |
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
|        14 | 2663 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2664 | `{` |
|         - | 2665 | `	ph7_hashmap *pMap;` |
|         - | 2666 | `	/* Make sure we are dealing with a valid hashmap */` |
|        16 | 2667 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2668 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2669 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2670 | `		return PH7_OK;` |
|         - | 2671 | `	}` |
|         - | 2672 | `	/* Point to the internal representation of the input hashmap */` |
|        16 | 2673 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        16 | 2674 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        16 | 2675 | `	if( pMap->nEntry > 1 ){` |
|        16 | 2676 | `		sxi32 iCmpFlags = 0;` |
|        16 | 2677 | `		if( nArg > 1 ){` |
|         - | 2678 | `			/* Extract comparison flags */` |
|       ! 0 | 2679 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2680 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2681 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2682 | `			}` |
|       ! 0 | 2683 | `		}` |
|         - | 2684 | `		/* Do the merge sort */` |
|        16 | 2685 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2686 | `		/* Fix the last link broken by the merge */` |
|        38 | 2687 | `		while(pMap->pLast->pPrev){` |
|        23 | 2688 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2689 | `		}` |
|         7 | 2690 | `	}` |
|         - | 2691 | `	/* All done,return TRUE */` |
|        16 | 2692 | `	ph7_result_bool(pCtx,1);` |
|        16 | 2693 | `	return PH7_OK;` |
|         9 | 2694 | `}` |
|         - | 2695 | `/*` |
|         - | 2696 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2697 | ` *  Sort an array by key in reverse order.` |
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
|         4 | 2710 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2711 | `{` |
|         - | 2712 | `	ph7_hashmap *pMap;` |
|         - | 2713 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2714 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2715 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2716 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2717 | `		return PH7_OK;` |
|         - | 2718 | `	}` |
|         - | 2719 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2720 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2721 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2722 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2723 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2724 | `		if( nArg > 1 ){` |
|         - | 2725 | `			/* Extract comparison flags */` |
|       ! 0 | 2726 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2727 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2728 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2729 | `			}` |
|       ! 0 | 2730 | `		}` |
|         - | 2731 | `		/* Do the merge sort */` |
|         5 | 2732 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2733 | `		/* Fix the last link broken by the merge */` |
|        17 | 2734 | `		while(pMap->pLast->pPrev){` |
|        13 | 2735 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2736 | `		}` |
|         2 | 2737 | `	}` |
|         - | 2738 | `	/* All done,return TRUE */` |
|         5 | 2739 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2740 | `	return PH7_OK;` |
|         3 | 2741 | `}` |
|         - | 2742 | `/*` |
|         - | 2743 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2744 | ` * Sort an array in reverse order.` |
|         - | 2745 | ` * Parameters` |
|         - | 2746 | ` *  $array` |
|         - | 2747 | ` *   The input array.` |
|         - | 2748 | ` * $sort_flags` |
|         - | 2749 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2750 | ` *  Sorting type flags:` |
|         - | 2751 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2752 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2753 | ` *   SORT_STRING - compare items as strings` |
|         - | 2754 | ` * Return` |
|         - | 2755 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2756 | ` */` |
|         4 | 2757 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2758 | `{` |
|         - | 2759 | `	ph7_hashmap *pMap;` |
|         - | 2760 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2761 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2762 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2763 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2764 | `		return PH7_OK;` |
|         - | 2765 | `	}` |
|         - | 2766 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2767 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2768 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2769 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2770 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2771 | `		if( nArg > 1 ){` |
|         - | 2772 | `			/* Extract comparison flags */` |
|         3 | 2773 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2774 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2775 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2776 | `			}` |
|         1 | 2777 | `		}` |
|         - | 2778 | `		/* Do the merge sort */` |
|         5 | 2779 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2780 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         5 | 2781 | `		HashmapSortRehash(pMap);` |
|         2 | 2782 | `	}` |
|         - | 2783 | `	/* All done,return TRUE */` |
|         5 | 2784 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2785 | `	return PH7_OK;` |
|         3 | 2786 | `}` |
|         - | 2787 | `/*` |
|         - | 2788 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2789 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2790 | ` * Parameters` |
|         - | 2791 | ` *  $array` |
|         - | 2792 | ` *   The input array.` |
|         - | 2793 | ` * $cmp_function` |
|         - | 2794 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2795 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2796 | ` *  to, or greater than the second.` |
|         - | 2797 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2798 | ` * Return` |
|         - | 2799 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2800 | ` */` |
|        22 | 2801 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2802 | `{` |
|         - | 2803 | `	ph7_hashmap *pMap;` |
|         - | 2804 | `	/* Make sure we are dealing with a valid hashmap */` |
|        25 | 2805 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2806 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2807 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2808 | `		return PH7_OK;` |
|         - | 2809 | `	}` |
|        25 | 2810 | `	if( nArg > 1 ){` |
|         - | 2811 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2812 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2813 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        25 | 2814 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        25 | 2815 | `		if( rcCb != PH7_OK ){` |
|         3 | 2816 | `			return rcCb;` |
|         - | 2817 | `		}` |
|        10 | 2818 | `	}` |
|         - | 2819 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2820 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2821 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2822 | `	if( pMap->nEntry > 1 ){` |
|        23 | 2823 | `		ph7_value *pCallback = 0;` |
|         - | 2824 | `		ProcNodeCmp xCmp;` |
|        23 | 2825 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        23 | 2826 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2827 | `			/* Point to the desired callback */` |
|        23 | 2828 | `			pCallback = apArg[1];` |
|        13 | 2829 | `		}else{` |
|         - | 2830 | `			/* Use the default comparison function */` |
|       ! 0 | 2831 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2832 | `		}` |
|         - | 2833 | `		/* Do the merge sort */` |
|        23 | 2834 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        23 | 2835 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2836 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        23 | 2837 | `		HashmapSortRehash(pMap);` |
|        23 | 2838 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2839 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2840 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2841 | `			return PH7_EXCEPTION;` |
|         - | 2842 | `		}` |
|         6 | 2843 | `	}` |
|         - | 2844 | `	/* All done,return TRUE */` |
|        14 | 2845 | `	ph7_result_bool(pCtx,1);` |
|        14 | 2846 | `	return PH7_OK;` |
|        14 | 2847 | `}` |
|         - | 2848 | `/*` |
|         - | 2849 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2850 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2851 | ` *  and maintain index association.` |
|         - | 2852 | ` * Parameters` |
|         - | 2853 | ` *  $array` |
|         - | 2854 | ` *   The input array.` |
|         - | 2855 | ` * $cmp_function` |
|         - | 2856 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2857 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2858 | ` *  to, or greater than the second.` |
|         - | 2859 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2860 | ` * Return` |
|         - | 2861 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2862 | ` */` |
|        12 | 2863 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2864 | `{` |
|         - | 2865 | `	ph7_hashmap *pMap;` |
|         - | 2866 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 2867 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2868 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2869 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2870 | `		return PH7_OK;` |
|         - | 2871 | `	}` |
|        13 | 2872 | `	if( nArg > 1 ){` |
|         - | 2873 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2874 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2875 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|        13 | 2876 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|        13 | 2877 | `		if( rcCb != PH7_OK ){` |
|         3 | 2878 | `			return rcCb;` |
|         - | 2879 | `		}` |
|         5 | 2880 | `	}` |
|         - | 2881 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2882 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2883 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2884 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2885 | `		ph7_value *pCallback = 0;` |
|         - | 2886 | `		ProcNodeCmp xCmp;` |
|        11 | 2887 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2888 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2889 | `			/* Point to the desired callback */` |
|        11 | 2890 | `			pCallback = apArg[1];` |
|         6 | 2891 | `		}else{` |
|         - | 2892 | `			/* Use the default comparison function */` |
|       ! 0 | 2893 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2894 | `		}` |
|         - | 2895 | `		/* Do the merge sort */` |
|        11 | 2896 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2897 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2898 | `		/* Fix the last link broken by the merge */` |
|        23 | 2899 | `		while(pMap->pLast->pPrev){` |
|        13 | 2900 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2901 | `		}` |
|        11 | 2902 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2903 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2904 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2905 | `			return PH7_EXCEPTION;` |
|         - | 2906 | `		}` |
|         5 | 2907 | `	}` |
|         - | 2908 | `	/* All done,return TRUE */` |
|        11 | 2909 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2910 | `	return PH7_OK;` |
|         7 | 2911 | `}` |
|         - | 2912 | `/*` |
|         - | 2913 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2914 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2915 | ` *  function and maintain index association.` |
|         - | 2916 | ` * Parameters` |
|         - | 2917 | ` *  $array` |
|         - | 2918 | ` *   The input array.` |
|         - | 2919 | ` * $cmp_function` |
|         - | 2920 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2921 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2922 | ` *  to, or greater than the second.` |
|         - | 2923 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2924 | ` * Return` |
|         - | 2925 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2926 | ` */` |
|         4 | 2927 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2928 | `{` |
|         - | 2929 | `	ph7_hashmap *pMap;` |
|         - | 2930 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2931 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2932 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2933 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2934 | `		return PH7_OK;` |
|         - | 2935 | `	}` |
|         5 | 2936 | `	if( nArg > 1 ){` |
|         - | 2937 | `		/* php rejects an uncallable comparator with a TypeError, BEFORE touching the` |
|         - | 2938 | `		 * array. PH7 handed it to the dispatcher, which answers NULL in silence -- so` |
|         - | 2939 | `		 * usort() with a bogus comparator reordered the array anyway, by nothing. */` |
|         5 | 2940 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",0);` |
|         5 | 2941 | `		if( rcCb != PH7_OK ){` |
|         3 | 2942 | `			return rcCb;` |
|         - | 2943 | `		}` |
|         1 | 2944 | `	}` |
|         - | 2945 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2946 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2947 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2948 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2949 | `		ph7_value *pCallback = 0;` |
|         - | 2950 | `		ProcNodeCmp xCmp;` |
|         3 | 2951 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2952 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2953 | `			/* Point to the desired callback */` |
|         3 | 2954 | `			pCallback = apArg[1];` |
|         2 | 2955 | `		}else{` |
|         - | 2956 | `			/* Use the default comparison function */` |
|       ! 0 | 2957 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2958 | `		}` |
|         - | 2959 | `		/* Do the merge sort */` |
|         3 | 2960 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2961 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2962 | `		/* Fix the last link broken by the merge */` |
|         3 | 2963 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2964 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2965 | `		}` |
|         3 | 2966 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2967 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2968 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2969 | `			return PH7_EXCEPTION;` |
|         - | 2970 | `		}` |
|         1 | 2971 | `	}` |
|         - | 2972 | `	/* All done,return TRUE */` |
|         3 | 2973 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2974 | `	return PH7_OK;` |
|         3 | 2975 | `}` |
|         - | 2976 | `/*` |
|         - | 2977 | ` * bool shuffle(array &$array)` |
|         - | 2978 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2979 | ` * Parameters` |
|         - | 2980 | ` *  $array` |
|         - | 2981 | ` *   The input array.` |
|         - | 2982 | ` * Return` |
|         - | 2983 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2984 | ` *` |
|         - | 2985 | ` */` |
|         2 | 2986 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2987 | `{` |
|         - | 2988 | `	ph7_hashmap *pMap;` |
|         - | 2989 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2990 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2991 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2992 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2993 | `		return PH7_OK;` |
|         - | 2994 | `	}` |
|         - | 2995 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2996 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2997 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2998 | `	if( pMap->nEntry > 1 ){` |
|         - | 2999 | `		/* Do the merge sort */` |
|         3 | 3000 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 3001 | `		/* Fix the last link broken by the merge */` |
|         5 | 3002 | `		while(pMap->pLast->pPrev){` |
|         3 | 3003 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 3004 | `		}` |
|         1 | 3005 | `	}` |
|         - | 3006 | `	/* All done,return TRUE */` |
|         3 | 3007 | `	ph7_result_bool(pCtx,1);` |
|         3 | 3008 | `	return PH7_OK;` |
|         2 | 3009 | `}` |
|         - | 3010 | `/*` |
|         - | 3011 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 3012 | ` *   Count all elements in an array, or something in an object.` |
|         - | 3013 | ` * Parameters` |
|         - | 3014 | ` *  $var` |
|         - | 3015 | ` *   The array or the object.` |
|         - | 3016 | ` * $mode` |
|         - | 3017 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 3018 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 3019 | ` *  all the elements of a multidimensional array.` |
|         - | 3020 | ` * Return` |
|         - | 3021 | ` *  Returns the number of elements in the array.` |
|         - | 3022 | ` */` |
|      1976 | 3023 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3024 | `{` |
|      1981 | 3025 | `	int bRecursive = FALSE;` |
|      1981 | 3026 | `	int bCycleDetected = FALSE;` |
|         - | 3027 | `	sxi64 iCount;` |
|      1981 | 3028 | `	if( nArg < 1 ){` |
|       ! 0 | 3029 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3030 | `			"ArgumentCountError",` |
|         - | 3031 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 3032 | `			);` |
|         - | 3033 | `	}` |
|      1981 | 3034 | `	if( nArg > 2 ){` |
|         4 | 3035 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3036 | `			"ArgumentCountError",` |
|         - | 3037 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3038 | `			nArg` |
|         - | 3039 | `			);` |
|         - | 3040 | `	}` |
|         - | 3041 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3042 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3043 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1979 | 3044 | `	if( nArg > 1 ){` |
|        44 | 3045 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 3046 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 3047 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3048 | `				"ValueError",` |
|         - | 3049 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3050 | `				);` |
|         - | 3051 | `		}` |
|        34 | 3052 | `		bRecursive = iMode == 1;` |
|        16 | 3053 | `	}` |
|      1971 | 3054 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3055 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 3056 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 3057 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 3058 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 3059 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 3060 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3061 | `					"count",sizeof("count")-1);` |
|        59 | 3062 | `				if( pMeth ){` |
|         - | 3063 | `					ph7_value sResult;` |
|        59 | 3064 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 3065 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3066 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3067 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3068 | `					return PH7_OK;` |
|         - | 3069 | `				}` |
|       ! 0 | 3070 | `			}` |
|         1 | 3071 | `		}` |
|        22 | 3072 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3073 | `			"TypeError",` |
|         - | 3074 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3075 | `			ph7_type_name(apArg[0])` |
|         - | 3076 | `			);` |
|         - | 3077 | `	}` |
|         - | 3078 | `	/* Count */` |
|      1903 | 3079 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1903 | 3080 | `	if( bCycleDetected ){` |
|         3 | 3081 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3082 | `	}` |
|      1903 | 3083 | `	ph7_result_int64(pCtx,iCount);` |
|      1903 | 3084 | `	return PH7_OK;` |
|       993 | 3085 | `}` |
|         - | 3086 | `/*` |
|         - | 3087 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3088 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3089 | ` * Parameters` |
|         - | 3090 | ` * $key` |
|         - | 3091 | ` *   Value to check.` |
|         - | 3092 | ` * $search` |
|         - | 3093 | ` *  An array with keys to check.` |
|         - | 3094 | ` * Return` |
|         - | 3095 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3096 | ` */` |
|        94 | 3097 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3098 | `{` |
|         - | 3099 | `	sxi32 rc;` |
|        99 | 3100 | `	if( nArg != 2 ){` |
|         - | 3101 | `		/* PHP requires exactly two arguments */` |
|         4 | 3102 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3103 | `			"ArgumentCountError",` |
|         - | 3104 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3105 | `			nArg` |
|         - | 3106 | `			);` |
|         - | 3107 | `	}` |
|         - | 3108 | `	/* Make sure we are dealing with a valid hashmap */` |
|        97 | 3109 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3110 | `		/* Type mismatch -> TypeError */` |
|         8 | 3111 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3112 | `			"TypeError",` |
|         - | 3113 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3114 | `			ph7_type_name(apArg[1])` |
|         - | 3115 | `			);` |
|         - | 3116 | `	}` |
|         - | 3117 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        92 | 3118 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3119 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3120 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3121 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3122 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3123 | `			"use an empty string instead"` |
|         - | 3124 | `			);` |
|        91 | 3125 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3126 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3127 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3128 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3129 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3130 | `				,rVal` |
|         - | 3131 | `				);` |
|         1 | 3132 | `		}` |
|         1 | 3133 | `	}` |
|         - | 3134 | `	/* Perform the lookup */` |
|        92 | 3135 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3136 | `	/* lookup result */` |
|        92 | 3137 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        92 | 3138 | `	return PH7_OK;` |
|        52 | 3139 | `}` |
|         - | 3140 | `/*` |
|         - | 3141 | ` * value array_pop(array $array)` |
|         - | 3142 | ` *   POP the last inserted element from the array.` |
|         - | 3143 | ` * Parameter` |
|         - | 3144 | ` *  The array to get the value from.` |
|         - | 3145 | ` * Return` |
|         - | 3146 | ` *  Poped value or NULL on failure.` |
|         - | 3147 | ` */` |
|       102 | 3148 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3149 | `{` |
|         - | 3150 | `	ph7_hashmap *pMap;` |
|         - | 3151 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       106 | 3152 | `	if( nArg != 1 ){` |
|         4 | 3153 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3154 | `			"ArgumentCountError",` |
|         - | 3155 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3156 | `			nArg` |
|         - | 3157 | `			);` |
|         - | 3158 | `	}` |
|         - | 3159 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3160 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3161 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3162 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3163 | `			"Error",` |
|         - | 3164 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3165 | `			);` |
|         - | 3166 | `	}` |
|         - | 3167 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3168 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3169 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3170 | `			"TypeError",` |
|         - | 3171 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3172 | `			ph7_type_name(apArg[0])` |
|         - | 3173 | `			);` |
|         - | 3174 | `	}` |
|        95 | 3175 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3176 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3177 | `	if( pMap->nEntry < 1 ){` |
|         - | 3178 | `		/* Nothing to pop,return NULL */` |
|         3 | 3179 | `		ph7_result_null(pCtx);` |
|         2 | 3180 | `	}else{` |
|        93 | 3181 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3182 | `		ph7_value *pObj;` |
|        93 | 3183 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3184 | `		if( pObj ){` |
|         - | 3185 | `			/* Node value */` |
|        93 | 3186 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3187 | `			/* Unlink the node */` |
|        93 | 3188 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3189 | `		}else{` |
|       ! 0 | 3190 | `			ph7_result_null(pCtx);` |
|         - | 3191 | `		}` |
|         - | 3192 | `		/* Reset the cursor */` |
|        93 | 3193 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3194 | `	}` |
|        95 | 3195 | `	return PH7_OK;` |
|        55 | 3196 | `}` |
|         - | 3197 | `/*` |
|         - | 3198 | ` * int array_push($array,$var,...)` |
|         - | 3199 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3200 | ` * Parameters` |
|         - | 3201 | ` *  array` |
|         - | 3202 | ` *    The input array.` |
|         - | 3203 | ` *  var` |
|         - | 3204 | ` *   On or more value to push.` |
|         - | 3205 | ` * Return` |
|         - | 3206 | ` *  New array count (including old items).` |
|         - | 3207 | ` */` |
|        22 | 3208 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3209 | `{` |
|         - | 3210 | `	ph7_hashmap *pMap;` |
|         - | 3211 | `	sxi32 rc;` |
|         - | 3212 | `	int i;` |
|        26 | 3213 | `	if( nArg < 1 ){` |
|       ! 0 | 3214 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3215 | `			"ArgumentCountError",` |
|         - | 3216 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3217 | `			nArg` |
|         - | 3218 | `			);` |
|         - | 3219 | `	}` |
|         - | 3220 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3221 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3222 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3224 | `			"Error",` |
|         - | 3225 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3226 | `			);` |
|         - | 3227 | `	}` |
|         - | 3228 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3229 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3230 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3231 | `			"TypeError",` |
|         - | 3232 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3233 | `			ph7_type_name(apArg[0])` |
|         - | 3234 | `			);` |
|         - | 3235 | `	}` |
|         - | 3236 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3237 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3238 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3239 | `	/* Start pushing given values */` |
|        34 | 3240 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3241 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3242 | `		if( rc != SXRET_OK ){` |
|         3 | 3243 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3244 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3245 | `				return rc;` |
|         - | 3246 | `			}` |
|       ! 0 | 3247 | `			break;` |
|         - | 3248 | `		}` |
|         9 | 3249 | `	}` |
|         - | 3250 | `	/* Return the new count */` |
|        15 | 3251 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3252 | `	return PH7_OK;` |
|        15 | 3253 | `}` |
|         - | 3254 | `/*` |
|         - | 3255 | ` * value array_shift(array $array)` |
|         - | 3256 | ` *   Shift an element off the beginning of array.` |
|         - | 3257 | ` * Parameter` |
|         - | 3258 | ` *  The array to get the value from.` |
|         - | 3259 | ` * Return` |
|         - | 3260 | ` *  Shifted value or NULL on failure.` |
|         - | 3261 | ` */` |
|        44 | 3262 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3263 | `{` |
|         - | 3264 | `	ph7_hashmap *pMap;` |
|         - | 3265 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3266 | `	if( nArg != 1 ){` |
|         4 | 3267 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3268 | `			"ArgumentCountError",` |
|         - | 3269 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3270 | `			nArg` |
|         - | 3271 | `			);` |
|         - | 3272 | `	}` |
|         - | 3273 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3274 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3275 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3276 | `			"Error",` |
|         - | 3277 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3278 | `			);` |
|         - | 3279 | `	}` |
|         - | 3280 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3281 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3282 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3283 | `			"TypeError",` |
|         - | 3284 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3285 | `			ph7_type_name(apArg[0])` |
|         - | 3286 | `			);` |
|         - | 3287 | `	}` |
|         - | 3288 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3289 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3290 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3291 | `	if( pMap->nEntry < 1 ){` |
|         - | 3292 | `		/* Empty hashmap,return NULL */` |
|         3 | 3293 | `		ph7_result_null(pCtx);` |
|         2 | 3294 | `	}else{` |
|        39 | 3295 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3296 | `		ph7_value *pObj;` |
|         - | 3297 | `		sxu32 n;` |
|        39 | 3298 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3299 | `		if( pObj ){` |
|         - | 3300 | `			/* Node value */` |
|        39 | 3301 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3302 | `			/* Unlink the first node */` |
|        39 | 3303 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3304 | `		}else{` |
|       ! 0 | 3305 | `			ph7_result_null(pCtx);` |
|         - | 3306 | `		}` |
|         - | 3307 | `		/* Rehash all int keys */` |
|        39 | 3308 | `		n = pMap->nEntry;` |
|        39 | 3309 | `		pEntry = pMap->pFirst;` |
|        39 | 3310 | `		pMap->iNextIdx = 0;` |
|        39 | 3311 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3312 | `		for(;;){` |
|        99 | 3313 | `			if( n < 1 ){` |
|        39 | 3314 | `				break;` |
|         - | 3315 | `			}` |
|        65 | 3316 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3317 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3318 | `			}` |
|         - | 3319 | `			/* Point to the next entry */` |
|        65 | 3320 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3321 | `			n--;` |
|         5 | 3322 | `		}` |
|         - | 3323 | `		/* Reset the cursor */` |
|        39 | 3324 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3325 | `	}` |
|        41 | 3326 | `	return PH7_OK;` |
|        27 | 3327 | `}` |
|         - | 3328 | `/*` |
|         - | 3329 | ` * Extract the node cursor value.` |
|         - | 3330 | ` */` |
|      1094 | 3331 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3332 | `{` |
|      1095 | 3333 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3334 | `	ph7_value *pVal;` |
|      1095 | 3335 | `	if( pCur == 0 ){` |
|         - | 3336 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3337 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3338 | `		return PH7_OK;` |
|         - | 3339 | `	}` |
|      1057 | 3340 | `	if( iDirection != 0 ){` |
|       201 | 3341 | `		if( iDirection > 0 ){` |
|         - | 3342 | `			/* Point to the next entry */` |
|       199 | 3343 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3344 | `			pCur = pMap->pCur;` |
|       100 | 3345 | `		}else{` |
|         - | 3346 | `			/* Point to the previous entry */` |
|         3 | 3347 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3348 | `			pCur = pMap->pCur;` |
|         - | 3349 | `		}` |
|       201 | 3350 | `		if( pCur == 0 ){` |
|         - | 3351 | `			/* End of input reached,return FALSE */` |
|        83 | 3352 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3353 | `			return PH7_OK;` |
|         - | 3354 | `		}` |
|        59 | 3355 | `	}` |
|         - | 3356 | `	/* Point to the desired element */` |
|       975 | 3357 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3358 | `	if( pVal ){` |
|       975 | 3359 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3360 | `	}else{` |
|       ! 0 | 3361 | `		ph7_result_bool(pCtx,0);` |
|         - | 3362 | `	}` |
|       975 | 3363 | `	return PH7_OK;` |
|       548 | 3364 | `}` |
|         - | 3365 | `/*` |
|         - | 3366 | ` * value current(array $array)` |
|         - | 3367 | ` *  Return the current element in an array.` |
|         - | 3368 | ` * Parameter` |
|         - | 3369 | ` *  $input: The input array.` |
|         - | 3370 | ` * Return` |
|         - | 3371 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3372 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3373 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3374 | ` *  is empty, current() returns FALSE.` |
|         - | 3375 | ` */` |
|       302 | 3376 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3377 | `{` |
|       303 | 3378 | `	if( nArg < 1 ){` |
|         - | 3379 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3380 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3381 | `		return PH7_OK;` |
|         - | 3382 | `	}` |
|         - | 3383 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3384 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3385 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3386 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3387 | `		return PH7_OK;` |
|         - | 3388 | `	}` |
|       303 | 3389 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3390 | `	return PH7_OK;` |
|       152 | 3391 | `}` |
|         - | 3392 | `/*` |
|         - | 3393 | ` * value next(array $input)` |
|         - | 3394 | ` *  Advance the internal array pointer of an array.` |
|         - | 3395 | ` * Parameter` |
|         - | 3396 | ` *  $input: The input array.` |
|         - | 3397 | ` * Return` |
|         - | 3398 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3399 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3400 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3401 | ` */` |
|       198 | 3402 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3403 | `{` |
|       199 | 3404 | `	if( nArg < 1 ){` |
|         - | 3405 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3406 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3407 | `		return PH7_OK;` |
|         - | 3408 | `	}` |
|         - | 3409 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3410 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3411 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3412 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3413 | `		return PH7_OK;` |
|         - | 3414 | `	}` |
|       199 | 3415 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3416 | `	return PH7_OK;` |
|       100 | 3417 | `}` |
|         - | 3418 | `/*` |
|         - | 3419 | ` * value prev(array $input)` |
|         - | 3420 | ` *  Rewind the internal array pointer.` |
|         - | 3421 | ` * Parameter` |
|         - | 3422 | ` *  $input: The input array.` |
|         - | 3423 | ` * Return` |
|         - | 3424 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3425 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3426 | ` *  elements.` |
|         - | 3427 | ` */` |
|         2 | 3428 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3429 | `{` |
|         3 | 3430 | `	if( nArg < 1 ){` |
|         - | 3431 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3432 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3433 | `		return PH7_OK;` |
|         - | 3434 | `	}` |
|         - | 3435 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3436 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3437 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3438 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3439 | `		return PH7_OK;` |
|         - | 3440 | `	}` |
|         3 | 3441 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3442 | `	return PH7_OK;` |
|         2 | 3443 | `}` |
|         - | 3444 | `/*` |
|         - | 3445 | ` * value end(array $input)` |
|         - | 3446 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3447 | ` * Parameter` |
|         - | 3448 | ` *  $input: The input array.` |
|         - | 3449 | ` * Return` |
|         - | 3450 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3451 | ` */` |
|       348 | 3452 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3453 | `{` |
|         - | 3454 | `	ph7_hashmap *pMap;` |
|       349 | 3455 | `	if( nArg < 1 ){` |
|         - | 3456 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3457 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3458 | `		return PH7_OK;` |
|         - | 3459 | `	}` |
|         - | 3460 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3461 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3462 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3463 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3464 | `		return PH7_OK;` |
|         - | 3465 | `	}` |
|         - | 3466 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3467 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3468 | `	/* Point to the last node */` |
|       349 | 3469 | `	pMap->pCur = pMap->pLast;` |
|         - | 3470 | `	/* Return the last node value */` |
|       349 | 3471 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3472 | `	return PH7_OK;` |
|       175 | 3473 | `}` |
|         - | 3474 | `/*` |
|         - | 3475 | ` * value reset(array $array )` |
|         - | 3476 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3477 | ` * Parameter` |
|         - | 3478 | ` *  $input: The input array.` |
|         - | 3479 | ` * Return` |
|         - | 3480 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3481 | ` */` |
|       244 | 3482 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3483 | `{` |
|         - | 3484 | `	ph7_hashmap *pMap;` |
|       245 | 3485 | `	if( nArg < 1 ){` |
|         - | 3486 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3487 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3488 | `		return PH7_OK;` |
|         - | 3489 | `	}` |
|         - | 3490 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3491 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3492 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3493 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3494 | `		return PH7_OK;` |
|         - | 3495 | `	}` |
|         - | 3496 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3497 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3498 | `	/* Point to the first node */` |
|       245 | 3499 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3500 | `	/* Return the last node value if available */` |
|       245 | 3501 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3502 | `	return PH7_OK;` |
|       123 | 3503 | `}` |
|         - | 3504 | `/*` |
|         - | 3505 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3506 | ` * array_key_first() and array_key_last().` |
|         - | 3507 | ` */` |
|       672 | 3508 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3509 | `{` |
|       673 | 3510 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3511 | `		/* Key is integer */` |
|       283 | 3512 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3513 | `	}else{` |
|         - | 3514 | `		/* Key is blob */` |
|       586 | 3515 | `		ph7_result_string(pCtx,` |
|       390 | 3516 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3517 | `	}` |
|       673 | 3518 | `}` |
|         - | 3519 | `/*` |
|         - | 3520 | ` * value key(array $array)` |
|         - | 3521 | ` *   Fetch a key from an array` |
|         - | 3522 | ` * Parameter` |
|         - | 3523 | ` *  $input` |
|         - | 3524 | ` *   The input array.` |
|         - | 3525 | ` * Return` |
|         - | 3526 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3527 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3528 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3529 | ` *  is empty, key() returns NULL.` |
|         - | 3530 | ` */` |
|       776 | 3531 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3532 | `{` |
|         - | 3533 | `	ph7_hashmap_node *pCur;` |
|         - | 3534 | `	ph7_hashmap *pMap;` |
|       777 | 3535 | `	if( nArg < 1 ){` |
|         - | 3536 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3537 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3538 | `		return PH7_OK;` |
|         - | 3539 | `	}` |
|         - | 3540 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3541 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3542 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3543 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3544 | `		return PH7_OK;` |
|         - | 3545 | `	}` |
|       777 | 3546 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3547 | `	pCur = pMap->pCur;` |
|       777 | 3548 | `	if( pCur == 0 ){` |
|         - | 3549 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3550 | `		ph7_result_null(pCtx);` |
|       121 | 3551 | `		return PH7_OK;` |
|         - | 3552 | `	}` |
|       657 | 3553 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3554 | `	return PH7_OK;` |
|       389 | 3555 | `}` |
|         - | 3556 | `/*` |
|         - | 3557 | ` * array each(array $input)` |
|         - | 3558 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3559 | ` * Parameter` |
|         - | 3560 | ` *  $input` |
|         - | 3561 | ` *    The input array.` |
|         - | 3562 | ` * Return` |
|         - | 3563 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3564 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3565 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3566 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3567 | ` *  each() returns FALSE.` |
|         - | 3568 | ` */` |
|        22 | 3569 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3570 | `{` |
|         - | 3571 | `	ph7_hashmap_node *pCur;` |
|         - | 3572 | `	ph7_hashmap *pMap;` |
|         - | 3573 | `	ph7_value *pArray;` |
|         - | 3574 | `	ph7_value *pVal;` |
|         - | 3575 | `	ph7_value sKey;` |
|        23 | 3576 | `	if( nArg < 1 ){` |
|         - | 3577 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3578 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3579 | `		return PH7_OK;` |
|         - | 3580 | `	}` |
|         - | 3581 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3582 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3583 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3584 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3585 | `		return PH7_OK;` |
|         - | 3586 | `	}` |
|         - | 3587 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3588 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3589 | `	if( pMap->pCur == 0 ){` |
|         - | 3590 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3591 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3592 | `		return PH7_OK;` |
|         - | 3593 | `	}` |
|        15 | 3594 | `	pCur = pMap->pCur;` |
|         - | 3595 | `	/* Create a new array */` |
|        15 | 3596 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3597 | `	if( pArray == 0 ){` |
|       ! 0 | 3598 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3599 | `		return PH7_OK;` |
|         - | 3600 | `	}` |
|        15 | 3601 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3602 | `	/* Insert the current value */` |
|        15 | 3603 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3604 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3605 | `	/* Make the key */` |
|        15 | 3606 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3607 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3608 | `	}else{` |
|         9 | 3609 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3610 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3611 | `	}` |
|         - | 3612 | `	/* Insert the current key */` |
|        15 | 3613 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3614 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3615 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3616 | `	/* Advance the cursor */` |
|        15 | 3617 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3618 | `	/* Return the current entry */` |
|        15 | 3619 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3620 | `	return PH7_OK;` |
|        12 | 3621 | `}` |
|         - | 3622 | `/*` |
|         - | 3623 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3624 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3625 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3626 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3627 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3628 | ` */` |
|         - | 3629 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3630 | `/*` |
|         - | 3631 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3632 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3633 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3634 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3635 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3636 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3637 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3638 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3639 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3640 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3641 | ` */` |
|         - | 3642 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3643 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3644 | `/*` |
|         - | 3645 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3646 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3647 | ` */` |
|       ! 0 | 3648 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3649 | `{` |
|       ! 0 | 3650 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3651 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3652 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3653 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3654 | `		zBuf[n] = 0;` |
|       ! 0 | 3655 | `		return zBuf;` |
|         - | 3656 | `	}` |
|       ! 0 | 3657 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3658 | `}` |
|         - | 3659 | `/*` |
|         - | 3660 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3661 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3662 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3663 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3664 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3665 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3666 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3667 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3668 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3669 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3670 | ` */` |
|       156 | 3671 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3672 | `{` |
|       157 | 3673 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3674 | `	sxu64 uVal = 0;` |
|       157 | 3675 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3676 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3677 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3678 | `		bNeg = (z[0] == '-');` |
|         3 | 3679 | `		z++;` |
|         1 | 3680 | `	}` |
|       237 | 3681 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3682 | `		int d = z[0] - '0';` |
|         - | 3683 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3684 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3685 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3686 | `			bOverflow = 1;` |
|       ! 0 | 3687 | `		}else{` |
|        81 | 3688 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3689 | `		}` |
|        81 | 3690 | `		bDigit = 1;` |
|        81 | 3691 | `		z++;` |
|         1 | 3692 | `	}` |
|       157 | 3693 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3694 | `		bReal = 1;` |
|         3 | 3695 | `		z++;` |
|         5 | 3696 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3697 | `			bDigit = 1;` |
|         3 | 3698 | `			z++;` |
|         1 | 3699 | `		}` |
|         1 | 3700 | `	}` |
|         - | 3701 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3702 | `	if( !bDigit ){` |
|        61 | 3703 | `		return RANGE_IN_ERROR;` |
|         - | 3704 | `	}` |
|         - | 3705 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3706 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3707 | `		z++;` |
|         9 | 3708 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3709 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3710 | `			return RANGE_IN_ERROR;` |
|         - | 3711 | `		}` |
|         9 | 3712 | `		bReal = 1;` |
|        17 | 3713 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3714 | `	}` |
|         - | 3715 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3716 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3717 | `	if( z != zEnd ){` |
|        13 | 3718 | `		return RANGE_IN_ERROR;` |
|         - | 3719 | `	}` |
|        84 | 3720 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3721 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3722 | `		bReal = 1;` |
|        84 | 3723 | `	}` |
|        43 | 3724 | `	if( bReal ){` |
|        11 | 3725 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3726 | `		return RANGE_IN_DOUBLE;` |
|         - | 3727 | `	}` |
|         - | 3728 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3729 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3730 | `	return RANGE_IN_LONG;` |
|        58 | 3731 | `}` |
|         - | 3732 | `/*` |
|         - | 3733 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3734 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3735 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3736 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3737 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3738 | ` */` |
|       328 | 3739 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3740 | `{` |
|         - | 3741 | `	char zMsg[160];` |
|       329 | 3742 | `	*pRc = PH7_OK;` |
|       329 | 3743 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3744 | `		char zType[80];` |
|       ! 0 | 3745 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3746 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3747 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3748 | `		return FALSE;` |
|         - | 3749 | `	}` |
|       329 | 3750 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3751 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3752 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3753 | `			iArg,zName);` |
|         5 | 3754 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3755 | `		*pbNullCoerced = TRUE;` |
|         2 | 3756 | `	}` |
|       329 | 3757 | `	return TRUE;` |
|       165 | 3758 | `}` |
|         - | 3759 | `/*` |
|         - | 3760 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3761 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3762 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3763 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3764 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3765 | ` */` |
|        60 | 3766 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3767 | `{` |
|        61 | 3768 | `	*pRc = PH7_OK;` |
|        61 | 3769 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3770 | `		char zType[80];` |
|       ! 0 | 3771 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3772 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3773 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3774 | `		return RANGE_IN_ERROR;` |
|         - | 3775 | `	}` |
|        61 | 3776 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3777 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3778 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3779 | `		*pLong = 0;` |
|         3 | 3780 | `		return RANGE_IN_LONG;` |
|         - | 3781 | `	}` |
|        59 | 3782 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3783 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3784 | `		return RANGE_IN_DOUBLE;` |
|         - | 3785 | `	}` |
|        35 | 3786 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3787 | `		const char *zStr;` |
|         - | 3788 | `		int nLen;` |
|         - | 3789 | `		sxu8 iKind;` |
|         3 | 3790 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3791 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3792 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3793 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3794 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3795 | `		}` |
|         3 | 3796 | `		return iKind;` |
|         - | 3797 | `	}` |
|         - | 3798 | `	/* int / bool */` |
|        33 | 3799 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3800 | `	return RANGE_IN_LONG;` |
|        31 | 3801 | `}` |
|         - | 3802 | `/*` |
|         - | 3803 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3804 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3805 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3806 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3807 | ` */` |
|       296 | 3808 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3809 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3810 | `{` |
|         - | 3811 | `	char zMsg[160];` |
|         - | 3812 | `	double r;` |
|       297 | 3813 | `	*pRc = PH7_OK;` |
|       297 | 3814 | `	if( bNullCoerced ){` |
|         - | 3815 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3816 | `		*pLong = 0;` |
|         5 | 3817 | `		*pDouble = 0.0;` |
|         5 | 3818 | `		return RANGE_IN_LONG;` |
|         - | 3819 | `	}` |
|       293 | 3820 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3821 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3822 | `check_dval:` |
|        25 | 3823 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3824 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3825 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3826 | `			return RANGE_IN_ERROR;` |
|         - | 3827 | `		}` |
|        21 | 3828 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3829 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3830 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3831 | `			return RANGE_IN_ERROR;` |
|         - | 3832 | `		}` |
|        17 | 3833 | `		*pDouble = r;` |
|        17 | 3834 | `		return RANGE_IN_DOUBLE;` |
|         - | 3835 | `	}` |
|       273 | 3836 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3837 | `		const char *zStr;` |
|         - | 3838 | `		int nLen;` |
|         - | 3839 | `		sxu8 iKind;` |
|        81 | 3840 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3841 | `		if( nLen == 0 ){` |
|         7 | 3842 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3843 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3844 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3845 | `			*pLong = 0;` |
|         5 | 3846 | `			*pDouble = 0.0;` |
|        41 | 3847 | `			return RANGE_IN_LONG;` |
|         - | 3848 | `		}` |
|        77 | 3849 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3850 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3851 | `			r = *pDouble;` |
|         5 | 3852 | `			goto check_dval;` |
|         - | 3853 | `		}` |
|        73 | 3854 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3855 | `			*pDouble = (double)*pLong;` |
|        23 | 3856 | `			if( nLen == 1 ){` |
|         - | 3857 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3858 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3859 | `				return RANGE_IN_DIGIT;` |
|         - | 3860 | `			}` |
|        15 | 3861 | `			return RANGE_IN_LONG;` |
|         - | 3862 | `		}` |
|        51 | 3863 | `		if( nLen != 1 ){` |
|        10 | 3864 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3865 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3866 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3867 | `		}` |
|        51 | 3868 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3869 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3870 | `		*pLong = 0;` |
|        51 | 3871 | `		*pDouble = 0.0;` |
|        51 | 3872 | `		return RANGE_IN_STRING;` |
|         - | 3873 | `	}` |
|         - | 3874 | `	/* int / bool */` |
|       193 | 3875 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3876 | `	*pDouble = (double)*pLong;` |
|       193 | 3877 | `	return RANGE_IN_LONG;` |
|       149 | 3878 | `}` |
|         - | 3879 | `/*` |
|         - | 3880 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3881 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3882 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3883 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3884 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3885 | ` * exactly like php's two macros.` |
|         - | 3886 | ` */` |
|         6 | 3887 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3888 | `{` |
|        10 | 3889 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3890 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3891 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3892 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3893 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3894 | `}` |
|         6 | 3895 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3896 | `{` |
|         - | 3897 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3898 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3899 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3900 | `	const unsigned int nBuf = 1500;` |
|         7 | 3901 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3902 | `	if( zMsg == 0 ){` |
|       ! 0 | 3903 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3904 | `	}` |
|         7 | 3905 | `	snprintf(zMsg,nBuf,` |
|         - | 3906 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3907 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3908 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3909 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3910 | `}` |
|         - | 3911 | `/*` |
|         - | 3912 | ` * Set the element container to the next range element and append it to the` |
|         - | 3913 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3914 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3915 | ` * below stay one line per iteration.` |
|         - | 3916 | ` */` |
|      1680 | 3917 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3918 | `{` |
|      1681 | 3919 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3920 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3921 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3922 | `	}` |
|      1681 | 3923 | `	return PH7_OK;` |
|       841 | 3924 | `}` |
|        70 | 3925 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3926 | `{` |
|        71 | 3927 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3928 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3929 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3930 | `	}` |
|        71 | 3931 | `	return PH7_OK;` |
|        36 | 3932 | `}` |
|       168 | 3933 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3934 | `{` |
|       169 | 3935 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3936 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3937 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3938 | `	}` |
|       169 | 3939 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3940 | `	return PH7_OK;` |
|        85 | 3941 | `}` |
|         - | 3942 | `/*` |
|         - | 3943 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3944 | ` *  Create an array containing a range of elements.` |
|         - | 3945 | ` * Return` |
|         - | 3946 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3947 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3948 | ` */` |
|       166 | 3949 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3950 | `{` |
|         - | 3951 | `	ph7_value *pValue,*pArray;` |
|       167 | 3952 | `	sxi32 rc = PH7_OK;` |
|       167 | 3953 | `	int is_step_double = 0,is_step_negative = 0;` |
|       167 | 3954 | `	double step_double = 1.0;` |
|       167 | 3955 | `	sxi64 step = 1;` |
|         - | 3956 | `	sxu8 start_type,end_type;` |
|       167 | 3957 | `	sxi64 start_long = 0,end_long = 0;` |
|       167 | 3958 | `	double start_double = 0.0,end_double = 0.0;` |
|       167 | 3959 | `	unsigned char cStart = 0,cEnd = 0;` |
|       167 | 3960 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3961 | `	sxu32 i,size;` |
|         - | 3962 |  |
|         - | 3963 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       167 | 3964 | `	if( nArg > 3 ){` |
|         4 | 3965 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3966 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3967 | `	}` |
|       165 | 3968 | `	if( nArg < 2 ){` |
|         - | 3969 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3970 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3971 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3972 | `	}` |
|         - | 3973 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3974 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       165 | 3975 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 3976 | `		return rc;` |
|         - | 3977 | `	}` |
|       165 | 3978 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3979 | `		return rc;` |
|         - | 3980 | `	}` |
|       165 | 3981 | `	if( nArg > 2 ){` |
|        61 | 3982 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 3983 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 3984 | `			return rc;` |
|         - | 3985 | `		}` |
|        59 | 3986 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3987 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3988 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3989 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3990 | `			}` |
|        23 | 3991 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3992 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3993 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3994 | `			}` |
|         - | 3995 | `			/* We only want positive step values. */` |
|        21 | 3996 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3997 | `				is_step_negative = 1;` |
|       ! 0 | 3998 | `				step_double *= -1;` |
|       ! 0 | 3999 | `			}` |
|         - | 4000 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 4001 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 4002 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 4003 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 4004 | `				step = (sxi64)step_double;` |
|        19 | 4005 | `				if( (double)step != step_double ){` |
|        17 | 4006 | `					is_step_double = 1;` |
|         8 | 4007 | `				}` |
|        10 | 4008 | `			}else{` |
|         - | 4009 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 4010 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 4011 | `				is_step_double = 1;` |
|         - | 4012 | `			}` |
|        11 | 4013 | `		}else{` |
|         - | 4014 | `			/* We only want positive step values. */` |
|        35 | 4015 | `			if( step < 0 ){` |
|        11 | 4016 | `				if( step == SMALLEST_INT64 ){` |
|         - | 4017 | `					/* -step would overflow */` |
|         4 | 4018 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 4019 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 4020 | `				}` |
|         9 | 4021 | `				is_step_negative = 1;` |
|         9 | 4022 | `				step = -step;` |
|         4 | 4023 | `			}` |
|        33 | 4024 | `			step_double = (double)step;` |
|         - | 4025 | `		}` |
|        53 | 4026 | `		if( step_double == 0.0 ){` |
|         7 | 4027 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4028 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 4029 | `		}` |
|        23 | 4030 | `	}` |
|       151 | 4031 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 4032 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 4033 | `		return rc;` |
|         - | 4034 | `	}` |
|       147 | 4035 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 4036 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 4037 | `		return rc;` |
|         - | 4038 | `	}` |
|         - | 4039 | `	/* Element container + result array */` |
|       143 | 4040 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 4041 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 4042 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4043 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4044 | `	}` |
|         - | 4045 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 4046 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4047 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4048 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4049 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4050 | `			 * and the range is numeric. */` |
|        15 | 4051 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4052 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4053 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4054 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4055 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4056 | `				}` |
|         7 | 4057 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4058 | `			}else{` |
|         9 | 4059 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4060 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4061 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4062 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4063 | `				}` |
|         9 | 4064 | `				start_type = RANGE_IN_LONG;` |
|         - | 4065 | `			}` |
|        15 | 4066 | `			goto handle_numeric_inputs;` |
|         - | 4067 | `		}` |
|        23 | 4068 | `		if( is_step_double ){` |
|         - | 4069 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4070 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4071 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4072 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4073 | `					" of characters, inputs converted to 0");` |
|         1 | 4074 | `			}` |
|         5 | 4075 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4076 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4077 | `			goto handle_numeric_inputs;` |
|         - | 4078 | `		}` |
|         - | 4079 | `		/* Generate an array of characters */` |
|        19 | 4080 | `		if( cStart > cEnd ){` |
|         - | 4081 | `			/* Decreasing char range */` |
|         - | 4082 | `			int iCur;` |
|         3 | 4083 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4084 | `				goto boundary_error;` |
|         - | 4085 | `			}` |
|        17 | 4086 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4087 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4088 | `					return rc;` |
|         - | 4089 | `				}` |
|         8 | 4090 | `			}` |
|        18 | 4091 | `		}else if( cEnd > cStart ){` |
|         - | 4092 | `			/* Increasing char range */` |
|         - | 4093 | `			int iCur;` |
|        15 | 4094 | `			if( is_step_negative ){` |
|         3 | 4095 | `				goto negative_step_error;` |
|         - | 4096 | `			}` |
|        13 | 4097 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4098 | `				goto boundary_error;` |
|         - | 4099 | `			}` |
|       163 | 4100 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4101 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4102 | `					return rc;` |
|         - | 4103 | `				}` |
|        77 | 4104 | `			}` |
|         6 | 4105 | `		}else{` |
|         3 | 4106 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4107 | `				return rc;` |
|         - | 4108 | `			}` |
|         - | 4109 | `		}` |
|        15 | 4110 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4111 | `		return PH7_OK;` |
|         - | 4112 | `	}` |
|        53 | 4113 | `handle_numeric_inputs:` |
|       133 | 4114 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4115 | `		/* Float range */` |
|         - | 4116 | `		double elem,calc;` |
|        25 | 4117 | `		if( start_double > end_double ){` |
|         - | 4118 | `			/* Decreasing float range */` |
|         7 | 4119 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4120 | `				goto boundary_error;` |
|         - | 4121 | `			}` |
|         7 | 4122 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4123 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4124 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4125 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4126 | `			}` |
|         5 | 4127 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4128 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4129 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4130 | `					return rc;` |
|         - | 4131 | `				}` |
|         8 | 4132 | `			}` |
|        21 | 4133 | `		}else if( end_double > start_double ){` |
|         - | 4134 | `			/* Increasing float range */` |
|        17 | 4135 | `			if( is_step_negative ){` |
|       ! 0 | 4136 | `				goto negative_step_error;` |
|         - | 4137 | `			}` |
|        17 | 4138 | `			if( end_double - start_double < step_double ){` |
|         3 | 4139 | `				goto boundary_error;` |
|         - | 4140 | `			}` |
|        15 | 4141 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4142 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4143 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4144 | `			}` |
|        11 | 4145 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4146 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4147 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4148 | `					return rc;` |
|         - | 4149 | `				}` |
|        28 | 4150 | `			}` |
|         6 | 4151 | `		}else{` |
|         3 | 4152 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4153 | `				return rc;` |
|         - | 4154 | `			}` |
|         - | 4155 | `		}` |
|         9 | 4156 | `	}else{` |
|         - | 4157 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4158 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4159 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4160 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4161 | `		sxu64 calc;` |
|       101 | 4162 | `		if( start_long > end_long ){` |
|         - | 4163 | `			/* Decreasing int range */` |
|        19 | 4164 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4165 | `				goto boundary_error;` |
|         - | 4166 | `			}` |
|        17 | 4167 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4168 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4169 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4170 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4171 | `			}` |
|        15 | 4172 | `			size = (sxu32)(calc + 1);` |
|       101 | 4173 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4174 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4175 | `					return rc;` |
|         - | 4176 | `				}` |
|        44 | 4177 | `			}` |
|        90 | 4178 | `		}else if( end_long > start_long ){` |
|         - | 4179 | `			/* Increasing int range */` |
|        77 | 4180 | `			if( is_step_negative ){` |
|         3 | 4181 | `				goto negative_step_error;` |
|         - | 4182 | `			}` |
|        75 | 4183 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4184 | `				goto boundary_error;` |
|         - | 4185 | `			}` |
|        73 | 4186 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4187 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4188 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4189 | `			}` |
|        69 | 4190 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4191 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4192 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4193 | `					return rc;` |
|         - | 4194 | `				}` |
|       795 | 4195 | `			}` |
|        35 | 4196 | `		}else{` |
|         7 | 4197 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4198 | `				return rc;` |
|         - | 4199 | `			}` |
|         - | 4200 | `		}` |
|         - | 4201 | `	}` |
|         - | 4202 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4203 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4204 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4205 | `	return PH7_OK;` |
|         2 | 4206 | `negative_step_error:` |
|         5 | 4207 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4208 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4209 | `boundary_error:` |
|         9 | 4210 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4211 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        84 | 4212 | `}` |
|         - | 4213 | `/*` |
|         - | 4214 | ` * array array_values(array $array)` |
|         - | 4215 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4216 | ` * Parameters` |
|         - | 4217 | ` *  $array` |
|         - | 4218 | ` *   The input array.` |
|         - | 4219 | ` * Return` |
|         - | 4220 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4221 | ` */` |
|        48 | 4222 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4223 | `{` |
|         - | 4224 | `	ph7_hashmap_node *pNode;` |
|         - | 4225 | `	ph7_hashmap *pMap;` |
|         - | 4226 | `	ph7_value *pArray;` |
|         - | 4227 | `	ph7_value *pObj;` |
|         - | 4228 | `	sxu32 n;` |
|        51 | 4229 | `	if( nArg != 1 ){` |
|         - | 4230 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4231 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4232 | `			"ArgumentCountError",` |
|         - | 4233 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4234 | `			nArg` |
|         - | 4235 | `			);` |
|         - | 4236 | `	}` |
|         - | 4237 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4238 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4239 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4240 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4241 | `			"TypeError",` |
|         - | 4242 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4243 | `			ph7_type_name(apArg[0])` |
|         - | 4244 | `			);` |
|         - | 4245 | `	}` |
|         - | 4246 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4247 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4248 | `	/* Create a new array */` |
|        46 | 4249 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4250 | `	if( pArray == 0 ){` |
|       ! 0 | 4251 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4252 | `		return PH7_OK;` |
|         - | 4253 | `	}` |
|         - | 4254 | `	/* Perform the requested operation */` |
|        46 | 4255 | `	pNode = pMap->pFirst;` |
|       144 | 4256 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4257 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4258 | `		if( pObj ){` |
|         - | 4259 | `			/* perform the insertion */` |
|       100 | 4260 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4261 | `		}` |
|         - | 4262 | `		/* Point to the next entry */` |
|       100 | 4263 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4264 | `	}` |
|         - | 4265 | `	/* return the new array */` |
|        46 | 4266 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4267 | `	return PH7_OK;` |
|        27 | 4268 | `}` |
|         - | 4269 | `/*` |
|         - | 4270 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4271 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4272 | ` * Parameters` |
|         - | 4273 | ` *  $input` |
|         - | 4274 | ` *   An array containing keys to return.` |
|         - | 4275 | ` * $search_value` |
|         - | 4276 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4277 | ` * $strict` |
|         - | 4278 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4279 | ` * Return` |
|         - | 4280 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4281 | ` */` |
|       160 | 4282 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4283 | `{` |
|         - | 4284 | `	ph7_hashmap_node *pNode;` |
|         - | 4285 | `	ph7_hashmap *pMap;` |
|         - | 4286 | `	ph7_value *pArray;` |
|         - | 4287 | `	ph7_value sObj;` |
|         - | 4288 | `	ph7_value sVal;` |
|         - | 4289 | `	SyString sKey;` |
|         - | 4290 | `	int bStrict;` |
|         - | 4291 | `	sxi32 rc;` |
|         - | 4292 | `	sxu32 n;` |
|       164 | 4293 | `	if( nArg < 1 ){` |
|         - | 4294 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4295 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4296 | `			"ArgumentCountError",` |
|         - | 4297 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4298 | `			);` |
|         - | 4299 | `	}` |
|         - | 4300 | `	/* Make sure we are dealing with a valid hashmap */` |
|       164 | 4301 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4302 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4303 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4304 | `			"TypeError",` |
|         - | 4305 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4306 | `			ph7_type_name(apArg[0])` |
|         - | 4307 | `			);` |
|         - | 4308 | `	}` |
|         - | 4309 | `	/* Point to the internal representation of the input hashmap */` |
|       161 | 4310 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4311 | `	/* Create a new array */` |
|       161 | 4312 | `	pArray = ph7_context_new_array(pCtx);` |
|       161 | 4313 | `	if( pArray == 0 ){` |
|       ! 0 | 4314 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4315 | `		return PH7_OK;` |
|         - | 4316 | `	}` |
|       161 | 4317 | `	bStrict = FALSE;` |
|       161 | 4318 | `	if( nArg > 2 ){` |
|         - | 4319 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4320 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4321 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4322 | `				"TypeError",` |
|         - | 4323 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4324 | `				ph7_type_name(apArg[2])` |
|         - | 4325 | `				);` |
|         - | 4326 | `		}` |
|         9 | 4327 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4328 | `	}` |
|         - | 4329 | `	/* Perform the requested operation */` |
|       161 | 4330 | `	pNode = pMap->pFirst;` |
|       161 | 4331 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1463 | 4332 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1305 | 4333 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       185 | 4334 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        94 | 4335 | `		}else{` |
|      1122 | 4336 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4337 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4338 | `		}` |
|      1305 | 4339 | `		rc = 0;` |
|      1305 | 4340 | `		if( nArg > 1 ){` |
|        65 | 4341 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4342 | `			if( pValue ){` |
|         - | 4343 | `				ph7_value sNeedle;` |
|        65 | 4344 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4345 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4346 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4347 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4348 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4349 | `				 * corrupt every later comparison. */` |
|        65 | 4350 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4351 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4352 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4353 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4354 | `			}` |
|        32 | 4355 | `		}` |
|      1305 | 4356 | `		if( rc == 0 ){` |
|         - | 4357 | `			/* Perform the insertion */` |
|      1273 | 4358 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       635 | 4359 | `		}` |
|      1305 | 4360 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4361 | `		/* Point to the next entry */` |
|      1305 | 4362 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       654 | 4363 | `	}` |
|         - | 4364 | `	/* return the new array */` |
|       161 | 4365 | `	ph7_result_value(pCtx,pArray);` |
|       161 | 4366 | `	return PH7_OK;` |
|        84 | 4367 | `}` |
|         - | 4368 | `/*` |
|         - | 4369 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4370 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4371 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4372 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4373 | ` * Parameters` |
|         - | 4374 | ` *  $arr1` |
|         - | 4375 | ` *   First array` |
|         - | 4376 | ` *  $arr2` |
|         - | 4377 | ` *   Second array` |
|         - | 4378 | ` * Return` |
|         - | 4379 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4380 | ` * Note` |
|         - | 4381 | ` *  This function is a symisc eXtension.` |
|         - | 4382 | ` */` |
|         4 | 4383 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4384 | `{` |
|         - | 4385 | `	ph7_hashmap *p1,*p2;` |
|         - | 4386 | `	int rc;` |
|         5 | 4387 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4388 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4389 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4390 | `		return PH7_OK;` |
|         - | 4391 | `	}` |
|         - | 4392 | `	/* Point to the hashmaps */` |
|         5 | 4393 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4394 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4395 | `	rc = (p1 == p2);` |
|         - | 4396 | `	/* Same instance? */` |
|         5 | 4397 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4398 | `	return PH7_OK;` |
|         3 | 4399 | `}` |
|         - | 4400 | `/*` |
|         - | 4401 | ` * array array_merge(array ...$arrays)` |
|         - | 4402 | ` *  Merge one or more arrays.` |
|         - | 4403 | ` * Parameters` |
|         - | 4404 | ` *  ...$arrays` |
|         - | 4405 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4406 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4407 | ` * Return` |
|         - | 4408 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4409 | ` *  with no arguments.` |
|         - | 4410 | ` */` |
|      1078 | 4411 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4412 | `{` |
|         - | 4413 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4414 | `	ph7_value *pArray;` |
|         - | 4415 | `	int i;` |
|         - | 4416 | `	/* Create a new array */` |
|      1083 | 4417 | `	pArray = ph7_context_new_array(pCtx);` |
|      1083 | 4418 | `	if( pArray == 0 ){` |
|       ! 0 | 4419 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4420 | `		return PH7_OK;` |
|         - | 4421 | `	}` |
|         - | 4422 | `	/* Point to the internal representation of the hashmap */` |
|      1083 | 4423 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4424 | `	/* Start merging */` |
|      3229 | 4425 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4426 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2155 | 4427 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4428 | `			/* Type mismatch -> TypeError */` |
|         8 | 4429 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4430 | `				"TypeError",` |
|         - | 4431 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4432 | `				i + 1,` |
|         4 | 4433 | `				ph7_type_name(apArg[i])` |
|         - | 4434 | `				);` |
|       ! 0 | 4435 | `		}else{` |
|      2151 | 4436 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4437 | `			/* Merge the two hashmaps */` |
|      2151 | 4438 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4439 | `		}` |
|      1078 | 4440 | `	}` |
|         - | 4441 | `	/* Return the freshly created array */` |
|      1079 | 4442 | `	ph7_result_value(pCtx,pArray);` |
|      1079 | 4443 | `	return PH7_OK;` |
|       544 | 4444 | `}` |
|         - | 4445 | `/*` |
|         - | 4446 | ` * array array_copy(array $source)` |
|         - | 4447 | ` *  Make a blind copy of the target array.` |
|         - | 4448 | ` * Parameters` |
|         - | 4449 | ` *  $source` |
|         - | 4450 | ` *   Target array` |
|         - | 4451 | ` * Return` |
|         - | 4452 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4453 | ` * Note` |
|         - | 4454 | ` *  This function is a symisc eXtension.` |
|         - | 4455 | ` */` |
|        18 | 4456 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4457 | `{` |
|         - | 4458 | `	ph7_hashmap *pMap;` |
|         - | 4459 | `	ph7_value *pArray;` |
|        19 | 4460 | `	if( nArg < 1 ){` |
|         - | 4461 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4462 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4463 | `		return PH7_OK;` |
|         - | 4464 | `	}` |
|         - | 4465 | `	/* Create a new array */` |
|        19 | 4466 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4467 | `	if( pArray == 0 ){` |
|       ! 0 | 4468 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4469 | `		return PH7_OK;` |
|         - | 4470 | `	}` |
|         - | 4471 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4472 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4473 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4474 | `		/* Point to the internal representation of the source */` |
|        19 | 4475 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4476 | `		/* Perform the copy */` |
|        19 | 4477 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4478 | `	}else{` |
|         - | 4479 | `		/* Simple insertion */` |
|       ! 0 | 4480 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4481 | `	}` |
|         - | 4482 | `	/* Return the duplicated array */` |
|        19 | 4483 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4484 | `	return PH7_OK;` |
|        10 | 4485 | `}` |
|         - | 4486 | `/*` |
|         - | 4487 | ` * bool array_erase(array $source)` |
|         - | 4488 | ` *  Remove all elements from a given array.` |
|         - | 4489 | ` * Parameters` |
|         - | 4490 | ` *  $source` |
|         - | 4491 | ` *   Target array` |
|         - | 4492 | ` * Return` |
|         - | 4493 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4494 | ` * Note` |
|         - | 4495 | ` *  This function is a symisc eXtension.` |
|         - | 4496 | ` */` |
|        26 | 4497 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4498 | `{` |
|         - | 4499 | `	ph7_hashmap *pMap;` |
|        28 | 4500 | `	if( nArg < 1 ){` |
|         - | 4501 | `		/* Missing arguments */` |
|       ! 0 | 4502 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4503 | `		return PH7_OK;` |
|         - | 4504 | `	}` |
|         - | 4505 | `	/* Point to the target hashmap */` |
|        28 | 4506 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4507 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4508 | `	/* Erase */` |
|        28 | 4509 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4510 | `	return PH7_OK;` |
|        15 | 4511 | `}` |
|         - | 4512 | `/*` |
|         - | 4513 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4514 | ` *  Extract a slice of the array.` |
|         - | 4515 | ` * Parameters` |
|         - | 4516 | ` *  $array` |
|         - | 4517 | ` *    The input array.` |
|         - | 4518 | ` * $offset` |
|         - | 4519 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4520 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4521 | ` * $length (optional, nullable)` |
|         - | 4522 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4523 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4524 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4525 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4526 | ` * $preserve_keys (optional)` |
|         - | 4527 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4528 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4529 | ` * Return` |
|         - | 4530 | ` *   The new slice.` |
|         - | 4531 | ` */` |
|        66 | 4532 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4533 | `{` |
|         - | 4534 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4535 | `	ph7_hashmap_node *pCur;` |
|         - | 4536 | `	ph7_value *pArray;` |
|         - | 4537 | `	int iLength,iOfft;` |
|         - | 4538 | `	int bPreserve;` |
|         - | 4539 | `	sxi32 rc;` |
|        71 | 4540 | `	if( nArg < 2 ){` |
|       ! 0 | 4541 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4542 | `			"ArgumentCountError",` |
|         - | 4543 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4544 | `			nArg` |
|         - | 4545 | `			);` |
|         - | 4546 | `	}` |
|        71 | 4547 | `	if( nArg > 4 ){` |
|         4 | 4548 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4549 | `			"ArgumentCountError",` |
|         - | 4550 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4551 | `			nArg` |
|         - | 4552 | `			);` |
|         - | 4553 | `	}` |
|        69 | 4554 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4555 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4556 | `			"TypeError",` |
|         - | 4557 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4558 | `			ph7_type_name(apArg[0])` |
|         - | 4559 | `			);` |
|         - | 4560 | `	}` |
|         - | 4561 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        92 | 4562 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        95 | 4563 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4564 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4565 | `			"TypeError",` |
|         - | 4566 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4567 | `			ph7_type_name(apArg[1])` |
|         - | 4568 | `			);` |
|         - | 4569 | `	}` |
|         - | 4570 | `	/* Validate $length type if provided: nullable int */` |
|        65 | 4571 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        56 | 4572 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        56 | 4573 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4574 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4575 | `				"TypeError",` |
|         - | 4576 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4577 | `				ph7_type_name(apArg[2])` |
|         - | 4578 | `				);` |
|         - | 4579 | `		}` |
|        18 | 4580 | `	}` |
|         - | 4581 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        63 | 4582 | `	if( nArg > 3 ){` |
|         7 | 4583 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4584 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4585 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4586 | `				"TypeError",` |
|         - | 4587 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4588 | `				ph7_type_name(apArg[3])` |
|         - | 4589 | `				);` |
|         - | 4590 | `		}` |
|         2 | 4591 | `	}` |
|         - | 4592 | `	/* Point the internal representation of the target array */` |
|        63 | 4593 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        63 | 4594 | `	bPreserve = FALSE;` |
|         - | 4595 | `	/* Get the offset */` |
|         - | 4596 | `	{` |
|        63 | 4597 | `		sxi64 iTmp = 0;` |
|        63 | 4598 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        63 | 4599 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4600 | `			return rcArg;` |
|         - | 4601 | `		}` |
|        63 | 4602 | `		iOfft = (int)iTmp;` |
|         - | 4603 | `	}` |
|        63 | 4604 | `	if( iOfft < 0 ){` |
|         5 | 4605 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4606 | `		if( iOfft < 0 ){` |
|         3 | 4607 | `			iOfft = 0;` |
|         1 | 4608 | `		}` |
|         2 | 4609 | `	}` |
|        63 | 4610 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4611 | `		/* Offset past end of array, return empty array */` |
|         5 | 4612 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4613 | `		if( pArray == 0 ){` |
|       ! 0 | 4614 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4615 | `			return PH7_OK;` |
|         - | 4616 | `		}` |
|         5 | 4617 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4618 | `		return PH7_OK;` |
|         - | 4619 | `	}` |
|         - | 4620 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        59 | 4621 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        59 | 4622 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        37 | 4623 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        37 | 4624 | `		if( iLength < 0 ){` |
|         5 | 4625 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4626 | `		}` |
|        37 | 4627 | `		if( iLength < 0 ){` |
|         3 | 4628 | `			iLength = 0;` |
|         1 | 4629 | `		}` |
|        37 | 4630 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4631 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4632 | `		}` |
|        18 | 4633 | `	}` |
|        59 | 4634 | `	if( nArg > 3 ){` |
|         5 | 4635 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4636 | `	}` |
|         - | 4637 | `	/* Create a new array */` |
|        59 | 4638 | `	pArray = ph7_context_new_array(pCtx);` |
|        59 | 4639 | `	if( pArray == 0 ){` |
|       ! 0 | 4640 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4641 | `		return PH7_OK;` |
|         - | 4642 | `	}` |
|        59 | 4643 | `	if( iLength < 1 ){` |
|         - | 4644 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4645 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4646 | `		return PH7_OK;` |
|         - | 4647 | `	}` |
|         - | 4648 | `	/* Point to the desired entry */` |
|        55 | 4649 | `	pCur = pSrc->pFirst;` |
|        54 | 4650 | `	for(;;){` |
|       113 | 4651 | `		if( iOfft < 1 ){` |
|        55 | 4652 | `			break;` |
|         - | 4653 | `		}` |
|         - | 4654 | `		/* Point to the next entry */` |
|        63 | 4655 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        63 | 4656 | `		iOfft--;` |
|         5 | 4657 | `	}` |
|         - | 4658 | `	/* Point to the internal representation of the hashmap */` |
|        55 | 4659 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       106 | 4660 | `	for(;;){` |
|       217 | 4661 | `		if( iLength < 1 ){` |
|        55 | 4662 | `			break;` |
|         - | 4663 | `		}` |
|         - | 4664 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4665 | `		{` |
|       167 | 4666 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|       167 | 4667 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4668 | `		}` |
|       167 | 4669 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4670 | `			break;` |
|         - | 4671 | `		}` |
|         - | 4672 | `		/* Point to the next entry */` |
|       167 | 4673 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       167 | 4674 | `		iLength--;` |
|         5 | 4675 | `	}` |
|         - | 4676 | `	/* Return the freshly created array */` |
|        55 | 4677 | `	ph7_result_value(pCtx,pArray);` |
|        55 | 4678 | `	return PH7_OK;` |
|        38 | 4679 | `}` |
|         - | 4680 | `/*` |
|         - | 4681 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4682 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4683 | ` * beginning (becomes the new pFirst).` |
|         - | 4684 | ` */` |
|        38 | 4685 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4686 | `{` |
|         - | 4687 | `	ph7_hashmap_node *pNode;` |
|         - | 4688 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4689 | `	pNode = pMap->pLast;` |
|        39 | 4690 | `	if( pNode == 0 ){` |
|       ! 0 | 4691 | `		return;` |
|         - | 4692 | `	}` |
|        39 | 4693 | `	if( pNode->pNext == 0 ){` |
|         - | 4694 | `		/* Only node in the list, nothing to move */` |
|         5 | 4695 | `		return;` |
|         - | 4696 | `	}` |
|        35 | 4697 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4698 | `		/* Already in the correct position */` |
|         9 | 4699 | `		return;` |
|         - | 4700 | `	}` |
|         - | 4701 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4702 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4703 | `	pMap->pLast->pPrev = 0;` |
|         - | 4704 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4705 | `	if( pAfter == 0 ){` |
|         - | 4706 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4707 | `		pNode->pNext = 0;` |
|         3 | 4708 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4709 | `		if( pMap->pFirst ){` |
|         3 | 4710 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4711 | `		}` |
|         3 | 4712 | `		pMap->pFirst = pNode;` |
|         2 | 4713 | `	}else{` |
|        25 | 4714 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4715 | `		pNode->pPrev = pOldNext;` |
|        25 | 4716 | `		pNode->pNext = pAfter;` |
|        25 | 4717 | `		pAfter->pPrev = pNode;` |
|        25 | 4718 | `		if( pOldNext ){` |
|        25 | 4719 | `			pOldNext->pNext = pNode;` |
|        13 | 4720 | `		}else{` |
|       ! 0 | 4721 | `			pMap->pLast = pNode;` |
|         - | 4722 | `		}` |
|         - | 4723 | `	}` |
|        20 | 4724 | `}` |
|         - | 4725 | `/*` |
|         - | 4726 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4727 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4728 | ` * Parameters` |
|         - | 4729 | ` *  $array` |
|         - | 4730 | ` *    The input array.` |
|         - | 4731 | ` *  $offset` |
|         - | 4732 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4733 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4734 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4735 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4736 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4737 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4738 | ` *  $length (optional)` |
|         - | 4739 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4740 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4741 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4742 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4743 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4744 | ` *  $replacement (optional)` |
|         - | 4745 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4746 | ` *    with elements from this array.` |
|         - | 4747 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4748 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4749 | ` *    offset.` |
|         - | 4750 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4751 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4752 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4753 | ` * Return` |
|         - | 4754 | ` *   A new array consisting of the extracted elements.` |
|         - | 4755 | ` */` |
|        64 | 4756 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4757 | `{` |
|         - | 4758 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4759 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4760 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4761 | `	int iLength,iOfft,i;` |
|         - | 4762 | `	sxi32 rc;` |
|        66 | 4763 | `	if( nArg < 2 ){` |
|       ! 0 | 4764 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4765 | `			"ArgumentCountError",` |
|         - | 4766 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4767 | `			nArg` |
|         - | 4768 | `			);` |
|         - | 4769 | `	}` |
|        66 | 4770 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4771 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4772 | `			"TypeError",` |
|         - | 4773 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4774 | `			ph7_type_name(apArg[0])` |
|         - | 4775 | `			);` |
|         - | 4776 | `	}` |
|         - | 4777 | `	/* Point to the internal representation of the target array */` |
|        63 | 4778 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4779 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4780 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4781 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4782 | `	if( iOfft < 0 ){` |
|         9 | 4783 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4784 | `		if( iOfft < 0 ){` |
|         3 | 4785 | `			iOfft = 0;` |
|         2 | 4786 | `		}` |
|        59 | 4787 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4788 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4789 | `	}` |
|         - | 4790 | `	/* Get the length and clamp to valid range.` |
|         - | 4791 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4792 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4793 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4794 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4795 | `		if( iLength < 0 ){` |
|         7 | 4796 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4797 | `			if( iLength < 0 ){` |
|         3 | 4798 | `				iLength = 0;` |
|         1 | 4799 | `			}` |
|         3 | 4800 | `		}` |
|        45 | 4801 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4802 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4803 | `		}` |
|        22 | 4804 | `	}` |
|         - | 4805 | `	/* Create the result array for removed elements */` |
|        63 | 4806 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4807 | `	if( pArray == 0 ){` |
|       ! 0 | 4808 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4809 | `		return PH7_OK;` |
|         - | 4810 | `	}` |
|         - | 4811 | `	/* Get replacement array if provided */` |
|        63 | 4812 | `	pRep = 0;` |
|        63 | 4813 | `	if( nArg > 3 ){` |
|        27 | 4814 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4815 | `			/* Perform an array cast */` |
|         3 | 4816 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4817 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4818 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4819 | `			}` |
|         2 | 4820 | `		}else{` |
|        25 | 4821 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4822 | `		}` |
|        27 | 4823 | `		if( pRep ){` |
|         - | 4824 | `			/* Reset the loop cursor */` |
|        27 | 4825 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4826 | `		}` |
|        13 | 4827 | `	}` |
|         - | 4828 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4829 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4830 | `	/* Navigate to the offset position */` |
|        63 | 4831 | `	pCur = pSrc->pFirst;` |
|       131 | 4832 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4833 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4834 | `	}` |
|         - | 4835 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4836 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4837 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4838 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4839 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4840 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4841 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4842 | `		pPrev = pCur->pPrev;` |
|        79 | 4843 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4844 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4845 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4846 | `			break;` |
|         - | 4847 | `		}` |
|        79 | 4848 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4849 | `	}` |
|         - | 4850 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4851 | `	if( pRep ){` |
|         - | 4852 | `		ph7_value sSafeVal;` |
|        78 | 4853 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4854 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4855 | `			if( pRvalue ){` |
|         - | 4856 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4857 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4858 | `				 * since it points into that same pool. */` |
|        39 | 4859 | `				sSafeVal = *pRvalue;` |
|        39 | 4860 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4861 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4862 | `					pNewNode = pSrc->pLast;` |
|        39 | 4863 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4864 | `					pInsertAfter = pNewNode;` |
|        19 | 4865 | `				}` |
|        19 | 4866 | `			}` |
|         1 | 4867 | `		}` |
|        13 | 4868 | `	}` |
|         - | 4869 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4870 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4871 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4872 | `	 * and removals left gaps. */` |
|         - | 4873 | `	{` |
|        63 | 4874 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4875 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4876 | `		pSrc->iNextIdx = 0;` |
|       233 | 4877 | `		while( n > 0 ){` |
|       171 | 4878 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4879 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4880 | `			}` |
|       171 | 4881 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4882 | `			n--;` |
|         1 | 4883 | `		}` |
|        63 | 4884 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4885 | `	}` |
|         - | 4886 | `	/* Return the freshly created array */` |
|        63 | 4887 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4888 | `	return PH7_OK;` |
|        34 | 4889 | `}` |
|         - | 4890 | `/*` |
|         - | 4891 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4892 | ` *  Checks if a value exists in an array.` |
|         - | 4893 | ` * Parameters` |
|         - | 4894 | ` *  $needle` |
|         - | 4895 | ` *   The searched value.` |
|         - | 4896 | ` *   Note:` |
|         - | 4897 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4898 | ` * $haystack` |
|         - | 4899 | ` *  The target array.` |
|         - | 4900 | ` * $strict` |
|         - | 4901 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4902 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4903 | ` */` |
|     32980 | 4904 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4905 | `{` |
|         - | 4906 | `	ph7_value *pNeedle;` |
|         - | 4907 | `	int bStrict;` |
|         - | 4908 | `	int rc;` |
|     32985 | 4909 | `	if( nArg < 2 ){` |
|         - | 4910 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4911 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4912 | `		return PH7_OK;` |
|         - | 4913 | `	}` |
|     32985 | 4914 | `	pNeedle = apArg[0];` |
|     32985 | 4915 | `	bStrict = 0;` |
|     32985 | 4916 | `	if( nArg > 2 ){` |
|        53 | 4917 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4918 | `	}` |
|     32985 | 4919 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4920 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4921 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4922 | `		/* Set the comparison result */` |
|       ! 0 | 4923 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4924 | `		return PH7_OK;` |
|         - | 4925 | `	}` |
|         - | 4926 | `	/* Perform the lookup */` |
|     32985 | 4927 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4928 | `	/* Lookup result */` |
|     32985 | 4929 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32985 | 4930 | `	return PH7_OK;` |
|     16495 | 4931 | `}` |
|         - | 4932 | `/*` |
|         - | 4933 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4934 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4935 | ` * Parameters` |
|         - | 4936 | ` * $needle` |
|         - | 4937 | ` *   The searched value.` |
|         - | 4938 | ` * $haystack` |
|         - | 4939 | ` *   The array.` |
|         - | 4940 | ` * $strict` |
|         - | 4941 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4942 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4943 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4944 | ` * Return` |
|         - | 4945 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4946 | ` */` |
|        26 | 4947 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4948 | `{` |
|         - | 4949 | `	ph7_hashmap_node *pEntry;` |
|         - | 4950 | `	ph7_value *pVal,sNeedle;` |
|         - | 4951 | `	ph7_hashmap *pMap;` |
|         - | 4952 | `	ph7_value sVal;` |
|         - | 4953 | `	int bStrict;` |
|         - | 4954 | `	sxu32 n;` |
|         - | 4955 | `	int rc;` |
|        28 | 4956 | `	if( nArg < 2 ){` |
|         - | 4957 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4958 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4959 | `			"ArgumentCountError",` |
|         - | 4960 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 4961 | `			nArg` |
|         - | 4962 | `			);` |
|         - | 4963 | `	}` |
|        28 | 4964 | `	bStrict = FALSE;` |
|        28 | 4965 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4966 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4967 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4968 | `			"TypeError",` |
|         - | 4969 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4970 | `			ph7_type_name(apArg[1])` |
|         - | 4971 | `			);` |
|         - | 4972 | `	}` |
|        25 | 4973 | `	if( nArg > 2 ){` |
|         - | 4974 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        11 | 4975 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4976 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4977 | `				"TypeError",` |
|         - | 4978 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4979 | `				ph7_type_name(apArg[2])` |
|         - | 4980 | `				);` |
|         - | 4981 | `		}` |
|        11 | 4982 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4983 | `	}` |
|         - | 4984 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4985 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4986 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4987 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4988 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4989 | `	pEntry = pMap->pFirst;` |
|        25 | 4990 | `	n = pMap->nEntry;` |
|        28 | 4991 | `	for(;;){` |
|        57 | 4992 | `		if( !n ){` |
|         9 | 4993 | `			break;` |
|         - | 4994 | `		}` |
|         - | 4995 | `		/* Extract node value */` |
|        49 | 4996 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4997 | `		if( pVal ){` |
|         - | 4998 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4999 | `			 * can change their type.` |
|         - | 5000 | `			 */` |
|        49 | 5001 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 5002 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 5003 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 5004 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 5005 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 5006 | `			if( rc == 0 ){` |
|         - | 5007 | `				/* Match found,return key */` |
|        17 | 5008 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 5009 | `					/* INT key */` |
|        11 | 5010 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 5011 | `				}else{` |
|         7 | 5012 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5013 | `					/* Blob key */` |
|         7 | 5014 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 5015 | `				}` |
|        17 | 5016 | `				return PH7_OK;` |
|         - | 5017 | `			}` |
|        16 | 5018 | `		}` |
|         - | 5019 | `		/* Point to the next entry */` |
|        33 | 5020 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5021 | `		n--;` |
|         1 | 5022 | `	}` |
|         - | 5023 | `	/* No such value,return FALSE */` |
|         9 | 5024 | `	ph7_result_bool(pCtx,0);` |
|         9 | 5025 | `	return PH7_OK;` |
|        15 | 5026 | `}` |
|         - | 5027 | `/*` |
|         - | 5028 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 5029 | ` *  Computes the difference of arrays.` |
|         - | 5030 | ` * Parameters` |
|         - | 5031 | ` *  $array1` |
|         - | 5032 | ` *    The array to compare from` |
|         - | 5033 | ` *  $array2` |
|         - | 5034 | ` *    An array to compare against` |
|         - | 5035 | ` *  $...` |
|         - | 5036 | ` *   More arrays to compare against` |
|         - | 5037 | ` * Return` |
|         - | 5038 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5039 | ` *  are not present in any of the other arrays.` |
|         - | 5040 | ` */` |
|        20 | 5041 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5042 | `{` |
|         - | 5043 | `	ph7_hashmap_node *pEntry;` |
|         - | 5044 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5045 | `	ph7_value *pArray;` |
|         - | 5046 | `	ph7_value *pVal;` |
|         - | 5047 | `	sxi32 rc;` |
|         - | 5048 | `	sxu32 n;` |
|         - | 5049 | `	int i;` |
|         - | 5050 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5051 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5052 | `	 * debugging difficult. */` |
|        23 | 5053 | `	if( nArg < 1 ){` |
|       ! 0 | 5054 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5055 | `			"ArgumentCountError",` |
|         - | 5056 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5057 | `			nArg` |
|         - | 5058 | `			);` |
|         - | 5059 | `	}` |
|        23 | 5060 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5061 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5062 | `			"TypeError",` |
|         - | 5063 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5064 | `			ph7_type_name(apArg[0])` |
|         - | 5065 | `			);` |
|         - | 5066 | `	}` |
|        36 | 5067 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5068 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5069 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5070 | `				"TypeError",` |
|         - | 5071 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5072 | `				i + 1,` |
|         2 | 5073 | `				ph7_type_name(apArg[i])` |
|         - | 5074 | `				);` |
|         - | 5075 | `		}` |
|         9 | 5076 | `	}` |
|        17 | 5077 | `	if( nArg == 1 ){` |
|         - | 5078 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5079 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5080 | `		return PH7_OK;` |
|         - | 5081 | `	}` |
|         - | 5082 | `	/* Create a new array */` |
|        15 | 5083 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5084 | `	if( pArray == 0 ){` |
|       ! 0 | 5085 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5086 | `		return PH7_OK;` |
|         - | 5087 | `	}` |
|         - | 5088 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5089 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5090 | `	/* Perform the diff */` |
|        15 | 5091 | `	pEntry = pSrc->pFirst;` |
|        15 | 5092 | `	n = pSrc->nEntry;` |
|        27 | 5093 | `	for(;;){` |
|        55 | 5094 | `		if( n < 1 ){` |
|        15 | 5095 | `			break;` |
|         - | 5096 | `		}` |
|         - | 5097 | `		/* Extract the node value */` |
|        41 | 5098 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5099 | `		if( pVal ){` |
|        69 | 5100 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5101 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5102 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5103 | `				/* Perform the lookup */` |
|        45 | 5104 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5105 | `				if( rc == SXRET_OK ){` |
|         - | 5106 | `					/* Value exist */` |
|        17 | 5107 | `					break;` |
|         - | 5108 | `				}` |
|        15 | 5109 | `			}` |
|        41 | 5110 | `			if( i >= nArg ){` |
|         - | 5111 | `				/* Perform the insertion */` |
|        25 | 5112 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5113 | `			}` |
|        20 | 5114 | `		}` |
|         - | 5115 | `		/* Point to the next entry */` |
|        41 | 5116 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5117 | `		n--;` |
|         1 | 5118 | `	}` |
|         - | 5119 | `	/* Return the freshly created array */` |
|        15 | 5120 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5121 | `	return PH7_OK;` |
|        13 | 5122 | `}` |
|         - | 5123 | `/*` |
|         - | 5124 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5125 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5126 | ` * Parameters` |
|         - | 5127 | ` *  $array1` |
|         - | 5128 | ` *    The array to compare from` |
|         - | 5129 | ` *  $array2` |
|         - | 5130 | ` *    An array to compare against` |
|         - | 5131 | ` *  $...` |
|         - | 5132 | ` *   More arrays to compare against.` |
|         - | 5133 | ` * $callback` |
|         - | 5134 | ` *  The callback comparison function.` |
|         - | 5135 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5136 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5137 | ` *  than the second.` |
|         - | 5138 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5139 | ` * Return` |
|         - | 5140 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5141 | ` *  are not present in any of the other arrays.` |
|         - | 5142 | ` */` |
|        20 | 5143 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5144 | `{` |
|         - | 5145 | `	ph7_hashmap_node *pEntry;` |
|         - | 5146 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5147 | `	ph7_value *pCallback;` |
|         - | 5148 | `	ph7_value *pArray;` |
|         - | 5149 | `	ph7_value *pVal;` |
|         - | 5150 | `	sxi32 rc;` |
|         - | 5151 | `	sxu32 n;` |
|         - | 5152 | `	int i;` |
|         - | 5153 |  |
|         - | 5154 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5155 | `	if( nArg < 2 ){` |
|       ! 0 | 5156 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5157 | `			"ArgumentCountError",` |
|         - | 5158 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5159 | `			nArg` |
|         - | 5160 | `			);` |
|         - | 5161 | `	}` |
|        25 | 5162 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5163 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5164 | `			"TypeError",` |
|         - | 5165 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5166 | `			ph7_type_name(apArg[0])` |
|         - | 5167 | `			);` |
|         - | 5168 | `	}` |
|         - | 5169 |  |
|        23 | 5170 | `	if( nArg == 2 ){` |
|         - | 5171 | `		/* Only the original array and the callback were provided. */` |
|         - | 5172 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5173 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5174 | `		 * validation order.` |
|         - | 5175 | `		 */` |
|         4 | 5176 | `	} else {` |
|         - | 5177 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5178 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5179 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5180 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5181 | `					"TypeError",` |
|         - | 5182 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5183 | `					i + 1,` |
|         6 | 5184 | `					ph7_type_name(apArg[i])` |
|         - | 5185 | `					);` |
|         - | 5186 | `			}` |
|         7 | 5187 | `		}` |
|         - | 5188 | `	}` |
|         - | 5189 |  |
|         - | 5190 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5191 | `	pCallback = apArg[nArg - 1];` |
|         - | 5192 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5193 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5194 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5195 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5196 | `				"TypeError",` |
|         - | 5197 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5198 | `				nArg` |
|         - | 5199 | `				);` |
|         - | 5200 | `		}` |
|         6 | 5201 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5202 | `			int len;` |
|         3 | 5203 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5204 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5205 | `				"TypeError",` |
|         - | 5206 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5207 | `				nArg,` |
|         1 | 5208 | `				zName` |
|         - | 5209 | `				);` |
|         - | 5210 | `		}` |
|         4 | 5211 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5212 | `			"TypeError",` |
|         - | 5213 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5214 | `			nArg` |
|         - | 5215 | `			);` |
|         - | 5216 | `	}` |
|         - | 5217 |  |
|         7 | 5218 | `	if( nArg == 2 ){` |
|         - | 5219 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5220 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5221 | `		return PH7_OK;` |
|         - | 5222 | `	}` |
|         - | 5223 |  |
|         - | 5224 | `	/* Create a new array */` |
|         5 | 5225 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5226 | `	if( pArray == 0 ){` |
|       ! 0 | 5227 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5228 | `		return PH7_OK;` |
|         - | 5229 | `	}` |
|         - | 5230 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5231 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5232 | `	/* Perform the diff */` |
|         5 | 5233 | `	pEntry = pSrc->pFirst;` |
|         5 | 5234 | `	n = pSrc->nEntry;` |
|         5 | 5235 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5236 | `	for(;;){` |
|        11 | 5237 | `		if( n < 1 ){` |
|         3 | 5238 | `			break;` |
|         - | 5239 | `		}` |
|         - | 5240 | `		/* Extract the node value */` |
|         9 | 5241 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5242 | `		if( pVal ){` |
|        15 | 5243 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5244 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5245 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5246 | `				/* Perform the lookup */` |
|         9 | 5247 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5248 | `				if( rc == SXRET_OK ){` |
|         - | 5249 | `					/* Value exist */` |
|         3 | 5250 | `					break;` |
|         - | 5251 | `				}` |
|         4 | 5252 | `			}` |
|         9 | 5253 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5254 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5255 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5256 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5257 | `				return PH7_EXCEPTION;` |
|         - | 5258 | `			}` |
|         7 | 5259 | `			if( i >= (nArg - 1)){` |
|         - | 5260 | `				/* Perform the insertion */` |
|         5 | 5261 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5262 | `			}` |
|         3 | 5263 | `		}` |
|         - | 5264 | `		/* Point to the next entry */` |
|         7 | 5265 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5266 | `		n--;` |
|         1 | 5267 | `	}` |
|         - | 5268 | `	/* Return the freshly created array */` |
|         3 | 5269 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5270 | `	return PH7_OK;` |
|        15 | 5271 | `}` |
|         - | 5272 | `/*` |
|         - | 5273 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5274 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5275 | ` * Parameters` |
|         - | 5276 | ` *  $array1` |
|         - | 5277 | ` *    The array to compare from` |
|         - | 5278 | ` *  $array2` |
|         - | 5279 | ` *    An array to compare against` |
|         - | 5280 | ` *  $...` |
|         - | 5281 | ` *   More arrays to compare against` |
|         - | 5282 | ` * Return` |
|         - | 5283 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5284 | ` *  are not present in any of the other arrays.` |
|         - | 5285 | ` */` |
|        20 | 5286 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5287 | `{` |
|         - | 5288 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5289 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5290 | `	ph7_value *pArray;` |
|         - | 5291 | `	ph7_value *pVal;` |
|         - | 5292 | `	sxi32 rc;` |
|         - | 5293 | `	sxu32 n;` |
|         - | 5294 | `	int i;` |
|         - | 5295 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5296 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5297 | `	 * accompanying integration tests to pass. */` |
|        24 | 5298 | `	if( nArg < 1 ){` |
|       ! 0 | 5299 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5300 | `			"ArgumentCountError",` |
|         - | 5301 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5302 | `			nArg` |
|         - | 5303 | `			);` |
|         - | 5304 | `	}` |
|        24 | 5305 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5306 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5307 | `			"TypeError",` |
|         - | 5308 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5309 | `			ph7_type_name(apArg[0])` |
|         - | 5310 | `			);` |
|         - | 5311 | `	}` |
|        37 | 5312 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5313 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5314 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5315 | `				"TypeError",` |
|         - | 5316 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5317 | `				i + 1,` |
|         4 | 5318 | `				ph7_type_name(apArg[i])` |
|         - | 5319 | `				);` |
|         - | 5320 | `		}` |
|        10 | 5321 | `	}` |
|        15 | 5322 | `	if( nArg == 1 ){` |
|         - | 5323 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5324 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5325 | `		return PH7_OK;` |
|         - | 5326 | `	}` |
|         - | 5327 | `	/* Create a new array */` |
|        13 | 5328 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5329 | `	if( pArray == 0 ){` |
|       ! 0 | 5330 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5331 | `		return PH7_OK;` |
|         - | 5332 | `	}` |
|         - | 5333 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5334 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5335 | `	/* Perform the diff */` |
|        13 | 5336 | `	pEntry = pSrc->pFirst;` |
|        13 | 5337 | `	n = pSrc->nEntry;` |
|        13 | 5338 | `	pN1 = pN2 = 0;` |
|        34 | 5339 | `	for(;;){` |
|         - | 5340 | `		int keep;` |
|        41 | 5341 | `		if( n < 1 ){` |
|        13 | 5342 | `			break;` |
|         - | 5343 | `		}` |
|         - | 5344 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5345 | `		keep = 1;` |
|        47 | 5346 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5347 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5348 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5349 | `			/* Perform a key lookup first */` |
|        33 | 5350 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5351 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5352 | `			}else{` |
|        21 | 5353 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5354 | `			}` |
|        33 | 5355 | `			if( rc != SXRET_OK ){` |
|         - | 5356 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5357 | `				continue;` |
|         - | 5358 | `			}` |
|         - | 5359 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5360 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5361 | `			if( pVal ){` |
|         - | 5362 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5363 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5364 | `				if( pVal2 ){` |
|         - | 5365 | `					ph7_value sV1,sV2;` |
|         - | 5366 | `					sxi32 cmp;` |
|         - | 5367 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5368 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5369 | `					 * null element used to come back bool(false) in the` |
|         - | 5370 | `					 * caller's array). */` |
|        17 | 5371 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5372 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5373 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5374 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5375 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5376 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5377 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5378 | `					if( cmp == 0 ){` |
|         - | 5379 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5380 | `						keep = 0;` |
|        15 | 5381 | `						break;` |
|         - | 5382 | `					}` |
|         1 | 5383 | `				}` |
|         1 | 5384 | `			}` |
|         2 | 5385 | `		}` |
|        29 | 5386 | `		if( keep ){` |
|         - | 5387 | `			/* Perform the insertion */` |
|        15 | 5388 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5389 | `		}` |
|         - | 5390 | `		/* Point to the next entry */` |
|        29 | 5391 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5392 | `		n--;` |
|         1 | 5393 | `	}` |
|         - | 5394 | `	/* Return the freshly created array */` |
|        13 | 5395 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5396 | `	return PH7_OK;` |
|        14 | 5397 | `}` |
|         - | 5398 | `/*` |
|         - | 5399 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5400 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5401 | ` *  by a user supplied callback function.` |
|         - | 5402 | ` * Parameters` |
|         - | 5403 | ` *  $array1` |
|         - | 5404 | ` *    The array to compare from` |
|         - | 5405 | ` *  $array2` |
|         - | 5406 | ` *    An array to compare against` |
|         - | 5407 | ` *  $...` |
|         - | 5408 | ` *   More arrays to compare against.` |
|         - | 5409 | ` *  $key_compare_func` |
|         - | 5410 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5411 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5412 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5413 | ` * Return` |
|         - | 5414 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5415 | ` *  are not present in any of the other arrays.` |
|         - | 5416 | ` */` |
|        22 | 5417 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5418 | `{` |
|         - | 5419 | `	ph7_hashmap_node *pEntry;` |
|         - | 5420 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5421 | `	ph7_value *pCallback;` |
|         - | 5422 | `	ph7_value *pArray;` |
|         - | 5423 | `	sxi32 rc;` |
|         - | 5424 | `	sxu32 n;` |
|         - | 5425 | `	int i;` |
|         - | 5426 |  |
|         - | 5427 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5428 | `	if( nArg < 2 ){` |
|       ! 0 | 5429 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5430 | `			"ArgumentCountError",` |
|         - | 5431 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5432 | `			nArg` |
|         - | 5433 | `			);` |
|         - | 5434 | `	}` |
|        26 | 5435 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5436 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5437 | `			"TypeError",` |
|         - | 5438 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5439 | `			ph7_type_name(apArg[0])` |
|         - | 5440 | `			);` |
|         - | 5441 | `	}` |
|         - | 5442 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5443 | `	 * expected to be a callback. */` |
|        38 | 5444 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5445 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5446 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5447 | `				"TypeError",` |
|         - | 5448 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5449 | `				i + 1,` |
|         2 | 5450 | `				ph7_type_name(apArg[i])` |
|         - | 5451 | `				);` |
|         - | 5452 | `		}` |
|         9 | 5453 | `	}` |
|         - | 5454 | `	/* Point to the callback value */` |
|        22 | 5455 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5456 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5457 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5458 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5459 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5460 | `		 * string given" which we also reproduce. */` |
|         9 | 5461 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5462 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5463 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5464 | `				"TypeError",` |
|         - | 5465 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5466 | `				nArg` |
|         - | 5467 | `				);` |
|         - | 5468 | `		}` |
|         6 | 5469 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5470 | `			/* neither array nor string */` |
|         8 | 5471 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5472 | `				"TypeError",` |
|         - | 5473 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5474 | `				nArg` |
|         - | 5475 | `				);` |
|         - | 5476 | `		}` |
|         - | 5477 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5478 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5479 | `			"TypeError",` |
|         - | 5480 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5481 | `			nArg,` |
|       ! 0 | 5482 | `			ph7_type_name(pCallback)` |
|         - | 5483 | `			);` |
|         - | 5484 | `	}` |
|        13 | 5485 | `	if( nArg == 2 ){` |
|         - | 5486 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5487 | `		 * input array. */` |
|         3 | 5488 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5489 | `		return PH7_OK;` |
|         - | 5490 | `	}` |
|         - | 5491 | `	/* Create a new array */` |
|        11 | 5492 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5493 | `	if( pArray == 0 ){` |
|       ! 0 | 5494 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5495 | `		return PH7_OK;` |
|         - | 5496 | `	}` |
|         - | 5497 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5498 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5499 | `	/* Perform the diff */` |
|        11 | 5500 | `	pEntry = pSrc->pFirst;` |
|        11 | 5501 | `	n = pSrc->nEntry;` |
|        21 | 5502 | `	for(;;){` |
|         - | 5503 | `		int keep;` |
|        27 | 5504 | `		if( n < 1 ){` |
|         9 | 5505 | `			break;` |
|         - | 5506 | `		}` |
|        19 | 5507 | `		keep = 1;` |
|        31 | 5508 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5509 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5510 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5511 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5512 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5513 | `			while( pIt ){` |
|         - | 5514 | `				/* build temporary key values for callback */` |
|         - | 5515 | `				ph7_value key1, key2, result;` |
|         - | 5516 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5517 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5518 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5519 | `				}else{` |
|         - | 5520 | `					SyString sStr;` |
|        33 | 5521 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5522 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5523 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5524 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5525 | `				}` |
|        33 | 5526 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5527 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5528 | `				}else{` |
|         - | 5529 | `					SyString sStr;` |
|        33 | 5530 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5531 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5532 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5533 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5534 | `				}` |
|        33 | 5535 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5536 | `				/* call user callback with (key1, key2) */` |
|         - | 5537 | `				{` |
|         - | 5538 | `					ph7_value *apK[2];` |
|        33 | 5539 | `					apK[0] = &key1;` |
|        33 | 5540 | `					apK[1] = &key2;` |
|        33 | 5541 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5542 | `				}` |
|        33 | 5543 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5544 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5545 | `					 * array_uintersect (which signal back from` |
|         - | 5546 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5547 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5548 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5549 | `					PH7_MemObjRelease(&result);` |
|         3 | 5550 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5551 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5552 | `					return PH7_EXCEPTION;` |
|         - | 5553 | `				}` |
|        31 | 5554 | `				if( rc == SXRET_OK ){` |
|        31 | 5555 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5556 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5557 | `					}` |
|        31 | 5558 | `					if( result.x.iVal == 0 ){` |
|         - | 5559 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5560 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5561 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5562 | `						if( pVal1 && pVal2 ){` |
|         - | 5563 | `							ph7_value sV1,sV2;` |
|         - | 5564 | `							sxi32 cmp;` |
|         - | 5565 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5566 | `							 * place and these are LIVE array elements. */` |
|        13 | 5567 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5568 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5569 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5570 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5571 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5572 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5573 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5574 | `							if( cmp == 0 ){` |
|         9 | 5575 | `								keep = 0;` |
|         9 | 5576 | `								PH7_MemObjRelease(&result);` |
|         - | 5577 | `								/* release keys too before breaking */` |
|         9 | 5578 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5579 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5580 | `								break;` |
|         - | 5581 | `							}` |
|         2 | 5582 | `						}` |
|         2 | 5583 | `					}` |
|        11 | 5584 | `				}` |
|        23 | 5585 | `				PH7_MemObjRelease(&result);` |
|        23 | 5586 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5587 | `				PH7_MemObjRelease(&key2);` |
|         - | 5588 | `				/* move to next node */` |
|        23 | 5589 | `				pIt = pIt->pPrev;` |
|        23 | 5590 | `				if( keep == 0 ) break;` |
|         1 | 5591 | `			}` |
|        21 | 5592 | `			if( keep == 0 ) break;` |
|         7 | 5593 | `		}` |
|        17 | 5594 | `		if( keep ){` |
|         - | 5595 | `			/* Perform the insertion */` |
|         9 | 5596 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5597 | `		}` |
|         - | 5598 | `		/* Point to the next entry */` |
|        17 | 5599 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5600 | `		n--;` |
|         1 | 5601 | `	}` |
|         - | 5602 | `	/* Return the freshly created array */` |
|         9 | 5603 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5604 | `	return PH7_OK;` |
|        15 | 5605 | `}` |
|         - | 5606 | `/*` |
|         - | 5607 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5608 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5609 | ` * Parameters` |
|         - | 5610 | ` *  $array1` |
|         - | 5611 | ` *    The array to compare from` |
|         - | 5612 | ` *  $array2` |
|         - | 5613 | ` *    An array to compare against` |
|         - | 5614 | ` *  $...` |
|         - | 5615 | ` *   More arrays to compare against` |
|         - | 5616 | ` * Return` |
|         - | 5617 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5618 | ` *  in any of the other arrays.` |
|         - | 5619 | ` * Note that NULL is returned on failure.` |
|         - | 5620 | ` */` |
|        12 | 5621 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5622 | `{` |
|         - | 5623 | `	ph7_hashmap_node *pEntry;` |
|         - | 5624 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5625 | `	ph7_value *pArray;` |
|         - | 5626 | `	sxi32 rc;` |
|         - | 5627 | `	sxu32 n;` |
|         - | 5628 | `	int i;` |
|         - | 5629 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5630 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5631 | `	 * helpers. */` |
|        15 | 5632 | `	if( nArg < 1 ){` |
|       ! 0 | 5633 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5634 | `			"ArgumentCountError",` |
|         - | 5635 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5636 | `			nArg` |
|         - | 5637 | `			);` |
|         - | 5638 | `	}` |
|        15 | 5639 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5640 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5641 | `			"TypeError",` |
|         - | 5642 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5643 | `			ph7_type_name(apArg[0])` |
|         - | 5644 | `			);` |
|         - | 5645 | `	}` |
|        20 | 5646 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5647 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5648 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5649 | `				"TypeError",` |
|         - | 5650 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5651 | `				i + 1,` |
|         2 | 5652 | `				ph7_type_name(apArg[i])` |
|         - | 5653 | `				);` |
|         - | 5654 | `		}` |
|         5 | 5655 | `	}` |
|         9 | 5656 | `	if( nArg == 1 ){` |
|         - | 5657 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5658 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5659 | `		return PH7_OK;` |
|         - | 5660 | `	}` |
|         - | 5661 | `	/* Create a new array */` |
|         7 | 5662 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5663 | `	if( pArray == 0 ){` |
|       ! 0 | 5664 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5665 | `		return PH7_OK;` |
|         - | 5666 | `	}` |
|         - | 5667 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5668 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5669 | `	/* Perfrom the diff */` |
|         7 | 5670 | `	pEntry = pSrc->pFirst;` |
|         7 | 5671 | `	n = pSrc->nEntry;` |
|        12 | 5672 | `	for(;;){` |
|        25 | 5673 | `		if( n < 1 ){` |
|         7 | 5674 | `			break;` |
|         - | 5675 | `		}` |
|        31 | 5676 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5677 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5678 | `				/* ignore */` |
|       ! 0 | 5679 | `				continue;` |
|         - | 5680 | `			}` |
|        23 | 5681 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5682 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5683 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5684 | `				/* Blob lookup */` |
|        17 | 5685 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5686 | `			}else{` |
|         - | 5687 | `				/* Int lookup */` |
|         7 | 5688 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5689 | `			}` |
|        23 | 5690 | `			if( rc == SXRET_OK ){` |
|         - | 5691 | `				/* Key exists,break immediately */` |
|        11 | 5692 | `				break;` |
|         - | 5693 | `			}` |
|         7 | 5694 | `		}` |
|        19 | 5695 | `		if( i >= nArg ){` |
|         - | 5696 | `			/* Perform the insertion */` |
|         9 | 5697 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5698 | `		}` |
|         - | 5699 | `		/* Point to the next entry */` |
|        19 | 5700 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5701 | `		n--;` |
|         1 | 5702 | `	}` |
|         - | 5703 | `	/* Return the freshly created array */` |
|         7 | 5704 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5705 | `	return PH7_OK;` |
|         9 | 5706 | `}` |
|         - | 5707 | `/*` |
|         - | 5708 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5709 | ` *  Computes the intersection of arrays.` |
|         - | 5710 | ` * Parameters` |
|         - | 5711 | ` *  $array1` |
|         - | 5712 | ` *    The array to compare from` |
|         - | 5713 | ` *  $array2` |
|         - | 5714 | ` *    An array to compare against` |
|         - | 5715 | ` *  $...` |
|         - | 5716 | ` *   More arrays to compare against` |
|         - | 5717 | ` * Return` |
|         - | 5718 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5719 | ` *  in all of the parameters.` |
|         - | 5720 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5721 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5722 | ` */` |
|        20 | 5723 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5724 | `{` |
|         - | 5725 | `	ph7_hashmap_node *pEntry;` |
|         - | 5726 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5727 | `	ph7_value *pArray;` |
|         - | 5728 | `	ph7_value *pVal;` |
|         - | 5729 | `	sxi32 rc;` |
|         - | 5730 | `	sxu32 n;` |
|         - | 5731 | `	int i;` |
|        23 | 5732 | `	if( nArg < 1 ){` |
|       ! 0 | 5733 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5734 | `			"ArgumentCountError",` |
|         - | 5735 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5736 | `			nArg` |
|         - | 5737 | `			);` |
|         - | 5738 | `	}` |
|        23 | 5739 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5740 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5741 | `			"TypeError",` |
|         - | 5742 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5743 | `			ph7_type_name(apArg[0])` |
|         - | 5744 | `			);` |
|         - | 5745 | `	}` |
|        36 | 5746 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5747 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5748 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5749 | `				"TypeError",` |
|         - | 5750 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5751 | `				i + 1,` |
|         2 | 5752 | `				ph7_type_name(apArg[i])` |
|         - | 5753 | `				);` |
|         - | 5754 | `		}` |
|         9 | 5755 | `	}` |
|        17 | 5756 | `	if( nArg == 1 ){` |
|         - | 5757 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5758 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5759 | `		return PH7_OK;` |
|         - | 5760 | `	}` |
|         - | 5761 | `	/* Create a new array */` |
|        15 | 5762 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5763 | `	if( pArray == 0 ){` |
|       ! 0 | 5764 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5765 | `		return PH7_OK;` |
|         - | 5766 | `	}` |
|         - | 5767 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5768 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5769 | `	/* Perform the intersection */` |
|        15 | 5770 | `	pEntry = pSrc->pFirst;` |
|        15 | 5771 | `	n = pSrc->nEntry;` |
|        31 | 5772 | `	for(;;){` |
|        63 | 5773 | `		if( n < 1 ){` |
|        15 | 5774 | `			break;` |
|         - | 5775 | `		}` |
|         - | 5776 | `		/* Extract the node value */` |
|        49 | 5777 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5778 | `		if( pVal ){` |
|        79 | 5779 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5780 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5781 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5782 | `				/* Perform the lookup */` |
|        55 | 5783 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5784 | `				if( rc != SXRET_OK ){` |
|         - | 5785 | `					/* Value does not exist */` |
|        25 | 5786 | `					break;` |
|         - | 5787 | `				}` |
|        16 | 5788 | `			}` |
|        49 | 5789 | `			if( i >= nArg ){` |
|         - | 5790 | `				/* Perform the insertion */` |
|        25 | 5791 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5792 | `			}` |
|        24 | 5793 | `		}` |
|         - | 5794 | `		/* Point to the next entry */` |
|        49 | 5795 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5796 | `		n--;` |
|         1 | 5797 | `	}` |
|         - | 5798 | `	/* Return the freshly created array */` |
|        15 | 5799 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5800 | `	return PH7_OK;` |
|        13 | 5801 | `}` |
|         - | 5802 | `/*` |
|         - | 5803 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5804 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5805 | ` * Parameters` |
|         - | 5806 | ` *  $array1` |
|         - | 5807 | ` *    The array to compare from` |
|         - | 5808 | ` *  $array2` |
|         - | 5809 | ` *    An array to compare against` |
|         - | 5810 | ` *  $...` |
|         - | 5811 | ` *   More arrays to compare against` |
|         - | 5812 | ` * Return` |
|         - | 5813 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5814 | ` *  in all the arguments, with matching keys.` |
|         - | 5815 | ` */` |
|        20 | 5816 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5817 | `{` |
|         - | 5818 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5819 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5820 | `	ph7_value *pArray;` |
|         - | 5821 | `	ph7_value *pVal;` |
|         - | 5822 | `	sxi32 rc;` |
|         - | 5823 | `	sxu32 n;` |
|         - | 5824 | `	int i;` |
|        23 | 5825 | `	if( nArg < 1 ){` |
|       ! 0 | 5826 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5827 | `			"ArgumentCountError",` |
|         - | 5828 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5829 | `			nArg` |
|         - | 5830 | `			);` |
|         - | 5831 | `	}` |
|        23 | 5832 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5833 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5834 | `			"TypeError",` |
|         - | 5835 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5836 | `			ph7_type_name(apArg[0])` |
|         - | 5837 | `			);` |
|         - | 5838 | `	}` |
|        36 | 5839 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5840 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5841 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5842 | `				"TypeError",` |
|         - | 5843 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5844 | `				i + 1,` |
|         2 | 5845 | `				ph7_type_name(apArg[i])` |
|         - | 5846 | `				);` |
|         - | 5847 | `		}` |
|         9 | 5848 | `	}` |
|        17 | 5849 | `	if( nArg == 1 ){` |
|         - | 5850 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5851 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5852 | `		return PH7_OK;` |
|         - | 5853 | `	}` |
|         - | 5854 | `	/* Create a new array */` |
|        15 | 5855 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5856 | `	if( pArray == 0 ){` |
|       ! 0 | 5857 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5858 | `		return PH7_OK;` |
|         - | 5859 | `	}` |
|         - | 5860 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5861 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5862 | `	/* Perform the intersection */` |
|        15 | 5863 | `	pEntry = pSrc->pFirst;` |
|        15 | 5864 | `	n = pSrc->nEntry;` |
|        15 | 5865 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5866 | `	for(;;){` |
|        47 | 5867 | `		if( n < 1 ){` |
|        15 | 5868 | `			break;` |
|         - | 5869 | `		}` |
|         - | 5870 | `		/* Extract the node value */` |
|        33 | 5871 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5872 | `		if( pVal ){` |
|        53 | 5873 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5874 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5875 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5876 | `				/* Perform a key lookup first */` |
|        37 | 5877 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5878 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5879 | `				}else{` |
|        23 | 5880 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5881 | `				}` |
|        37 | 5882 | `				if( rc != SXRET_OK ){` |
|         - | 5883 | `					/* No such key,break immediately */` |
|         7 | 5884 | `					break;` |
|         - | 5885 | `				}` |
|         - | 5886 | `				/* Perform the lookup */` |
|        31 | 5887 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5888 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5889 | `					/* Value does not exist */` |
|         6 | 5890 | `					break;` |
|         - | 5891 | `				}` |
|        11 | 5892 | `			}` |
|        33 | 5893 | `			if( i >= nArg ){` |
|         - | 5894 | `				/* Perform the insertion */` |
|        17 | 5895 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5896 | `			}` |
|        16 | 5897 | `		}` |
|         - | 5898 | `		/* Point to the next entry */` |
|        33 | 5899 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5900 | `		n--;` |
|         1 | 5901 | `	}` |
|         - | 5902 | `	/* Return the freshly created array */` |
|        15 | 5903 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5904 | `	return PH7_OK;` |
|        13 | 5905 | `}` |
|         - | 5906 | `/*` |
|         - | 5907 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5908 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5909 | ` * Parameters` |
|         - | 5910 | ` *  $array1` |
|         - | 5911 | ` *    The array to compare from` |
|         - | 5912 | ` *  $...` |
|         - | 5913 | ` *   More arrays to compare against` |
|         - | 5914 | ` * Return` |
|         - | 5915 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5916 | ` *  have keys that are present in all arguments.` |
|         - | 5917 | ` * Note that NULL is returned on failure.` |
|         - | 5918 | ` */` |
|        20 | 5919 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5920 | `{` |
|         - | 5921 | `	ph7_hashmap_node *pEntry;` |
|         - | 5922 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5923 | `	ph7_value *pArray;` |
|         - | 5924 | `	sxi32 rc;` |
|         - | 5925 | `	sxu32 n;` |
|         - | 5926 | `	int i;` |
|        23 | 5927 | `	if( nArg < 1 ){` |
|       ! 0 | 5928 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5929 | `			"ArgumentCountError",` |
|         - | 5930 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5931 | `			nArg` |
|         - | 5932 | `			);` |
|         - | 5933 | `	}` |
|        23 | 5934 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5935 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5936 | `			"TypeError",` |
|         - | 5937 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5938 | `			ph7_type_name(apArg[0])` |
|         - | 5939 | `			);` |
|         - | 5940 | `	}` |
|        36 | 5941 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5942 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5943 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5944 | `				"TypeError",` |
|         - | 5945 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5946 | `				i + 1,` |
|         2 | 5947 | `				ph7_type_name(apArg[i])` |
|         - | 5948 | `				);` |
|         - | 5949 | `		}` |
|         9 | 5950 | `	}` |
|        17 | 5951 | `	if( nArg == 1 ){` |
|         - | 5952 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5953 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5954 | `		return PH7_OK;` |
|         - | 5955 | `	}` |
|         - | 5956 | `	/* Create a new array */` |
|        15 | 5957 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5958 | `	if( pArray == 0 ){` |
|       ! 0 | 5959 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5960 | `		return PH7_OK;` |
|         - | 5961 | `	}` |
|         - | 5962 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5963 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5964 | `	/* Perform the intersection */` |
|        15 | 5965 | `	pEntry = pSrc->pFirst;` |
|        15 | 5966 | `	n = pSrc->nEntry;` |
|        24 | 5967 | `	for(;;){` |
|        49 | 5968 | `		if( n < 1 ){` |
|        15 | 5969 | `			break;` |
|         - | 5970 | `		}` |
|        57 | 5971 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5972 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5973 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5974 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5975 | `				/* Blob lookup */` |
|        27 | 5976 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5977 | `			}else{` |
|         - | 5978 | `				/* Int key */` |
|        13 | 5979 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5980 | `			}` |
|        39 | 5981 | `			if( rc != SXRET_OK ){` |
|         - | 5982 | `				/* Key does not exist, break immediately */` |
|        17 | 5983 | `				break;` |
|         - | 5984 | `			}` |
|        12 | 5985 | `		}` |
|        35 | 5986 | `		if( i >= nArg ){` |
|         - | 5987 | `			/* Perform the insertion */` |
|        19 | 5988 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5989 | `		}` |
|         - | 5990 | `		/* Point to the next entry */` |
|        35 | 5991 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5992 | `		n--;` |
|         1 | 5993 | `	}` |
|         - | 5994 | `	/* Return the freshly created array */` |
|        15 | 5995 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5996 | `	return PH7_OK;` |
|        13 | 5997 | `}` |
|         - | 5998 | `/*` |
|         - | 5999 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 6000 | ` *  Computes the intersection of arrays.` |
|         - | 6001 | ` * Parameters` |
|         - | 6002 | ` *  $array1` |
|         - | 6003 | ` *    The array to compare from` |
|         - | 6004 | ` *  $array2` |
|         - | 6005 | ` *    An array to compare against` |
|         - | 6006 | ` *  $...` |
|         - | 6007 | ` *   More arrays to compare against` |
|         - | 6008 | ` * $callback` |
|         - | 6009 | ` *  The callback comparison function.` |
|         - | 6010 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 6011 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 6012 | ` *  than the second.` |
|         - | 6013 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 6014 | ` * Return` |
|         - | 6015 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 6016 | ` *  in all of the parameters. .` |
|         - | 6017 | ` * Note that NULL is returned on failure.` |
|         - | 6018 | ` */` |
|        24 | 6019 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6020 | `{` |
|         - | 6021 | `	ph7_hashmap_node *pEntry;` |
|         - | 6022 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 6023 | `	ph7_value *pCallback;` |
|         - | 6024 | `	ph7_value *pArray;` |
|         - | 6025 | `	ph7_value *pVal;` |
|         - | 6026 | `	sxi32 rc;` |
|         - | 6027 | `	sxu32 n;` |
|         - | 6028 | `	int i;` |
|         - | 6029 |  |
|         - | 6030 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 6031 | `	if( nArg < 2 ){` |
|       ! 0 | 6032 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6033 | `			"ArgumentCountError",` |
|         - | 6034 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 6035 | `			nArg` |
|         - | 6036 | `			);` |
|         - | 6037 | `	}` |
|        29 | 6038 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6039 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6040 | `			"TypeError",` |
|         - | 6041 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6042 | `			ph7_type_name(apArg[0])` |
|         - | 6043 | `			);` |
|         - | 6044 | `	}` |
|         - | 6045 |  |
|        27 | 6046 | `	if( nArg == 2 ){` |
|         - | 6047 | `		/* Only the original array and the callback were provided. */` |
|         - | 6048 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6049 | `		 * validation ordering. */` |
|         3 | 6050 | `	} else {` |
|         - | 6051 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6052 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6053 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6054 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6055 | `					"TypeError",` |
|         - | 6056 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6057 | `					i + 1,` |
|         2 | 6058 | `					ph7_type_name(apArg[i])` |
|         - | 6059 | `					);` |
|         - | 6060 | `			}` |
|        13 | 6061 | `		}` |
|         - | 6062 | `	}` |
|         - | 6063 |  |
|         - | 6064 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6065 | `	pCallback = apArg[nArg - 1];` |
|         - | 6066 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6067 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6068 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6069 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6070 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6071 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6072 | `			 */` |
|         9 | 6073 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6074 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6075 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6076 | `					"TypeError",` |
|         - | 6077 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6078 | `					nArg` |
|         - | 6079 | `					);` |
|         - | 6080 | `			}` |
|         - | 6081 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6082 | `			{` |
|         6 | 6083 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6084 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6085 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6086 | `					int nMethodLen;` |
|         6 | 6087 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6088 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6089 | `					if( pClass ){` |
|         - | 6090 | `						/* Class exists but method is missing. */` |
|         4 | 6091 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6092 | `							"TypeError",` |
|         - | 6093 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6094 | `							nArg,` |
|         1 | 6095 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6096 | `							zMethod` |
|         - | 6097 | `							);` |
|         - | 6098 | `					}` |
|         - | 6099 | `					/* Class not found */` |
|         - | 6100 | `					{` |
|         - | 6101 | `						int nName;` |
|         3 | 6102 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6103 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6104 | `							"TypeError",` |
|         - | 6105 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6106 | `							nArg,` |
|         1 | 6107 | `							zName` |
|         - | 6108 | `							);` |
|         - | 6109 | `					}` |
|         - | 6110 | `				}` |
|         - | 6111 | `			}` |
|         - | 6112 | `			/* Fallback message */` |
|       ! 0 | 6113 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6114 | `				"TypeError",` |
|         - | 6115 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6116 | `				nArg` |
|         - | 6117 | `				);` |
|         - | 6118 | `		}` |
|         6 | 6119 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6120 | `			int len;` |
|         3 | 6121 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6122 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6123 | `				"TypeError",` |
|         - | 6124 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6125 | `				nArg,` |
|         1 | 6126 | `				zName` |
|         - | 6127 | `				);` |
|         - | 6128 | `		}` |
|         4 | 6129 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6130 | `			"TypeError",` |
|         - | 6131 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6132 | `			nArg` |
|         - | 6133 | `			);` |
|         - | 6134 | `	}` |
|         - | 6135 |  |
|        11 | 6136 | `	if( nArg == 2 ){` |
|         - | 6137 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6138 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6139 | `		return PH7_OK;` |
|         - | 6140 | `	}` |
|         - | 6141 |  |
|         - | 6142 | `	/* Create a new array */` |
|         7 | 6143 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6144 | `	if( pArray == 0 ){` |
|       ! 0 | 6145 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6146 | `		return PH7_OK;` |
|         - | 6147 | `	}` |
|         - | 6148 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6149 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6150 | `	/* Perform the intersection */` |
|         7 | 6151 | `	pEntry = pSrc->pFirst;` |
|         7 | 6152 | `	n = pSrc->nEntry;` |
|         7 | 6153 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6154 | `	for(;;){` |
|        19 | 6155 | `		if( n < 1 ){` |
|         5 | 6156 | `			break;` |
|         - | 6157 | `		}` |
|         - | 6158 | `		/* Extract the node value */` |
|        15 | 6159 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6160 | `		if( pVal ){` |
|        23 | 6161 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6162 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6163 | `					/* ignore */` |
|       ! 0 | 6164 | `					continue;` |
|         - | 6165 | `				}` |
|         - | 6166 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6167 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6168 | `				/* Perform the lookup */` |
|        15 | 6169 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6170 | `				if( rc != SXRET_OK ){` |
|         - | 6171 | `					/* Value does not exist */` |
|         7 | 6172 | `					break;` |
|         - | 6173 | `				}` |
|         5 | 6174 | `			}` |
|        15 | 6175 | `			if( i >= (nArg-1) ){` |
|         - | 6176 | `				/* Perform the insertion */` |
|         9 | 6177 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6178 | `			}` |
|         7 | 6179 | `		}` |
|        15 | 6180 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6181 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6182 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6183 | `			return PH7_EXCEPTION;` |
|         - | 6184 | `		}` |
|         - | 6185 | `		/* Point to the next entry */` |
|        13 | 6186 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6187 | `		n--;` |
|         1 | 6188 | `	}` |
|         - | 6189 | `	/* Return the freshly created array */` |
|         5 | 6190 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6191 | `	return PH7_OK;` |
|        17 | 6192 | `}` |
|         - | 6193 | `/*` |
|         - | 6194 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6195 | ` *  Fill an array with values.` |
|         - | 6196 | ` * Parameters` |
|         - | 6197 | ` *  $start_index` |
|         - | 6198 | ` *    The first index of the returned array.` |
|         - | 6199 | ` *  $num` |
|         - | 6200 | ` *   Number of elements to insert.` |
|         - | 6201 | ` *  $value` |
|         - | 6202 | ` *    Value to use for filling.` |
|         - | 6203 | ` * Return` |
|         - | 6204 | ` *  The filled array or null on failure.` |
|         - | 6205 | ` */` |
|       240 | 6206 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6207 | `{` |
|         - | 6208 | `	ph7_value *pArray;` |
|         - | 6209 | `	int i,nEntry;` |
|         - | 6210 |  |
|         - | 6211 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6212 | `	if( nArg != 3 ){` |
|         - | 6213 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6214 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6215 | `			"ArgumentCountError",` |
|         - | 6216 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6217 | `			nArg` |
|         - | 6218 | `			);` |
|         - | 6219 | `	}` |
|         - | 6220 |  |
|         - | 6221 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6222 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6223 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6224 | `	 * and NULLs are rejected outright. */` |
|       357 | 6225 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6226 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6227 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6228 | `			"TypeError",` |
|         - | 6229 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6230 | `			ph7_type_name(apArg[0])` |
|         - | 6231 | `			);` |
|         - | 6232 | `	}` |
|       242 | 6233 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6234 | `		int len;` |
|         8 | 6235 | `		sxu8 bReal = FALSE;` |
|         8 | 6236 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6237 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6238 | `			/* Non‑numeric string is an error. */` |
|         3 | 6239 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6240 | `				"TypeError",` |
|         - | 6241 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6242 | `				);` |
|         - | 6243 | `		}` |
|         5 | 6244 | `		if( bReal ){` |
|         - | 6245 | `			/* float-string -> deprecation warning */` |
|         4 | 6246 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6247 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6248 | `				zStr` |
|         - | 6249 | `				);` |
|         1 | 6250 | `		}` |
|         2 | 6251 | `	}` |
|         - | 6252 |  |
|         - | 6253 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6254 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6255 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6256 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6257 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6258 | `			"TypeError",` |
|         - | 6259 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6260 | `			ph7_type_name(apArg[1])` |
|         - | 6261 | `			);` |
|         - | 6262 | `	}` |
|       239 | 6263 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6264 | `		int len;` |
|         3 | 6265 | `		sxu8 bReal = FALSE;` |
|         3 | 6266 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6267 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6268 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6269 | `				"TypeError",` |
|         - | 6270 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6271 | `				);` |
|         - | 6272 | `		}` |
|       ! 0 | 6273 | `	}` |
|         - | 6274 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6275 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6276 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6277 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6278 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6279 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6280 | `		if( d != (double)i64 ){` |
|         7 | 6281 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6282 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6283 | `				d` |
|         - | 6284 | `				);` |
|         2 | 6285 | `		}` |
|         2 | 6286 | `	}` |
|         - | 6287 |  |
|         - | 6288 | `	/* Total number of entries to insert */` |
|       236 | 6289 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6290 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6291 | `	if( nEntry < 0 ){` |
|         3 | 6292 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6293 | `			"ValueError",` |
|         - | 6294 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6295 | `			);` |
|         - | 6296 | `	}` |
|         - | 6297 |  |
|         - | 6298 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6299 | `	if( nEntry == 0 ){` |
|         7 | 6300 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6301 | `		return PH7_OK;` |
|         - | 6302 | `	}` |
|         - | 6303 |  |
|         - | 6304 | `	/* Create a new array */` |
|       227 | 6305 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6306 | `	if( pArray == 0 ){` |
|       ! 0 | 6307 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6308 | `	}` |
|         - | 6309 |  |
|         - | 6310 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6311 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6312 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6313 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6314 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6315 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6316 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6317 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6318 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6319 | `		}` |
|   1058803 | 6320 | `	}` |
|         - | 6321 | `	/* Return the filled array */` |
|       227 | 6322 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6323 | `	return PH7_OK;` |
|       124 | 6324 | `}` |
|         - | 6325 | `/*` |
|         - | 6326 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6327 | ` *  Fill an array with values, specifying keys.` |
|         - | 6328 | ` * Parameters` |
|         - | 6329 | ` *  $input` |
|         - | 6330 | ` *   Array of values that will be used as key.` |
|         - | 6331 | ` *  $value` |
|         - | 6332 | ` *    Value to use for filling.` |
|         - | 6333 | ` * Return` |
|         - | 6334 | ` *  The filled array.` |
|         - | 6335 | ` * Throws` |
|         - | 6336 | ` *  ValueError if $input is not an array.` |
|         - | 6337 | ` */` |
|        22 | 6338 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6339 | `{` |
|         - | 6340 | `	ph7_hashmap_node *pEntry;` |
|         - | 6341 | `	ph7_hashmap *pSrc;` |
|         - | 6342 | `	ph7_value *pArray;` |
|         - | 6343 | `	sxu32 n;` |
|         - | 6344 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6345 | `	if( nArg != 2 ){` |
|         4 | 6346 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6347 | `			"ArgumentCountError",` |
|         - | 6348 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6349 | `			nArg` |
|         - | 6350 | `			);` |
|         - | 6351 | `	}` |
|         - | 6352 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6353 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6354 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6355 | `			"TypeError",` |
|         - | 6356 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6357 | `			ph7_type_name(apArg[0])` |
|         - | 6358 | `			);` |
|         - | 6359 | `	}` |
|         - | 6360 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6361 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6362 | `	/* Create a new array */` |
|        17 | 6363 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6364 | `	if( pArray == 0 ){` |
|       ! 0 | 6365 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6366 | `		return PH7_OK;` |
|         - | 6367 | `	}` |
|         - | 6368 | `	/* Perform the requested operation */` |
|        17 | 6369 | `	pEntry = pSrc->pFirst;` |
|        45 | 6370 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6371 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6372 | `		/* Point to the next entry */` |
|        29 | 6373 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6374 | `	}` |
|         - | 6375 | `	/* Return the filled array */` |
|        17 | 6376 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6377 | `	return PH7_OK;` |
|        14 | 6378 | `}` |
|         - | 6379 | `/*` |
|         - | 6380 | ` * array array_combine(array $keys,array $values)` |
|         - | 6381 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6382 | ` * Parameters` |
|         - | 6383 | ` *  $keys` |
|         - | 6384 | ` *    Array of keys to be used.` |
|         - | 6385 | ` * $values` |
|         - | 6386 | ` *   Array of values to be used.` |
|         - | 6387 | ` * Return` |
|         - | 6388 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6389 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6390 | ` *  not an array.` |
|         - | 6391 | ` */` |
|        16 | 6392 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6393 | `{` |
|         - | 6394 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6395 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6396 | `	ph7_value *pArray;` |
|         - | 6397 | `	sxu32 n;` |
|         - | 6398 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6399 | `	if( nArg != 2 ){` |
|         - | 6400 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6401 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6402 | `			"ArgumentCountError",` |
|         - | 6403 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6404 | `			nArg` |
|         - | 6405 | `			);` |
|         - | 6406 | `	}` |
|         - | 6407 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6408 | `	 * argument index in the error message. */` |
|        20 | 6409 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6410 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6411 | `			"TypeError",` |
|         - | 6412 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6413 | `			ph7_type_name(apArg[0])` |
|         - | 6414 | `			);` |
|         - | 6415 | `	}` |
|        17 | 6416 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6417 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6418 | `			"TypeError",` |
|         - | 6419 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6420 | `			ph7_type_name(apArg[1])` |
|         - | 6421 | `			);` |
|         - | 6422 | `	}` |
|         - | 6423 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6424 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6425 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6426 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6427 | `		/* Length mismatch -> ValueError */` |
|         3 | 6428 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6429 | `			"ValueError",` |
|         - | 6430 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6431 | `			);` |
|         - | 6432 | `	}` |
|         - | 6433 | `	/* Create a new array */` |
|        11 | 6434 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6435 | `	if( pArray == 0 ){` |
|       ! 0 | 6436 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6437 | `		return PH7_OK;` |
|         - | 6438 | `	}` |
|         - | 6439 | `	/* Perform the requested operation */` |
|        11 | 6440 | `	pKe = pKey->pFirst;` |
|        11 | 6441 | `	pVe = pValue->pFirst;` |
|        33 | 6442 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6443 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6444 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6445 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6446 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6447 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6448 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6449 | `		 * original array must not be mutated. */` |
|        23 | 6450 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6451 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6452 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6453 | `			if( pTmpKey ){` |
|         5 | 6454 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6455 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6456 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6457 | `				pKeyCopy = pTmpKey;` |
|         2 | 6458 | `			}` |
|         2 | 6459 | `		}` |
|        23 | 6460 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6461 | `		/* Point to the next entry */` |
|        23 | 6462 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6463 | `		pVe = pVe->pPrev;` |
|        12 | 6464 | `	}` |
|         - | 6465 | `	/* Return the filled array */` |
|        11 | 6466 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6467 | `	return PH7_OK;` |
|        12 | 6468 | `}` |
|         - | 6469 | `/*` |
|         - | 6470 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6471 | ` *  Return an array with elements in reverse order.` |
|         - | 6472 | ` * Parameters` |
|         - | 6473 | ` *  $array` |
|         - | 6474 | ` *   The input array.` |
|         - | 6475 | ` *  $preserve_keys (optional)` |
|         - | 6476 | ` *   If set to TRUE keys are preserved.` |
|         - | 6477 | ` * Return` |
|         - | 6478 | ` *  The reversed array.` |
|         - | 6479 | ` */` |
|        18 | 6480 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6481 | `{` |
|         - | 6482 | `	ph7_hashmap_node *pEntry;` |
|         - | 6483 | `	ph7_hashmap *pSrc;` |
|         - | 6484 | `	ph7_value *pArray;` |
|         - | 6485 | `	int bPreserve;` |
|         - | 6486 | `	sxu32 n;` |
|        20 | 6487 | `	if( nArg < 1 ){` |
|       ! 0 | 6488 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6489 | `			"ArgumentCountError",` |
|         - | 6490 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6491 | `			nArg` |
|         - | 6492 | `			);` |
|         - | 6493 | `	}` |
|         - | 6494 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6495 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6496 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6497 | `			"TypeError",` |
|         - | 6498 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6499 | `			ph7_type_name(apArg[0])` |
|         - | 6500 | `			);` |
|         - | 6501 | `	}` |
|        17 | 6502 | `	bPreserve = FALSE;` |
|        17 | 6503 | `	if( nArg > 1 ){` |
|         7 | 6504 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6505 | `	}` |
|         - | 6506 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6507 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6508 | `	/* Create a new array */` |
|        17 | 6509 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6510 | `	if( pArray == 0 ){` |
|       ! 0 | 6511 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6512 | `		return PH7_OK;` |
|         - | 6513 | `	}` |
|         - | 6514 | `	/* Perform the requested operation */` |
|        17 | 6515 | `	pEntry = pSrc->pLast;` |
|        55 | 6516 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6517 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6518 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6519 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6520 | `		/* Point to the previous entry */` |
|        39 | 6521 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6522 | `	}` |
|        17 | 6523 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6524 | `	return PH7_OK;` |
|        11 | 6525 | `}` |
|         - | 6526 | `/*` |
|         - | 6527 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6528 | ` *  Removes duplicate values from an array.` |
|         - | 6529 | ` * Parameters` |
|         - | 6530 | ` *  $array` |
|         - | 6531 | ` *   The input array.` |
|         - | 6532 | ` *  $flags` |
|         - | 6533 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6534 | ` *   behavior using these values:` |
|         - | 6535 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6536 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6537 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6538 | ` * Return` |
|         - | 6539 | ` *  The filtered array.` |
|         - | 6540 | ` */` |
|        22 | 6541 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6542 | `{` |
|         - | 6543 | `	ph7_hashmap_node *pEntry;` |
|         - | 6544 | `	ph7_value *pNeedle;` |
|         - | 6545 | `	ph7_hashmap *pSrc;` |
|         - | 6546 | `	ph7_value *pArray;` |
|         - | 6547 | `	int bStrict;` |
|         - | 6548 | `	sxi32 rc;` |
|         - | 6549 | `	sxu32 n;` |
|        25 | 6550 | `	if( nArg < 1 ){` |
|         - | 6551 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6552 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6553 | `			"ArgumentCountError",` |
|         - | 6554 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6555 | `			);` |
|         - | 6556 | `	}` |
|        25 | 6557 | `	if( nArg > 2 ){` |
|         - | 6558 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6559 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6560 | `			"ArgumentCountError",` |
|         - | 6561 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6562 | `			nArg` |
|         - | 6563 | `			);` |
|         - | 6564 | `	}` |
|         - | 6565 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6566 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6567 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6568 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6569 | `			"TypeError",` |
|         - | 6570 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6571 | `			ph7_type_name(apArg[0])` |
|         - | 6572 | `			);` |
|         - | 6573 | `	}` |
|        19 | 6574 | `	bStrict = FALSE;` |
|         - | 6575 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6576 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6577 | `	/* Create a new array */` |
|        19 | 6578 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6579 | `	if( pArray == 0 ){` |
|       ! 0 | 6580 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6581 | `		return PH7_OK;` |
|         - | 6582 | `	}` |
|         - | 6583 | `	/* Perform the requested operation */` |
|        19 | 6584 | `	pEntry = pSrc->pFirst;` |
|        83 | 6585 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6586 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6587 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6588 | `		if( pNeedle ){` |
|        65 | 6589 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6590 | `		}` |
|        65 | 6591 | `		if( rc != SXRET_OK ){` |
|         - | 6592 | `			/* Perform the insertion */` |
|        37 | 6593 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6594 | `		}` |
|         - | 6595 | `		/* Point to the next entry */` |
|        65 | 6596 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6597 | `	}` |
|         - | 6598 | `	/* Return the freshly created array */` |
|        19 | 6599 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6600 | `	return PH7_OK;` |
|        14 | 6601 | `}` |
|         - | 6602 | `/*` |
|         - | 6603 | ` * array array_flip(array $input)` |
|         - | 6604 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6605 | ` * Parameter` |
|         - | 6606 | ` *  $input` |
|         - | 6607 | ` *   Input array.` |
|         - | 6608 | ` * Return` |
|         - | 6609 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6610 | ` */` |
|        30 | 6611 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6612 | `{` |
|         - | 6613 | `	ph7_hashmap_node *pEntry;` |
|         - | 6614 | `	ph7_hashmap *pSrc;` |
|         - | 6615 | `	ph7_value *pArray;` |
|         - | 6616 | `	ph7_value *pKey;` |
|         - | 6617 | `	ph7_value sVal;` |
|         - | 6618 | `	sxu32 n;` |
|         - | 6619 |  |
|         - | 6620 | `	/* PHP requires exactly one argument */` |
|        33 | 6621 | `	if( nArg != 1 ){` |
|         - | 6622 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6623 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6624 | `			"ArgumentCountError",` |
|         - | 6625 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6626 | `			nArg` |
|         - | 6627 | `			);` |
|         - | 6628 | `	}` |
|         - | 6629 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6630 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6631 | `		/* Type mismatch -> TypeError */` |
|         4 | 6632 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6633 | `			"TypeError",` |
|         - | 6634 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6635 | `			ph7_type_name(apArg[0])` |
|         - | 6636 | `			);` |
|         - | 6637 | `	}` |
|         - | 6638 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6639 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6640 | `	/* Create a new array */` |
|        27 | 6641 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6642 | `	if( pArray == 0 ){` |
|       ! 0 | 6643 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6644 | `		return PH7_OK;` |
|         - | 6645 | `	}` |
|         - | 6646 | `	/* Start processing */` |
|        27 | 6647 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6648 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6649 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6650 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6651 | `		if( pKey ){` |
|         - | 6652 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6653 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6654 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6655 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6656 | `					);` |
|     22236 | 6657 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6658 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6659 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6660 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6661 | `				}else{` |
|         - | 6662 | `					SyString sStr;` |
|      2227 | 6663 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6664 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6665 | `				}` |
|         - | 6666 | `				/* Perform the insertion */` |
|     22227 | 6667 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6668 | `				/* Safely release the value because each inserted entry` |
|         - | 6669 | `				 * has its own private copy of the value.` |
|         - | 6670 | `				 */` |
|     22227 | 6671 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6672 | `			}else{` |
|         - | 6673 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6674 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6675 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6676 | `					);` |
|         - | 6677 | `			}` |
|     11118 | 6678 | `		}` |
|         - | 6679 | `		/* Point to the next entry */` |
|     22237 | 6680 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6681 | `	}` |
|         - | 6682 | `	/* Return the freshly created array */` |
|        27 | 6683 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6684 | `	return PH7_OK;` |
|        18 | 6685 | `}` |
|         - | 6686 | `/*` |
|         - | 6687 | ` * number array_sum(array $array )` |
|         - | 6688 | ` *  Calculate the sum of values in an array.` |
|         - | 6689 | ` * Parameters` |
|         - | 6690 | ` *  $array: The input array.` |
|         - | 6691 | ` * Return` |
|         - | 6692 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6693 | ` */` |
|        24 | 6694 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6695 | `{` |
|         - | 6696 | `	ph7_hashmap_node *pEntry;` |
|         - | 6697 | `	ph7_value *pObj;` |
|        26 | 6698 | `	double dSum = 0;` |
|         - | 6699 | `	sxu32 n;` |
|        26 | 6700 | `	pEntry = pMap->pFirst;` |
|        92 | 6701 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6702 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6703 | `		if( pObj ){` |
|        68 | 6704 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6705 | `				dSum += pObj->rVal;` |
|        54 | 6706 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6707 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6708 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6709 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6710 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6711 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6712 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6713 | `						"Addition is not supported on type string");` |
|        14 | 6714 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6715 | `					double dv = 0;` |
|        13 | 6716 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6717 | `					dSum += dv;` |
|         8 | 6718 | `				}` |
|        12 | 6719 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6720 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6721 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6722 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6723 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6724 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6725 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6726 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6727 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6728 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6729 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6730 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6731 | `			}` |
|         - | 6732 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6733 | `		}` |
|         - | 6734 | `		/* Point to the next entry */` |
|        68 | 6735 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6736 | `	}` |
|         - | 6737 | `	/* Return sum */` |
|        26 | 6738 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6739 | `}` |
|       688 | 6740 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6741 | `{` |
|         - | 6742 | `	ph7_hashmap_node *pEntry;` |
|         - | 6743 | `	ph7_value *pObj;` |
|       690 | 6744 | `	sxi64 nSum = 0;` |
|         - | 6745 | `	sxu32 n;` |
|       690 | 6746 | `	pEntry = pMap->pFirst;` |
|      4702 | 6747 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6748 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6749 | `		if( pObj ){` |
|      4014 | 6750 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6751 | `				nSum += pObj->x.iVal;` |
|      2018 | 6752 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6753 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6754 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6755 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6756 | `						"Addition is not supported on type string");` |
|        10 | 6757 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6758 | `					sxi64 nv = 0;` |
|         8 | 6759 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6760 | `					nSum += nv;` |
|         5 | 6761 | `				}` |
|        17 | 6762 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6763 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6764 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6765 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6766 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6767 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6768 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6769 | `					"Addition is not supported on type %s",` |
|         2 | 6770 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6771 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6772 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6773 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6774 | `			}` |
|         - | 6775 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6776 | `		}` |
|         - | 6777 | `		/* Point to the next entry */` |
|      4014 | 6778 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6779 | `	}` |
|         - | 6780 | `	/* Return sum */` |
|       690 | 6781 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6782 | `}` |
|         - | 6783 | `/* number array_sum(array $array )` |
|         - | 6784 | ` * (See block-coment above)` |
|         - | 6785 | ` */` |
|       724 | 6786 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6787 | `{` |
|         - | 6788 | `	ph7_hashmap_node *pEntry;` |
|         - | 6789 | `	ph7_hashmap *pMap;` |
|         - | 6790 | `	ph7_value *pObj;` |
|       728 | 6791 | `	int useDouble = 0;` |
|         - | 6792 | `	sxu32 n;` |
|         - | 6793 | `	/* PHP requires exactly one argument */` |
|       728 | 6794 | `	if( nArg != 1 ){` |
|         4 | 6795 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6796 | `			"ArgumentCountError",` |
|         - | 6797 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6798 | `			nArg` |
|         - | 6799 | `			);` |
|         - | 6800 | `	}` |
|         - | 6801 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6802 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6803 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6804 | `		char zBuf[64];` |
|         8 | 6805 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6806 | `			"TypeError",` |
|         - | 6807 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6808 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6809 | `			);` |
|         - | 6810 | `	}` |
|       720 | 6811 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6812 | `	if( pMap->nEntry < 1 ){` |
|         - | 6813 | `		/* Nothing to compute,return 0 */` |
|         7 | 6814 | `		ph7_result_int(pCtx,0);` |
|         7 | 6815 | `		return PH7_OK;` |
|         - | 6816 | `	}` |
|         - | 6817 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6818 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6819 | `	 */` |
|       714 | 6820 | `	pEntry = pMap->pFirst;` |
|      4734 | 6821 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6822 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6823 | `		if( pObj ){` |
|      4046 | 6824 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6825 | `				useDouble = 1;` |
|        20 | 6826 | `				break;` |
|         - | 6827 | `			}` |
|      4028 | 6828 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6829 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6830 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6831 | `				sxu32 i;` |
|        32 | 6832 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6833 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6834 | `						useDouble = 1;` |
|         7 | 6835 | `						break;` |
|         - | 6836 | `					}` |
|         9 | 6837 | `				}` |
|        18 | 6838 | `				if( useDouble ){` |
|         7 | 6839 | `					break;` |
|         - | 6840 | `				}` |
|         5 | 6841 | `			}` |
|      2010 | 6842 | `		}` |
|      4022 | 6843 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6844 | `	}` |
|       714 | 6845 | `	if( useDouble ){` |
|        26 | 6846 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6847 | `	}else{` |
|       690 | 6848 | `		Int64Sum(pCtx,pMap);` |
|         - | 6849 | `	}` |
|       714 | 6850 | `	return PH7_OK;` |
|       366 | 6851 | `}` |
|         - | 6852 | `/*` |
|         - | 6853 | ` * number array_product(array $array )` |
|         - | 6854 | ` *  Calculate the product of values in an array.` |
|         - | 6855 | ` * Parameters` |
|         - | 6856 | ` *  $array: The input array.` |
|         - | 6857 | ` * Return` |
|         - | 6858 | ` *  Returns the product of values as an integer or float.` |
|         - | 6859 | ` */` |
|         2 | 6860 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6861 | `{` |
|         - | 6862 | `	ph7_hashmap_node *pEntry;` |
|         - | 6863 | `	ph7_value *pObj;` |
|         - | 6864 | `	double dProd;` |
|         - | 6865 | `	sxu32 n;` |
|         3 | 6866 | `	pEntry = pMap->pFirst;` |
|         3 | 6867 | `	dProd = 1;` |
|         7 | 6868 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6869 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6870 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6871 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6872 | `				dProd *= pObj->rVal;` |
|         4 | 6873 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6874 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6875 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6876 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6877 | `					double dv = 0;` |
|       ! 0 | 6878 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6879 | `					dProd *= dv;` |
|       ! 0 | 6880 | `				}` |
|       ! 0 | 6881 | `			}` |
|         2 | 6882 | `		}` |
|         - | 6883 | `		/* Point to the next entry */` |
|         5 | 6884 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6885 | `	}` |
|         - | 6886 | `	/* Return product */` |
|         3 | 6887 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6888 | `}` |
|         2 | 6889 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6890 | `{` |
|         - | 6891 | `	ph7_hashmap_node *pEntry;` |
|         - | 6892 | `	ph7_value *pObj;` |
|         - | 6893 | `	sxi64 nProd;` |
|         - | 6894 | `	sxu32 n;` |
|         3 | 6895 | `	pEntry = pMap->pFirst;` |
|         3 | 6896 | `	nProd = 1;` |
|         9 | 6897 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6898 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6899 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6900 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6901 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6902 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6903 | `				nProd *= pObj->x.iVal;` |
|         3 | 6904 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6905 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6906 | `					sxi64 nv = 0;` |
|       ! 0 | 6907 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6908 | `					nProd *= nv;` |
|       ! 0 | 6909 | `				}` |
|       ! 0 | 6910 | `			}` |
|         3 | 6911 | `		}` |
|         - | 6912 | `		/* Point to the next entry */` |
|         7 | 6913 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6914 | `	}` |
|         - | 6915 | `	/* Return product */` |
|         3 | 6916 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6917 | `}` |
|         - | 6918 | `/* number array_product(array $array )` |
|         - | 6919 | ` * (See block-block comment above)` |
|         - | 6920 | ` */` |
|        16 | 6921 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6922 | `{` |
|         - | 6923 | `	ph7_hashmap *pMap;` |
|         - | 6924 | `	ph7_value *pObj;` |
|        17 | 6925 | `	if( nArg < 1 ){` |
|         - | 6926 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6927 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6928 | `		return PH7_OK;` |
|         - | 6929 | `	}` |
|         - | 6930 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 6931 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6932 | `		char zBuf[64];` |
|        16 | 6933 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6934 | `			"TypeError",` |
|         - | 6935 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 6936 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6937 | `			);` |
|         - | 6938 | `	}` |
|         7 | 6939 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6940 | `	if( pMap->nEntry < 1 ){` |
|         - | 6941 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6942 | `		ph7_result_int(pCtx,1);` |
|         3 | 6943 | `		return PH7_OK;` |
|         - | 6944 | `	}` |
|         - | 6945 | `	/* If the first element is of type float,then perform floating` |
|         - | 6946 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6947 | `	 */` |
|         5 | 6948 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6949 | `	if( pObj == 0 ){` |
|       ! 0 | 6950 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6951 | `		return PH7_OK;` |
|         - | 6952 | `	}` |
|         5 | 6953 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6954 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6955 | `	}else{` |
|         3 | 6956 | `		Int64Prod(pCtx,pMap);` |
|         - | 6957 | `	}` |
|         5 | 6958 | `	return PH7_OK;` |
|         9 | 6959 | `}` |
|         - | 6960 | `/*` |
|         - | 6961 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6962 | ` *  Pick one or more random entries out of an array.` |
|         - | 6963 | ` * Parameters` |
|         - | 6964 | ` * $input` |
|         - | 6965 | ` *  The input array.` |
|         - | 6966 | ` * $num_req` |
|         - | 6967 | ` *  Specifies how many entries you want to pick.` |
|         - | 6968 | ` * Return` |
|         - | 6969 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6970 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6971 | ` *  NULL is returned on failure.` |
|         - | 6972 | ` */` |
|        36 | 6973 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6974 | `{` |
|         - | 6975 | `	ph7_hashmap_node *pNode;` |
|         - | 6976 | `	ph7_hashmap *pMap;` |
|        37 | 6977 | `	int nItem = 1;` |
|        37 | 6978 | `	if( nArg < 1 ){` |
|         - | 6979 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6980 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6981 | `		return PH7_OK;` |
|         - | 6982 | `	}` |
|         - | 6983 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 6984 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6985 | `		char zBuf[64];` |
|        10 | 6986 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6987 | `			"TypeError",` |
|         - | 6988 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6989 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6990 | `			);` |
|         - | 6991 | `	}` |
|         - | 6992 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6993 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 6994 | `	if( nArg > 1 ){` |
|        23 | 6995 | `		ph7_value *pNum = apArg[1];` |
|        22 | 6996 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 6997 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6998 | `			char zBuf[64];` |
|       ! 0 | 6999 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7000 | `				"TypeError",` |
|         - | 7001 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 7002 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 7003 | `				);` |
|         - | 7004 | `		}` |
|        23 | 7005 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 7006 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 7007 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 7008 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 7009 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 7010 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 7011 | `			int len;` |
|         9 | 7012 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 7013 | `			sxi64 iLong; double dReal;` |
|         9 | 7014 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 7015 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 7016 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7017 | `					"TypeError",` |
|         - | 7018 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 7019 | `					);` |
|         - | 7020 | `			}` |
|         - | 7021 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 7022 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 7023 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 7024 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 7025 | `			}` |
|         3 | 7026 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 7027 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 7028 | `			nItem = (int)iLong;` |
|         2 | 7029 | `		}else{` |
|        15 | 7030 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 7031 | `		}` |
|         8 | 7032 | `	}` |
|         - | 7033 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 7034 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7035 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 7036 | `	if( pMap->nEntry < 1 ){` |
|         5 | 7037 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7038 | `			"ValueError",` |
|         - | 7039 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7040 | `			);` |
|         - | 7041 | `	}` |
|         - | 7042 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7043 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7044 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7045 | `			"ValueError",` |
|         - | 7046 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7047 | `			);` |
|         - | 7048 | `	}` |
|        13 | 7049 | `	if( nItem < 2 ){` |
|         - | 7050 | `		sxu32 nEntry;` |
|         - | 7051 | `		/* Select a random number */` |
|         9 | 7052 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7053 | `		/* Extract the desired entry.` |
|         - | 7054 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7055 | `		 */` |
|         9 | 7056 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         4 | 7057 | `			pNode = pMap->pLast;` |
|         4 | 7058 | `			nEntry = pMap->nEntry - nEntry;` |
|         4 | 7059 | `			if( nEntry > 1 ){` |
|       ! 0 | 7060 | `				for(;;){` |
|       ! 0 | 7061 | `					if( nEntry == 0 ){` |
|       ! 0 | 7062 | `						break;` |
|         - | 7063 | `					}` |
|         - | 7064 | `					/* Point to the previous entry */` |
|       ! 0 | 7065 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7066 | `					nEntry--;` |
|       ! 0 | 7067 | `				}` |
|       ! 0 | 7068 | `			}` |
|         3 | 7069 | `		}else{` |
|         5 | 7070 | `			pNode = pMap->pFirst;` |
|         4 | 7071 | `			for(;;){` |
|         8 | 7072 | `				if( nEntry == 0 ){` |
|         5 | 7073 | `					break;` |
|         - | 7074 | `				}` |
|         - | 7075 | `				/* Point to the next entry */` |
|         3 | 7076 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         3 | 7077 | `				nEntry--;` |
|       ! 0 | 7078 | `			}` |
|         - | 7079 | `		}` |
|         9 | 7080 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7081 | `			/* Int key */` |
|         7 | 7082 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7083 | `		}else{` |
|         - | 7084 | `			/* Blob key */` |
|         3 | 7085 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7086 | `		}` |
|         5 | 7087 | `	}else{` |
|         - | 7088 | `		ph7_value sKey,*pArray;` |
|         - | 7089 | `		ph7_hashmap *pDest;` |
|         - | 7090 | `		/* Create a new array */` |
|         5 | 7091 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7092 | `		if( pArray == 0 ){` |
|       ! 0 | 7093 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7094 | `			return PH7_OK;` |
|         - | 7095 | `		}` |
|         - | 7096 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7097 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7098 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7099 | `		/* Copy the first n items */` |
|         5 | 7100 | `		pNode = pMap->pFirst;` |
|         5 | 7101 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7102 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7103 | `		}` |
|        15 | 7104 | `		while( nItem > 0){` |
|        11 | 7105 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7106 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7107 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7108 | `			/* Point to the next entry */` |
|        11 | 7109 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7110 | `			nItem--;` |
|         1 | 7111 | `		}` |
|         - | 7112 | `		/* Shuffle the array */` |
|         5 | 7113 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7114 | `		/* Rehash node */` |
|         5 | 7115 | `		HashmapSortRehash(pDest);` |
|         - | 7116 | `		/* Return the random array */` |
|         5 | 7117 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7118 | `	}` |
|        13 | 7119 | `	return PH7_OK;` |
|        19 | 7120 | `}` |
|         - | 7121 | `/*` |
|         - | 7122 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7123 | ` *  Split an array into chunks.` |
|         - | 7124 | ` * Parameters` |
|         - | 7125 | ` * $input` |
|         - | 7126 | ` *   The array to work on` |
|         - | 7127 | ` * $size` |
|         - | 7128 | ` *   The size of each chunk` |
|         - | 7129 | ` * $preserve_keys` |
|         - | 7130 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7131 | ` *   the chunk numerically.` |
|         - | 7132 | ` * Return` |
|         - | 7133 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7134 | ` *  zero, with each dimension containing size elements.` |
|         - | 7135 | ` */` |
|        36 | 7136 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7137 | `{` |
|         - | 7138 | `	ph7_value *pArray,*pChunk;` |
|         - | 7139 | `	ph7_hashmap_node *pEntry;` |
|         - | 7140 | `	ph7_hashmap *pMap;` |
|         - | 7141 | `	int bPreserve;` |
|         - | 7142 | `	sxu32 nChunk;` |
|         - | 7143 | `	sxu32 nSize;` |
|         - | 7144 | `	sxu32 n;` |
|         - | 7145 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7146 | `	if( nArg < 2 ){` |
|         - | 7147 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7148 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7149 | `			"ArgumentCountError",` |
|         - | 7150 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7151 | `			nArg` |
|         - | 7152 | `			);` |
|         - | 7153 | `	}` |
|        41 | 7154 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7155 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7156 | `			"TypeError",` |
|         - | 7157 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7158 | `			ph7_type_name(apArg[0])` |
|         - | 7159 | `			);` |
|         - | 7160 | `	}` |
|         - | 7161 | `	/* Create a new array */` |
|        38 | 7162 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7163 | `	if( pArray == 0 ){` |
|       ! 0 | 7164 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7165 | `		return PH7_OK;` |
|         - | 7166 | `	}` |
|         - | 7167 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7168 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7169 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7170 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7171 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7172 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7173 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7174 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7175 | `			"TypeError",` |
|         - | 7176 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7177 | `			ph7_type_name(apArg[1])` |
|         - | 7178 | `			);` |
|         - | 7179 | `	}` |
|         - | 7180 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7181 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7182 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7183 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7184 | `		int len;` |
|         3 | 7185 | `		sxu8 bReal = FALSE;` |
|         3 | 7186 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7187 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7188 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7189 | `				"TypeError",` |
|         - | 7190 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7191 | `				);` |
|         - | 7192 | `		}` |
|       ! 0 | 7193 | `		if( bReal ){` |
|         - | 7194 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7195 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7196 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7197 | `				zStr` |
|         - | 7198 | `				);` |
|       ! 0 | 7199 | `		}` |
|       ! 0 | 7200 | `	}` |
|         - | 7201 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7202 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7203 | `	 * later via ph7_value_to_int. */` |
|        35 | 7204 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7205 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7206 | `		sxi64 i = (sxi64)d;` |
|         3 | 7207 | `		if( d != (double)i ){` |
|         4 | 7208 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7209 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7210 | `				d` |
|         - | 7211 | `				);` |
|         1 | 7212 | `		}` |
|         1 | 7213 | `	}` |
|         - | 7214 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7215 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7216 | `	{` |
|        35 | 7217 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7218 | `		if( nSizeSigned < 1 ){` |
|         - | 7219 | `			/* size <= 0 -> ValueError */` |
|         6 | 7220 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7221 | `				"ValueError",` |
|         - | 7222 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7223 | `				);` |
|         - | 7224 | `		}` |
|        29 | 7225 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7226 | `	}` |
|        29 | 7227 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7228 | `		/* Return the whole array */` |
|         3 | 7229 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7230 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7231 | `		return PH7_OK;` |
|         - | 7232 | `	}` |
|        27 | 7233 | `	bPreserve = 0;` |
|        27 | 7234 | `	if( nArg > 2 ){` |
|         - | 7235 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7236 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7237 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7238 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7239 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7240 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7241 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7242 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7243 | `				"TypeError",` |
|         - | 7244 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7245 | `				ph7_type_name(apArg[2])` |
|         - | 7246 | `				);` |
|         - | 7247 | `		}` |
|        21 | 7248 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7249 | `	}` |
|         - | 7250 | `	/* Start processing */` |
|        27 | 7251 | `	pEntry = pMap->pFirst;` |
|        27 | 7252 | `	nChunk = 0;` |
|        27 | 7253 | `	pChunk = 0;` |
|        27 | 7254 | `	n = pMap->nEntry;` |
|        56 | 7255 | `	for( ;; ){` |
|       113 | 7256 | `		if( n < 1 ){` |
|         - | 7257 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7258 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7259 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7260 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7261 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7262 | `			 * exists. */` |
|        27 | 7263 | `			if( pChunk ){` |
|        27 | 7264 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7265 | `			}` |
|        27 | 7266 | `			break;` |
|         - | 7267 | `		}` |
|        87 | 7268 | `		if( nChunk < 1 ){` |
|        71 | 7269 | `			if( pChunk ){` |
|         - | 7270 | `				/* Put the first chunk */` |
|        45 | 7271 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7272 | `			}` |
|         - | 7273 | `			/* Create a new dimension */` |
|        71 | 7274 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7275 | `												   * will be automatically released as soon we return` |
|         - | 7276 | `												   * from this function */` |
|        71 | 7277 | `			if( pChunk == 0 ){` |
|       ! 0 | 7278 | `				break;` |
|         - | 7279 | `			}` |
|        71 | 7280 | `			nChunk = nSize;` |
|        35 | 7281 | `		}` |
|         - | 7282 | `		/* Insert the entry */` |
|        87 | 7283 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7284 | `		/* Point to the next entry */` |
|        87 | 7285 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7286 | `		nChunk--;` |
|        87 | 7287 | `		n--;` |
|         1 | 7288 | `	}` |
|         - | 7289 | `	/* Return the multidimensional array */` |
|        27 | 7290 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7291 | `	return PH7_OK;` |
|        23 | 7292 | `}` |
|         - | 7293 | `/*` |
|         - | 7294 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7295 | ` *  Pad array to the specified length with a value.` |
|         - | 7296 | ` * $input` |
|         - | 7297 | ` *   Initial array of values to pad.` |
|         - | 7298 | ` * $pad_size` |
|         - | 7299 | ` *   New size of the array.` |
|         - | 7300 | ` * $pad_value` |
|         - | 7301 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7302 | ` */` |
|         - | 7303 | `/*` |
|         - | 7304 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7305 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7306 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7307 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7308 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7309 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7310 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7311 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7312 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7313 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7314 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7315 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7316 | ` */` |
|        50 | 7317 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7318 | `	ph7_context *pCtx,` |
|         - | 7319 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7320 | `	int iArg,              /* 1-based argument position */` |
|         - | 7321 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7322 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7323 | `	)` |
|         1 | 7324 | `{` |
|        51 | 7325 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7326 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7327 | `			"ValueError",` |
|         - | 7328 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7329 | `			zFunc,iArg,zParam` |
|         - | 7330 | `			);` |
|         - | 7331 | `	}` |
|        37 | 7332 | `	return SXRET_OK;` |
|        26 | 7333 | `}` |
|        62 | 7334 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7335 | `{` |
|         - | 7336 | `	ph7_hashmap *pMap;` |
|         - | 7337 | `	ph7_value *pArray;` |
|         - | 7338 | `	sxi64 iLen,iAbs;` |
|         - | 7339 | `	int nEntry;` |
|         - | 7340 | `	sxi32 rc;` |
|        65 | 7341 | `	if( nArg != 3 ){` |
|         4 | 7342 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7343 | `			"ArgumentCountError",` |
|         - | 7344 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7345 | `			nArg` |
|         - | 7346 | `			);` |
|         - | 7347 | `	}` |
|        62 | 7348 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7349 | `		char zBuf[64];` |
|        11 | 7350 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7351 | `			"TypeError",` |
|         - | 7352 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7353 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7354 | `			);` |
|         - | 7355 | `	}` |
|         - | 7356 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7357 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7358 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7359 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7360 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7361 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7362 | `		char zBuf[64];` |
|       ! 0 | 7363 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7364 | `			"TypeError",` |
|         - | 7365 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7366 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7367 | `			);` |
|         - | 7368 | `	}` |
|        55 | 7369 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7370 | `		int nStr;` |
|        11 | 7371 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7372 | `		sxi64 iLong; double dReal;` |
|        11 | 7373 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7374 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7375 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7376 | `				"TypeError",` |
|         - | 7377 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7378 | `				);` |
|         - | 7379 | `		}` |
|         7 | 7380 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7381 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7382 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7383 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7384 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7385 | `					"TypeError",` |
|         - | 7386 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7387 | `					);` |
|         - | 7388 | `			}` |
|         3 | 7389 | `			iLen = (sxi64)dReal;` |
|         3 | 7390 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7391 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7392 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7393 | `					zStr` |
|         - | 7394 | `					);` |
|       ! 0 | 7395 | `			}` |
|         2 | 7396 | `		}else{` |
|         5 | 7397 | `			iLen = iLong;` |
|         - | 7398 | `		}` |
|         4 | 7399 | `	}else{` |
|        45 | 7400 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7401 | `	}` |
|         - | 7402 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7403 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7404 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7405 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7406 | `	 * overflow). */` |
|        51 | 7407 | `	iAbs = iLen;` |
|        51 | 7408 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7409 | `		iAbs = -iAbs;` |
|         7 | 7410 | `	}` |
|        51 | 7411 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7412 | `	if( rc != SXRET_OK ){` |
|        15 | 7413 | `		return rc;` |
|         - | 7414 | `	}` |
|        37 | 7415 | `	nEntry = (int)iLen;` |
|         - | 7416 | `	/* Create a new array */` |
|        37 | 7417 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7418 | `	if( pArray == 0 ){` |
|       ! 0 | 7419 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7420 | `	}` |
|        37 | 7421 | `	if( nEntry < 0 ){` |
|        11 | 7422 | `		nEntry = -nEntry;` |
|        11 | 7423 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7424 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7425 | `			/* Insert given items first */` |
|        25 | 7426 | `			while( nEntry > 0 ){` |
|        19 | 7427 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7428 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7429 | `				}` |
|        19 | 7430 | `				nEntry--;` |
|         1 | 7431 | `			}` |
|         - | 7432 | `			/* Merge the two arrays */` |
|         7 | 7433 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7434 | `		}else{` |
|         5 | 7435 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7436 | `		}` |
|        32 | 7437 | `	}else if( nEntry > 0 ){` |
|        25 | 7438 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7439 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7440 | `			/* Merge the two arrays first */` |
|        19 | 7441 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7442 | `			/* Insert given items */` |
|       275 | 7443 | `			while( nEntry > 0 ){` |
|       257 | 7444 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7445 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7446 | `				}` |
|       257 | 7447 | `				nEntry--;` |
|         1 | 7448 | `			}` |
|        10 | 7449 | `		}else{` |
|         7 | 7450 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7451 | `		}` |
|        13 | 7452 | `	}else{` |
|         - | 7453 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7454 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7455 | `	}` |
|         - | 7456 | `	/* Return the new array */` |
|        37 | 7457 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7458 | `	return PH7_OK;` |
|        34 | 7459 | `}` |
|         - | 7460 | `/*` |
|         - | 7461 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7462 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7463 | ` * Parameters` |
|         - | 7464 | ` * $array` |
|         - | 7465 | ` *   The array in which elements are replaced.` |
|         - | 7466 | ` * $array1` |
|         - | 7467 | ` *   The array from which elements will be extracted.` |
|         - | 7468 | ` * ....` |
|         - | 7469 | ` *  More arrays from which elements will be extracted.` |
|         - | 7470 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7471 | ` * Return` |
|         - | 7472 | ` *  Returns an array.` |
|         - | 7473 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7474 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7475 | ` */` |
|        20 | 7476 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7477 | `{` |
|         - | 7478 | `	ph7_hashmap *pMap;` |
|         - | 7479 | `	ph7_value *pArray;` |
|         - | 7480 | `	int i;` |
|        23 | 7481 | `	if( nArg < 1 ){` |
|       ! 0 | 7482 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7483 | `			"ArgumentCountError",` |
|         - | 7484 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7485 | `			);` |
|         - | 7486 | `	}` |
|        23 | 7487 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7488 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7489 | `			"TypeError",` |
|         - | 7490 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7491 | `			ph7_type_name(apArg[0])` |
|         - | 7492 | `			);` |
|         - | 7493 | `	}` |
|         - | 7494 | `	/* Create a new array */` |
|        20 | 7495 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7496 | `	if( pArray == 0 ){` |
|       ! 0 | 7497 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7498 | `		return PH7_OK;` |
|         - | 7499 | `	}` |
|         - | 7500 | `	/* Overwrite from the first array */` |
|        20 | 7501 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7502 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7503 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7504 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7505 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7506 | `			/* Type mismatch -> TypeError */` |
|         4 | 7507 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7508 | `				"TypeError",` |
|         - | 7509 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7510 | `				i + 1,` |
|         2 | 7511 | `				ph7_type_name(apArg[i])` |
|         - | 7512 | `				);` |
|         - | 7513 | `		}` |
|         - | 7514 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7515 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7516 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7517 | `	}` |
|         - | 7518 | `	/* Return the new array */` |
|        17 | 7519 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7520 | `	return PH7_OK;` |
|        13 | 7521 | `}` |
|         - | 7522 | `/*` |
|         - | 7523 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7524 | ` *  Filters elements of an array using a callback function.` |
|         - | 7525 | ` * Parameters` |
|         - | 7526 | ` *  $input` |
|         - | 7527 | ` *    The array to iterate over` |
|         - | 7528 | ` * $callback` |
|         - | 7529 | ` *    The callback function to use` |
|         - | 7530 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7531 | ` *    will be removed.` |
|         - | 7532 | ` * Return` |
|         - | 7533 | ` *  The filtered array.` |
|         - | 7534 | ` */` |
|        30 | 7535 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7536 | `{` |
|         - | 7537 | `	ph7_hashmap_node *pEntry;` |
|         - | 7538 | `	ph7_hashmap *pMap;` |
|         - | 7539 | `	ph7_value *pArray;` |
|         - | 7540 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7541 | `	ph7_value *pValue;` |
|         - | 7542 | `	sxi32 rc;` |
|         - | 7543 | `	int keep;` |
|         - | 7544 | `	sxu32 n;` |
|        32 | 7545 | `	if( nArg < 1 ){` |
|         - | 7546 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7547 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7548 | `		return PH7_OK;` |
|         - | 7549 | `	}` |
|         - | 7550 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7551 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7552 | `		char zBuf[64];` |
|        19 | 7553 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7554 | `			"TypeError",` |
|         - | 7555 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7556 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7557 | `			);` |
|         - | 7558 | `	}` |
|         - | 7559 | `	/* Create a new array */` |
|        20 | 7560 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7561 | `	if( pArray == 0 ){` |
|       ! 0 | 7562 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7563 | `		return PH7_OK;` |
|         - | 7564 | `	}` |
|         - | 7565 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7566 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7567 | `	pEntry = pMap->pFirst;` |
|        20 | 7568 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7569 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7570 | `	/* Perform the requested operation */` |
|        78 | 7571 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7572 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7573 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7574 | `		if( pValue == 0 ){` |
|         - | 7575 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7576 | `			keep = FALSE;` |
|        64 | 7577 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7578 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7579 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7580 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7581 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7582 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7583 | `					int len;` |
|         3 | 7584 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7585 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7586 | `						"TypeError",` |
|         - | 7587 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7588 | `						zName` |
|         - | 7589 | `						);` |
|       ! 0 | 7590 | `				}else{` |
|       ! 0 | 7591 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7592 | `						"TypeError",` |
|         - | 7593 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7594 | `						ph7_type_name(apArg[1])` |
|         - | 7595 | `						);` |
|         - | 7596 | `				}` |
|         - | 7597 | `			}` |
|        33 | 7598 | `			keep = FALSE;` |
|        33 | 7599 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7600 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7601 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7602 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7603 | `				return PH7_EXCEPTION;` |
|         - | 7604 | `			}` |
|        31 | 7605 | `			if( rc == SXRET_OK ){` |
|         - | 7606 | `				/* Perform a boolean cast */` |
|        31 | 7607 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7608 | `			}` |
|        31 | 7609 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7610 | `		}else{` |
|         - | 7611 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7612 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7613 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7614 | `			 */` |
|        29 | 7615 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7616 | `		}` |
|        59 | 7617 | `		if( keep ){` |
|         - | 7618 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7619 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7620 | `		}` |
|         - | 7621 | `		/* Point to the next entry */` |
|        59 | 7622 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7623 | `	}` |
|        15 | 7624 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7625 | `	return PH7_OK;` |
|        17 | 7626 | `}` |
|         - | 7627 | `/*` |
|         - | 7628 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7629 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7630 | ` * Parameters` |
|         - | 7631 | ` *  $callback` |
|         - | 7632 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7633 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7634 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7635 | ` *   are zipped together.` |
|         - | 7636 | ` *  $array` |
|         - | 7637 | ` *   The first array to run through the callback function.` |
|         - | 7638 | ` *  $arrays` |
|         - | 7639 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7640 | ` * Return` |
|         - | 7641 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7642 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7643 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7644 | ` *  padding shorter arrays with NULL.` |
|         - | 7645 | ` */` |
|        62 | 7646 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7647 | `{` |
|         - | 7648 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7649 | `	ph7_hashmap_node *pEntry;` |
|         - | 7650 | `	ph7_hashmap *pMap;` |
|         - | 7651 | `	ph7_vm *pVm;` |
|         - | 7652 | `	int bNullCallback;` |
|         - | 7653 | `	sxi32 rc;` |
|         - | 7654 | `	int i;` |
|         - | 7655 | `	sxu32 n;` |
|        66 | 7656 | `	if( nArg < 2 ){` |
|       ! 0 | 7657 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7658 | `			"ArgumentCountError",` |
|         - | 7659 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7660 | `			nArg` |
|         - | 7661 | `			);` |
|         - | 7662 | `	}` |
|        66 | 7663 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        66 | 7664 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         8 | 7665 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         6 | 7666 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         8 | 7667 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7668 | `				"TypeError",` |
|         - | 7669 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7670 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7671 | `				zFunc` |
|         - | 7672 | `				);` |
|         - | 7673 | `		}` |
|         3 | 7674 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7675 | `			"TypeError",` |
|         - | 7676 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7677 | `			"no array or string given"` |
|         - | 7678 | `			);` |
|         - | 7679 | `	}` |
|         - | 7680 | `	/* Every remaining argument must be an array */` |
|       125 | 7681 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        71 | 7682 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7683 | `			if( i == 1 ){` |
|         4 | 7684 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7685 | `					"TypeError",` |
|         - | 7686 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7687 | `					ph7_type_name(apArg[1])` |
|         - | 7688 | `					);` |
|         - | 7689 | `			}` |
|       ! 0 | 7690 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7691 | `				"TypeError",` |
|         - | 7692 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7693 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7694 | `				);` |
|         - | 7695 | `		}` |
|        36 | 7696 | `	}` |
|        57 | 7697 | `	pVm = pCtx->pVm;` |
|         - | 7698 | `	/* Create a new array */` |
|        57 | 7699 | `	pArray = ph7_context_new_array(pCtx);` |
|        57 | 7700 | `	if( pArray == 0 ){` |
|       ! 0 | 7701 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7702 | `		return PH7_OK;` |
|         - | 7703 | `	}` |
|        57 | 7704 | `	PH7_MemObjInit(pVm,&sResult);` |
|        57 | 7705 | `	PH7_MemObjInit(pVm,&sKey);` |
|        57 | 7706 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7707 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        57 | 7708 | `	if( nArg == 2 ){` |
|         - | 7709 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        47 | 7710 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        47 | 7711 | `		pEntry = pMap->pFirst;` |
|       143 | 7712 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7713 | `			/* Extract the node value */` |
|       103 | 7714 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|       103 | 7715 | `			if( pValue ){` |
|         - | 7716 | `				/* Extract the node key */` |
|       103 | 7717 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       103 | 7718 | `				if( bNullCallback ){` |
|         - | 7719 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7720 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7721 | `				}else{` |
|         - | 7722 | `					/* Invoke the supplied callback */` |
|        93 | 7723 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        93 | 7724 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7725 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7726 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7727 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7728 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7729 | `						return PH7_EXCEPTION;` |
|         - | 7730 | `					}` |
|         - | 7731 | `					/* Insert the callback return value */` |
|        89 | 7732 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7733 | `				}` |
|        99 | 7734 | `				PH7_MemObjRelease(&sKey);` |
|        99 | 7735 | `				PH7_MemObjRelease(&sResult);` |
|        48 | 7736 | `			}` |
|         - | 7737 | `			/* Point to the next entry */` |
|        99 | 7738 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        51 | 7739 | `		}` |
|        23 | 7740 | `	}else{` |
|         - | 7741 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7742 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7743 | `		int nArrays = nArg - 1;` |
|         - | 7744 | `		ph7_hashmap_node **apCur;` |
|         - | 7745 | `		ph7_value **apCallArg;` |
|         - | 7746 | `		ph7_value sNull;` |
|        11 | 7747 | `		sxu32 nMax = 0;` |
|        11 | 7748 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7749 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7750 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7751 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7752 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7753 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7754 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7755 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7756 | `			return PH7_OK;` |
|         - | 7757 | `		}` |
|        11 | 7758 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7759 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7760 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7761 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7762 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7763 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7764 | `				nMax = pMap->nEntry;` |
|         6 | 7765 | `			}` |
|        12 | 7766 | `		}` |
|        35 | 7767 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7768 | `			ph7_value *pZip = 0;` |
|        25 | 7769 | `			if( bNullCallback ){` |
|         - | 7770 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7771 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7772 | `			}` |
|        79 | 7773 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7774 | `				ph7_value *pv = &sNull;` |
|        55 | 7775 | `				if( apCur[i] ){` |
|        53 | 7776 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7777 | `					if( pNodeVal ){` |
|        53 | 7778 | `						pv = pNodeVal;` |
|        26 | 7779 | `					}` |
|        53 | 7780 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7781 | `				}` |
|        55 | 7782 | `				if( bNullCallback ){` |
|         9 | 7783 | `					if( pZip ){` |
|         9 | 7784 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7785 | `					}` |
|         5 | 7786 | `				}else{` |
|        47 | 7787 | `					apCallArg[i] = pv;` |
|         - | 7788 | `				}` |
|        28 | 7789 | `			}` |
|        25 | 7790 | `			if( bNullCallback ){` |
|         5 | 7791 | `				if( pZip ){` |
|         5 | 7792 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7793 | `				}` |
|         3 | 7794 | `			}else{` |
|        21 | 7795 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7796 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7797 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7798 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7799 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7800 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7801 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7802 | `					return PH7_EXCEPTION;` |
|         - | 7803 | `				}` |
|        21 | 7804 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7805 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7806 | `			}` |
|        13 | 7807 | `		}` |
|        11 | 7808 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7809 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7810 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7811 | `	}` |
|        53 | 7812 | `	PH7_MemObjRelease(&sKey);` |
|        53 | 7813 | `	PH7_MemObjRelease(&sResult);` |
|        53 | 7814 | `	ph7_result_value(pCtx,pArray);` |
|        53 | 7815 | `	return PH7_OK;` |
|        35 | 7816 | `}` |
|         - | 7817 | `/*` |
|         - | 7818 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7819 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7820 | ` * Parameters` |
|         - | 7821 | ` *  $array` |
|         - | 7822 | ` *   The input array.` |
|         - | 7823 | ` *  $callback` |
|         - | 7824 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7825 | ` *  $initial` |
|         - | 7826 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7827 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7828 | ` * Return` |
|         - | 7829 | ` *  Returns the resulting value.` |
|         - | 7830 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7831 | ` */` |
|        30 | 7832 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7833 | `{` |
|         - | 7834 | `	ph7_hashmap_node *pEntry;` |
|         - | 7835 | `	ph7_hashmap *pMap;` |
|         - | 7836 | `	ph7_value *pValue;` |
|         - | 7837 | `	ph7_value sResult;` |
|         - | 7838 | `	sxi32 rc;` |
|         - | 7839 | `	sxu32 n;` |
|        35 | 7840 | `	if( nArg < 2 ){` |
|       ! 0 | 7841 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7842 | `			"ArgumentCountError",` |
|         - | 7843 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7844 | `			nArg` |
|         - | 7845 | `			);` |
|         - | 7846 | `	}` |
|        35 | 7847 | `	if( nArg > 3 ){` |
|         4 | 7848 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7849 | `			"ArgumentCountError",` |
|         - | 7850 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7851 | `			nArg` |
|         - | 7852 | `			);` |
|         - | 7853 | `	}` |
|        33 | 7854 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7855 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7856 | `			"TypeError",` |
|         - | 7857 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7858 | `			ph7_type_name(apArg[0])` |
|         - | 7859 | `			);` |
|         - | 7860 | `	}` |
|        31 | 7861 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7862 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7863 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7864 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7865 | `				"TypeError",` |
|         - | 7866 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7867 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7868 | `				zFunc` |
|         - | 7869 | `				);` |
|         - | 7870 | `		}` |
|         9 | 7871 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7872 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7873 | `				"TypeError",` |
|         - | 7874 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7875 | `				"array callback must have exactly two members"` |
|         - | 7876 | `				);` |
|         - | 7877 | `		}` |
|         6 | 7878 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7879 | `			"TypeError",` |
|         - | 7880 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7881 | `			"no array or string given"` |
|         - | 7882 | `			);` |
|         - | 7883 | `	}` |
|         - | 7884 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7885 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7886 | `	/* Assume a NULL initial value */` |
|        19 | 7887 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7888 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7889 | `	if( nArg > 2 ){` |
|         - | 7890 | `		/* Set the initial value */` |
|        13 | 7891 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7892 | `	}` |
|         - | 7893 | `	/* Perform the requested operation */` |
|        19 | 7894 | `	pEntry = pMap->pFirst;` |
|        55 | 7895 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7896 | `		/* Extract the node value */` |
|        39 | 7897 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7898 | `		/* Invoke the supplied callback */` |
|        39 | 7899 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7900 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7901 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7902 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7903 | `			return PH7_EXCEPTION;` |
|         - | 7904 | `		}` |
|         - | 7905 | `		/* Point to the next entry */` |
|        37 | 7906 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7907 | `	}` |
|        17 | 7908 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7909 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7910 | `	return PH7_OK;` |
|        20 | 7911 | `}` |
|         - | 7912 | `/*` |
|         - | 7913 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7914 | ` *  Apply a user function to every member of an array.` |
|         - | 7915 | ` * Parameters` |
|         - | 7916 | ` *  $array` |
|         - | 7917 | ` *   The input array.` |
|         - | 7918 | ` *  $funcname` |
|         - | 7919 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7920 | ` *   the first, and the key/index second.` |
|         - | 7921 | ` * Note:` |
|         - | 7922 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7923 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7924 | ` *  be made in the original array itself.` |
|         - | 7925 | ` *  $userdata` |
|         - | 7926 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7927 | ` *   to the callback funcname.` |
|         - | 7928 | ` * Return` |
|         - | 7929 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7930 | ` */` |
|        36 | 7931 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7932 | `{` |
|         - | 7933 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7934 | `	ph7_hashmap_node *pEntry;` |
|         - | 7935 | `	ph7_hashmap *pMap;` |
|         - | 7936 | `	sxu32 n;` |
|        41 | 7937 | `	if( nArg < 2 ){` |
|       ! 0 | 7938 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7939 | `			"ArgumentCountError",` |
|         - | 7940 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7941 | `			nArg` |
|         - | 7942 | `			);` |
|         - | 7943 | `	}` |
|        41 | 7944 | `	if( nArg > 3 ){` |
|         4 | 7945 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7946 | `			"ArgumentCountError",` |
|         - | 7947 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7948 | `			nArg` |
|         - | 7949 | `			);` |
|         - | 7950 | `	}` |
|        39 | 7951 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7952 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7953 | `			"TypeError",` |
|         - | 7954 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7955 | `			ph7_type_name(apArg[0])` |
|         - | 7956 | `			);` |
|         - | 7957 | `	}` |
|        37 | 7958 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        17 | 7959 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         6 | 7960 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         8 | 7961 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7962 | `				"TypeError",` |
|         - | 7963 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7964 | `				"function \"%s\" not found or invalid function name",` |
|         2 | 7965 | `				zFunc` |
|         - | 7966 | `				);` |
|         - | 7967 | `		}` |
|        12 | 7968 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7969 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7970 | `				"TypeError",` |
|         - | 7971 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7972 | `				"array callback must have exactly two members"` |
|         - | 7973 | `				);` |
|         - | 7974 | `		}` |
|         6 | 7975 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7976 | `			"TypeError",` |
|         - | 7977 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7978 | `			"no array or string given"` |
|         - | 7979 | `			);` |
|         - | 7980 | `	}` |
|        21 | 7981 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7982 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7983 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7984 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7985 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7986 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7987 | `	/* Perform the desired operation */` |
|        21 | 7988 | `	pEntry = pMap->pFirst;` |
|        61 | 7989 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7990 | `		/* Extract the node value */` |
|        43 | 7991 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7992 | `		if( pValue ){` |
|         - | 7993 | `			sxi32 rcW;` |
|         - | 7994 | `			/* Extract the entry key */` |
|        43 | 7995 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7996 | `			/* Invoke the supplied callback */` |
|        43 | 7997 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7998 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7999 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 8000 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 8001 | `				return PH7_EXCEPTION;` |
|         - | 8002 | `			}` |
|        20 | 8003 | `		}` |
|         - | 8004 | `		/* Point to the next entry */` |
|        41 | 8005 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 8006 | `	}` |
|         - | 8007 | `	/* All done, return TRUE */` |
|        19 | 8008 | `	ph7_result_bool(pCtx,1);` |
|        19 | 8009 | `	return PH7_OK;` |
|        23 | 8010 | `}` |
|         - | 8011 | `/*` |
|         - | 8012 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 8013 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 8014 | ` */` |
|        22 | 8015 | `static sxi32 HashmapWalkRecursive(` |
|         - | 8016 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 8017 | `	ph7_value *pCallback, /* User callback */` |
|         - | 8018 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 8019 | `	int iNest             /* Nesting level */` |
|         - | 8020 | `	)` |
|         1 | 8021 | `{` |
|         - | 8022 | `	ph7_hashmap_node *pEntry;` |
|         - | 8023 | `	ph7_value *pValue,sKey;` |
|         - | 8024 | `	sxi32 rc;` |
|         - | 8025 | `	sxu32 n;` |
|         - | 8026 | `	/* Iterate through hashmap entries */` |
|        23 | 8027 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 8028 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 8029 | `	pEntry = pMap->pFirst;` |
|        59 | 8030 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8031 | `		/* Extract the node value */` |
|        37 | 8032 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 8033 | `		if( pValue ){` |
|        37 | 8034 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 8035 | `				if( iNest < 32 ){` |
|         - | 8036 | `					/* Recurse */` |
|        11 | 8037 | `					iNest++;` |
|        11 | 8038 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8039 | `					iNest--;` |
|        11 | 8040 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8041 | `						return PH7_EXCEPTION;` |
|         - | 8042 | `					}` |
|         5 | 8043 | `				}` |
|         6 | 8044 | `			}else{` |
|         - | 8045 | `				/* Extract the node key */` |
|        27 | 8046 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8047 | `				/* Invoke the supplied callback */` |
|        27 | 8048 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8049 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8050 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8051 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8052 | `					return PH7_EXCEPTION;` |
|         - | 8053 | `				}` |
|         - | 8054 | `			}` |
|        18 | 8055 | `		}` |
|         - | 8056 | `		/* Point to the next entry */` |
|        37 | 8057 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8058 | `	}` |
|        23 | 8059 | `	return PH7_OK;` |
|        12 | 8060 | `}` |
|         - | 8061 | `/*` |
|         - | 8062 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8063 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8064 | ` * Parameters` |
|         - | 8065 | ` *  $array` |
|         - | 8066 | ` *   The input array.` |
|         - | 8067 | ` *  $funcname` |
|         - | 8068 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8069 | ` *   the first, and the key/index second.` |
|         - | 8070 | ` * Note:` |
|         - | 8071 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8072 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8073 | ` *  be made in the original array itself.` |
|         - | 8074 | ` *  $userdata` |
|         - | 8075 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8076 | ` *   to the callback funcname.` |
|         - | 8077 | ` * Return` |
|         - | 8078 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8079 | ` */` |
|        26 | 8080 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8081 | `{` |
|         - | 8082 | `	ph7_hashmap *pMap;` |
|        31 | 8083 | `	if( nArg < 2 ){` |
|       ! 0 | 8084 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8085 | `			"ArgumentCountError",` |
|         - | 8086 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8087 | `			nArg` |
|         - | 8088 | `			);` |
|         - | 8089 | `	}` |
|        31 | 8090 | `	if( nArg > 3 ){` |
|         4 | 8091 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8092 | `			"ArgumentCountError",` |
|         - | 8093 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8094 | `			nArg` |
|         - | 8095 | `			);` |
|         - | 8096 | `	}` |
|        29 | 8097 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8098 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8099 | `			"TypeError",` |
|         - | 8100 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8101 | `			ph7_type_name(apArg[0])` |
|         - | 8102 | `			);` |
|         - | 8103 | `	}` |
|        27 | 8104 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8105 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8106 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8107 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8108 | `				"TypeError",` |
|         - | 8109 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8110 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8111 | `				zFunc` |
|         - | 8112 | `				);` |
|         - | 8113 | `		}` |
|        12 | 8114 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8115 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8116 | `				"TypeError",` |
|         - | 8117 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8118 | `				"array callback must have exactly two members"` |
|         - | 8119 | `				);` |
|         - | 8120 | `		}` |
|         6 | 8121 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8122 | `			"TypeError",` |
|         - | 8123 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8124 | `			"no array or string given"` |
|         - | 8125 | `			);` |
|         - | 8126 | `	}` |
|         - | 8127 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8128 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8129 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8130 | `	/* Perform the desired operation */` |
|        13 | 8131 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8132 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8133 | `		return PH7_EXCEPTION;` |
|         - | 8134 | `	}` |
|         - | 8135 | `	/* All done, return TRUE */` |
|        13 | 8136 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8137 | `	return PH7_OK;` |
|        18 | 8138 | `}` |
|         - | 8139 | `/*` |
|         - | 8140 | ` * bool array_is_list(array $array)` |
|         - | 8141 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8142 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8143 | ` * Return` |
|         - | 8144 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8145 | ` */` |
|         - | 8146 | `/*` |
|         - | 8147 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8148 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8149 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8150 | ` */` |
|       278 | 8151 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8152 | `{` |
|       279 | 8153 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       279 | 8154 | `	sxi64 iExpect = 0;` |
|         - | 8155 | `	sxu32 n;` |
|       599 | 8156 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       445 | 8157 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8158 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       125 | 8159 | `			return 0;` |
|         - | 8160 | `		}` |
|       321 | 8161 | `		++iExpect;` |
|       321 | 8162 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       161 | 8163 | `	}` |
|       155 | 8164 | `	return 1;` |
|       140 | 8165 | `}` |
|        12 | 8166 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8167 | `{` |
|        13 | 8168 | `	if( nArg < 1 ){` |
|       ! 0 | 8169 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8170 | `			"ArgumentCountError",` |
|         - | 8171 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8172 | `			);` |
|         - | 8173 | `	}` |
|        13 | 8174 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8175 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8176 | `			"TypeError",` |
|         - | 8177 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8178 | `			ph7_type_name(apArg[0])` |
|         - | 8179 | `			);` |
|         - | 8180 | `	}` |
|        13 | 8181 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8182 | `	return PH7_OK;` |
|         7 | 8183 | `}` |
|         - | 8184 | `/*` |
|         - | 8185 | ` * mixed array_first(array $array)` |
|         - | 8186 | ` * mixed array_last(array $array)` |
|         - | 8187 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8188 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8189 | ` *  untouched (unlike reset()/end()).` |
|         - | 8190 | ` */` |
|        18 | 8191 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8192 | `{` |
|         - | 8193 | `	ph7_hashmap *pMap;` |
|         - | 8194 | `	ph7_hashmap_node *pNode;` |
|         - | 8195 | `	ph7_value *pVal;` |
|        19 | 8196 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8197 | `	if( nArg < 1 ){` |
|       ! 0 | 8198 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8199 | `			"ArgumentCountError",` |
|         - | 8200 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8201 | `			zName` |
|         - | 8202 | `			);` |
|         - | 8203 | `	}` |
|        19 | 8204 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8205 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8206 | `			"TypeError",` |
|         - | 8207 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8208 | `			zName,` |
|         1 | 8209 | `			ph7_type_name(apArg[0])` |
|         - | 8210 | `			);` |
|         - | 8211 | `	}` |
|        17 | 8212 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8213 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8214 | `	if( pNode == 0 ){` |
|         - | 8215 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8216 | `		ph7_result_null(pCtx);` |
|         5 | 8217 | `		return PH7_OK;` |
|         - | 8218 | `	}` |
|        13 | 8219 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8220 | `	if( pVal ){` |
|        13 | 8221 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8222 | `	}else{` |
|       ! 0 | 8223 | `		ph7_result_null(pCtx);` |
|         - | 8224 | `	}` |
|        13 | 8225 | `	return PH7_OK;` |
|        10 | 8226 | `}` |
|         8 | 8227 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8228 | `{` |
|         9 | 8229 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8230 | `}` |
|        10 | 8231 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8232 | `{` |
|        11 | 8233 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8234 | `}` |
|         - | 8235 | `/*` |
|         - | 8236 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8237 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8238 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8239 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8240 | ` *  untouched.` |
|         - | 8241 | ` */` |
|        22 | 8242 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8243 | `{` |
|         - | 8244 | `	ph7_hashmap *pMap;` |
|         - | 8245 | `	ph7_hashmap_node *pNode;` |
|        23 | 8246 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8247 | `	if( nArg < 1 ){` |
|       ! 0 | 8248 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8249 | `			"ArgumentCountError",` |
|         - | 8250 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8251 | `			zName` |
|         - | 8252 | `			);` |
|         - | 8253 | `	}` |
|        23 | 8254 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8255 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8256 | `			"TypeError",` |
|         - | 8257 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8258 | `			zName,` |
|         1 | 8259 | `			ph7_type_name(apArg[0])` |
|         - | 8260 | `			);` |
|         - | 8261 | `	}` |
|        21 | 8262 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8263 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8264 | `	if( pNode == 0 ){` |
|         - | 8265 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8266 | `		ph7_result_null(pCtx);` |
|         5 | 8267 | `		return PH7_OK;` |
|         - | 8268 | `	}` |
|        17 | 8269 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8270 | `	return PH7_OK;` |
|        12 | 8271 | `}` |
|        10 | 8272 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8273 | `{` |
|        11 | 8274 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8275 | `}` |
|        12 | 8276 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8277 | `{` |
|        13 | 8278 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8279 | `}` |
|         - | 8280 | `/*` |
|         - | 8281 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8282 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8283 | ` * array_column() for both the column value and the index key.` |
|         - | 8284 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8285 | ` * container or the key is absent.` |
|         - | 8286 | ` */` |
|        32 | 8287 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8288 | `{` |
|        33 | 8289 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8290 | `		ph7_hashmap_node *pNode;` |
|        25 | 8291 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8292 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8293 | `		}` |
|        11 | 8294 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8295 | `		ph7_value sName;` |
|         - | 8296 | `		const char *zName;` |
|         - | 8297 | `		ph7_value *pAttr;` |
|         - | 8298 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8299 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8300 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8301 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8302 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8303 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8304 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8305 | `		return pAttr;` |
|         - | 8306 | `	}` |
|         5 | 8307 | `	return 0;` |
|        17 | 8308 | `}` |
|         - | 8309 | `/*` |
|         - | 8310 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8311 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8312 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8313 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8314 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8315 | ` *  Each row may be an array or an object.` |
|         - | 8316 | ` */` |
|        12 | 8317 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8318 | `{` |
|         - | 8319 | `	ph7_hashmap_node *pNode;` |
|         - | 8320 | `	ph7_hashmap *pMap;` |
|         - | 8321 | `	ph7_value *pArray;` |
|         - | 8322 | `	ph7_value *pRow;` |
|         - | 8323 | `	ph7_value *pCol;` |
|         - | 8324 | `	ph7_value *pIdx;` |
|         - | 8325 | `	int bWantCol;` |
|         - | 8326 | `	int bWantIdx;` |
|         - | 8327 | `	sxu32 n;` |
|        13 | 8328 | `	if( nArg < 2 ){` |
|       ! 0 | 8329 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8330 | `			"ArgumentCountError",` |
|         - | 8331 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8332 | `			nArg` |
|         - | 8333 | `			);` |
|         - | 8334 | `	}` |
|        13 | 8335 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8336 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8337 | `			"TypeError",` |
|         - | 8338 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8339 | `			ph7_type_name(apArg[0])` |
|         - | 8340 | `			);` |
|         - | 8341 | `	}` |
|        13 | 8342 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8343 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8344 | `	if( pArray == 0 ){` |
|       ! 0 | 8345 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8346 | `		return PH7_OK;` |
|         - | 8347 | `	}` |
|         - | 8348 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8349 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8350 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8351 | `	pNode = pMap->pFirst;` |
|        33 | 8352 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8353 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8354 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8355 | `		if( pRow == 0 ){` |
|       ! 0 | 8356 | `			continue;` |
|         - | 8357 | `		}` |
|        21 | 8358 | `		if( bWantCol ){` |
|        19 | 8359 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8360 | `			if( pCol == 0 ){` |
|         - | 8361 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8362 | `				continue;` |
|         - | 8363 | `			}` |
|         9 | 8364 | `		}else{` |
|         3 | 8365 | `			pCol = pRow;` |
|         - | 8366 | `		}` |
|        19 | 8367 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8368 | `		if( pIdx ){` |
|        13 | 8369 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8370 | `		}else{` |
|         7 | 8371 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8372 | `		}` |
|        10 | 8373 | `	}` |
|        13 | 8374 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8375 | `	return PH7_OK;` |
|         7 | 8376 | `}` |
|         - | 8377 | `/*` |
|         - | 8378 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8379 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8380 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8381 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8382 | ` */` |
|        28 | 8383 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8384 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8385 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8386 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8387 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8388 | `	)` |
|         1 | 8389 | `{` |
|         - | 8390 | `	ph7_hashmap_node *pEntry;` |
|         - | 8391 | `	ph7_hashmap *pMap;` |
|         - | 8392 | `	ph7_value *pValue;` |
|         - | 8393 | `	ph7_value *apCbArg[2];` |
|         - | 8394 | `	ph7_value sKey;` |
|         - | 8395 | `	ph7_value sResult;` |
|         - | 8396 | `	sxi32 rc;` |
|         - | 8397 | `	sxu32 n;` |
|        29 | 8398 | `	*ppMatch = 0;` |
|        29 | 8399 | `	if( nArg < 2 ){` |
|       ! 0 | 8400 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8401 | `			"ArgumentCountError",` |
|         - | 8402 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8403 | `			zName,nArg` |
|         - | 8404 | `			);` |
|         - | 8405 | `	}` |
|        29 | 8406 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8407 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8408 | `			"TypeError",` |
|         - | 8409 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8410 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8411 | `			);` |
|         - | 8412 | `	}` |
|        29 | 8413 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8414 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8415 | `			"TypeError",` |
|         - | 8416 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8417 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8418 | `			);` |
|         - | 8419 | `	}` |
|        29 | 8420 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8421 | `	pEntry = pMap->pFirst;` |
|        29 | 8422 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8423 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8424 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8425 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8426 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8427 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8428 | `		if( pValue ){` |
|         - | 8429 | `			/* The callback receives ($value, $key). */` |
|        59 | 8430 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8431 | `			apCbArg[0] = pValue;` |
|        59 | 8432 | `			apCbArg[1] = &sKey;` |
|        59 | 8433 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8434 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8435 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8436 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8437 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8438 | `				return PH7_EXCEPTION;` |
|         - | 8439 | `			}` |
|        59 | 8440 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8441 | `				*ppMatch = pEntry;` |
|        15 | 8442 | `				break;` |
|         - | 8443 | `			}` |
|        22 | 8444 | `		}` |
|        45 | 8445 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8446 | `	}` |
|        29 | 8447 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8448 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8449 | `	return PH7_OK;` |
|        15 | 8450 | `}` |
|         - | 8451 | `/*` |
|         - | 8452 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8453 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8454 | ` *  is truthy, or NULL if none match.` |
|         - | 8455 | ` */` |
|         6 | 8456 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8457 | `{` |
|         - | 8458 | `	ph7_hashmap_node *pMatch;` |
|         - | 8459 | `	ph7_value *pVal;` |
|         - | 8460 | `	sxi32 rc;` |
|         7 | 8461 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8462 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8463 | `		return rc;` |
|         - | 8464 | `	}` |
|         7 | 8465 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8466 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8467 | `	}else{` |
|         3 | 8468 | `		ph7_result_null(pCtx);` |
|         - | 8469 | `	}` |
|         7 | 8470 | `	return PH7_OK;` |
|         4 | 8471 | `}` |
|         - | 8472 | `/*` |
|         - | 8473 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8474 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8475 | ` *  is truthy, or NULL if none match.` |
|         - | 8476 | ` */` |
|         6 | 8477 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8478 | `{` |
|         - | 8479 | `	ph7_hashmap_node *pMatch;` |
|         - | 8480 | `	sxi32 rc;` |
|         7 | 8481 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8482 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8483 | `		return rc;` |
|         - | 8484 | `	}` |
|         7 | 8485 | `	if( pMatch == 0 ){` |
|         3 | 8486 | `		ph7_result_null(pCtx);` |
|         6 | 8487 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8488 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8489 | `	}else{` |
|         4 | 8490 | `		ph7_result_string(pCtx,` |
|         2 | 8491 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8492 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8493 | `	}` |
|         7 | 8494 | `	return PH7_OK;` |
|         4 | 8495 | `}` |
|         - | 8496 | `/*` |
|         - | 8497 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8498 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8499 | ` *  FALSE for an empty array.` |
|         - | 8500 | ` */` |
|         8 | 8501 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8502 | `{` |
|         - | 8503 | `	ph7_hashmap_node *pMatch;` |
|         - | 8504 | `	sxi32 rc;` |
|         9 | 8505 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8506 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8507 | `		return rc;` |
|         - | 8508 | `	}` |
|         9 | 8509 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8510 | `	return PH7_OK;` |
|         5 | 8511 | `}` |
|         - | 8512 | `/*` |
|         - | 8513 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8514 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8515 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8516 | ` */` |
|         8 | 8517 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8518 | `{` |
|         - | 8519 | `	ph7_hashmap_node *pMatch;` |
|         - | 8520 | `	sxi32 rc;` |
|         9 | 8521 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8522 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8523 | `		return rc;` |
|         - | 8524 | `	}` |
|         9 | 8525 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8526 | `	return PH7_OK;` |
|         5 | 8527 | `}` |
|         - | 8528 | `/*` |
|         - | 8529 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8530 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8531 | ` */` |
|         - | 8532 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8533 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8534 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8535 | `{` |
|        84 | 8536 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8537 | `	(void)pVm;` |
|        84 | 8538 | `	p->nCount++;` |
|        84 | 8539 | `	if( p->pArray ){` |
|         - | 8540 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8541 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8542 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8543 | `	}` |
|        84 | 8544 | `	return SXRET_OK;` |
|         4 | 8545 | `}` |
|         - | 8546 | `/*` |
|         - | 8547 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8548 | ` */` |
|        30 | 8549 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8550 | `{` |
|         - | 8551 | `	struct IterCollect sCol;` |
|         - | 8552 | `	ph7_value *pArray;` |
|         - | 8553 | `	sxi32 rc;` |
|        34 | 8554 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8555 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8556 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8557 | `	sCol.pArray = pArray;` |
|        34 | 8558 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8559 | `	sCol.nCount = 0;` |
|        34 | 8560 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8561 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8562 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8563 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8564 | `		sxu32 n;` |
|         9 | 8565 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8566 | `			ph7_value sKey, *pVal;` |
|         7 | 8567 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8568 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8569 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8570 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8571 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8572 | `			pEntry = pEntry->pPrev;` |
|         4 | 8573 | `		}` |
|         3 | 8574 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8575 | `		return PH7_OK;` |
|         - | 8576 | `	}` |
|        32 | 8577 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8578 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8579 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8580 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8581 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8582 | `			ph7_type_name(apArg[0]));` |
|         - | 8583 | `	}` |
|        30 | 8584 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8585 | `	return PH7_OK;` |
|        19 | 8586 | `}` |
|         - | 8587 | `/*` |
|         - | 8588 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8589 | ` */` |
|         8 | 8590 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8591 | `{` |
|         - | 8592 | `	struct IterCollect sCol;` |
|         - | 8593 | `	sxi32 rc;` |
|         9 | 8594 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8595 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8596 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8597 | `		return PH7_OK;` |
|         - | 8598 | `	}` |
|         7 | 8599 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8600 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8601 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8602 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8603 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8604 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8605 | `			ph7_type_name(apArg[0]));` |
|         - | 8606 | `	}` |
|         7 | 8607 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8608 | `	return PH7_OK;` |
|         5 | 8609 | `}` |
|         - | 8610 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8611 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8612 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8613 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8614 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8615 | `{` |
|        33 | 8616 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8617 | `	ph7_value sResult;` |
|         - | 8618 | `	SySet aArg;` |
|         - | 8619 | `	sxi32 rc;` |
|         - | 8620 | `	int bContinue;` |
|        16 | 8621 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8622 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8623 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8624 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8625 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8626 | `		sxu32 n;` |
|        17 | 8627 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8628 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8629 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8630 | `			pEntry = pEntry->pPrev;` |
|         5 | 8631 | `		}` |
|         4 | 8632 | `	}` |
|        33 | 8633 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8634 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8635 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8636 | `	SySetRelease(&aArg);` |
|        33 | 8637 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8638 | `	p->nCount++;` |
|        31 | 8639 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8640 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8641 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8642 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8643 | `}` |
|         - | 8644 | `/*` |
|         - | 8645 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8646 | ` */` |
|        12 | 8647 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8648 | `{` |
|         - | 8649 | `	struct IterApply sApp;` |
|         - | 8650 | `	sxi32 rc;` |
|        13 | 8651 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8652 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8653 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8654 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8655 | `	}` |
|        13 | 8656 | `	sApp.pCallback = apArg[1];` |
|        13 | 8657 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8658 | `	sApp.nCount = 0;` |
|        13 | 8659 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8660 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8661 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8662 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8663 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8664 | `			ph7_type_name(apArg[0]));` |
|         - | 8665 | `	}` |
|        11 | 8666 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8667 | `	return PH7_OK;` |
|         7 | 8668 | `}` |
|         - | 8669 | `/*` |
|         - | 8670 | ` * Table of hashmap functions.` |
|         - | 8671 | ` */` |
|         - | 8672 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8673 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8674 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8675 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8676 | `	{"count",             ph7_hashmap_count },` |
|         - | 8677 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8678 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8679 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8680 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8681 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8682 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8683 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8684 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8685 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8686 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8687 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8688 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8689 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8690 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8691 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8692 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8693 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8694 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8695 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8696 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8697 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8698 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8699 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8700 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8701 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8702 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8703 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8704 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8705 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8706 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8707 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8708 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8709 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8710 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8711 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8712 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8713 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8714 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8715 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8716 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8717 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8718 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8719 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8720 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8721 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8722 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8723 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8724 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8725 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8726 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8727 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8728 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8729 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8730 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8731 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8732 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8733 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8734 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8735 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8736 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8737 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8738 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8739 | `	{"current",           ph7_hashmap_current },` |
|         - | 8740 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8741 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8742 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8743 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8744 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8745 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8746 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8747 | `};` |
|         - | 8748 | `/*` |
|         - | 8749 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8750 | ` */` |
|      3356 | 8751 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8752 | `{` |
|         - | 8753 | `	sxu32 n;` |
|    251705 | 8754 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    248349 | 8755 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    124177 | 8756 | `	}` |
|      3361 | 8757 | `}` |
|         - | 8758 | `/*` |
|         - | 8759 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8760 | ` * the BLOB given as the first argument.` |
|         - | 8761 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8762 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8763 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8764 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8765 | ` */` |
|         - | 8766 | `/*` |
|         - | 8767 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8768 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8769 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8770 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8771 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8772 | ` */` |
|       120 | 8773 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8774 | `{` |
|       122 | 8775 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8776 | `	ph7_value *pObj;` |
|       122 | 8777 | `	sxu32 n = 0;` |
|         - | 8778 | `	int isRef;` |
|       122 | 8779 | `	sxi32 rc = SXRET_OK;` |
|         - | 8780 | `	int i;` |
|       195 | 8781 | `	for(;;){` |
|       392 | 8782 | `		if( n >= pMap->nEntry ){` |
|       122 | 8783 | `			break;` |
|         - | 8784 | `		}` |
|       272 | 8785 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8786 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8787 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       540 | 8788 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       270 | 8789 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       272 | 8790 | `		if( ShowType ){` |
|         - | 8791 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8792 | `			 * on the next line at the same indent (php). */` |
|       104 | 8793 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        70 | 8794 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        36 | 8795 | `			}` |
|        36 | 8796 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8797 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8798 | `			}else{` |
|        20 | 8799 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8800 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8801 | `			}` |
|        36 | 8802 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        36 | 8803 | `			if( pObj ){` |
|        36 | 8804 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        36 | 8805 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8806 | `					break;` |
|         - | 8807 | `				}` |
|        17 | 8808 | `			}` |
|        19 | 8809 | `		}else{` |
|         - | 8810 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8811 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8812 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8813 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8814 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8815 | `			}` |
|       238 | 8816 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8817 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8818 | `			}else{` |
|       170 | 8819 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8820 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8821 | `			}` |
|       236 | 8822 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8823 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8824 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8825 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8826 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8827 | `					break;` |
|         - | 8828 | `				}` |
|        13 | 8829 | `			}else{` |
|       214 | 8830 | `				if( pObj ){` |
|       214 | 8831 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8832 | `				}` |
|       214 | 8833 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8834 | `			}` |
|         - | 8835 | `		}` |
|         - | 8836 | `		/* Point to the next entry */` |
|       272 | 8837 | `		n++;` |
|       272 | 8838 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8839 | `	}` |
|       122 | 8840 | `	return rc;` |
|         2 | 8841 | `}` |
|       116 | 8842 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8843 | `{` |
|         - | 8844 | `	sxi32 rc;` |
|         - | 8845 | `	int i;` |
|       118 | 8846 | `	if( nDepth > 31 ){` |
|         - | 8847 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8848 | `		/* Nesting limit reached */` |
|       ! 0 | 8849 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8850 | `		return SXERR_LIMIT;` |
|         - | 8851 | `	}` |
|       118 | 8852 | `	if( ShowType ){` |
|         - | 8853 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8854 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8855 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8856 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8857 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8858 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8859 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8860 | `		}` |
|        14 | 8861 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8862 | `		return rc;` |
|         - | 8863 | `	}` |
|         - | 8864 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8865 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8866 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8867 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8868 | `	}` |
|       105 | 8869 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8870 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8871 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8872 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8873 | `	}` |
|       105 | 8874 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8875 | `	return rc;` |
|        60 | 8876 | `}` |
|         - | 8877 | `/*` |
|         - | 8878 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8879 | ` * retrieved entry.` |
|         - | 8880 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8881 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8882 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8883 | ` * a value different from PH7_OK.` |
|         - | 8884 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8885 | ` */` |
|     34078 | 8886 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8887 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8888 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8889 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8890 | `	)` |
|         5 | 8891 | `{` |
|         - | 8892 | `	ph7_hashmap_node *pEntry;` |
|         - | 8893 | `	ph7_value sKey,sValue;` |
|         - | 8894 | `	sxi32 rc;` |
|         - | 8895 | `	sxu32 n;` |
|         - | 8896 | `	/* Initialize walker parameter */` |
|     34083 | 8897 | `	rc = SXRET_OK;` |
|     34083 | 8898 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34083 | 8899 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34083 | 8900 | `	n = pMap->nEntry;` |
|     34083 | 8901 | `	pEntry = pMap->pFirst;` |
|         - | 8902 | `	/* Start the iteration process */` |
|     93082 | 8903 | `	for(;;){` |
|    186169 | 8904 | `		if( n < 1 ){` |
|     34083 | 8905 | `			break;` |
|         - | 8906 | `		}` |
|         - | 8907 | `		/* Extract a copy of the key and a copy the current value */` |
|    152091 | 8908 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    152091 | 8909 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8910 | `		/* Invoke the user callback */` |
|    152091 | 8911 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8912 | `		/* Release the copy of the key and the value */` |
|    152091 | 8913 | `		PH7_MemObjRelease(&sKey);` |
|    152091 | 8914 | `		PH7_MemObjRelease(&sValue);` |
|    152091 | 8915 | `		if( rc != PH7_OK ){` |
|         - | 8916 | `			/* Callback request an operation abort */` |
|       ! 0 | 8917 | `			return SXERR_ABORT;` |
|         - | 8918 | `		}` |
|         - | 8919 | `		/* Point to the next entry */` |
|    152091 | 8920 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    152091 | 8921 | `		n--;` |
|         5 | 8922 | `	}` |
|         - | 8923 | `	/* All done */` |
|     34083 | 8924 | `	return SXRET_OK;` |
|     17044 | 8925 | `}` |
|         - | 8926 |  |
