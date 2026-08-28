# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3989/4404 lines (90.58%)

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
|   7455618 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7455623 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7455623 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    642876 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    642881 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    642881 |   35 | `	sxu32 nH = 5381;` |
|    642881 |   36 | `	zEnd = &zIn[nLen];` |
|    728512 |   37 | `	for(;;){` |
|   1457029 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1240117 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1112341 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    969783 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    642881 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1712 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1717 |   54 | `	sxi64 iCount = 0;` |
|      1717 |   55 | `	if( !bRecursive ){` |
|      1543 |   56 | `		iCount = pMap->nEntry;` |
|       774 |   57 | `	}else{` |
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
|      1717 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3155268 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3155273 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3155273 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3155273 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3155273 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3155273 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3155273 |  112 | `	pNode->nHash = nHash;` |
|   3155273 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3155273 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3155273 |  115 | `	return pNode;` |
|   1577639 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    271678 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    271683 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    271683 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    271683 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    271683 |  133 | `	pNode->pMap  = &(*pMap);` |
|    271683 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    271683 |  135 | `	pNode->nHash = nHash;` |
|    271683 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    271683 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    271683 |  138 | `	pNode->nValIdx = nValIdx;` |
|    271683 |  139 | `	return pNode;` |
|    135844 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3426946 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3426951 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2944719 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2944719 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1472357 |  150 | `	}` |
|   3426951 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3426951 |  153 | `	if( pMap->pFirst == 0 ){` |
|     92049 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     92049 |  156 | `		pMap->pCur = pNode;` |
|     46027 |  157 | `	}else{` |
|   3334907 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3426951 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3426951 |  174 | `	++pMap->nEntry;` |
|   3426951 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7804 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7809 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7809 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7809 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7343 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3674 |  187 | `	}else{` |
|       468 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7809 |  190 | `	if( pNode->pNextCollide ){` |
|      5119 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2558 |  192 | `	}` |
|      7809 |  193 | `	if( pMap->pFirst == pNode ){` |
|       171 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        83 |  195 | `	}` |
|      7809 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       171 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        83 |  199 | `	}` |
|      7809 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7809 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7809 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       161 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       161 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       161 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|        78 |  218 | `		}` |
|        78 |  219 | `	}` |
|      7809 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7601 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3798 |  222 | `	}` |
|      7809 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7809 |  224 | `	pMap->nEntry--;` |
|      7809 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|        97 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        97 |  228 | `		pMap->apBucket = 0;` |
|        97 |  229 | `		pMap->nSize = 0;` |
|        97 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        46 |  231 | `	}` |
|      7809 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3426946 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3426951 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     97103 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     97103 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     97103 |  245 | `		if( nNew < 1 ){` |
|     92049 |  246 | `			nNew = 16;` |
|     46022 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     97103 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     97103 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     97103 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     97103 |  260 | `		pMap->apBucket = apNew;` |
|     97103 |  261 | `		pMap->nSize = nNew;` |
|     97103 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     92049 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5059 |  267 | `		pEntry = pMap->pFirst;` |
|      5059 |  268 | `		n = 0;` |
|   2107087 |  269 | `		for( ;; ){` |
|   4214179 |  270 | `			if( n >= pMap->nEntry ){` |
|      5059 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4209125 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4209125 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4209125 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3593187 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3593187 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1796591 |  280 | `			}` |
|   4209125 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4209125 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4209125 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5059 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2527 |  288 | `	}` |
|   3334907 |  289 | `	return SXRET_OK;` |
|   1713478 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3155268 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3155273 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3155235 |  310 | `		if( pValue ){` |
|   3155229 |  311 | `			sSafeVal = *pValue;` |
|   3155229 |  312 | `			pValue = &sSafeVal;` |
|   1577612 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3155235 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3155235 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3155235 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3155229 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1577612 |  322 | `		}` |
|   3155235 |  323 | `		nIdx = pObj->nIdx;` |
|   1577620 |  324 | `	}else{` |
|        39 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3155273 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3155273 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3155273 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3155273 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        39 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3155273 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3155273 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3155273 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3155273 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3155273 |  349 | `	return SXRET_OK;` |
|   1577639 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    271678 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    271683 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    224431 |  370 | `		if( pValue ){` |
|    224141 |  371 | `			sSafeVal = *pValue;` |
|    224141 |  372 | `			pValue = &sSafeVal;` |
|    112068 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    224431 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    224431 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    224431 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    224141 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    112068 |  382 | `		}` |
|    224431 |  383 | `		nIdx = pObj->nIdx;` |
|    112218 |  384 | `	}else{` |
|     47257 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    271683 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    271683 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    271683 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    271683 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     47257 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23626 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    271683 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    271683 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    271683 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    271683 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    271683 |  409 | `	return SXRET_OK;` |
|    135844 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4287124 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4287129 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       717 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4286417 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4286417 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110563739 |  433 | `	for(;;){` |
| 221127483 |  434 | `		if( pNode == 0 ){` |
|   4282163 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216845320 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216842308 |  438 | `			&& pNode->nHash == nHash` |
| 108421780 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      4259 |  441 | `				if( ppNode ){` |
|      4241 |  442 | `					*ppNode = pNode;` |
|      2118 |  443 | `				}` |
|      4259 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841067 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4282163 |  450 | `	return SXERR_NOTFOUND;` |
|   2143567 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    407202 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    407207 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     36009 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    371203 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    371203 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    306929 |  475 | `	for(;;){` |
|    613863 |  476 | `		if( pNode == 0 ){` |
|    312169 |  477 | `			break;` |
|         - |  478 | `		}` |
|    301694 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    300183 |  480 | `			&& pNode->nHash == nHash` |
|    178903 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     59139 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     59039 |  484 | `				if( ppNode ){` |
|     59011 |  485 | `					*ppNode = pNode;` |
|     29503 |  486 | `				}` |
|     59039 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    242665 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    312169 |  493 | `	return SXERR_NOTFOUND;` |
|    203606 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    407334 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    407339 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    407339 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    407339 |  504 | `	int isNeg = FALSE, nDigit;` |
|    407339 |  505 | `	if( zIn >= zEnd ){` |
|       ! 0 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    407339 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    407335 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    407335 |  516 | `	zDigit = zIn;` |
|    204099 |  517 | `	for(;;){` |
|    408203 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    407953 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    407085 |  523 | `			return FALSE;` |
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
|    203672 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    139844 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    139849 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    139849 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    135705 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast */` |
|       ! 0 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  559 | `		}` |
|    135705 |  560 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Perform a blob lookup */` |
|    135685 |  562 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    135685 |  563 | `			goto result;` |
|         - |  564 | `		}` |
|        10 |  565 | `	}` |
|         - |  566 | `	/* Perform an int lookup */` |
|      4169 |  567 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  568 | `		/* Force an integer cast */` |
|        35 |  569 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  570 | `	}` |
|         - |  571 | `	/* Perform an int lookup */` |
|      4169 |  572 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     69922 |  573 | `result:` |
|    139849 |  574 | `	if( rc == SXRET_OK ){` |
|         - |  575 | `		/* Node found */` |
|     62493 |  576 | `		if( ppNode ){` |
|     62443 |  577 | `			*ppNode = pNode;` |
|     31219 |  578 | `		}` |
|     62493 |  579 | `		return SXRET_OK;` |
|         - |  580 | `	}` |
|         - |  581 | `	/* No such entry */` |
|     77361 |  582 | `	return SXERR_NOTFOUND;` |
|     69927 |  583 | `}` |
|         - |  584 | `/*` |
|         - |  585 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  586 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  587 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  588 | ` * via HashmapAppendIndexBusy.` |
|         - |  589 | ` */` |
|   2141456 |  590 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  591 | `{` |
|   2141461 |  592 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141191 |  593 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  594 | `		/* Make sure the automatic index is not reserved */` |
|   2141191 |  595 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  596 | `			pMap->iNextIdx++;` |
|       ! 0 |  597 | `		}` |
|   1070593 |  598 | `	}` |
|   2141461 |  599 | `}` |
|         - |  600 | `/*` |
|         - |  601 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  602 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  603 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  604 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  605 | ` */` |
|   1013450 |  606 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  607 | `{` |
|   1013455 |  608 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  609 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  610 | `		return TRUE;` |
|         - |  611 | `	}` |
|   1013449 |  612 | `	return FALSE;` |
|    506730 |  613 | `}` |
|         - |  614 | `/*` |
|         - |  615 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  616 | ` * hashmap.` |
|         - |  617 | ` * If a node with the given key already exists in the database` |
|         - |  618 | ` * then this function overwrite the old value.` |
|         - |  619 | ` */` |
|   3379188 |  620 | `static sxi32 HashmapInsert(` |
|         - |  621 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  622 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  623 | `	ph7_value *pVal    /* Node value */` |
|         - |  624 | `	)` |
|         5 |  625 | `{` |
|   3379193 |  626 | `	ph7_hashmap_node *pNode = 0;` |
|   3379193 |  627 | `	sxi32 rc = SXRET_OK;` |
|   3379193 |  628 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    227885 |  629 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  630 | `			/* Force a string cast */` |
|         3 |  631 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  632 | `		}` |
|    227885 |  633 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3737 |  634 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  635 | `				/* Automatic index assign */` |
|      3509 |  636 | `				pKey = 0;` |
|      1752 |  637 | `			}` |
|      3737 |  638 | `			goto IntKey;` |
|         - |  639 | `		}` |
|    336227 |  640 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    112074 |  641 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  642 | `				/* Overwrite the old value */` |
|         - |  643 | `				ph7_value *pElem;` |
|       437 |  644 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       437 |  645 | `				if( pElem ){` |
|       437 |  646 | `					if( pVal ){` |
|       437 |  647 | `						PH7_MemObjStore(pVal,pElem);` |
|       220 |  648 | `					}else{` |
|         - |  649 | `						/* Nullify the entry */` |
|       ! 0 |  650 | `						PH7_MemObjToNull(pElem);` |
|         - |  651 | `					}` |
|       217 |  652 | `				}` |
|       437 |  653 | `				return SXRET_OK;` |
|         - |  654 | `		}` |
|    223719 |  655 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  656 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  657 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       131 |  658 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  659 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  660 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  661 | `				return SXRET_OK;` |
|         - |  662 | `			}` |
|       196 |  663 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       130 |  664 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        65 |  665 | `				pVal,SXU32_HIGH);` |
|         - |  666 | `		}` |
|         - |  667 | `		/* Perform a blob-key insertion */` |
|    223589 |  668 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    223589 |  669 | `		return rc;` |
|         - |  670 | `	}` |
|   1575654 |  671 | `IntKey:` |
|   3155045 |  672 | `	if( pKey ){` |
|   2141625 |  673 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  674 | `			/* Force an integer cast */` |
|       261 |  675 | `			PH7_MemObjToInteger(pKey);` |
|       130 |  676 | `		}` |
|   2141625 |  677 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  678 | `			/* Overwrite the old value */` |
|         - |  679 | `			ph7_value *pElem;` |
|       169 |  680 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       169 |  681 | `			if( pElem ){` |
|       169 |  682 | `				if( pVal ){` |
|       169 |  683 | `					PH7_MemObjStore(pVal,pElem);` |
|        85 |  684 | `				}else{` |
|         - |  685 | `					/* Nullify the entry */` |
|       ! 0 |  686 | `					PH7_MemObjToNull(pElem);` |
|         - |  687 | `				}` |
|        84 |  688 | `			}` |
|       169 |  689 | `			return SXRET_OK;` |
|         - |  690 | `		}` |
|   2141457 |  691 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  692 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  693 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  694 | `			char zKey[24];` |
|         3 |  695 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  696 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  697 | `		}` |
|         - |  698 | `		/* Perform a 64-bit-int-key insertion */` |
|   2141455 |  699 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2141455 |  700 | `		if( rc == SXRET_OK ){` |
|   2141455 |  701 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070725 |  702 | `		}` |
|   1070730 |  703 | `	}else{` |
|   1013425 |  704 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  705 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  706 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  707 | `		}` |
|   1013423 |  708 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  709 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  710 | `		}` |
|         - |  711 | `		/* Assign an automatic index */` |
|   1013417 |  712 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1013417 |  713 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1013415 |  714 | `			++pMap->iNextIdx;` |
|    506705 |  715 | `		}` |
|         - |  716 | `	}` |
|         - |  717 | `	/* Insertion result */` |
|   3154867 |  718 | `	return rc;` |
|   1689599 |  719 | `}` |
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
|     47300 |  747 | `static sxi32 HashmapInsertByRef(` |
|         - |  748 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  749 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  750 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  751 | `	)` |
|         5 |  752 | `{` |
|     47305 |  753 | `	ph7_hashmap_node *pNode = 0;` |
|     47305 |  754 | `	sxi32 rc = SXRET_OK;` |
|     47305 |  755 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     47269 |  756 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  757 | `			/* Force a string cast */` |
|       ! 0 |  758 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  759 | `		}` |
|     47269 |  760 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  761 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  762 | `				/* Automatic index assign */` |
|       ! 0 |  763 | `				pKey = 0;` |
|       ! 0 |  764 | `			}` |
|         3 |  765 | `			goto IntKey;` |
|         - |  766 | `		}` |
|     70898 |  767 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23631 |  768 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  769 | `				/* Overwrite */` |
|        11 |  770 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  771 | `				pNode->nValIdx = nRefIdx;` |
|         - |  772 | `				/* Install in the reference table */` |
|        11 |  773 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  774 | `				return SXRET_OK;` |
|         - |  775 | `		}` |
|         - |  776 | `		/* Perform a blob-key insertion */` |
|     47257 |  777 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     47257 |  778 | `		return rc;` |
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
|     23655 |  811 | `}` |
|         - |  812 | `/*` |
|         - |  813 | ` * Extract node value.` |
|         - |  814 | ` */` |
|   1437401 |  815 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  816 | `{` |
|         - |  817 | `	/* Point to the desired object */` |
|         - |  818 | `	ph7_value *pObj;` |
|   1437406 |  819 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1437406 |  820 | `	return pObj;` |
|         5 |  821 | `}` |
|         - |  822 | `/*` |
|         - |  823 | ` * Insert a node in the given hashmap.` |
|         - |  824 | ` * If a node with the given key already exists in the database` |
|         - |  825 | ` * then this function overwrite the old value.` |
|         - |  826 | ` */` |
|       460 |  827 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  828 | `{` |
|         - |  829 | `	ph7_value *pObj;` |
|         - |  830 | `	sxi32 rc;` |
|         - |  831 | `	/* Extract the node value */` |
|       465 |  832 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       465 |  833 | `	if( pObj == 0 ){` |
|       ! 0 |  834 | `		return SXERR_EMPTY;` |
|         - |  835 | `	}` |
|         - |  836 | `	/* Preserve key */` |
|       465 |  837 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  838 | `		/* Int64 key */` |
|       333 |  839 | `		if( !bPreserve ){` |
|         - |  840 | `			/* Assign an automatic index */` |
|       185 |  841 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        95 |  842 | `		}else{` |
|       149 |  843 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  844 | `		}` |
|       169 |  845 | `	}else{` |
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
|       465 |  856 | `	return rc;` |
|       235 |  857 | `}` |
|         - |  858 | `/*` |
|         - |  859 | ` * Compare two node values.` |
|         - |  860 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  861 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  862 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  863 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  864 | ` * documenation.` |
|         - |  865 | ` */` |
|     71508 |  866 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  867 | `{` |
|         - |  868 | `	ph7_value sObj1,sObj2;` |
|         - |  869 | `	sxi32 rc;` |
|     71513 |  870 | `	if( pLeft == pRight ){` |
|         - |  871 | `		/*` |
|         - |  872 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  873 | `		 * below for more information on this sceanario.` |
|         - |  874 | `		 */` |
|       ! 0 |  875 | `		return 0;` |
|         - |  876 | `	}` |
|         - |  877 | `	/* Do the comparison */` |
|     71513 |  878 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71513 |  879 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71513 |  880 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71513 |  881 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71513 |  882 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71513 |  883 | `	PH7_MemObjRelease(&sObj1);` |
|     71513 |  884 | `	PH7_MemObjRelease(&sObj2);` |
|     71513 |  885 | `	return rc;` |
|     35784 |  886 | `}` |
|         - |  887 | `/*` |
|         - |  888 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  889 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  890 | ` */` |
|     13938 |  891 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  892 | `{` |
|     13943 |  893 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  894 | `	sxu32 nBucket;` |
|         - |  895 | `	/* Remove old collision links */` |
|     13943 |  896 | `	if( pEntry->pPrevCollide ){` |
|     11313 |  897 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5672 |  898 | `	}else{` |
|      2635 |  899 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  900 | `	}` |
|     13943 |  901 | `	if( pEntry->pNextCollide ){` |
|      1123 |  902 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       578 |  903 | `	}` |
|     13943 |  904 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  905 | `	/* Compute the new hash */` |
|     13943 |  906 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13943 |  907 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13943 |  908 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  909 | `	/* Link to the new bucket */` |
|     13943 |  910 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13943 |  911 | `	if( pMap->apBucket[nBucket] ){` |
|     11640 |  912 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5831 |  913 | `	}` |
|     13943 |  914 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13943 |  915 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  916 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  917 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  918 | `	 * the no-overflow invariant uniform). */` |
|     13943 |  919 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13943 |  920 | `		pMap->iNextIdx++;` |
|      6969 |  921 | `	}` |
|     13943 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Perform a linear search on a given hashmap.` |
|         - |  925 | ` * Write a pointer to the target node on success.` |
|         - |  926 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  927 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  928 | ` * for more information.` |
|         - |  929 | ` */` |
|     33364 |  930 | `static int HashmapFindValue(` |
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
|     33369 |  943 | `	pEntry = pMap->pFirst;` |
|     33369 |  944 | `	n = pMap->nEntry;` |
|     33369 |  945 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33369 |  946 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     79211 |  947 | `	for(;;){` |
|    158428 |  948 | `		if( n < 1 ){` |
|       115 |  949 | `			break;` |
|         - |  950 | `		}` |
|         - |  951 | `		/* Extract node value */` |
|    158314 |  952 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    158314 |  953 | `		if( pVal ){` |
|         - |  954 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  955 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  956 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  957 | `			 * so null needles/values take the same path as everything else` |
|         - |  958 | `			 * (the historical null-to-null shortcut here made` |
|         - |  959 | `			 * in_array(null, [""]) false where php says true). */` |
|    158314 |  960 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    158314 |  961 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    158314 |  962 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    158314 |  963 | `			PH7_MemObjRelease(&sVal);` |
|    158314 |  964 | `			PH7_MemObjRelease(&sNeedle);` |
|    158314 |  965 | `			if( rc == 0 ){` |
|     33255 |  966 | `				if( ppNode ){` |
|        23 |  967 | `					*ppNode = pEntry;` |
|        11 |  968 | `				}` |
|         - |  969 | `				/* Match found*/` |
|     33255 |  970 | `				return SXRET_OK;` |
|         - |  971 | `			}` |
|     62529 |  972 | `		}` |
|         - |  973 | `		/* Point to the next entry */` |
|    125064 |  974 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    125064 |  975 | `		n--;` |
|         5 |  976 | `	}` |
|         - |  977 | `	/* No such entry */` |
|       115 |  978 | `	return SXERR_NOTFOUND;` |
|     16687 |  979 | `}` |
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
|    657812 | 1165 | `static sxi32 HashmapDuplicateNode(` |
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
|    657817 | 1176 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
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
|    657811 | 1201 | `	sSafeVal = *pVal;` |
|         - | 1202 |  |
|    657811 | 1203 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1204 | `		/* Blob key insertion */` |
|      4063 | 1205 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4063 | 1206 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4063 | 1207 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4063 | 1208 | `		PH7_MemObjRelease(&sKey);` |
|      2034 | 1209 | `	}else{` |
|         - | 1210 | `		/* Int key */` |
|    653753 | 1211 | `		if( iAction == 0 ){ /* Merge */` |
|    653517 | 1212 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    326995 | 1213 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1214 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1215 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1216 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1217 | `		}else{ /* Dup */` |
|       209 | 1218 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1219 | `		}` |
|         - | 1220 | `	}` |
|    657811 | 1221 | `	return rc;` |
|    328911 | 1222 | `}` |
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
|      2752 | 1235 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1236 | `{` |
|         - | 1237 | `	ph7_hashmap_node *pEntry;` |
|         - | 1238 | `	ph7_value *pVal;` |
|         - | 1239 | `	sxi32 rc;` |
|         - | 1240 | `	sxu32 n;` |
|      2757 | 1241 | `	if( pSrc == pDest ){` |
|         - | 1242 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1243 | `		 * Unlike the zend engine.` |
|         - | 1244 | `		 */` |
|       ! 0 | 1245 | `		return SXRET_OK;` |
|         - | 1246 | `	}` |
|         - | 1247 | `	/* Point to the first inserted entry in the source */` |
|      2757 | 1248 | `	pEntry = pSrc->pFirst;` |
|         - | 1249 | `	/* Perform the merge */` |
|    656327 | 1250 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1251 | `		/* Extract the node value */` |
|    653575 | 1252 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    653575 | 1253 | `		if( pVal ){` |
|         - | 1254 | `			/* Make a local copy of the value.` |
|         - | 1255 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1256 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1257 | `			 * to the old pool.` |
|         - | 1258 | `			 */` |
|    653575 | 1259 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    326790 | 1260 | `		}else{` |
|       ! 0 | 1261 | `			rc = SXRET_OK;` |
|         - | 1262 | `		}` |
|    653575 | 1263 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1264 | `			return rc;` |
|         - | 1265 | `		}` |
|         - | 1266 | `		/* Point to the next entry */` |
|    653575 | 1267 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    326790 | 1268 | `	}` |
|      2757 | 1269 | `	return SXRET_OK;` |
|      1381 | 1270 | `}` |
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
|      3960 | 1320 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1321 | `{` |
|         - | 1322 | `	ph7_hashmap_node *pEntry;` |
|         - | 1323 | `	ph7_value *pVal;` |
|         - | 1324 | `	sxi32 rc;` |
|         - | 1325 | `	sxu32 n;` |
|      3965 | 1326 | `	if( pSrc == pDest ){` |
|         - | 1327 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1328 | `		 * Unlike the zend engine.` |
|         - | 1329 | `		 */` |
|       ! 0 | 1330 | `		return SXRET_OK;` |
|         - | 1331 | `	}` |
|         - | 1332 | `	/* Point to the first inserted entry in the source */` |
|      3965 | 1333 | `	pEntry = pSrc->pFirst;` |
|         - | 1334 | `	/* Perform the duplication */` |
|      8163 | 1335 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1336 | `		/* Extract the node value */` |
|      4203 | 1337 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4203 | 1338 | `		if( pVal ){` |
|      4203 | 1339 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2104 | 1340 | `		}else{` |
|       ! 0 | 1341 | `			rc = SXRET_OK;` |
|         - | 1342 | `		}` |
|      4203 | 1343 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1344 | `			return rc;` |
|         - | 1345 | `		}` |
|         - | 1346 | `		/* Point to the next entry */` |
|      4203 | 1347 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2104 | 1348 | `	}` |
|      3965 | 1349 | `	return SXRET_OK;` |
|      1985 | 1350 | `}` |
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
|       749 | 1369 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1370 | `		/* Extract the node value (resolves foreign references) */` |
|       737 | 1371 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       736 | 1372 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       496 | 1373 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1374 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1375 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1376 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1377 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1378 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1379 | `			pVal = 0;` |
|         2 | 1380 | `		}` |
|       737 | 1381 | `		if( pVal ){` |
|       733 | 1382 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1093 | 1383 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       364 | 1384 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       365 | 1385 | `			}else{` |
|         5 | 1386 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1387 | `			}` |
|       733 | 1388 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1389 | `				return rc;` |
|         - | 1390 | `			}` |
|       366 | 1391 | `		}` |
|         - | 1392 | `		/* Point to the next entry */` |
|       737 | 1393 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       369 | 1394 | `	}` |
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
|    232300 | 1426 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1427 | `{` |
|    232305 | 1428 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1429 | `	ph7_hashmap *pNew;` |
|         - | 1430 | `	ph7_value *pBacking;` |
|         - | 1431 | `	sxu32 nValIdx;` |
|         - | 1432 | `	int bValueInPool;` |
|    232305 | 1433 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    232305 | 1434 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1435 | `		/* Sole owner, no separation needed */` |
|    229813 | 1436 | `		return pMap;` |
|         - | 1437 | `	}` |
|      2497 | 1438 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1439 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1440 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1441 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1442 | `		return pMap;` |
|         - | 1443 | `	}` |
|         - | 1444 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1445 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1446 | `	 * frame is popped. */` |
|      2371 | 1447 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2371 | 1448 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2366 | 1449 | `		if( pBacking && pBacking != pValue` |
|      2342 | 1450 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2323 | 1451 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1452 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2323 | 1453 | `			pMap->iRef--;` |
|      2323 | 1454 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1455 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2281 | 1456 | `				pMap->iRef++;` |
|      2281 | 1457 | `				return pMap;` |
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
|        24 | 1488 | `	}` |
|         - | 1489 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1490 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1491 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1492 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1493 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1494 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1495 | `	 * pBacking in the backing-variable branch above). */` |
|        50 | 1496 | `	nValIdx = pValue->nIdx;` |
|        74 | 1497 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        48 | 1498 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        50 | 1499 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        50 | 1500 | `	if( pNew == 0 ){` |
|         - | 1501 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1502 | `		return pMap;` |
|         - | 1503 | `	}` |
|        50 | 1504 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1505 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1506 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1507 | `		return pMap;` |
|         - | 1508 | `	}` |
|        50 | 1509 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        50 | 1510 | `	pMap->iRef--;` |
|        50 | 1511 | `	if( bValueInPool ){` |
|         - | 1512 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        50 | 1513 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        50 | 1514 | `		if( pValue == 0 ){` |
|       ! 0 | 1515 | `			return pNew;` |
|         - | 1516 | `		}` |
|        24 | 1517 | `	}` |
|        50 | 1518 | `	pValue->x.pOther = pNew;` |
|        50 | 1519 | `	return pNew;` |
|    116155 | 1520 | `}` |
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
|      3842 | 1558 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1559 | `{` |
|         - | 1560 | `	ph7_hashmap_node *pEntry;` |
|      3847 | 1561 | `	sxi32 rc = SXRET_OK;` |
|         - | 1562 | `	ph7_value *pObj;` |
|         - | 1563 | `	sxu32 n;` |
|      3847 | 1564 | `	if( pLeft == pRight ){` |
|         - | 1565 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1566 | `		 * Unlike the zend engine.` |
|         - | 1567 | `		 */` |
|       ! 0 | 1568 | `		return SXRET_OK;` |
|         - | 1569 | `	}` |
|         - | 1570 | `	/* Perform the union */` |
|      3847 | 1571 | `	pEntry = pRight->pFirst;` |
|      3881 | 1572 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1573 | `		/* Make sure the given key does not exists in the left array */` |
|        38 | 1574 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
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
|        38 | 1604 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 1605 | `	}` |
|      3847 | 1606 | `	return SXRET_OK;` |
|      1926 | 1607 | `}` |
|         - | 1608 | `/*` |
|         - | 1609 | ` * Allocate a new hashmap.` |
|         - | 1610 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1611 | ` */` |
|    144772 | 1612 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1613 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1614 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1615 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1616 | `	)` |
|         5 | 1617 | `{` |
|         - | 1618 | `	ph7_hashmap *pMap;` |
|         - | 1619 | `	/* Allocate a new instance */` |
|    144777 | 1620 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    144777 | 1621 | `	if( pMap == 0 ){` |
|       ! 0 | 1622 | `		return 0;` |
|         - | 1623 | `	}` |
|         - | 1624 | `	/* Zero the structure */` |
|    144777 | 1625 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1626 | `	/* Fill in the structure */` |
|    144777 | 1627 | `	pMap->pVm = &(*pVm);` |
|    144777 | 1628 | `	pMap->iRef = 1;` |
|         - | 1629 | `	/* Default hash functions */` |
|    144777 | 1630 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    144777 | 1631 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    144777 | 1632 | `	return pMap;` |
|     72391 | 1633 | `}` |
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
|      3494 | 1654 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|      3499 | 1674 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3499 | 1675 | `	if( pMap == 0 ){` |
|       ! 0 | 1676 | `		return SXERR_MEM;` |
|         - | 1677 | `	}` |
|      3499 | 1678 | `	pVm->pGlobal = pMap;` |
|         - | 1679 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3499 | 1680 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3499 | 1681 | `	if( pObj == 0 ){` |
|       ! 0 | 1682 | `		return SXERR_MEM;` |
|         - | 1683 | `	}` |
|      3499 | 1684 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1685 | `	/* Record object index */` |
|      3499 | 1686 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1687 | `	/* Install the special $GLOBALS array */` |
|      3499 | 1688 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3499 | 1689 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1690 | `		return rc;` |
|         - | 1691 | `	}` |
|         - | 1692 | `	/* Install superglobals now */` |
|     38439 | 1693 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1694 | `		ph7_value *pSuper;` |
|         - | 1695 | `		/* Request an empty array */` |
|     34945 | 1696 | `		pSuper = ph7_new_array(&(*pVm));` |
|     34945 | 1697 | `		if( pSuper == 0 ){` |
|       ! 0 | 1698 | `			return SXERR_MEM;` |
|         - | 1699 | `		}` |
|         - | 1700 | `		/* Install */` |
|     34945 | 1701 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     34945 | 1702 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1703 | `			return rc;` |
|         - | 1704 | `		}` |
|         - | 1705 | `		/* Release the value now it have been installed */` |
|     34945 | 1706 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17475 | 1707 | `	}` |
|         - | 1708 | `	/* Set some $_SERVER entries */` |
|      3499 | 1709 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1710 | `	/*` |
|         - | 1711 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1712 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1713 | `	 */` |
|      6993 | 1714 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1715 | `		"SCRIPT_FILENAME",` |
|      1747 | 1716 | `		pFile ? pFile->zString : ":Memory:",` |
|      3494 | 1717 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1718 | `		);` |
|         - | 1719 | `	/* All done,all super-global are installed now */` |
|      3499 | 1720 | `	return SXRET_OK;` |
|      1752 | 1721 | `}` |
|         - | 1722 | `/*` |
|         - | 1723 | ` * Release a hashmap.` |
|         - | 1724 | ` */` |
|    101544 | 1725 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1726 | `{` |
|         - | 1727 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    101549 | 1728 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1729 | `	sxu32 n;` |
|    101549 | 1730 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1731 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1732 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1733 | `		return SXRET_OK;` |
|         - | 1734 | `	}` |
|    101549 | 1735 | `	if( pMap->pActiveSteps ){` |
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
|    101549 | 1748 | `	n = 0;` |
|    101549 | 1749 | `	pEntry = pMap->pFirst;` |
|   1720681 | 1750 | `	for(;;){` |
|   3441367 | 1751 | `		if( n >= pMap->nEntry ){` |
|    101549 | 1752 | `			break;` |
|         - | 1753 | `		}` |
|   3339823 | 1754 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1755 | `		/* Remove the reference from the foreign table */` |
|   3339823 | 1756 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3339823 | 1757 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1758 | `			/* Restore the ph7_value to the free list */` |
|   3339813 | 1759 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1669904 | 1760 | `		}` |
|         - | 1761 | `		/* Release the node */` |
|   3339823 | 1762 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    196319 | 1763 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     98157 | 1764 | `		}` |
|   3339823 | 1765 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1766 | `		/* Point to the next entry */` |
|   3339823 | 1767 | `		pEntry = pNext;` |
|   3339823 | 1768 | `		n++;` |
|         5 | 1769 | `	}` |
|    101549 | 1770 | `	if( pMap->nEntry > 0 ){` |
|         - | 1771 | `		/* Release the hash bucket */` |
|     76655 | 1772 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     38325 | 1773 | `	}` |
|    101549 | 1774 | `	if( FreeDS ){` |
|         - | 1775 | `		/* Free the whole instance */` |
|    101523 | 1776 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     50764 | 1777 | `	}else{` |
|         - | 1778 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1779 | `		pMap->apBucket = 0;` |
|        28 | 1780 | `		pMap->iNextIdx = 0;` |
|        28 | 1781 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1782 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1783 | `	}` |
|    101549 | 1784 | `	return SXRET_OK;` |
|     50777 | 1785 | `}` |
|         - | 1786 | `/*` |
|         - | 1787 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1788 | ` * If the count reaches zero which mean no more variables` |
|         - | 1789 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1790 | ` */` |
|    837784 | 1791 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1792 | `{` |
|    837789 | 1793 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1794 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    837789 | 1795 | `	pMap->iRef--;` |
|    837789 | 1796 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    101503 | 1797 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     50749 | 1798 | `	}` |
|    837789 | 1799 | `}` |
|         - | 1800 | `/*` |
|         - | 1801 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1802 | ` * Write a pointer to the target node on success.` |
|         - | 1803 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1804 | ` */` |
|    139998 | 1805 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1806 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1807 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1808 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1809 | `	)` |
|         5 | 1810 | `{` |
|         - | 1811 | `	sxi32 rc;` |
|    140003 | 1812 | `	if( pMap->nEntry < 1 ){` |
|         - | 1813 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1814 | `		 */` |
|       159 | 1815 | `		return SXERR_NOTFOUND;` |
|         - | 1816 | `	}` |
|    139849 | 1817 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    139849 | 1818 | `	return rc;` |
|     70004 | 1819 | `}` |
|         - | 1820 | `/*` |
|         - | 1821 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1822 | ` * hashmap.` |
|         - | 1823 | ` * If a node with the given key already exists in the database` |
|         - | 1824 | ` * then this function overwrite the old value.` |
|         - | 1825 | ` */` |
|   2725424 | 1826 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2725429 | 1837 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2725429 | 1838 | `	return rc;` |
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
|     47294 | 1877 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1878 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1879 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1880 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1881 | `	)` |
|         5 | 1882 | `{` |
|         - | 1883 | `	sxi32 rc;` |
|     47299 | 1884 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1885 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1886 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1887 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1888 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1889 | `		return PH7_ABORT;` |
|         - | 1890 | `	}` |
|     47299 | 1891 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     47299 | 1892 | `	return rc;` |
|     23652 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1896 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1897 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1898 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1899 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1900 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1901 | ` */` |
|     18842 | 1902 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1903 | `{` |
|     18847 | 1904 | `	pStep->pCursor = pMap->pFirst;` |
|     18847 | 1905 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     18847 | 1906 | `	pMap->pActiveSteps = pStep;` |
|     18847 | 1907 | `}` |
|         - | 1908 | `/*` |
|         - | 1909 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1910 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1911 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1912 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1913 | ` */` |
|     18742 | 1914 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1915 | `{` |
|     18747 | 1916 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18747 | 1917 | `	while( *ppLink ){` |
|     18747 | 1918 | `		if( *ppLink == pStep ){` |
|     18747 | 1919 | `			*ppLink = pStep->pNextActive;` |
|     18747 | 1920 | `			pStep->pNextActive = 0;` |
|     18747 | 1921 | `			return;` |
|         - | 1922 | `		}` |
|       ! 0 | 1923 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1924 | `	}` |
|      9376 | 1925 | `}` |
|         - | 1926 | `/*` |
|         - | 1927 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1928 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1929 | ` * return NULL.` |
|         - | 1930 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1931 | ` */` |
|        64 | 1932 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 1933 | `{` |
|        65 | 1934 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 1935 | `	if( pCur == 0 ){` |
|         - | 1936 | `		/* End of the list,return null */` |
|        27 | 1937 | `		return 0;` |
|         - | 1938 | `	}` |
|         - | 1939 | `	/* Advance the node cursor */` |
|        39 | 1940 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 1941 | `	return pCur;` |
|        33 | 1942 | `}` |
|         - | 1943 | `/*` |
|         - | 1944 | ` * Extract a node value.` |
|         - | 1945 | ` */` |
|    587760 | 1946 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1947 | `{` |
|    587765 | 1948 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    587765 | 1949 | `	if( pEntry ){` |
|    587765 | 1950 | `		if( bStore ){` |
|    233187 | 1951 | `			PH7_MemObjStore(pEntry,pValue);` |
|    116596 | 1952 | `		}else{` |
|    354583 | 1953 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1954 | `		}` |
|    293935 | 1955 | `	}else{` |
|       ! 0 | 1956 | `		PH7_MemObjRelease(pValue);` |
|         - | 1957 | `	}` |
|    587765 | 1958 | `}` |
|         - | 1959 | `/*` |
|         - | 1960 | ` * Extract a node key.` |
|         - | 1961 | ` */` |
|    154500 | 1962 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1963 | `{` |
|         - | 1964 | `	/* Fill with the current key */` |
|    154505 | 1965 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    149545 | 1966 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 1967 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 1968 | `		}` |
|    149545 | 1969 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    149545 | 1970 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     74775 | 1971 | `	}else{` |
|      4965 | 1972 | `		SyBlobReset(&pKey->sBlob);` |
|      4965 | 1973 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      4965 | 1974 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1975 | `	}` |
|    154505 | 1976 | `}` |
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
|     35640 | 2027 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2028 | `{` |
|         - | 2029 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2030 | `    /* Prevent compiler warning */` |
|     35645 | 2031 | `	result.pNext = result.pPrev = 0;` |
|     35645 | 2032 | `	pTail = &result;` |
|    107296 | 2033 | `	while( pA && pB ){` |
|     71656 | 2034 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47368 | 2035 | `			pTail->pPrev = pA;` |
|     47368 | 2036 | `			pA->pNext = pTail;` |
|     47368 | 2037 | `			pTail = pA;` |
|     47368 | 2038 | `			pA = pA->pPrev;` |
|     23691 | 2039 | `		}else{` |
|     24293 | 2040 | `			pTail->pPrev = pB;` |
|     24293 | 2041 | `			pB->pNext = pTail;` |
|     24293 | 2042 | `			pTail = pB;` |
|     24293 | 2043 | `			pB = pB->pPrev;` |
|         - | 2044 | `		}` |
|         5 | 2045 | `	}` |
|     35645 | 2046 | `	if( pA ){` |
|     25157 | 2047 | `		pTail->pPrev = pA;` |
|     25157 | 2048 | `		pA->pNext = pTail;` |
|     23091 | 2049 | `	}else if( pB ){` |
|     10265 | 2050 | `		pTail->pPrev = pB;` |
|     10265 | 2051 | `		pB->pNext = pTail;` |
|      5113 | 2052 | `	}else{` |
|       233 | 2053 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2054 | `	}` |
|     35645 | 2055 | `	return result.pPrev;` |
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
|       742 | 2069 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2070 | `{` |
|         - | 2071 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2072 | `	sxu32 i;` |
|       747 | 2073 | `	SyZero(a,sizeof(a));` |
|         - | 2074 | `	/* Point to the first inserted entry */` |
|       747 | 2075 | `	pIn = pMap->pFirst;` |
|     14693 | 2076 | `	while( pIn ){` |
|     13951 | 2077 | `		p = pIn;` |
|     13951 | 2078 | `		pIn = p->pPrev;` |
|     13951 | 2079 | `		p->pPrev = 0;` |
|     26589 | 2080 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26589 | 2081 | `			if( a[i]==0 ){` |
|     13951 | 2082 | `				a[i] = p;` |
|     13951 | 2083 | `				break;` |
|       ! 0 | 2084 | `			}else{` |
|     12643 | 2085 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12643 | 2086 | `				a[i] = 0;` |
|         - | 2087 | `			}` |
|      6324 | 2088 | `		}` |
|     13951 | 2089 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2090 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2091 | `			 * But that is impossible.` |
|         - | 2092 | `			 */` |
|       ! 0 | 2093 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2094 | `		}` |
|         5 | 2095 | `	}` |
|       747 | 2096 | `	p = a[0];` |
|     23749 | 2097 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     23007 | 2098 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11506 | 2099 | `	}` |
|       747 | 2100 | `	p->pNext = 0;` |
|         - | 2101 | `	/* Reflect the change */` |
|       747 | 2102 | `	pMap->pFirst = p;` |
|         - | 2103 | `	/* Reset the loop cursor */` |
|       747 | 2104 | `	pMap->pCur = pMap->pFirst;` |
|       747 | 2105 | `	return SXRET_OK;` |
|         5 | 2106 | `}` |
|         - | 2107 | `/* SPDX-SnippetEnd */` |
|         - | 2108 | `/*` |
|         - | 2109 | ` * Node comparison callback.` |
|         - | 2110 | ` * used-by: [sort(),asort(),...]` |
|         - | 2111 | ` */` |
|     71378 | 2112 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2113 | `{` |
|         - | 2114 | `	ph7_value sA,sB;` |
|         - | 2115 | `	sxi32 iFlags;` |
|         - | 2116 | `	int rc;` |
|     71383 | 2117 | `	if( pCmpData == 0 ){` |
|         - | 2118 | `		/* Perform a standard comparison */` |
|     71359 | 2119 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     71359 | 2120 | `		return rc;` |
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
|     35719 | 2158 | `}` |
|         - | 2159 | `/*` |
|         - | 2160 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2161 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2162 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2163 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2164 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2165 | ` */` |
|        56 | 2166 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         1 | 2167 | `{` |
|         - | 2168 | `	sxi32 rc;` |
|        57 | 2169 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2170 | `		/* Perform a string comparison */` |
|        19 | 2171 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|        10 | 2172 | `	}else{` |
|         - | 2173 | `		SyString sStr;` |
|        39 | 2174 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2175 | `		int bNum = 1;` |
|        39 | 2176 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2177 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2178 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2179 | `				bNum = 0;` |
|         6 | 2180 | `			}else{` |
|       ! 0 | 2181 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2182 | `			}` |
|         6 | 2183 | `		}else{` |
|        29 | 2184 | `			iA = pA->xKey.iKey;` |
|         - | 2185 | `		}` |
|        39 | 2186 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2187 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2188 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2189 | `				bNum = 0;` |
|         4 | 2190 | `			}else{` |
|       ! 0 | 2191 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2192 | `			}` |
|         4 | 2193 | `		}else{` |
|        33 | 2194 | `			iB = pB->xKey.iKey;` |
|         - | 2195 | `		}` |
|        39 | 2196 | `		if( bNum ){` |
|        23 | 2197 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2198 | `		}else{` |
|         - | 2199 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2200 | `			char zNumA[24],zNumB[24];` |
|         - | 2201 | `			SyString sA,sB;` |
|        17 | 2202 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2203 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2204 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2205 | `			}else{` |
|        11 | 2206 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2207 | `			}` |
|        17 | 2208 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2209 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2210 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2211 | `			}else{` |
|         7 | 2212 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2213 | `			}` |
|        17 | 2214 | `			rc = SyStrncmp(sA.zString,sB.zString,SXMAX(sA.nByte,sB.nByte));` |
|         - | 2215 | `		}` |
|         - | 2216 | `	}` |
|        57 | 2217 | `	return rc;` |
|         1 | 2218 | `}` |
|         - | 2219 | `/*` |
|         - | 2220 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2221 | ` * used-by: [ksort()]` |
|         - | 2222 | ` */` |
|        42 | 2223 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2224 | `{` |
|        21 | 2225 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        43 | 2226 | `	return HashmapKeyNodeCmp(pA,pB);` |
|         1 | 2227 | `}` |
|         - | 2228 | `/*` |
|         - | 2229 | ` * Node comparison callback.` |
|         - | 2230 | ` * Used by: [rsort(),arsort()];` |
|         - | 2231 | ` */` |
|        78 | 2232 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2233 | `{` |
|         - | 2234 | `	ph7_value sA,sB;` |
|         - | 2235 | `	sxi32 iFlags;` |
|         - | 2236 | `	int rc;` |
|        79 | 2237 | `	if( pCmpData == 0 ){` |
|         - | 2238 | `		/* Perform a standard comparison */` |
|        59 | 2239 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2240 | `		return -rc;` |
|         - | 2241 | `	}` |
|        21 | 2242 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2243 | `	/* Duplicate node values */` |
|        21 | 2244 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2245 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2246 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2247 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2248 | `	if( iFlags == 5 ){` |
|         - | 2249 | `		/* String cast */` |
|         - | 2250 | `		const char *zA,*zB;` |
|         - | 2251 | `		sxu32 nA,nB,nMin;` |
|        11 | 2252 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2253 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2254 | `		}` |
|        11 | 2255 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2256 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2257 | `		}` |
|         - | 2258 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2259 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2260 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2261 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2262 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2263 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2264 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2265 | `		if( rc == 0 ){` |
|         3 | 2266 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2267 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2268 | `		}` |
|         6 | 2269 | `	}else{` |
|         - | 2270 | `		/* Numeric cast */` |
|        11 | 2271 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2272 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2273 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2274 | `	}` |
|        21 | 2275 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2276 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2277 | `	return -rc;` |
|        40 | 2278 | `}` |
|         - | 2279 | `/*` |
|         - | 2280 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2281 | ` * used-by: [usort(),uasort()]` |
|         - | 2282 | ` */` |
|       110 | 2283 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2284 | `{` |
|         - | 2285 | `	ph7_value sResult,*pCallback;` |
|         - | 2286 | `	ph7_value *pV1,*pV2;` |
|         - | 2287 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2288 | `	sxi32 rc;` |
|         - | 2289 | `	/* Point to the desired callback */` |
|       112 | 2290 | `	pCallback = (ph7_value *)pCmpData;` |
|       112 | 2291 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2292 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2293 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2294 | `		return 0;` |
|         - | 2295 | `	}` |
|         - | 2296 | `	/* initialize the result value */` |
|       106 | 2297 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2298 | `	/* Extract nodes values */` |
|       106 | 2299 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       106 | 2300 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       106 | 2301 | `	apArg[0] = pV1;` |
|       106 | 2302 | `	apArg[1] = pV2;` |
|         - | 2303 | `	/* Invoke the callback */` |
|       106 | 2304 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       106 | 2305 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2306 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2307 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2308 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2309 | `		rc = 0;` |
|       102 | 2310 | `	}else if( rc != SXRET_OK ){` |
|         - | 2311 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2312 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2313 | `	}else{` |
|         - | 2314 | `		/* Extract callback result */` |
|        98 | 2315 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2316 | `			/* Perform an int cast */` |
|       ! 0 | 2317 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2318 | `		}` |
|        98 | 2319 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2320 | `	}` |
|       106 | 2321 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2322 | `	/* Callback result */` |
|       106 | 2323 | `	return rc;` |
|        57 | 2324 | `}` |
|         - | 2325 | `/*` |
|         - | 2326 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2327 | ` * used-by: [krsort()]` |
|         - | 2328 | ` */` |
|        14 | 2329 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2330 | `{` |
|         7 | 2331 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        15 | 2332 | `	return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         1 | 2333 | `}` |
|         - | 2334 | `/*` |
|         - | 2335 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2336 | ` * used-by: [uksort()]` |
|         - | 2337 | ` */` |
|         6 | 2338 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2339 | `{` |
|         - | 2340 | `	ph7_value sResult,*pCallback;` |
|         - | 2341 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2342 | `	ph7_value sK1,sK2;` |
|         - | 2343 | `	sxi32 rc;` |
|         - | 2344 | `	/* Point to the desired callback */` |
|         7 | 2345 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2346 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2347 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2348 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2349 | `		return 0;` |
|         - | 2350 | `	}` |
|         - | 2351 | `	/* initialize the result value */` |
|         7 | 2352 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2353 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2354 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2355 | `	/* Extract nodes keys */` |
|         7 | 2356 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2357 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2358 | `	apArg[0] = &sK1;` |
|         7 | 2359 | `	apArg[1] = &sK2;` |
|         - | 2360 | `	/* Mark keys as constants */` |
|         7 | 2361 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2362 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2363 | `	/* Invoke the callback */` |
|         7 | 2364 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2365 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2366 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2367 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2368 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2369 | `		rc = 0;` |
|         7 | 2370 | `	}else if( rc != SXRET_OK ){` |
|         - | 2371 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2372 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2373 | `	}else{` |
|         - | 2374 | `		/* Extract callback result */` |
|         7 | 2375 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2376 | `			/* Perform an int cast */` |
|       ! 0 | 2377 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2378 | `		}` |
|         7 | 2379 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2380 | `	}` |
|         7 | 2381 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2382 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2383 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2384 | `	/* Callback result */` |
|         7 | 2385 | `	return rc;` |
|         4 | 2386 | `}` |
|         - | 2387 | `/*` |
|         - | 2388 | ` * Node comparison callback: Random node comparison.` |
|         - | 2389 | ` * used-by: [shuffle()]` |
|         - | 2390 | ` */` |
|        23 | 2391 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2392 | `{` |
|         - | 2393 | `	sxu32 n;` |
|        13 | 2394 | `	SXUNUSED(pB); /* cc warning */` |
|        13 | 2395 | `	SXUNUSED(pCmpData);` |
|         - | 2396 | `	/* Grab a random number */` |
|        24 | 2397 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2398 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2399 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2400 | `	 */` |
|        24 | 2401 | `	return n&1 ? 1 : -1;` |
|         1 | 2402 | `}` |
|         - | 2403 | `/*` |
|         - | 2404 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2405 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2406 | ` */` |
|       674 | 2407 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2408 | `{` |
|         - | 2409 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2410 | `	sxu32 i;` |
|         - | 2411 | `	/* Rehash all entries */` |
|       679 | 2412 | `	pLast = p = pMap->pFirst;` |
|       679 | 2413 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|       679 | 2414 | `	i = 0;` |
|      7194 | 2415 | `	for( ;; ){` |
|     14393 | 2416 | `		if( i >= pMap->nEntry ){` |
|       679 | 2417 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       679 | 2418 | `			break;` |
|         - | 2419 | `		}` |
|     13719 | 2420 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2421 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2422 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2423 | `			/* Change key type */` |
|         5 | 2424 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2425 | `		}` |
|     13719 | 2426 | `		HashmapRehashIntNode(p);` |
|         - | 2427 | `		/* Point to the next entry */` |
|     13719 | 2428 | `		i++;` |
|     13719 | 2429 | `		pLast = p;` |
|     13719 | 2430 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2431 | `	}` |
|       679 | 2432 | `}` |
|         - | 2433 | `/*` |
|         - | 2434 | ` * Array functions implementation.` |
|         - | 2435 | ` * Status:` |
|         - | 2436 | ` *  Stable.` |
|         - | 2437 | ` */` |
|         - | 2438 | `/*` |
|         - | 2439 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2440 | ` * Sort an array.` |
|         - | 2441 | ` * Parameters` |
|         - | 2442 | ` *  $array` |
|         - | 2443 | ` *   The input array.` |
|         - | 2444 | ` * $sort_flags` |
|         - | 2445 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2446 | ` *  Sorting type flags:` |
|         - | 2447 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2448 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2449 | ` *   SORT_STRING - compare items as strings` |
|         - | 2450 | ` * Return` |
|         - | 2451 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2452 | ` *` |
|         - | 2453 | ` */` |
|      1002 | 2454 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2455 | `{` |
|         - | 2456 | `	ph7_hashmap *pMap;` |
|         - | 2457 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1007 | 2458 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2459 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2460 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2461 | `		return PH7_OK;` |
|         - | 2462 | `	}` |
|         - | 2463 | `	/* Point to the internal representation of the input hashmap */` |
|      1007 | 2464 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1007 | 2465 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1007 | 2466 | `	if( pMap->nEntry > 1 ){` |
|       655 | 2467 | `		sxi32 iCmpFlags = 0;` |
|       655 | 2468 | `		if( nArg > 1 ){` |
|         - | 2469 | `			/* Extract comparison flags */` |
|         3 | 2470 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2471 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2472 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2473 | `			}` |
|         1 | 2474 | `		}` |
|         - | 2475 | `		/* Do the merge sort */` |
|       655 | 2476 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2477 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       655 | 2478 | `		HashmapSortRehash(pMap);` |
|       325 | 2479 | `	}` |
|         - | 2480 | `	/* All done,return TRUE */` |
|      1007 | 2481 | `	ph7_result_bool(pCtx,1);` |
|      1007 | 2482 | `	return PH7_OK;` |
|       506 | 2483 | `}` |
|         - | 2484 | `/*` |
|         - | 2485 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2486 | ` *  Sort an array and maintain index association.` |
|         - | 2487 | ` * Parameters` |
|         - | 2488 | ` *  $array` |
|         - | 2489 | ` *   The input array.` |
|         - | 2490 | ` * $sort_flags` |
|         - | 2491 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2492 | ` *  Sorting type flags:` |
|         - | 2493 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2494 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2495 | ` *   SORT_STRING - compare items as strings` |
|         - | 2496 | ` * Return` |
|         - | 2497 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2498 | ` */` |
|        34 | 2499 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2500 | `{` |
|         - | 2501 | `	ph7_hashmap *pMap;` |
|         - | 2502 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        39 | 2503 | `	if( nArg < 1 ){` |
|         3 | 2504 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2505 | `			"ArgumentCountError",` |
|         - | 2506 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2507 | `			);` |
|         - | 2508 | `	}` |
|         - | 2509 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2510 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2511 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2512 | `			"TypeError",` |
|         - | 2513 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2514 | `			ph7_type_name(apArg[0])` |
|         - | 2515 | `			);` |
|         - | 2516 | `	}` |
|         - | 2517 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2518 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2519 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2520 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2521 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2522 | `		if( nArg > 1 ){` |
|         - | 2523 | `			/* Extract comparison flags */` |
|         5 | 2524 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2525 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2526 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2527 | `			}` |
|         2 | 2528 | `		}` |
|         - | 2529 | `		/* Do the merge sort */` |
|        21 | 2530 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2531 | `		/* Fix the last link broken by the merge */` |
|        49 | 2532 | `		while(pMap->pLast->pPrev){` |
|        29 | 2533 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2534 | `		}` |
|        10 | 2535 | `	}` |
|         - | 2536 | `	/* All done,return TRUE */` |
|        25 | 2537 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2538 | `	return PH7_OK;` |
|        22 | 2539 | `}` |
|         - | 2540 | `/*` |
|         - | 2541 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2542 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2543 | ` * Parameters` |
|         - | 2544 | ` *  $array` |
|         - | 2545 | ` *   The input array.` |
|         - | 2546 | ` * $sort_flags` |
|         - | 2547 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2548 | ` *  Sorting type flags:` |
|         - | 2549 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2550 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2551 | ` *   SORT_STRING - compare items as strings` |
|         - | 2552 | ` * Return` |
|         - | 2553 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2554 | ` */` |
|        32 | 2555 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2556 | `{` |
|         - | 2557 | `	ph7_hashmap *pMap;` |
|         - | 2558 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2559 | `	if( nArg < 1 ){` |
|         3 | 2560 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2561 | `			"ArgumentCountError",` |
|         - | 2562 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2563 | `			);` |
|         - | 2564 | `	}` |
|         - | 2565 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2566 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2567 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2568 | `			"TypeError",` |
|         - | 2569 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2570 | `			ph7_type_name(apArg[0])` |
|         - | 2571 | `			);` |
|         - | 2572 | `	}` |
|         - | 2573 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2574 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2575 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2576 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2577 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2578 | `		if( nArg > 1 ){` |
|         - | 2579 | `			/* Extract comparison flags */` |
|         5 | 2580 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2581 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2582 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2583 | `			}` |
|         2 | 2584 | `		}` |
|         - | 2585 | `		/* Do the merge sort */` |
|        19 | 2586 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2587 | `		/* Fix the last link broken by the merge */` |
|        35 | 2588 | `		while(pMap->pLast->pPrev){` |
|        17 | 2589 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2590 | `		}` |
|         9 | 2591 | `	}` |
|         - | 2592 | `	/* All done,return TRUE */` |
|        23 | 2593 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2594 | `	return PH7_OK;` |
|        21 | 2595 | `}` |
|         - | 2596 | `/*` |
|         - | 2597 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2598 | ` *  Sort an array by key.` |
|         - | 2599 | ` * Parameters` |
|         - | 2600 | ` *  $array` |
|         - | 2601 | ` *   The input array.` |
|         - | 2602 | ` * $sort_flags` |
|         - | 2603 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2604 | ` *  Sorting type flags:` |
|         - | 2605 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2606 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2607 | ` *   SORT_STRING - compare items as strings` |
|         - | 2608 | ` * Return` |
|         - | 2609 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2610 | ` */` |
|        12 | 2611 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2612 | `{` |
|         - | 2613 | `	ph7_hashmap *pMap;` |
|         - | 2614 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 2615 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2616 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2617 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2618 | `		return PH7_OK;` |
|         - | 2619 | `	}` |
|         - | 2620 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 2621 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 2622 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 2623 | `	if( pMap->nEntry > 1 ){` |
|        13 | 2624 | `		sxi32 iCmpFlags = 0;` |
|        13 | 2625 | `		if( nArg > 1 ){` |
|         - | 2626 | `			/* Extract comparison flags */` |
|       ! 0 | 2627 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2628 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2629 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2630 | `			}` |
|       ! 0 | 2631 | `		}` |
|         - | 2632 | `		/* Do the merge sort */` |
|        13 | 2633 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2634 | `		/* Fix the last link broken by the merge */` |
|        35 | 2635 | `		while(pMap->pLast->pPrev){` |
|        23 | 2636 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2637 | `		}` |
|         6 | 2638 | `	}` |
|         - | 2639 | `	/* All done,return TRUE */` |
|        13 | 2640 | `	ph7_result_bool(pCtx,1);` |
|        13 | 2641 | `	return PH7_OK;` |
|         7 | 2642 | `}` |
|         - | 2643 | `/*` |
|         - | 2644 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2645 | ` *  Sort an array by key in reverse order.` |
|         - | 2646 | ` * Parameters` |
|         - | 2647 | ` *  $array` |
|         - | 2648 | ` *   The input array.` |
|         - | 2649 | ` * $sort_flags` |
|         - | 2650 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2651 | ` *  Sorting type flags:` |
|         - | 2652 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2653 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2654 | ` *   SORT_STRING - compare items as strings` |
|         - | 2655 | ` * Return` |
|         - | 2656 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2657 | ` */` |
|         4 | 2658 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2659 | `{` |
|         - | 2660 | `	ph7_hashmap *pMap;` |
|         - | 2661 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2662 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2663 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2664 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2665 | `		return PH7_OK;` |
|         - | 2666 | `	}` |
|         - | 2667 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2668 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2669 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2670 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2671 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2672 | `		if( nArg > 1 ){` |
|         - | 2673 | `			/* Extract comparison flags */` |
|       ! 0 | 2674 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2675 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2676 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2677 | `			}` |
|       ! 0 | 2678 | `		}` |
|         - | 2679 | `		/* Do the merge sort */` |
|         5 | 2680 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2681 | `		/* Fix the last link broken by the merge */` |
|        17 | 2682 | `		while(pMap->pLast->pPrev){` |
|        13 | 2683 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2684 | `		}` |
|         2 | 2685 | `	}` |
|         - | 2686 | `	/* All done,return TRUE */` |
|         5 | 2687 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2688 | `	return PH7_OK;` |
|         3 | 2689 | `}` |
|         - | 2690 | `/*` |
|         - | 2691 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2692 | ` * Sort an array in reverse order.` |
|         - | 2693 | ` * Parameters` |
|         - | 2694 | ` *  $array` |
|         - | 2695 | ` *   The input array.` |
|         - | 2696 | ` * $sort_flags` |
|         - | 2697 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2698 | ` *  Sorting type flags:` |
|         - | 2699 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2700 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2701 | ` *   SORT_STRING - compare items as strings` |
|         - | 2702 | ` * Return` |
|         - | 2703 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2704 | ` */` |
|         2 | 2705 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2706 | `{` |
|         - | 2707 | `	ph7_hashmap *pMap;` |
|         - | 2708 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2709 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2710 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2711 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2712 | `		return PH7_OK;` |
|         - | 2713 | `	}` |
|         - | 2714 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2715 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2716 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2717 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2718 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2719 | `		if( nArg > 1 ){` |
|         - | 2720 | `			/* Extract comparison flags */` |
|       ! 0 | 2721 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2722 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2723 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2724 | `			}` |
|       ! 0 | 2725 | `		}` |
|         - | 2726 | `		/* Do the merge sort */` |
|         3 | 2727 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2728 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2729 | `		HashmapSortRehash(pMap);` |
|         1 | 2730 | `	}` |
|         - | 2731 | `	/* All done,return TRUE */` |
|         3 | 2732 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2733 | `	return PH7_OK;` |
|         2 | 2734 | `}` |
|         - | 2735 | `/*` |
|         - | 2736 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2737 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2738 | ` * Parameters` |
|         - | 2739 | ` *  $array` |
|         - | 2740 | ` *   The input array.` |
|         - | 2741 | ` * $cmp_function` |
|         - | 2742 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2743 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2744 | ` *  to, or greater than the second.` |
|         - | 2745 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2746 | ` * Return` |
|         - | 2747 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2748 | ` */` |
|        18 | 2749 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2750 | `{` |
|         - | 2751 | `	ph7_hashmap *pMap;` |
|         - | 2752 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 2753 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2754 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2755 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2756 | `		return PH7_OK;` |
|         - | 2757 | `	}` |
|         - | 2758 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 2759 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        20 | 2760 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 2761 | `	if( pMap->nEntry > 1 ){` |
|        20 | 2762 | `		ph7_value *pCallback = 0;` |
|         - | 2763 | `		ProcNodeCmp xCmp;` |
|        20 | 2764 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        20 | 2765 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2766 | `			/* Point to the desired callback */` |
|        20 | 2767 | `			pCallback = apArg[1];` |
|        11 | 2768 | `		}else{` |
|         - | 2769 | `			/* Use the default comparison function */` |
|       ! 0 | 2770 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2771 | `		}` |
|         - | 2772 | `		/* Do the merge sort */` |
|        20 | 2773 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        20 | 2774 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2775 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        20 | 2776 | `		HashmapSortRehash(pMap);` |
|        20 | 2777 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2778 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2779 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2780 | `			return PH7_EXCEPTION;` |
|         - | 2781 | `		}` |
|         5 | 2782 | `	}` |
|         - | 2783 | `	/* All done,return TRUE */` |
|        12 | 2784 | `	ph7_result_bool(pCtx,1);` |
|        12 | 2785 | `	return PH7_OK;` |
|        11 | 2786 | `}` |
|         - | 2787 | `/*` |
|         - | 2788 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2789 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2790 | ` *  and maintain index association.` |
|         - | 2791 | ` * Parameters` |
|         - | 2792 | ` *  $array` |
|         - | 2793 | ` *   The input array.` |
|         - | 2794 | ` * $cmp_function` |
|         - | 2795 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2796 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2797 | ` *  to, or greater than the second.` |
|         - | 2798 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2799 | ` * Return` |
|         - | 2800 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2801 | ` */` |
|        10 | 2802 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2803 | `{` |
|         - | 2804 | `	ph7_hashmap *pMap;` |
|         - | 2805 | `	/* Make sure we are dealing with a valid hashmap */` |
|        11 | 2806 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2807 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2808 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2809 | `		return PH7_OK;` |
|         - | 2810 | `	}` |
|         - | 2811 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2812 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2813 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2814 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2815 | `		ph7_value *pCallback = 0;` |
|         - | 2816 | `		ProcNodeCmp xCmp;` |
|        11 | 2817 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2818 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2819 | `			/* Point to the desired callback */` |
|        11 | 2820 | `			pCallback = apArg[1];` |
|         6 | 2821 | `		}else{` |
|         - | 2822 | `			/* Use the default comparison function */` |
|       ! 0 | 2823 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2824 | `		}` |
|         - | 2825 | `		/* Do the merge sort */` |
|        11 | 2826 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2827 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2828 | `		/* Fix the last link broken by the merge */` |
|        23 | 2829 | `		while(pMap->pLast->pPrev){` |
|        13 | 2830 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2831 | `		}` |
|        11 | 2832 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2833 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2834 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2835 | `			return PH7_EXCEPTION;` |
|         - | 2836 | `		}` |
|         5 | 2837 | `	}` |
|         - | 2838 | `	/* All done,return TRUE */` |
|        11 | 2839 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2840 | `	return PH7_OK;` |
|         6 | 2841 | `}` |
|         - | 2842 | `/*` |
|         - | 2843 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2844 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2845 | ` *  function and maintain index association.` |
|         - | 2846 | ` * Parameters` |
|         - | 2847 | ` *  $array` |
|         - | 2848 | ` *   The input array.` |
|         - | 2849 | ` * $cmp_function` |
|         - | 2850 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2851 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2852 | ` *  to, or greater than the second.` |
|         - | 2853 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2854 | ` * Return` |
|         - | 2855 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2856 | ` */` |
|         2 | 2857 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2858 | `{` |
|         - | 2859 | `	ph7_hashmap *pMap;` |
|         - | 2860 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2861 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2862 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2863 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2864 | `		return PH7_OK;` |
|         - | 2865 | `	}` |
|         - | 2866 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2867 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2868 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2869 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2870 | `		ph7_value *pCallback = 0;` |
|         - | 2871 | `		ProcNodeCmp xCmp;` |
|         3 | 2872 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2873 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2874 | `			/* Point to the desired callback */` |
|         3 | 2875 | `			pCallback = apArg[1];` |
|         2 | 2876 | `		}else{` |
|         - | 2877 | `			/* Use the default comparison function */` |
|       ! 0 | 2878 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2879 | `		}` |
|         - | 2880 | `		/* Do the merge sort */` |
|         3 | 2881 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2882 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2883 | `		/* Fix the last link broken by the merge */` |
|         3 | 2884 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2885 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2886 | `		}` |
|         3 | 2887 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2888 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2889 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2890 | `			return PH7_EXCEPTION;` |
|         - | 2891 | `		}` |
|         1 | 2892 | `	}` |
|         - | 2893 | `	/* All done,return TRUE */` |
|         3 | 2894 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2895 | `	return PH7_OK;` |
|         2 | 2896 | `}` |
|         - | 2897 | `/*` |
|         - | 2898 | ` * bool shuffle(array &$array)` |
|         - | 2899 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2900 | ` * Parameters` |
|         - | 2901 | ` *  $array` |
|         - | 2902 | ` *   The input array.` |
|         - | 2903 | ` * Return` |
|         - | 2904 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2905 | ` *` |
|         - | 2906 | ` */` |
|         2 | 2907 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2908 | `{` |
|         - | 2909 | `	ph7_hashmap *pMap;` |
|         - | 2910 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2911 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2912 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2913 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2914 | `		return PH7_OK;` |
|         - | 2915 | `	}` |
|         - | 2916 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2917 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2918 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2919 | `	if( pMap->nEntry > 1 ){` |
|         - | 2920 | `		/* Do the merge sort */` |
|         3 | 2921 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2922 | `		/* Fix the last link broken by the merge */` |
|         7 | 2923 | `		while(pMap->pLast->pPrev){` |
|         5 | 2924 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2925 | `		}` |
|         1 | 2926 | `	}` |
|         - | 2927 | `	/* All done,return TRUE */` |
|         3 | 2928 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2929 | `	return PH7_OK;` |
|         2 | 2930 | `}` |
|         - | 2931 | `/*` |
|         - | 2932 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2933 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2934 | ` * Parameters` |
|         - | 2935 | ` *  $var` |
|         - | 2936 | ` *   The array or the object.` |
|         - | 2937 | ` * $mode` |
|         - | 2938 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2939 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2940 | ` *  all the elements of a multidimensional array.` |
|         - | 2941 | ` * Return` |
|         - | 2942 | ` *  Returns the number of elements in the array.` |
|         - | 2943 | ` */` |
|      1636 | 2944 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2945 | `{` |
|      1641 | 2946 | `	int bRecursive = FALSE;` |
|      1641 | 2947 | `	int bCycleDetected = FALSE;` |
|         - | 2948 | `	sxi64 iCount;` |
|      1641 | 2949 | `	if( nArg < 1 ){` |
|         3 | 2950 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2951 | `			"ArgumentCountError",` |
|         - | 2952 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2953 | `			);` |
|         - | 2954 | `	}` |
|      1639 | 2955 | `	if( nArg > 2 ){` |
|         4 | 2956 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2957 | `			"ArgumentCountError",` |
|         - | 2958 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2959 | `			nArg` |
|         - | 2960 | `			);` |
|         - | 2961 | `	}` |
|         - | 2962 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2963 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2964 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1637 | 2965 | `	if( nArg > 1 ){` |
|        45 | 2966 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2967 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2968 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2969 | `				"ValueError",` |
|         - | 2970 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2971 | `				);` |
|         - | 2972 | `		}` |
|        34 | 2973 | `		bRecursive = iMode == 1;` |
|        16 | 2974 | `	}` |
|      1629 | 2975 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2976 | `		/* Countable object: dispatch to ->count() */` |
|        65 | 2977 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        53 | 2978 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        53 | 2979 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        53 | 2980 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        50 | 2981 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 2982 | `					"count",sizeof("count")-1);` |
|        50 | 2983 | `				if( pMeth ){` |
|         - | 2984 | `					ph7_value sResult;` |
|        50 | 2985 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        50 | 2986 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        50 | 2987 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        50 | 2988 | `					PH7_MemObjRelease(&sResult);` |
|        50 | 2989 | `					return PH7_OK;` |
|         - | 2990 | `				}` |
|       ! 0 | 2991 | `			}` |
|         1 | 2992 | `		}` |
|        22 | 2993 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2994 | `			"TypeError",` |
|         - | 2995 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 2996 | `			ph7_type_name(apArg[0])` |
|         - | 2997 | `			);` |
|         - | 2998 | `	}` |
|         - | 2999 | `	/* Count */` |
|      1569 | 3000 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1569 | 3001 | `	if( bCycleDetected ){` |
|         3 | 3002 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3003 | `	}` |
|      1569 | 3004 | `	ph7_result_int64(pCtx,iCount);` |
|      1569 | 3005 | `	return PH7_OK;` |
|       823 | 3006 | `}` |
|         - | 3007 | `/*` |
|         - | 3008 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3009 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3010 | ` * Parameters` |
|         - | 3011 | ` * $key` |
|         - | 3012 | ` *   Value to check.` |
|         - | 3013 | ` * $search` |
|         - | 3014 | ` *  An array with keys to check.` |
|         - | 3015 | ` * Return` |
|         - | 3016 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3017 | ` */` |
|        94 | 3018 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3019 | `{` |
|         - | 3020 | `	sxi32 rc;` |
|        99 | 3021 | `	if( nArg != 2 ){` |
|         - | 3022 | `		/* PHP requires exactly two arguments */` |
|        12 | 3023 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3024 | `			"ArgumentCountError",` |
|         - | 3025 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         3 | 3026 | `			nArg` |
|         - | 3027 | `			);` |
|         - | 3028 | `	}` |
|         - | 3029 | `	/* Make sure we are dealing with a valid hashmap */` |
|        93 | 3030 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3031 | `		/* Type mismatch -> TypeError */` |
|         8 | 3032 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3033 | `			"TypeError",` |
|         - | 3034 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3035 | `			ph7_type_name(apArg[1])` |
|         - | 3036 | `			);` |
|         - | 3037 | `	}` |
|         - | 3038 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        88 | 3039 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         3 | 3040 | `		ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3041 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3042 | `			"use an empty string instead"` |
|         - | 3043 | `			);` |
|        87 | 3044 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3045 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3046 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3047 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3048 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3049 | `				,rVal` |
|         - | 3050 | `				);` |
|         1 | 3051 | `		}` |
|         1 | 3052 | `	}` |
|         - | 3053 | `	/* Perform the lookup */` |
|        88 | 3054 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3055 | `	/* lookup result */` |
|        88 | 3056 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        88 | 3057 | `	return PH7_OK;` |
|        52 | 3058 | `}` |
|         - | 3059 | `/*` |
|         - | 3060 | ` * value array_pop(array $array)` |
|         - | 3061 | ` *   POP the last inserted element from the array.` |
|         - | 3062 | ` * Parameter` |
|         - | 3063 | ` *  The array to get the value from.` |
|         - | 3064 | ` * Return` |
|         - | 3065 | ` *  Poped value or NULL on failure.` |
|         - | 3066 | ` */` |
|        56 | 3067 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3068 | `{` |
|         - | 3069 | `	ph7_hashmap *pMap;` |
|         - | 3070 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        61 | 3071 | `	if( nArg != 1 ){` |
|         8 | 3072 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3073 | `			"ArgumentCountError",` |
|         - | 3074 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         2 | 3075 | `			nArg` |
|         - | 3076 | `			);` |
|         - | 3077 | `	}` |
|         - | 3078 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3079 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        56 | 3080 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3081 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3082 | `			"Error",` |
|         - | 3083 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3084 | `			);` |
|         - | 3085 | `	}` |
|         - | 3086 | `	/* Make sure we are dealing with a valid hashmap */` |
|        50 | 3087 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3088 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3089 | `			"TypeError",` |
|         - | 3090 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3091 | `			ph7_type_name(apArg[0])` |
|         - | 3092 | `			);` |
|         - | 3093 | `	}` |
|        47 | 3094 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        47 | 3095 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        47 | 3096 | `	if( pMap->nEntry < 1 ){` |
|         - | 3097 | `		/* Nothing to pop,return NULL */` |
|         3 | 3098 | `		ph7_result_null(pCtx);` |
|         2 | 3099 | `	}else{` |
|        45 | 3100 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3101 | `		ph7_value *pObj;` |
|        45 | 3102 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        45 | 3103 | `		if( pObj ){` |
|         - | 3104 | `			/* Node value */` |
|        45 | 3105 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3106 | `			/* Unlink the node */` |
|        45 | 3107 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        23 | 3108 | `		}else{` |
|       ! 0 | 3109 | `			ph7_result_null(pCtx);` |
|         - | 3110 | `		}` |
|         - | 3111 | `		/* Reset the cursor */` |
|        45 | 3112 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3113 | `	}` |
|        47 | 3114 | `	return PH7_OK;` |
|        33 | 3115 | `}` |
|         - | 3116 | `/*` |
|         - | 3117 | ` * int array_push($array,$var,...)` |
|         - | 3118 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3119 | ` * Parameters` |
|         - | 3120 | ` *  array` |
|         - | 3121 | ` *    The input array.` |
|         - | 3122 | ` *  var` |
|         - | 3123 | ` *   On or more value to push.` |
|         - | 3124 | ` * Return` |
|         - | 3125 | ` *  New array count (including old items).` |
|         - | 3126 | ` */` |
|        24 | 3127 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3128 | `{` |
|         - | 3129 | `	ph7_hashmap *pMap;` |
|         - | 3130 | `	sxi32 rc;` |
|         - | 3131 | `	int i;` |
|        29 | 3132 | `	if( nArg < 1 ){` |
|         4 | 3133 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3134 | `			"ArgumentCountError",` |
|         - | 3135 | `			"array_push() expects at least 1 argument, %d given",` |
|         1 | 3136 | `			nArg` |
|         - | 3137 | `			);` |
|         - | 3138 | `	}` |
|         - | 3139 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3140 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3141 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3142 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3143 | `			"Error",` |
|         - | 3144 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3145 | `			);` |
|         - | 3146 | `	}` |
|         - | 3147 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3148 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3149 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3150 | `			"TypeError",` |
|         - | 3151 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3152 | `			ph7_type_name(apArg[0])` |
|         - | 3153 | `			);` |
|         - | 3154 | `	}` |
|         - | 3155 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3156 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3157 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3158 | `	/* Start pushing given values */` |
|        34 | 3159 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3160 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3161 | `		if( rc != SXRET_OK ){` |
|         3 | 3162 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3163 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3164 | `				return rc;` |
|         - | 3165 | `			}` |
|       ! 0 | 3166 | `			break;` |
|         - | 3167 | `		}` |
|         9 | 3168 | `	}` |
|         - | 3169 | `	/* Return the new count */` |
|        15 | 3170 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3171 | `	return PH7_OK;` |
|        17 | 3172 | `}` |
|         - | 3173 | `/*` |
|         - | 3174 | ` * value array_shift(array $array)` |
|         - | 3175 | ` *   Shift an element off the beginning of array.` |
|         - | 3176 | ` * Parameter` |
|         - | 3177 | ` *  The array to get the value from.` |
|         - | 3178 | ` * Return` |
|         - | 3179 | ` *  Shifted value or NULL on failure.` |
|         - | 3180 | ` */` |
|        46 | 3181 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3182 | `{` |
|         - | 3183 | `	ph7_hashmap *pMap;` |
|         - | 3184 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        51 | 3185 | `	if( nArg != 1 ){` |
|         8 | 3186 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3187 | `			"ArgumentCountError",` |
|         - | 3188 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         2 | 3189 | `			nArg` |
|         - | 3190 | `			);` |
|         - | 3191 | `	}` |
|         - | 3192 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3193 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3194 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3195 | `			"Error",` |
|         - | 3196 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3197 | `			);` |
|         - | 3198 | `	}` |
|         - | 3199 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3200 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3201 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3202 | `			"TypeError",` |
|         - | 3203 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3204 | `			ph7_type_name(apArg[0])` |
|         - | 3205 | `			);` |
|         - | 3206 | `	}` |
|         - | 3207 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3208 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3209 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3210 | `	if( pMap->nEntry < 1 ){` |
|         - | 3211 | `		/* Empty hashmap,return NULL */` |
|         3 | 3212 | `		ph7_result_null(pCtx);` |
|         2 | 3213 | `	}else{` |
|        39 | 3214 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3215 | `		ph7_value *pObj;` |
|         - | 3216 | `		sxu32 n;` |
|        39 | 3217 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3218 | `		if( pObj ){` |
|         - | 3219 | `			/* Node value */` |
|        39 | 3220 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3221 | `			/* Unlink the first node */` |
|        39 | 3222 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3223 | `		}else{` |
|       ! 0 | 3224 | `			ph7_result_null(pCtx);` |
|         - | 3225 | `		}` |
|         - | 3226 | `		/* Rehash all int keys */` |
|        39 | 3227 | `		n = pMap->nEntry;` |
|        39 | 3228 | `		pEntry = pMap->pFirst;` |
|        39 | 3229 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|        47 | 3230 | `		for(;;){` |
|        99 | 3231 | `			if( n < 1 ){` |
|        39 | 3232 | `				break;` |
|         - | 3233 | `			}` |
|        65 | 3234 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3235 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3236 | `			}` |
|         - | 3237 | `			/* Point to the next entry */` |
|        65 | 3238 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3239 | `			n--;` |
|         5 | 3240 | `		}` |
|         - | 3241 | `		/* Reset the cursor */` |
|        39 | 3242 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3243 | `	}` |
|        41 | 3244 | `	return PH7_OK;` |
|        28 | 3245 | `}` |
|         - | 3246 | `/*` |
|         - | 3247 | ` * Extract the node cursor value.` |
|         - | 3248 | ` */` |
|       400 | 3249 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3250 | `{` |
|       401 | 3251 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3252 | `	ph7_value *pVal;` |
|       401 | 3253 | `	if( pCur == 0 ){` |
|         - | 3254 | `		/* Cursor does not point to anything,return FALSE */` |
|        15 | 3255 | `		ph7_result_bool(pCtx,0);` |
|        15 | 3256 | `		return PH7_OK;` |
|         - | 3257 | `	}` |
|       387 | 3258 | `	if( iDirection != 0 ){` |
|       129 | 3259 | `		if( iDirection > 0 ){` |
|         - | 3260 | `			/* Point to the next entry */` |
|       127 | 3261 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       127 | 3262 | `			pCur = pMap->pCur;` |
|        64 | 3263 | `		}else{` |
|         - | 3264 | `			/* Point to the previous entry */` |
|         3 | 3265 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3266 | `			pCur = pMap->pCur;` |
|         - | 3267 | `		}` |
|       129 | 3268 | `		if( pCur == 0 ){` |
|         - | 3269 | `			/* End of input reached,return FALSE */` |
|        47 | 3270 | `			ph7_result_bool(pCtx,0);` |
|        47 | 3271 | `			return PH7_OK;` |
|         - | 3272 | `		}` |
|        41 | 3273 | `	}` |
|         - | 3274 | `	/* Point to the desired element */` |
|       341 | 3275 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       341 | 3276 | `	if( pVal ){` |
|       341 | 3277 | `		ph7_result_value(pCtx,pVal);` |
|       171 | 3278 | `	}else{` |
|       ! 0 | 3279 | `		ph7_result_bool(pCtx,0);` |
|         - | 3280 | `	}` |
|       341 | 3281 | `	return PH7_OK;` |
|       201 | 3282 | `}` |
|         - | 3283 | `/*` |
|         - | 3284 | ` * value current(array $array)` |
|         - | 3285 | ` *  Return the current element in an array.` |
|         - | 3286 | ` * Parameter` |
|         - | 3287 | ` *  $input: The input array.` |
|         - | 3288 | ` * Return` |
|         - | 3289 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3290 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3291 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3292 | ` *  is empty, current() returns FALSE.` |
|         - | 3293 | ` */` |
|       132 | 3294 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3295 | `{` |
|       133 | 3296 | `	if( nArg < 1 ){` |
|         - | 3297 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3298 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3299 | `		return PH7_OK;` |
|         - | 3300 | `	}` |
|         - | 3301 | `	/* Make sure we are dealing with a valid hashmap */` |
|       133 | 3302 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3303 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3304 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3305 | `		return PH7_OK;` |
|         - | 3306 | `	}` |
|       133 | 3307 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       133 | 3308 | `	return PH7_OK;` |
|        67 | 3309 | `}` |
|         - | 3310 | `/*` |
|         - | 3311 | ` * value next(array $input)` |
|         - | 3312 | ` *  Advance the internal array pointer of an array.` |
|         - | 3313 | ` * Parameter` |
|         - | 3314 | ` *  $input: The input array.` |
|         - | 3315 | ` * Return` |
|         - | 3316 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3317 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3318 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3319 | ` */` |
|       126 | 3320 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3321 | `{` |
|       127 | 3322 | `	if( nArg < 1 ){` |
|         - | 3323 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3324 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3325 | `		return PH7_OK;` |
|         - | 3326 | `	}` |
|         - | 3327 | `	/* Make sure we are dealing with a valid hashmap */` |
|       127 | 3328 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3329 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3330 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3331 | `		return PH7_OK;` |
|         - | 3332 | `	}` |
|       127 | 3333 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       127 | 3334 | `	return PH7_OK;` |
|        64 | 3335 | `}` |
|         - | 3336 | `/*` |
|         - | 3337 | ` * value prev(array $input)` |
|         - | 3338 | ` *  Rewind the internal array pointer.` |
|         - | 3339 | ` * Parameter` |
|         - | 3340 | ` *  $input: The input array.` |
|         - | 3341 | ` * Return` |
|         - | 3342 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3343 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3344 | ` *  elements.` |
|         - | 3345 | ` */` |
|         2 | 3346 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3347 | `{` |
|         3 | 3348 | `	if( nArg < 1 ){` |
|         - | 3349 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3350 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3351 | `		return PH7_OK;` |
|         - | 3352 | `	}` |
|         - | 3353 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3354 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3355 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3356 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3357 | `		return PH7_OK;` |
|         - | 3358 | `	}` |
|         3 | 3359 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3360 | `	return PH7_OK;` |
|         2 | 3361 | `}` |
|         - | 3362 | `/*` |
|         - | 3363 | ` * value end(array $input)` |
|         - | 3364 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3365 | ` * Parameter` |
|         - | 3366 | ` *  $input: The input array.` |
|         - | 3367 | ` * Return` |
|         - | 3368 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3369 | ` */` |
|         2 | 3370 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3371 | `{` |
|         - | 3372 | `	ph7_hashmap *pMap;` |
|         3 | 3373 | `	if( nArg < 1 ){` |
|         - | 3374 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3375 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3376 | `		return PH7_OK;` |
|         - | 3377 | `	}` |
|         - | 3378 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3379 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3380 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3381 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3382 | `		return PH7_OK;` |
|         - | 3383 | `	}` |
|         - | 3384 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3385 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3386 | `	/* Point to the last node */` |
|         3 | 3387 | `	pMap->pCur = pMap->pLast;` |
|         - | 3388 | `	/* Return the last node value */` |
|         3 | 3389 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         3 | 3390 | `	return PH7_OK;` |
|         2 | 3391 | `}` |
|         - | 3392 | `/*` |
|         - | 3393 | ` * value reset(array $array )` |
|         - | 3394 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3395 | ` * Parameter` |
|         - | 3396 | ` *  $input: The input array.` |
|         - | 3397 | ` * Return` |
|         - | 3398 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3399 | ` */` |
|       138 | 3400 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3401 | `{` |
|         - | 3402 | `	ph7_hashmap *pMap;` |
|       139 | 3403 | `	if( nArg < 1 ){` |
|         - | 3404 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3405 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3406 | `		return PH7_OK;` |
|         - | 3407 | `	}` |
|         - | 3408 | `	/* Make sure we are dealing with a valid hashmap */` |
|       139 | 3409 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3410 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3411 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3412 | `		return PH7_OK;` |
|         - | 3413 | `	}` |
|         - | 3414 | `	/* Point to the internal representation of the input hashmap */` |
|       139 | 3415 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3416 | `	/* Point to the first node */` |
|       139 | 3417 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3418 | `	/* Return the last node value if available */` |
|       139 | 3419 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       139 | 3420 | `	return PH7_OK;` |
|        70 | 3421 | `}` |
|         - | 3422 | `/*` |
|         - | 3423 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3424 | ` * array_key_first() and array_key_last().` |
|         - | 3425 | ` */` |
|       374 | 3426 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3427 | `{` |
|       375 | 3428 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3429 | `		/* Key is integer */` |
|       283 | 3430 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3431 | `	}else{` |
|         - | 3432 | `		/* Key is blob */` |
|       139 | 3433 | `		ph7_result_string(pCtx,` |
|        92 | 3434 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3435 | `	}` |
|       375 | 3436 | `}` |
|         - | 3437 | `/*` |
|         - | 3438 | ` * value key(array $array)` |
|         - | 3439 | ` *   Fetch a key from an array` |
|         - | 3440 | ` * Parameter` |
|         - | 3441 | ` *  $input` |
|         - | 3442 | ` *   The input array.` |
|         - | 3443 | ` * Return` |
|         - | 3444 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3445 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3446 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3447 | ` *  is empty, key() returns NULL.` |
|         - | 3448 | ` */` |
|       430 | 3449 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3450 | `{` |
|         - | 3451 | `	ph7_hashmap_node *pCur;` |
|         - | 3452 | `	ph7_hashmap *pMap;` |
|       431 | 3453 | `	if( nArg < 1 ){` |
|         - | 3454 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3455 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3456 | `		return PH7_OK;` |
|         - | 3457 | `	}` |
|         - | 3458 | `	/* Make sure we are dealing with a valid hashmap */` |
|       431 | 3459 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3460 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3461 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3462 | `		return PH7_OK;` |
|         - | 3463 | `	}` |
|       431 | 3464 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       431 | 3465 | `	pCur = pMap->pCur;` |
|       431 | 3466 | `	if( pCur == 0 ){` |
|         - | 3467 | `		/* Cursor does not point to anything,return NULL */` |
|        73 | 3468 | `		ph7_result_null(pCtx);` |
|        73 | 3469 | `		return PH7_OK;` |
|         - | 3470 | `	}` |
|       359 | 3471 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       359 | 3472 | `	return PH7_OK;` |
|       216 | 3473 | `}` |
|         - | 3474 | `/*` |
|         - | 3475 | ` * array each(array $input)` |
|         - | 3476 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3477 | ` * Parameter` |
|         - | 3478 | ` *  $input` |
|         - | 3479 | ` *    The input array.` |
|         - | 3480 | ` * Return` |
|         - | 3481 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3482 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3483 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3484 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3485 | ` *  each() returns FALSE.` |
|         - | 3486 | ` */` |
|        22 | 3487 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3488 | `{` |
|         - | 3489 | `	ph7_hashmap_node *pCur;` |
|         - | 3490 | `	ph7_hashmap *pMap;` |
|         - | 3491 | `	ph7_value *pArray;` |
|         - | 3492 | `	ph7_value *pVal;` |
|         - | 3493 | `	ph7_value sKey;` |
|        23 | 3494 | `	if( nArg < 1 ){` |
|         - | 3495 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3496 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3497 | `		return PH7_OK;` |
|         - | 3498 | `	}` |
|         - | 3499 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3500 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3501 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3502 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3503 | `		return PH7_OK;` |
|         - | 3504 | `	}` |
|         - | 3505 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3506 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3507 | `	if( pMap->pCur == 0 ){` |
|         - | 3508 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3509 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3510 | `		return PH7_OK;` |
|         - | 3511 | `	}` |
|        15 | 3512 | `	pCur = pMap->pCur;` |
|         - | 3513 | `	/* Create a new array */` |
|        15 | 3514 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3515 | `	if( pArray == 0 ){` |
|       ! 0 | 3516 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3517 | `		return PH7_OK;` |
|         - | 3518 | `	}` |
|        15 | 3519 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3520 | `	/* Insert the current value */` |
|        15 | 3521 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3522 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3523 | `	/* Make the key */` |
|        15 | 3524 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3525 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3526 | `	}else{` |
|         9 | 3527 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3528 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3529 | `	}` |
|         - | 3530 | `	/* Insert the current key */` |
|        15 | 3531 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3532 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3533 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3534 | `	/* Advance the cursor */` |
|        15 | 3535 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3536 | `	/* Return the current entry */` |
|        15 | 3537 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3538 | `	return PH7_OK;` |
|        12 | 3539 | `}` |
|         - | 3540 | `/*` |
|         - | 3541 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3542 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3543 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3544 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3545 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3546 | ` */` |
|         - | 3547 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3548 | `/*` |
|         - | 3549 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3550 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3551 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3552 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3553 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3554 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3555 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3556 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3557 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3558 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3559 | ` */` |
|         - | 3560 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3561 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3562 | `/*` |
|         - | 3563 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3564 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3565 | ` */` |
|         8 | 3566 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|         1 | 3567 | `{` |
|         9 | 3568 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|         3 | 3569 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|         3 | 3570 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|         3 | 3571 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|         3 | 3572 | `		zBuf[n] = 0;` |
|         3 | 3573 | `		return zBuf;` |
|         - | 3574 | `	}` |
|         7 | 3575 | `	return ph7_type_name(pVal);` |
|         5 | 3576 | `}` |
|         - | 3577 | `/*` |
|         - | 3578 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3579 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3580 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3581 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3582 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3583 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3584 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3585 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3586 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3587 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3588 | ` */` |
|       156 | 3589 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3590 | `{` |
|       157 | 3591 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3592 | `	sxu64 uVal = 0;` |
|       157 | 3593 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3594 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3595 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3596 | `		bNeg = (z[0] == '-');` |
|         3 | 3597 | `		z++;` |
|         1 | 3598 | `	}` |
|       237 | 3599 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3600 | `		int d = z[0] - '0';` |
|         - | 3601 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3602 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3603 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3604 | `			bOverflow = 1;` |
|       ! 0 | 3605 | `		}else{` |
|        81 | 3606 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3607 | `		}` |
|        81 | 3608 | `		bDigit = 1;` |
|        81 | 3609 | `		z++;` |
|         1 | 3610 | `	}` |
|       157 | 3611 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3612 | `		bReal = 1;` |
|         3 | 3613 | `		z++;` |
|         5 | 3614 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3615 | `			bDigit = 1;` |
|         3 | 3616 | `			z++;` |
|         1 | 3617 | `		}` |
|         1 | 3618 | `	}` |
|         - | 3619 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3620 | `	if( !bDigit ){` |
|        61 | 3621 | `		return RANGE_IN_ERROR;` |
|         - | 3622 | `	}` |
|         - | 3623 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3624 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3625 | `		z++;` |
|         9 | 3626 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3627 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3628 | `			return RANGE_IN_ERROR;` |
|         - | 3629 | `		}` |
|         9 | 3630 | `		bReal = 1;` |
|        17 | 3631 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3632 | `	}` |
|         - | 3633 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3634 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3635 | `	if( z != zEnd ){` |
|        13 | 3636 | `		return RANGE_IN_ERROR;` |
|         - | 3637 | `	}` |
|        84 | 3638 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3639 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3640 | `		bReal = 1;` |
|        84 | 3641 | `	}` |
|        43 | 3642 | `	if( bReal ){` |
|        11 | 3643 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3644 | `		return RANGE_IN_DOUBLE;` |
|         - | 3645 | `	}` |
|         - | 3646 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3647 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3648 | `	return RANGE_IN_LONG;` |
|        58 | 3649 | `}` |
|         - | 3650 | `/*` |
|         - | 3651 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3652 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3653 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3654 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3655 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3656 | ` */` |
|       338 | 3657 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3658 | `{` |
|         - | 3659 | `	char zMsg[160];` |
|       339 | 3660 | `	*pRc = PH7_OK;` |
|       339 | 3661 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3662 | `		char zType[80];` |
|        10 | 3663 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3664 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|         3 | 3665 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         7 | 3666 | `		return FALSE;` |
|         - | 3667 | `	}` |
|       333 | 3668 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3669 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3670 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3671 | `			iArg,zName);` |
|         5 | 3672 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3673 | `		*pbNullCoerced = TRUE;` |
|         2 | 3674 | `	}` |
|       333 | 3675 | `	return TRUE;` |
|       170 | 3676 | `}` |
|         - | 3677 | `/*` |
|         - | 3678 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3679 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3680 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3681 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3682 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3683 | ` */` |
|        62 | 3684 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3685 | `{` |
|        63 | 3686 | `	*pRc = PH7_OK;` |
|        63 | 3687 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3688 | `		char zType[80];` |
|         4 | 3689 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3690 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|         1 | 3691 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         3 | 3692 | `		return RANGE_IN_ERROR;` |
|         - | 3693 | `	}` |
|        61 | 3694 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3695 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3696 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3697 | `		*pLong = 0;` |
|         3 | 3698 | `		return RANGE_IN_LONG;` |
|         - | 3699 | `	}` |
|        59 | 3700 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3701 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3702 | `		return RANGE_IN_DOUBLE;` |
|         - | 3703 | `	}` |
|        35 | 3704 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3705 | `		const char *zStr;` |
|         - | 3706 | `		int nLen;` |
|         - | 3707 | `		sxu8 iKind;` |
|         3 | 3708 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3709 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3710 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3711 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3712 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3713 | `		}` |
|         3 | 3714 | `		return iKind;` |
|         - | 3715 | `	}` |
|         - | 3716 | `	/* int / bool */` |
|        33 | 3717 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3718 | `	return RANGE_IN_LONG;` |
|        32 | 3719 | `}` |
|         - | 3720 | `/*` |
|         - | 3721 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3722 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3723 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3724 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3725 | ` */` |
|       296 | 3726 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3727 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3728 | `{` |
|         - | 3729 | `	char zMsg[160];` |
|         - | 3730 | `	double r;` |
|       297 | 3731 | `	*pRc = PH7_OK;` |
|       297 | 3732 | `	if( bNullCoerced ){` |
|         - | 3733 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3734 | `		*pLong = 0;` |
|         5 | 3735 | `		*pDouble = 0.0;` |
|         5 | 3736 | `		return RANGE_IN_LONG;` |
|         - | 3737 | `	}` |
|       293 | 3738 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3739 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3740 | `check_dval:` |
|        25 | 3741 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3742 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3743 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3744 | `			return RANGE_IN_ERROR;` |
|         - | 3745 | `		}` |
|        21 | 3746 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3747 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3748 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3749 | `			return RANGE_IN_ERROR;` |
|         - | 3750 | `		}` |
|        17 | 3751 | `		*pDouble = r;` |
|        17 | 3752 | `		return RANGE_IN_DOUBLE;` |
|         - | 3753 | `	}` |
|       273 | 3754 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3755 | `		const char *zStr;` |
|         - | 3756 | `		int nLen;` |
|         - | 3757 | `		sxu8 iKind;` |
|        81 | 3758 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3759 | `		if( nLen == 0 ){` |
|         7 | 3760 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3761 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3762 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3763 | `			*pLong = 0;` |
|         5 | 3764 | `			*pDouble = 0.0;` |
|        41 | 3765 | `			return RANGE_IN_LONG;` |
|         - | 3766 | `		}` |
|        77 | 3767 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3768 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3769 | `			r = *pDouble;` |
|         5 | 3770 | `			goto check_dval;` |
|         - | 3771 | `		}` |
|        73 | 3772 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3773 | `			*pDouble = (double)*pLong;` |
|        23 | 3774 | `			if( nLen == 1 ){` |
|         - | 3775 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3776 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3777 | `				return RANGE_IN_DIGIT;` |
|         - | 3778 | `			}` |
|        15 | 3779 | `			return RANGE_IN_LONG;` |
|         - | 3780 | `		}` |
|        51 | 3781 | `		if( nLen != 1 ){` |
|        10 | 3782 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3783 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3784 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3785 | `		}` |
|        51 | 3786 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3787 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3788 | `		*pLong = 0;` |
|        51 | 3789 | `		*pDouble = 0.0;` |
|        51 | 3790 | `		return RANGE_IN_STRING;` |
|         - | 3791 | `	}` |
|         - | 3792 | `	/* int / bool */` |
|       193 | 3793 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3794 | `	*pDouble = (double)*pLong;` |
|       193 | 3795 | `	return RANGE_IN_LONG;` |
|       149 | 3796 | `}` |
|         - | 3797 | `/*` |
|         - | 3798 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3799 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3800 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3801 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3802 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3803 | ` * exactly like php's two macros.` |
|         - | 3804 | ` */` |
|         6 | 3805 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3806 | `{` |
|        10 | 3807 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3808 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3809 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3810 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3811 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3812 | `}` |
|         6 | 3813 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3814 | `{` |
|         - | 3815 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3816 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3817 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3818 | `	const unsigned int nBuf = 1500;` |
|         7 | 3819 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3820 | `	if( zMsg == 0 ){` |
|       ! 0 | 3821 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3822 | `	}` |
|         7 | 3823 | `	snprintf(zMsg,nBuf,` |
|         - | 3824 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3825 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3826 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3827 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3828 | `}` |
|         - | 3829 | `/*` |
|         - | 3830 | ` * Set the element container to the next range element and append it to the` |
|         - | 3831 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3832 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3833 | ` * below stay one line per iteration.` |
|         - | 3834 | ` */` |
|      1680 | 3835 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3836 | `{` |
|      1681 | 3837 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3838 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3839 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3840 | `	}` |
|      1681 | 3841 | `	return PH7_OK;` |
|       841 | 3842 | `}` |
|        70 | 3843 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3844 | `{` |
|        71 | 3845 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3846 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3847 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3848 | `	}` |
|        71 | 3849 | `	return PH7_OK;` |
|        36 | 3850 | `}` |
|       168 | 3851 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3852 | `{` |
|       169 | 3853 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3854 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3855 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3856 | `	}` |
|       169 | 3857 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3858 | `	return PH7_OK;` |
|        85 | 3859 | `}` |
|         - | 3860 | `/*` |
|         - | 3861 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3862 | ` *  Create an array containing a range of elements.` |
|         - | 3863 | ` * Return` |
|         - | 3864 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3865 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3866 | ` */` |
|       174 | 3867 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3868 | `{` |
|         - | 3869 | `	ph7_value *pValue,*pArray;` |
|       175 | 3870 | `	sxi32 rc = PH7_OK;` |
|       175 | 3871 | `	int is_step_double = 0,is_step_negative = 0;` |
|       175 | 3872 | `	double step_double = 1.0;` |
|       175 | 3873 | `	sxi64 step = 1;` |
|         - | 3874 | `	sxu8 start_type,end_type;` |
|       175 | 3875 | `	sxi64 start_long = 0,end_long = 0;` |
|       175 | 3876 | `	double start_double = 0.0,end_double = 0.0;` |
|       175 | 3877 | `	unsigned char cStart = 0,cEnd = 0;` |
|       175 | 3878 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3879 | `	sxu32 i,size;` |
|         - | 3880 |  |
|         - | 3881 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       175 | 3882 | `	if( nArg > 3 ){` |
|         4 | 3883 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3884 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3885 | `	}` |
|       173 | 3886 | `	if( nArg < 2 ){` |
|         - | 3887 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3888 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3889 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3890 | `	}` |
|         - | 3891 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3892 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       173 | 3893 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|         7 | 3894 | `		return rc;` |
|         - | 3895 | `	}` |
|       167 | 3896 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3897 | `		return rc;` |
|         - | 3898 | `	}` |
|       167 | 3899 | `	if( nArg > 2 ){` |
|        63 | 3900 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        63 | 3901 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         5 | 3902 | `			return rc;` |
|         - | 3903 | `		}` |
|        59 | 3904 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3905 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3906 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3907 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3908 | `			}` |
|        23 | 3909 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3910 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3911 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3912 | `			}` |
|         - | 3913 | `			/* We only want positive step values. */` |
|        21 | 3914 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3915 | `				is_step_negative = 1;` |
|       ! 0 | 3916 | `				step_double *= -1;` |
|       ! 0 | 3917 | `			}` |
|         - | 3918 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3919 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3920 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3921 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3922 | `				step = (sxi64)step_double;` |
|        19 | 3923 | `				if( (double)step != step_double ){` |
|        17 | 3924 | `					is_step_double = 1;` |
|         8 | 3925 | `				}` |
|        10 | 3926 | `			}else{` |
|         - | 3927 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3928 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3929 | `				is_step_double = 1;` |
|         - | 3930 | `			}` |
|        11 | 3931 | `		}else{` |
|         - | 3932 | `			/* We only want positive step values. */` |
|        35 | 3933 | `			if( step < 0 ){` |
|        11 | 3934 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3935 | `					/* -step would overflow */` |
|         4 | 3936 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3937 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3938 | `				}` |
|         9 | 3939 | `				is_step_negative = 1;` |
|         9 | 3940 | `				step = -step;` |
|         4 | 3941 | `			}` |
|        33 | 3942 | `			step_double = (double)step;` |
|         - | 3943 | `		}` |
|        53 | 3944 | `		if( step_double == 0.0 ){` |
|         7 | 3945 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3946 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3947 | `		}` |
|        23 | 3948 | `	}` |
|       151 | 3949 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 3950 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3951 | `		return rc;` |
|         - | 3952 | `	}` |
|       147 | 3953 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 3954 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3955 | `		return rc;` |
|         - | 3956 | `	}` |
|         - | 3957 | `	/* Element container + result array */` |
|       143 | 3958 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 3959 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 3960 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3961 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3962 | `	}` |
|         - | 3963 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 3964 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3965 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3966 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3967 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3968 | `			 * and the range is numeric. */` |
|        15 | 3969 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3970 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3971 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3972 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3973 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3974 | `				}` |
|         7 | 3975 | `				end_type = RANGE_IN_LONG;` |
|         4 | 3976 | `			}else{` |
|         9 | 3977 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 3978 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3979 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 3980 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 3981 | `				}` |
|         9 | 3982 | `				start_type = RANGE_IN_LONG;` |
|         - | 3983 | `			}` |
|        15 | 3984 | `			goto handle_numeric_inputs;` |
|         - | 3985 | `		}` |
|        23 | 3986 | `		if( is_step_double ){` |
|         - | 3987 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 3988 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 3989 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3990 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 3991 | `					" of characters, inputs converted to 0");` |
|         1 | 3992 | `			}` |
|         5 | 3993 | `			start_type = RANGE_IN_LONG;` |
|         5 | 3994 | `			end_type = RANGE_IN_LONG;` |
|         5 | 3995 | `			goto handle_numeric_inputs;` |
|         - | 3996 | `		}` |
|         - | 3997 | `		/* Generate an array of characters */` |
|        19 | 3998 | `		if( cStart > cEnd ){` |
|         - | 3999 | `			/* Decreasing char range */` |
|         - | 4000 | `			int iCur;` |
|         3 | 4001 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4002 | `				goto boundary_error;` |
|         - | 4003 | `			}` |
|        17 | 4004 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4005 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4006 | `					return rc;` |
|         - | 4007 | `				}` |
|         8 | 4008 | `			}` |
|        18 | 4009 | `		}else if( cEnd > cStart ){` |
|         - | 4010 | `			/* Increasing char range */` |
|         - | 4011 | `			int iCur;` |
|        15 | 4012 | `			if( is_step_negative ){` |
|         3 | 4013 | `				goto negative_step_error;` |
|         - | 4014 | `			}` |
|        13 | 4015 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4016 | `				goto boundary_error;` |
|         - | 4017 | `			}` |
|       163 | 4018 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4019 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4020 | `					return rc;` |
|         - | 4021 | `				}` |
|        77 | 4022 | `			}` |
|         6 | 4023 | `		}else{` |
|         3 | 4024 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4025 | `				return rc;` |
|         - | 4026 | `			}` |
|         - | 4027 | `		}` |
|        15 | 4028 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4029 | `		return PH7_OK;` |
|         - | 4030 | `	}` |
|        53 | 4031 | `handle_numeric_inputs:` |
|       133 | 4032 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4033 | `		/* Float range */` |
|         - | 4034 | `		double elem,calc;` |
|        25 | 4035 | `		if( start_double > end_double ){` |
|         - | 4036 | `			/* Decreasing float range */` |
|         7 | 4037 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4038 | `				goto boundary_error;` |
|         - | 4039 | `			}` |
|         7 | 4040 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4041 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4042 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4043 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4044 | `			}` |
|         5 | 4045 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4046 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4047 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4048 | `					return rc;` |
|         - | 4049 | `				}` |
|         8 | 4050 | `			}` |
|        21 | 4051 | `		}else if( end_double > start_double ){` |
|         - | 4052 | `			/* Increasing float range */` |
|        17 | 4053 | `			if( is_step_negative ){` |
|       ! 0 | 4054 | `				goto negative_step_error;` |
|         - | 4055 | `			}` |
|        17 | 4056 | `			if( end_double - start_double < step_double ){` |
|         3 | 4057 | `				goto boundary_error;` |
|         - | 4058 | `			}` |
|        15 | 4059 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4060 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4061 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4062 | `			}` |
|        11 | 4063 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4064 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4065 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4066 | `					return rc;` |
|         - | 4067 | `				}` |
|        28 | 4068 | `			}` |
|         6 | 4069 | `		}else{` |
|         3 | 4070 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4071 | `				return rc;` |
|         - | 4072 | `			}` |
|         - | 4073 | `		}` |
|         9 | 4074 | `	}else{` |
|         - | 4075 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4076 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4077 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4078 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4079 | `		sxu64 calc;` |
|       101 | 4080 | `		if( start_long > end_long ){` |
|         - | 4081 | `			/* Decreasing int range */` |
|        19 | 4082 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4083 | `				goto boundary_error;` |
|         - | 4084 | `			}` |
|        17 | 4085 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4086 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4087 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4088 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4089 | `			}` |
|        15 | 4090 | `			size = (sxu32)(calc + 1);` |
|       101 | 4091 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4092 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4093 | `					return rc;` |
|         - | 4094 | `				}` |
|        44 | 4095 | `			}` |
|        90 | 4096 | `		}else if( end_long > start_long ){` |
|         - | 4097 | `			/* Increasing int range */` |
|        77 | 4098 | `			if( is_step_negative ){` |
|         3 | 4099 | `				goto negative_step_error;` |
|         - | 4100 | `			}` |
|        75 | 4101 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4102 | `				goto boundary_error;` |
|         - | 4103 | `			}` |
|        73 | 4104 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4105 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4106 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4107 | `			}` |
|        69 | 4108 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4109 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4110 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4111 | `					return rc;` |
|         - | 4112 | `				}` |
|       795 | 4113 | `			}` |
|        35 | 4114 | `		}else{` |
|         7 | 4115 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4116 | `				return rc;` |
|         - | 4117 | `			}` |
|         - | 4118 | `		}` |
|         - | 4119 | `	}` |
|         - | 4120 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4121 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4122 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4123 | `	return PH7_OK;` |
|         2 | 4124 | `negative_step_error:` |
|         5 | 4125 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4126 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4127 | `boundary_error:` |
|         9 | 4128 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4129 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        88 | 4130 | `}` |
|         - | 4131 | `/*` |
|         - | 4132 | ` * array array_values(array $array)` |
|         - | 4133 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4134 | ` * Parameters` |
|         - | 4135 | ` *  $array` |
|         - | 4136 | ` *   The input array.` |
|         - | 4137 | ` * Return` |
|         - | 4138 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4139 | ` */` |
|        50 | 4140 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4141 | `{` |
|         - | 4142 | `	ph7_hashmap_node *pNode;` |
|         - | 4143 | `	ph7_hashmap *pMap;` |
|         - | 4144 | `	ph7_value *pArray;` |
|         - | 4145 | `	ph7_value *pObj;` |
|         - | 4146 | `	sxu32 n;` |
|        54 | 4147 | `	if( nArg != 1 ){` |
|         - | 4148 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4149 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4150 | `			"ArgumentCountError",` |
|         - | 4151 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4152 | `			nArg` |
|         - | 4153 | `			);` |
|         - | 4154 | `	}` |
|         - | 4155 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4156 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4157 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4158 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4159 | `			"TypeError",` |
|         - | 4160 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4161 | `			ph7_type_name(apArg[0])` |
|         - | 4162 | `			);` |
|         - | 4163 | `	}` |
|         - | 4164 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4165 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4166 | `	/* Create a new array */` |
|        46 | 4167 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4168 | `	if( pArray == 0 ){` |
|       ! 0 | 4169 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4170 | `		return PH7_OK;` |
|         - | 4171 | `	}` |
|         - | 4172 | `	/* Perform the requested operation */` |
|        46 | 4173 | `	pNode = pMap->pFirst;` |
|       144 | 4174 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4175 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4176 | `		if( pObj ){` |
|         - | 4177 | `			/* perform the insertion */` |
|       100 | 4178 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4179 | `		}` |
|         - | 4180 | `		/* Point to the next entry */` |
|       100 | 4181 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4182 | `	}` |
|         - | 4183 | `	/* return the new array */` |
|        46 | 4184 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4185 | `	return PH7_OK;` |
|        29 | 4186 | `}` |
|         - | 4187 | `/*` |
|         - | 4188 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4189 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4190 | ` * Parameters` |
|         - | 4191 | ` *  $input` |
|         - | 4192 | ` *   An array containing keys to return.` |
|         - | 4193 | ` * $search_value` |
|         - | 4194 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4195 | ` * $strict` |
|         - | 4196 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4197 | ` * Return` |
|         - | 4198 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4199 | ` */` |
|       162 | 4200 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4201 | `{` |
|         - | 4202 | `	ph7_hashmap_node *pNode;` |
|         - | 4203 | `	ph7_hashmap *pMap;` |
|         - | 4204 | `	ph7_value *pArray;` |
|         - | 4205 | `	ph7_value sObj;` |
|         - | 4206 | `	ph7_value sVal;` |
|         - | 4207 | `	SyString sKey;` |
|         - | 4208 | `	int bStrict;` |
|         - | 4209 | `	sxi32 rc;` |
|         - | 4210 | `	sxu32 n;` |
|       166 | 4211 | `	if( nArg < 1 ){` |
|         - | 4212 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4213 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4214 | `			"ArgumentCountError",` |
|         - | 4215 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4216 | `			);` |
|         - | 4217 | `	}` |
|         - | 4218 | `	/* Make sure we are dealing with a valid hashmap */` |
|       164 | 4219 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4220 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4221 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4222 | `			"TypeError",` |
|         - | 4223 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4224 | `			ph7_type_name(apArg[0])` |
|         - | 4225 | `			);` |
|         - | 4226 | `	}` |
|         - | 4227 | `	/* Point to the internal representation of the input hashmap */` |
|       162 | 4228 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4229 | `	/* Create a new array */` |
|       162 | 4230 | `	pArray = ph7_context_new_array(pCtx);` |
|       162 | 4231 | `	if( pArray == 0 ){` |
|       ! 0 | 4232 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4233 | `		return PH7_OK;` |
|         - | 4234 | `	}` |
|       162 | 4235 | `	bStrict = FALSE;` |
|       162 | 4236 | `	if( nArg > 2 ){` |
|         - | 4237 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        12 | 4238 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4239 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4240 | `				"TypeError",` |
|         - | 4241 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4242 | `				ph7_type_name(apArg[2])` |
|         - | 4243 | `				);` |
|         - | 4244 | `		}` |
|         9 | 4245 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4246 | `	}` |
|         - | 4247 | `	/* Perform the requested operation */` |
|       160 | 4248 | `	pNode = pMap->pFirst;` |
|       160 | 4249 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1456 | 4250 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1300 | 4251 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       179 | 4252 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        91 | 4253 | `		}else{` |
|      1122 | 4254 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4255 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4256 | `		}` |
|      1300 | 4257 | `		rc = 0;` |
|      1300 | 4258 | `		if( nArg > 1 ){` |
|        65 | 4259 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4260 | `			if( pValue ){` |
|         - | 4261 | `				ph7_value sNeedle;` |
|        65 | 4262 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4263 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4264 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4265 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4266 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4267 | `				 * corrupt every later comparison. */` |
|        65 | 4268 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4269 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4270 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4271 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4272 | `			}` |
|        32 | 4273 | `		}` |
|      1300 | 4274 | `		if( rc == 0 ){` |
|         - | 4275 | `			/* Perform the insertion */` |
|      1268 | 4276 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       632 | 4277 | `		}` |
|      1300 | 4278 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4279 | `		/* Point to the next entry */` |
|      1300 | 4280 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       652 | 4281 | `	}` |
|         - | 4282 | `	/* return the new array */` |
|       160 | 4283 | `	ph7_result_value(pCtx,pArray);` |
|       160 | 4284 | `	return PH7_OK;` |
|        85 | 4285 | `}` |
|         - | 4286 | `/*` |
|         - | 4287 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4288 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4289 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4290 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4291 | ` * Parameters` |
|         - | 4292 | ` *  $arr1` |
|         - | 4293 | ` *   First array` |
|         - | 4294 | ` *  $arr2` |
|         - | 4295 | ` *   Second array` |
|         - | 4296 | ` * Return` |
|         - | 4297 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4298 | ` * Note` |
|         - | 4299 | ` *  This function is a symisc eXtension.` |
|         - | 4300 | ` */` |
|         4 | 4301 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4302 | `{` |
|         - | 4303 | `	ph7_hashmap *p1,*p2;` |
|         - | 4304 | `	int rc;` |
|         5 | 4305 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4306 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4307 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4308 | `		return PH7_OK;` |
|         - | 4309 | `	}` |
|         - | 4310 | `	/* Point to the hashmaps */` |
|         5 | 4311 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4312 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4313 | `	rc = (p1 == p2);` |
|         - | 4314 | `	/* Same instance? */` |
|         5 | 4315 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4316 | `	return PH7_OK;` |
|         3 | 4317 | `}` |
|         - | 4318 | `/*` |
|         - | 4319 | ` * array array_merge(array ...$arrays)` |
|         - | 4320 | ` *  Merge one or more arrays.` |
|         - | 4321 | ` * Parameters` |
|         - | 4322 | ` *  ...$arrays` |
|         - | 4323 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4324 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4325 | ` * Return` |
|         - | 4326 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4327 | ` *  with no arguments.` |
|         - | 4328 | ` */` |
|      1042 | 4329 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4330 | `{` |
|         - | 4331 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4332 | `	ph7_value *pArray;` |
|         - | 4333 | `	int i;` |
|         - | 4334 | `	/* Create a new array */` |
|      1047 | 4335 | `	pArray = ph7_context_new_array(pCtx);` |
|      1047 | 4336 | `	if( pArray == 0 ){` |
|       ! 0 | 4337 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4338 | `		return PH7_OK;` |
|         - | 4339 | `	}` |
|         - | 4340 | `	/* Point to the internal representation of the hashmap */` |
|      1047 | 4341 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4342 | `	/* Start merging */` |
|      3121 | 4343 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4344 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2083 | 4345 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4346 | `			/* Type mismatch -> TypeError */` |
|         8 | 4347 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4348 | `				"TypeError",` |
|         - | 4349 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4350 | `				i + 1,` |
|         4 | 4351 | `				ph7_type_name(apArg[i])` |
|         - | 4352 | `				);` |
|       ! 0 | 4353 | `		}else{` |
|      2079 | 4354 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4355 | `			/* Merge the two hashmaps */` |
|      2079 | 4356 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4357 | `		}` |
|      1042 | 4358 | `	}` |
|         - | 4359 | `	/* Return the freshly created array */` |
|      1043 | 4360 | `	ph7_result_value(pCtx,pArray);` |
|      1043 | 4361 | `	return PH7_OK;` |
|       526 | 4362 | `}` |
|         - | 4363 | `/*` |
|         - | 4364 | ` * array array_copy(array $source)` |
|         - | 4365 | ` *  Make a blind copy of the target array.` |
|         - | 4366 | ` * Parameters` |
|         - | 4367 | ` *  $source` |
|         - | 4368 | ` *   Target array` |
|         - | 4369 | ` * Return` |
|         - | 4370 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4371 | ` * Note` |
|         - | 4372 | ` *  This function is a symisc eXtension.` |
|         - | 4373 | ` */` |
|        18 | 4374 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4375 | `{` |
|         - | 4376 | `	ph7_hashmap *pMap;` |
|         - | 4377 | `	ph7_value *pArray;` |
|        19 | 4378 | `	if( nArg < 1 ){` |
|         - | 4379 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4380 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4381 | `		return PH7_OK;` |
|         - | 4382 | `	}` |
|         - | 4383 | `	/* Create a new array */` |
|        19 | 4384 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4385 | `	if( pArray == 0 ){` |
|       ! 0 | 4386 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4387 | `		return PH7_OK;` |
|         - | 4388 | `	}` |
|         - | 4389 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4390 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4391 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4392 | `		/* Point to the internal representation of the source */` |
|        19 | 4393 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4394 | `		/* Perform the copy */` |
|        19 | 4395 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4396 | `	}else{` |
|         - | 4397 | `		/* Simple insertion */` |
|       ! 0 | 4398 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4399 | `	}` |
|         - | 4400 | `	/* Return the duplicated array */` |
|        19 | 4401 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4402 | `	return PH7_OK;` |
|        10 | 4403 | `}` |
|         - | 4404 | `/*` |
|         - | 4405 | ` * bool array_erase(array $source)` |
|         - | 4406 | ` *  Remove all elements from a given array.` |
|         - | 4407 | ` * Parameters` |
|         - | 4408 | ` *  $source` |
|         - | 4409 | ` *   Target array` |
|         - | 4410 | ` * Return` |
|         - | 4411 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4412 | ` * Note` |
|         - | 4413 | ` *  This function is a symisc eXtension.` |
|         - | 4414 | ` */` |
|        26 | 4415 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4416 | `{` |
|         - | 4417 | `	ph7_hashmap *pMap;` |
|        28 | 4418 | `	if( nArg < 1 ){` |
|         - | 4419 | `		/* Missing arguments */` |
|       ! 0 | 4420 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4421 | `		return PH7_OK;` |
|         - | 4422 | `	}` |
|         - | 4423 | `	/* Point to the target hashmap */` |
|        28 | 4424 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4425 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4426 | `	/* Erase */` |
|        28 | 4427 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4428 | `	return PH7_OK;` |
|        15 | 4429 | `}` |
|         - | 4430 | `/*` |
|         - | 4431 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4432 | ` *  Extract a slice of the array.` |
|         - | 4433 | ` * Parameters` |
|         - | 4434 | ` *  $array` |
|         - | 4435 | ` *    The input array.` |
|         - | 4436 | ` * $offset` |
|         - | 4437 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4438 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4439 | ` * $length (optional, nullable)` |
|         - | 4440 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4441 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4442 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4443 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4444 | ` * $preserve_keys (optional)` |
|         - | 4445 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4446 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4447 | ` * Return` |
|         - | 4448 | ` *   The new slice.` |
|         - | 4449 | ` */` |
|        52 | 4450 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4451 | `{` |
|         - | 4452 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4453 | `	ph7_hashmap_node *pCur;` |
|         - | 4454 | `	ph7_value *pArray;` |
|         - | 4455 | `	int iLength,iOfft;` |
|         - | 4456 | `	int bPreserve;` |
|         - | 4457 | `	sxi32 rc;` |
|        57 | 4458 | `	if( nArg < 2 ){` |
|         8 | 4459 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4460 | `			"ArgumentCountError",` |
|         - | 4461 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4462 | `			nArg` |
|         - | 4463 | `			);` |
|         - | 4464 | `	}` |
|        53 | 4465 | `	if( nArg > 4 ){` |
|         4 | 4466 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4467 | `			"ArgumentCountError",` |
|         - | 4468 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4469 | `			nArg` |
|         - | 4470 | `			);` |
|         - | 4471 | `	}` |
|        51 | 4472 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4473 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4474 | `			"TypeError",` |
|         - | 4475 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4476 | `			ph7_type_name(apArg[0])` |
|         - | 4477 | `			);` |
|         - | 4478 | `	}` |
|         - | 4479 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        65 | 4480 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        68 | 4481 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4482 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4483 | `			"TypeError",` |
|         - | 4484 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4485 | `			ph7_type_name(apArg[1])` |
|         - | 4486 | `			);` |
|         - | 4487 | `	}` |
|         - | 4488 | `	/* Validate $length type if provided: nullable int */` |
|        47 | 4489 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        29 | 4490 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        29 | 4491 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4492 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4493 | `				"TypeError",` |
|         - | 4494 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4495 | `				ph7_type_name(apArg[2])` |
|         - | 4496 | `				);` |
|         - | 4497 | `		}` |
|         9 | 4498 | `	}` |
|         - | 4499 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        45 | 4500 | `	if( nArg > 3 ){` |
|        10 | 4501 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4502 | `			ph7_value_is_resource(apArg[3]) ){` |
|         4 | 4503 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4504 | `				"TypeError",` |
|         - | 4505 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|         2 | 4506 | `				ph7_type_name(apArg[3])` |
|         - | 4507 | `				);` |
|         - | 4508 | `		}` |
|         2 | 4509 | `	}` |
|         - | 4510 | `	/* Point the internal representation of the target array */` |
|        43 | 4511 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        43 | 4512 | `	bPreserve = FALSE;` |
|         - | 4513 | `	/* Get the offset */` |
|        43 | 4514 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        43 | 4515 | `	if( iOfft < 0 ){` |
|         5 | 4516 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4517 | `		if( iOfft < 0 ){` |
|         3 | 4518 | `			iOfft = 0;` |
|         1 | 4519 | `		}` |
|         2 | 4520 | `	}` |
|        43 | 4521 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4522 | `		/* Offset past end of array, return empty array */` |
|         5 | 4523 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4524 | `		if( pArray == 0 ){` |
|       ! 0 | 4525 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4526 | `			return PH7_OK;` |
|         - | 4527 | `		}` |
|         5 | 4528 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4529 | `		return PH7_OK;` |
|         - | 4530 | `	}` |
|         - | 4531 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        39 | 4532 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        39 | 4533 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        17 | 4534 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        17 | 4535 | `		if( iLength < 0 ){` |
|         5 | 4536 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4537 | `		}` |
|        17 | 4538 | `		if( iLength < 0 ){` |
|         3 | 4539 | `			iLength = 0;` |
|         1 | 4540 | `		}` |
|        17 | 4541 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4542 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4543 | `		}` |
|         8 | 4544 | `	}` |
|        39 | 4545 | `	if( nArg > 3 ){` |
|         5 | 4546 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4547 | `	}` |
|         - | 4548 | `	/* Create a new array */` |
|        39 | 4549 | `	pArray = ph7_context_new_array(pCtx);` |
|        39 | 4550 | `	if( pArray == 0 ){` |
|       ! 0 | 4551 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4552 | `		return PH7_OK;` |
|         - | 4553 | `	}` |
|        39 | 4554 | `	if( iLength < 1 ){` |
|         - | 4555 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4556 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4557 | `		return PH7_OK;` |
|         - | 4558 | `	}` |
|         - | 4559 | `	/* Point to the desired entry */` |
|        35 | 4560 | `	pCur = pSrc->pFirst;` |
|        29 | 4561 | `	for(;;){` |
|        63 | 4562 | `		if( iOfft < 1 ){` |
|        35 | 4563 | `			break;` |
|         - | 4564 | `		}` |
|         - | 4565 | `		/* Point to the next entry */` |
|        33 | 4566 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4567 | `		iOfft--;` |
|         5 | 4568 | `	}` |
|         - | 4569 | `	/* Point to the internal representation of the hashmap */` |
|        35 | 4570 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        54 | 4571 | `	for(;;){` |
|       113 | 4572 | `		if( iLength < 1 ){` |
|        35 | 4573 | `			break;` |
|         - | 4574 | `		}` |
|         - | 4575 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4576 | `		{` |
|        83 | 4577 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        83 | 4578 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4579 | `		}` |
|        83 | 4580 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4581 | `			break;` |
|         - | 4582 | `		}` |
|         - | 4583 | `		/* Point to the next entry */` |
|        83 | 4584 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        83 | 4585 | `		iLength--;` |
|         5 | 4586 | `	}` |
|         - | 4587 | `	/* Return the freshly created array */` |
|        35 | 4588 | `	ph7_result_value(pCtx,pArray);` |
|        35 | 4589 | `	return PH7_OK;` |
|        31 | 4590 | `}` |
|         - | 4591 | `/*` |
|         - | 4592 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4593 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4594 | ` * beginning (becomes the new pFirst).` |
|         - | 4595 | ` */` |
|        38 | 4596 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4597 | `{` |
|         - | 4598 | `	ph7_hashmap_node *pNode;` |
|         - | 4599 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4600 | `	pNode = pMap->pLast;` |
|        39 | 4601 | `	if( pNode == 0 ){` |
|       ! 0 | 4602 | `		return;` |
|         - | 4603 | `	}` |
|        39 | 4604 | `	if( pNode->pNext == 0 ){` |
|         - | 4605 | `		/* Only node in the list, nothing to move */` |
|         5 | 4606 | `		return;` |
|         - | 4607 | `	}` |
|        35 | 4608 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4609 | `		/* Already in the correct position */` |
|         9 | 4610 | `		return;` |
|         - | 4611 | `	}` |
|         - | 4612 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4613 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4614 | `	pMap->pLast->pPrev = 0;` |
|         - | 4615 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4616 | `	if( pAfter == 0 ){` |
|         - | 4617 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4618 | `		pNode->pNext = 0;` |
|         3 | 4619 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4620 | `		if( pMap->pFirst ){` |
|         3 | 4621 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4622 | `		}` |
|         3 | 4623 | `		pMap->pFirst = pNode;` |
|         2 | 4624 | `	}else{` |
|        25 | 4625 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4626 | `		pNode->pPrev = pOldNext;` |
|        25 | 4627 | `		pNode->pNext = pAfter;` |
|        25 | 4628 | `		pAfter->pPrev = pNode;` |
|        25 | 4629 | `		if( pOldNext ){` |
|        25 | 4630 | `			pOldNext->pNext = pNode;` |
|        13 | 4631 | `		}else{` |
|       ! 0 | 4632 | `			pMap->pLast = pNode;` |
|         - | 4633 | `		}` |
|         - | 4634 | `	}` |
|        20 | 4635 | `}` |
|         - | 4636 | `/*` |
|         - | 4637 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4638 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4639 | ` * Parameters` |
|         - | 4640 | ` *  $array` |
|         - | 4641 | ` *    The input array.` |
|         - | 4642 | ` *  $offset` |
|         - | 4643 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4644 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4645 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4646 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4647 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4648 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4649 | ` *  $length (optional)` |
|         - | 4650 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4651 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4652 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4653 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4654 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4655 | ` *  $replacement (optional)` |
|         - | 4656 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4657 | ` *    with elements from this array.` |
|         - | 4658 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4659 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4660 | ` *    offset.` |
|         - | 4661 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4662 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4663 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4664 | ` * Return` |
|         - | 4665 | ` *   A new array consisting of the extracted elements.` |
|         - | 4666 | ` */` |
|        68 | 4667 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4668 | `{` |
|         - | 4669 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4670 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4671 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4672 | `	int iLength,iOfft,i;` |
|         - | 4673 | `	sxi32 rc;` |
|        72 | 4674 | `	if( nArg < 2 ){` |
|         8 | 4675 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4676 | `			"ArgumentCountError",` |
|         - | 4677 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4678 | `			nArg` |
|         - | 4679 | `			);` |
|         - | 4680 | `	}` |
|        66 | 4681 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4682 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4683 | `			"TypeError",` |
|         - | 4684 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4685 | `			ph7_type_name(apArg[0])` |
|         - | 4686 | `			);` |
|         - | 4687 | `	}` |
|         - | 4688 | `	/* Point to the internal representation of the target array */` |
|        63 | 4689 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4690 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4691 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4692 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4693 | `	if( iOfft < 0 ){` |
|         9 | 4694 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4695 | `		if( iOfft < 0 ){` |
|         3 | 4696 | `			iOfft = 0;` |
|         2 | 4697 | `		}` |
|        59 | 4698 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4699 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4700 | `	}` |
|         - | 4701 | `	/* Get the length and clamp to valid range.` |
|         - | 4702 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4703 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4704 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4705 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4706 | `		if( iLength < 0 ){` |
|         7 | 4707 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4708 | `			if( iLength < 0 ){` |
|         3 | 4709 | `				iLength = 0;` |
|         1 | 4710 | `			}` |
|         3 | 4711 | `		}` |
|        45 | 4712 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4713 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4714 | `		}` |
|        22 | 4715 | `	}` |
|         - | 4716 | `	/* Create the result array for removed elements */` |
|        63 | 4717 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4718 | `	if( pArray == 0 ){` |
|       ! 0 | 4719 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4720 | `		return PH7_OK;` |
|         - | 4721 | `	}` |
|         - | 4722 | `	/* Get replacement array if provided */` |
|        63 | 4723 | `	pRep = 0;` |
|        63 | 4724 | `	if( nArg > 3 ){` |
|        27 | 4725 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4726 | `			/* Perform an array cast */` |
|         3 | 4727 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4728 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4729 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4730 | `			}` |
|         2 | 4731 | `		}else{` |
|        25 | 4732 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4733 | `		}` |
|        27 | 4734 | `		if( pRep ){` |
|         - | 4735 | `			/* Reset the loop cursor */` |
|        27 | 4736 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4737 | `		}` |
|        13 | 4738 | `	}` |
|         - | 4739 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4740 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4741 | `	/* Navigate to the offset position */` |
|        63 | 4742 | `	pCur = pSrc->pFirst;` |
|       131 | 4743 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4744 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4745 | `	}` |
|         - | 4746 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4747 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4748 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4749 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4750 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4751 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4752 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4753 | `		pPrev = pCur->pPrev;` |
|        79 | 4754 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4755 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4756 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4757 | `			break;` |
|         - | 4758 | `		}` |
|        79 | 4759 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4760 | `	}` |
|         - | 4761 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4762 | `	if( pRep ){` |
|         - | 4763 | `		ph7_value sSafeVal;` |
|        78 | 4764 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4765 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4766 | `			if( pRvalue ){` |
|         - | 4767 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4768 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4769 | `				 * since it points into that same pool. */` |
|        39 | 4770 | `				sSafeVal = *pRvalue;` |
|        39 | 4771 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4772 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4773 | `					pNewNode = pSrc->pLast;` |
|        39 | 4774 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4775 | `					pInsertAfter = pNewNode;` |
|        19 | 4776 | `				}` |
|        19 | 4777 | `			}` |
|         1 | 4778 | `		}` |
|        13 | 4779 | `	}` |
|         - | 4780 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4781 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4782 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4783 | `	 * and removals left gaps. */` |
|         - | 4784 | `	{` |
|        63 | 4785 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4786 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4787 | `		pSrc->iNextIdx = 0;` |
|       233 | 4788 | `		while( n > 0 ){` |
|       171 | 4789 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4790 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4791 | `			}` |
|       171 | 4792 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4793 | `			n--;` |
|         1 | 4794 | `		}` |
|        63 | 4795 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4796 | `	}` |
|         - | 4797 | `	/* Return the freshly created array */` |
|        63 | 4798 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4799 | `	return PH7_OK;` |
|        38 | 4800 | `}` |
|         - | 4801 | `/*` |
|         - | 4802 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4803 | ` *  Checks if a value exists in an array.` |
|         - | 4804 | ` * Parameters` |
|         - | 4805 | ` *  $needle` |
|         - | 4806 | ` *   The searched value.` |
|         - | 4807 | ` *   Note:` |
|         - | 4808 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4809 | ` * $haystack` |
|         - | 4810 | ` *  The target array.` |
|         - | 4811 | ` * $strict` |
|         - | 4812 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4813 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4814 | ` */` |
|     33172 | 4815 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4816 | `{` |
|         - | 4817 | `	ph7_value *pNeedle;` |
|         - | 4818 | `	int bStrict;` |
|         - | 4819 | `	int rc;` |
|     33177 | 4820 | `	if( nArg < 2 ){` |
|         - | 4821 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4822 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4823 | `		return PH7_OK;` |
|         - | 4824 | `	}` |
|     33177 | 4825 | `	pNeedle = apArg[0];` |
|     33177 | 4826 | `	bStrict = 0;` |
|     33177 | 4827 | `	if( nArg > 2 ){` |
|        53 | 4828 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4829 | `	}` |
|     33177 | 4830 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4831 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4832 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4833 | `		/* Set the comparison result */` |
|       ! 0 | 4834 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4835 | `		return PH7_OK;` |
|         - | 4836 | `	}` |
|         - | 4837 | `	/* Perform the lookup */` |
|     33177 | 4838 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4839 | `	/* Lookup result */` |
|     33177 | 4840 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     33177 | 4841 | `	return PH7_OK;` |
|     16591 | 4842 | `}` |
|         - | 4843 | `/*` |
|         - | 4844 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4845 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4846 | ` * Parameters` |
|         - | 4847 | ` * $needle` |
|         - | 4848 | ` *   The searched value.` |
|         - | 4849 | ` * $haystack` |
|         - | 4850 | ` *   The array.` |
|         - | 4851 | ` * $strict` |
|         - | 4852 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4853 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4854 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4855 | ` * Return` |
|         - | 4856 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4857 | ` */` |
|        32 | 4858 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4859 | `{` |
|         - | 4860 | `	ph7_hashmap_node *pEntry;` |
|         - | 4861 | `	ph7_value *pVal,sNeedle;` |
|         - | 4862 | `	ph7_hashmap *pMap;` |
|         - | 4863 | `	ph7_value sVal;` |
|         - | 4864 | `	int bStrict;` |
|         - | 4865 | `	sxu32 n;` |
|         - | 4866 | `	int rc;` |
|        37 | 4867 | `	if( nArg < 2 ){` |
|         - | 4868 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4869 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4870 | `			"ArgumentCountError",` |
|         - | 4871 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4872 | `			nArg` |
|         - | 4873 | `			);` |
|         - | 4874 | `	}` |
|        31 | 4875 | `	bStrict = FALSE;` |
|        31 | 4876 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4877 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4878 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4879 | `			"TypeError",` |
|         - | 4880 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4881 | `			ph7_type_name(apArg[1])` |
|         - | 4882 | `			);` |
|         - | 4883 | `	}` |
|        28 | 4884 | `	if( nArg > 2 ){` |
|         - | 4885 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        14 | 4886 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4887 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4888 | `				"TypeError",` |
|         - | 4889 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4890 | `				ph7_type_name(apArg[2])` |
|         - | 4891 | `				);` |
|         - | 4892 | `		}` |
|        11 | 4893 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4894 | `	}` |
|         - | 4895 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4896 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4897 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4898 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4899 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4900 | `	pEntry = pMap->pFirst;` |
|        25 | 4901 | `	n = pMap->nEntry;` |
|        28 | 4902 | `	for(;;){` |
|        57 | 4903 | `		if( !n ){` |
|         9 | 4904 | `			break;` |
|         - | 4905 | `		}` |
|         - | 4906 | `		/* Extract node value */` |
|        49 | 4907 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4908 | `		if( pVal ){` |
|         - | 4909 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4910 | `			 * can change their type.` |
|         - | 4911 | `			 */` |
|        49 | 4912 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4913 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4914 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4915 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4916 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4917 | `			if( rc == 0 ){` |
|         - | 4918 | `				/* Match found,return key */` |
|        17 | 4919 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4920 | `					/* INT key */` |
|        11 | 4921 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4922 | `				}else{` |
|         7 | 4923 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4924 | `					/* Blob key */` |
|         7 | 4925 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4926 | `				}` |
|        17 | 4927 | `				return PH7_OK;` |
|         - | 4928 | `			}` |
|        16 | 4929 | `		}` |
|         - | 4930 | `		/* Point to the next entry */` |
|        33 | 4931 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4932 | `		n--;` |
|         1 | 4933 | `	}` |
|         - | 4934 | `	/* No such value,return FALSE */` |
|         9 | 4935 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4936 | `	return PH7_OK;` |
|        21 | 4937 | `}` |
|         - | 4938 | `/*` |
|         - | 4939 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4940 | ` *  Computes the difference of arrays.` |
|         - | 4941 | ` * Parameters` |
|         - | 4942 | ` *  $array1` |
|         - | 4943 | ` *    The array to compare from` |
|         - | 4944 | ` *  $array2` |
|         - | 4945 | ` *    An array to compare against` |
|         - | 4946 | ` *  $...` |
|         - | 4947 | ` *   More arrays to compare against` |
|         - | 4948 | ` * Return` |
|         - | 4949 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4950 | ` *  are not present in any of the other arrays.` |
|         - | 4951 | ` */` |
|        22 | 4952 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4953 | `{` |
|         - | 4954 | `	ph7_hashmap_node *pEntry;` |
|         - | 4955 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4956 | `	ph7_value *pArray;` |
|         - | 4957 | `	ph7_value *pVal;` |
|         - | 4958 | `	sxi32 rc;` |
|         - | 4959 | `	sxu32 n;` |
|         - | 4960 | `	int i;` |
|         - | 4961 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4962 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4963 | `	 * debugging difficult. */` |
|        26 | 4964 | `	if( nArg < 1 ){` |
|         4 | 4965 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4966 | `			"ArgumentCountError",` |
|         - | 4967 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4968 | `			nArg` |
|         - | 4969 | `			);` |
|         - | 4970 | `	}` |
|        23 | 4971 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4972 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4973 | `			"TypeError",` |
|         - | 4974 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4975 | `			ph7_type_name(apArg[0])` |
|         - | 4976 | `			);` |
|         - | 4977 | `	}` |
|        36 | 4978 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4979 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4980 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4981 | `				"TypeError",` |
|         - | 4982 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4983 | `				i + 1,` |
|         2 | 4984 | `				ph7_type_name(apArg[i])` |
|         - | 4985 | `				);` |
|         - | 4986 | `		}` |
|         9 | 4987 | `	}` |
|        17 | 4988 | `	if( nArg == 1 ){` |
|         - | 4989 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 4990 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 4991 | `		return PH7_OK;` |
|         - | 4992 | `	}` |
|         - | 4993 | `	/* Create a new array */` |
|        15 | 4994 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 4995 | `	if( pArray == 0 ){` |
|       ! 0 | 4996 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4997 | `		return PH7_OK;` |
|         - | 4998 | `	}` |
|         - | 4999 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5000 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5001 | `	/* Perform the diff */` |
|        15 | 5002 | `	pEntry = pSrc->pFirst;` |
|        15 | 5003 | `	n = pSrc->nEntry;` |
|        27 | 5004 | `	for(;;){` |
|        55 | 5005 | `		if( n < 1 ){` |
|        15 | 5006 | `			break;` |
|         - | 5007 | `		}` |
|         - | 5008 | `		/* Extract the node value */` |
|        41 | 5009 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5010 | `		if( pVal ){` |
|        69 | 5011 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5012 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5013 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5014 | `				/* Perform the lookup */` |
|        45 | 5015 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5016 | `				if( rc == SXRET_OK ){` |
|         - | 5017 | `					/* Value exist */` |
|        17 | 5018 | `					break;` |
|         - | 5019 | `				}` |
|        15 | 5020 | `			}` |
|        41 | 5021 | `			if( i >= nArg ){` |
|         - | 5022 | `				/* Perform the insertion */` |
|        25 | 5023 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5024 | `			}` |
|        20 | 5025 | `		}` |
|         - | 5026 | `		/* Point to the next entry */` |
|        41 | 5027 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5028 | `		n--;` |
|         1 | 5029 | `	}` |
|         - | 5030 | `	/* Return the freshly created array */` |
|        15 | 5031 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5032 | `	return PH7_OK;` |
|        15 | 5033 | `}` |
|         - | 5034 | `/*` |
|         - | 5035 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5036 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5037 | ` * Parameters` |
|         - | 5038 | ` *  $array1` |
|         - | 5039 | ` *    The array to compare from` |
|         - | 5040 | ` *  $array2` |
|         - | 5041 | ` *    An array to compare against` |
|         - | 5042 | ` *  $...` |
|         - | 5043 | ` *   More arrays to compare against.` |
|         - | 5044 | ` * $callback` |
|         - | 5045 | ` *  The callback comparison function.` |
|         - | 5046 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5047 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5048 | ` *  than the second.` |
|         - | 5049 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5050 | ` * Return` |
|         - | 5051 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5052 | ` *  are not present in any of the other arrays.` |
|         - | 5053 | ` */` |
|        22 | 5054 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5055 | `{` |
|         - | 5056 | `	ph7_hashmap_node *pEntry;` |
|         - | 5057 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5058 | `	ph7_value *pCallback;` |
|         - | 5059 | `	ph7_value *pArray;` |
|         - | 5060 | `	ph7_value *pVal;` |
|         - | 5061 | `	sxi32 rc;` |
|         - | 5062 | `	sxu32 n;` |
|         - | 5063 | `	int i;` |
|         - | 5064 |  |
|         - | 5065 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 5066 | `	if( nArg < 2 ){` |
|         4 | 5067 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5068 | `			"ArgumentCountError",` |
|         - | 5069 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 5070 | `			nArg` |
|         - | 5071 | `			);` |
|         - | 5072 | `	}` |
|        25 | 5073 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5074 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5075 | `			"TypeError",` |
|         - | 5076 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5077 | `			ph7_type_name(apArg[0])` |
|         - | 5078 | `			);` |
|         - | 5079 | `	}` |
|         - | 5080 |  |
|        23 | 5081 | `	if( nArg == 2 ){` |
|         - | 5082 | `		/* Only the original array and the callback were provided. */` |
|         - | 5083 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5084 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5085 | `		 * validation order.` |
|         - | 5086 | `		 */` |
|         4 | 5087 | `	} else {` |
|         - | 5088 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5089 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5090 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5091 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5092 | `					"TypeError",` |
|         - | 5093 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5094 | `					i + 1,` |
|         6 | 5095 | `					ph7_type_name(apArg[i])` |
|         - | 5096 | `					);` |
|         - | 5097 | `			}` |
|         7 | 5098 | `		}` |
|         - | 5099 | `	}` |
|         - | 5100 |  |
|         - | 5101 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5102 | `	pCallback = apArg[nArg - 1];` |
|         - | 5103 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5104 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5105 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5106 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5107 | `				"TypeError",` |
|         - | 5108 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5109 | `				nArg` |
|         - | 5110 | `				);` |
|         - | 5111 | `		}` |
|         6 | 5112 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5113 | `			int len;` |
|         3 | 5114 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5115 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5116 | `				"TypeError",` |
|         - | 5117 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5118 | `				nArg,` |
|         1 | 5119 | `				zName` |
|         - | 5120 | `				);` |
|         - | 5121 | `		}` |
|         4 | 5122 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5123 | `			"TypeError",` |
|         - | 5124 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5125 | `			nArg` |
|         - | 5126 | `			);` |
|         - | 5127 | `	}` |
|         - | 5128 |  |
|         7 | 5129 | `	if( nArg == 2 ){` |
|         - | 5130 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5131 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5132 | `		return PH7_OK;` |
|         - | 5133 | `	}` |
|         - | 5134 |  |
|         - | 5135 | `	/* Create a new array */` |
|         5 | 5136 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5137 | `	if( pArray == 0 ){` |
|       ! 0 | 5138 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5139 | `		return PH7_OK;` |
|         - | 5140 | `	}` |
|         - | 5141 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5142 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5143 | `	/* Perform the diff */` |
|         5 | 5144 | `	pEntry = pSrc->pFirst;` |
|         5 | 5145 | `	n = pSrc->nEntry;` |
|         5 | 5146 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5147 | `	for(;;){` |
|        11 | 5148 | `		if( n < 1 ){` |
|         3 | 5149 | `			break;` |
|         - | 5150 | `		}` |
|         - | 5151 | `		/* Extract the node value */` |
|         9 | 5152 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5153 | `		if( pVal ){` |
|        15 | 5154 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5155 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5156 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5157 | `				/* Perform the lookup */` |
|         9 | 5158 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5159 | `				if( rc == SXRET_OK ){` |
|         - | 5160 | `					/* Value exist */` |
|         3 | 5161 | `					break;` |
|         - | 5162 | `				}` |
|         4 | 5163 | `			}` |
|         9 | 5164 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5165 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5166 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5167 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5168 | `				return PH7_EXCEPTION;` |
|         - | 5169 | `			}` |
|         7 | 5170 | `			if( i >= (nArg - 1)){` |
|         - | 5171 | `				/* Perform the insertion */` |
|         5 | 5172 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5173 | `			}` |
|         3 | 5174 | `		}` |
|         - | 5175 | `		/* Point to the next entry */` |
|         7 | 5176 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5177 | `		n--;` |
|         1 | 5178 | `	}` |
|         - | 5179 | `	/* Return the freshly created array */` |
|         3 | 5180 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5181 | `	return PH7_OK;` |
|        16 | 5182 | `}` |
|         - | 5183 | `/*` |
|         - | 5184 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5185 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5186 | ` * Parameters` |
|         - | 5187 | ` *  $array1` |
|         - | 5188 | ` *    The array to compare from` |
|         - | 5189 | ` *  $array2` |
|         - | 5190 | ` *    An array to compare against` |
|         - | 5191 | ` *  $...` |
|         - | 5192 | ` *   More arrays to compare against` |
|         - | 5193 | ` * Return` |
|         - | 5194 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5195 | ` *  are not present in any of the other arrays.` |
|         - | 5196 | ` */` |
|        22 | 5197 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5198 | `{` |
|         - | 5199 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5200 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5201 | `	ph7_value *pArray;` |
|         - | 5202 | `	ph7_value *pVal;` |
|         - | 5203 | `	sxi32 rc;` |
|         - | 5204 | `	sxu32 n;` |
|         - | 5205 | `	int i;` |
|         - | 5206 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5207 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5208 | `	 * accompanying integration tests to pass. */` |
|        27 | 5209 | `	if( nArg < 1 ){` |
|         4 | 5210 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5211 | `			"ArgumentCountError",` |
|         - | 5212 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5213 | `			nArg` |
|         - | 5214 | `			);` |
|         - | 5215 | `	}` |
|        24 | 5216 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5217 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5218 | `			"TypeError",` |
|         - | 5219 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5220 | `			ph7_type_name(apArg[0])` |
|         - | 5221 | `			);` |
|         - | 5222 | `	}` |
|        37 | 5223 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5224 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5225 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5226 | `				"TypeError",` |
|         - | 5227 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5228 | `				i + 1,` |
|         4 | 5229 | `				ph7_type_name(apArg[i])` |
|         - | 5230 | `				);` |
|         - | 5231 | `		}` |
|        10 | 5232 | `	}` |
|        15 | 5233 | `	if( nArg == 1 ){` |
|         - | 5234 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5235 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5236 | `		return PH7_OK;` |
|         - | 5237 | `	}` |
|         - | 5238 | `	/* Create a new array */` |
|        13 | 5239 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5240 | `	if( pArray == 0 ){` |
|       ! 0 | 5241 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5242 | `		return PH7_OK;` |
|         - | 5243 | `	}` |
|         - | 5244 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5245 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5246 | `	/* Perform the diff */` |
|        13 | 5247 | `	pEntry = pSrc->pFirst;` |
|        13 | 5248 | `	n = pSrc->nEntry;` |
|        13 | 5249 | `	pN1 = pN2 = 0;` |
|        34 | 5250 | `	for(;;){` |
|         - | 5251 | `		int keep;` |
|        41 | 5252 | `		if( n < 1 ){` |
|        13 | 5253 | `			break;` |
|         - | 5254 | `		}` |
|         - | 5255 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5256 | `		keep = 1;` |
|        47 | 5257 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5258 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5259 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5260 | `			/* Perform a key lookup first */` |
|        33 | 5261 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5262 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5263 | `			}else{` |
|        21 | 5264 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5265 | `			}` |
|        33 | 5266 | `			if( rc != SXRET_OK ){` |
|         - | 5267 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5268 | `				continue;` |
|         - | 5269 | `			}` |
|         - | 5270 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5271 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5272 | `			if( pVal ){` |
|         - | 5273 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5274 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5275 | `				if( pVal2 ){` |
|         - | 5276 | `					ph7_value sV1,sV2;` |
|         - | 5277 | `					sxi32 cmp;` |
|         - | 5278 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5279 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5280 | `					 * null element used to come back bool(false) in the` |
|         - | 5281 | `					 * caller's array). */` |
|        17 | 5282 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5283 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5284 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5285 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5286 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5287 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5288 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5289 | `					if( cmp == 0 ){` |
|         - | 5290 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5291 | `						keep = 0;` |
|        15 | 5292 | `						break;` |
|         - | 5293 | `					}` |
|         1 | 5294 | `				}` |
|         1 | 5295 | `			}` |
|         2 | 5296 | `		}` |
|        29 | 5297 | `		if( keep ){` |
|         - | 5298 | `			/* Perform the insertion */` |
|        15 | 5299 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5300 | `		}` |
|         - | 5301 | `		/* Point to the next entry */` |
|        29 | 5302 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5303 | `		n--;` |
|         1 | 5304 | `	}` |
|         - | 5305 | `	/* Return the freshly created array */` |
|        13 | 5306 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5307 | `	return PH7_OK;` |
|        16 | 5308 | `}` |
|         - | 5309 | `/*` |
|         - | 5310 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5311 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5312 | ` *  by a user supplied callback function.` |
|         - | 5313 | ` * Parameters` |
|         - | 5314 | ` *  $array1` |
|         - | 5315 | ` *    The array to compare from` |
|         - | 5316 | ` *  $array2` |
|         - | 5317 | ` *    An array to compare against` |
|         - | 5318 | ` *  $...` |
|         - | 5319 | ` *   More arrays to compare against.` |
|         - | 5320 | ` *  $key_compare_func` |
|         - | 5321 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5322 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5323 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5324 | ` * Return` |
|         - | 5325 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5326 | ` *  are not present in any of the other arrays.` |
|         - | 5327 | ` */` |
|        24 | 5328 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5329 | `{` |
|         - | 5330 | `	ph7_hashmap_node *pEntry;` |
|         - | 5331 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5332 | `	ph7_value *pCallback;` |
|         - | 5333 | `	ph7_value *pArray;` |
|         - | 5334 | `	sxi32 rc;` |
|         - | 5335 | `	sxu32 n;` |
|         - | 5336 | `	int i;` |
|         - | 5337 |  |
|         - | 5338 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5339 | `	if( nArg < 2 ){` |
|         4 | 5340 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5341 | `			"ArgumentCountError",` |
|         - | 5342 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5343 | `			nArg` |
|         - | 5344 | `			);` |
|         - | 5345 | `	}` |
|        26 | 5346 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5347 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5348 | `			"TypeError",` |
|         - | 5349 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5350 | `			ph7_type_name(apArg[0])` |
|         - | 5351 | `			);` |
|         - | 5352 | `	}` |
|         - | 5353 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5354 | `	 * expected to be a callback. */` |
|        38 | 5355 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5356 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5357 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5358 | `				"TypeError",` |
|         - | 5359 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5360 | `				i + 1,` |
|         2 | 5361 | `				ph7_type_name(apArg[i])` |
|         - | 5362 | `				);` |
|         - | 5363 | `		}` |
|         9 | 5364 | `	}` |
|         - | 5365 | `	/* Point to the callback value */` |
|        22 | 5366 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5367 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5368 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5369 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5370 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5371 | `		 * string given" which we also reproduce. */` |
|         9 | 5372 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5373 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5374 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5375 | `				"TypeError",` |
|         - | 5376 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5377 | `				nArg` |
|         - | 5378 | `				);` |
|         - | 5379 | `		}` |
|         6 | 5380 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5381 | `			/* neither array nor string */` |
|         8 | 5382 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5383 | `				"TypeError",` |
|         - | 5384 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5385 | `				nArg` |
|         - | 5386 | `				);` |
|         - | 5387 | `		}` |
|         - | 5388 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5389 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5390 | `			"TypeError",` |
|         - | 5391 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5392 | `			nArg,` |
|       ! 0 | 5393 | `			ph7_type_name(pCallback)` |
|         - | 5394 | `			);` |
|         - | 5395 | `	}` |
|        13 | 5396 | `	if( nArg == 2 ){` |
|         - | 5397 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5398 | `		 * input array. */` |
|         3 | 5399 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5400 | `		return PH7_OK;` |
|         - | 5401 | `	}` |
|         - | 5402 | `	/* Create a new array */` |
|        11 | 5403 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5404 | `	if( pArray == 0 ){` |
|       ! 0 | 5405 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5406 | `		return PH7_OK;` |
|         - | 5407 | `	}` |
|         - | 5408 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5409 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5410 | `	/* Perform the diff */` |
|        11 | 5411 | `	pEntry = pSrc->pFirst;` |
|        11 | 5412 | `	n = pSrc->nEntry;` |
|        21 | 5413 | `	for(;;){` |
|         - | 5414 | `		int keep;` |
|        27 | 5415 | `		if( n < 1 ){` |
|         9 | 5416 | `			break;` |
|         - | 5417 | `		}` |
|        19 | 5418 | `		keep = 1;` |
|        31 | 5419 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5420 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5421 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5422 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5423 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5424 | `			while( pIt ){` |
|         - | 5425 | `				/* build temporary key values for callback */` |
|         - | 5426 | `				ph7_value key1, key2, result;` |
|         - | 5427 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5428 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5429 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5430 | `				}else{` |
|         - | 5431 | `					SyString sStr;` |
|        33 | 5432 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5433 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5434 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5435 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5436 | `				}` |
|        33 | 5437 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5438 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5439 | `				}else{` |
|         - | 5440 | `					SyString sStr;` |
|        33 | 5441 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5442 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5443 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5444 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5445 | `				}` |
|        33 | 5446 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5447 | `				/* call user callback with (key1, key2) */` |
|         - | 5448 | `				{` |
|         - | 5449 | `					ph7_value *apK[2];` |
|        33 | 5450 | `					apK[0] = &key1;` |
|        33 | 5451 | `					apK[1] = &key2;` |
|        33 | 5452 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5453 | `				}` |
|        33 | 5454 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5455 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5456 | `					 * array_uintersect (which signal back from` |
|         - | 5457 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5458 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5459 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5460 | `					PH7_MemObjRelease(&result);` |
|         3 | 5461 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5462 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5463 | `					return PH7_EXCEPTION;` |
|         - | 5464 | `				}` |
|        31 | 5465 | `				if( rc == SXRET_OK ){` |
|        31 | 5466 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5467 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5468 | `					}` |
|        31 | 5469 | `					if( result.x.iVal == 0 ){` |
|         - | 5470 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5471 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5472 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5473 | `						if( pVal1 && pVal2 ){` |
|         - | 5474 | `							ph7_value sV1,sV2;` |
|         - | 5475 | `							sxi32 cmp;` |
|         - | 5476 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5477 | `							 * place and these are LIVE array elements. */` |
|        13 | 5478 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5479 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5480 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5481 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5482 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5483 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5484 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5485 | `							if( cmp == 0 ){` |
|         9 | 5486 | `								keep = 0;` |
|         9 | 5487 | `								PH7_MemObjRelease(&result);` |
|         - | 5488 | `								/* release keys too before breaking */` |
|         9 | 5489 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5490 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5491 | `								break;` |
|         - | 5492 | `							}` |
|         2 | 5493 | `						}` |
|         2 | 5494 | `					}` |
|        11 | 5495 | `				}` |
|        23 | 5496 | `				PH7_MemObjRelease(&result);` |
|        23 | 5497 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5498 | `				PH7_MemObjRelease(&key2);` |
|         - | 5499 | `				/* move to next node */` |
|        23 | 5500 | `				pIt = pIt->pPrev;` |
|        23 | 5501 | `				if( keep == 0 ) break;` |
|         1 | 5502 | `			}` |
|        21 | 5503 | `			if( keep == 0 ) break;` |
|         7 | 5504 | `		}` |
|        17 | 5505 | `		if( keep ){` |
|         - | 5506 | `			/* Perform the insertion */` |
|         9 | 5507 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5508 | `		}` |
|         - | 5509 | `		/* Point to the next entry */` |
|        17 | 5510 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5511 | `		n--;` |
|         1 | 5512 | `	}` |
|         - | 5513 | `	/* Return the freshly created array */` |
|         9 | 5514 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5515 | `	return PH7_OK;` |
|        17 | 5516 | `}` |
|         - | 5517 | `/*` |
|         - | 5518 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5519 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5520 | ` * Parameters` |
|         - | 5521 | ` *  $array1` |
|         - | 5522 | ` *    The array to compare from` |
|         - | 5523 | ` *  $array2` |
|         - | 5524 | ` *    An array to compare against` |
|         - | 5525 | ` *  $...` |
|         - | 5526 | ` *   More arrays to compare against` |
|         - | 5527 | ` * Return` |
|         - | 5528 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5529 | ` *  in any of the other arrays.` |
|         - | 5530 | ` * Note that NULL is returned on failure.` |
|         - | 5531 | ` */` |
|        14 | 5532 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5533 | `{` |
|         - | 5534 | `	ph7_hashmap_node *pEntry;` |
|         - | 5535 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5536 | `	ph7_value *pArray;` |
|         - | 5537 | `	sxi32 rc;` |
|         - | 5538 | `	sxu32 n;` |
|         - | 5539 | `	int i;` |
|         - | 5540 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5541 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5542 | `	 * helpers. */` |
|        18 | 5543 | `	if( nArg < 1 ){` |
|         4 | 5544 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5545 | `			"ArgumentCountError",` |
|         - | 5546 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5547 | `			nArg` |
|         - | 5548 | `			);` |
|         - | 5549 | `	}` |
|        15 | 5550 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5551 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5552 | `			"TypeError",` |
|         - | 5553 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5554 | `			ph7_type_name(apArg[0])` |
|         - | 5555 | `			);` |
|         - | 5556 | `	}` |
|        20 | 5557 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5558 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5559 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5560 | `				"TypeError",` |
|         - | 5561 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5562 | `				i + 1,` |
|         2 | 5563 | `				ph7_type_name(apArg[i])` |
|         - | 5564 | `				);` |
|         - | 5565 | `		}` |
|         5 | 5566 | `	}` |
|         9 | 5567 | `	if( nArg == 1 ){` |
|         - | 5568 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5569 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5570 | `		return PH7_OK;` |
|         - | 5571 | `	}` |
|         - | 5572 | `	/* Create a new array */` |
|         7 | 5573 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5574 | `	if( pArray == 0 ){` |
|       ! 0 | 5575 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5576 | `		return PH7_OK;` |
|         - | 5577 | `	}` |
|         - | 5578 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5579 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5580 | `	/* Perfrom the diff */` |
|         7 | 5581 | `	pEntry = pSrc->pFirst;` |
|         7 | 5582 | `	n = pSrc->nEntry;` |
|        12 | 5583 | `	for(;;){` |
|        25 | 5584 | `		if( n < 1 ){` |
|         7 | 5585 | `			break;` |
|         - | 5586 | `		}` |
|        31 | 5587 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5588 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5589 | `				/* ignore */` |
|       ! 0 | 5590 | `				continue;` |
|         - | 5591 | `			}` |
|        23 | 5592 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5593 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5594 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5595 | `				/* Blob lookup */` |
|        17 | 5596 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5597 | `			}else{` |
|         - | 5598 | `				/* Int lookup */` |
|         7 | 5599 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5600 | `			}` |
|        23 | 5601 | `			if( rc == SXRET_OK ){` |
|         - | 5602 | `				/* Key exists,break immediately */` |
|        11 | 5603 | `				break;` |
|         - | 5604 | `			}` |
|         7 | 5605 | `		}` |
|        19 | 5606 | `		if( i >= nArg ){` |
|         - | 5607 | `			/* Perform the insertion */` |
|         9 | 5608 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5609 | `		}` |
|         - | 5610 | `		/* Point to the next entry */` |
|        19 | 5611 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5612 | `		n--;` |
|         1 | 5613 | `	}` |
|         - | 5614 | `	/* Return the freshly created array */` |
|         7 | 5615 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5616 | `	return PH7_OK;` |
|        11 | 5617 | `}` |
|         - | 5618 | `/*` |
|         - | 5619 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5620 | ` *  Computes the intersection of arrays.` |
|         - | 5621 | ` * Parameters` |
|         - | 5622 | ` *  $array1` |
|         - | 5623 | ` *    The array to compare from` |
|         - | 5624 | ` *  $array2` |
|         - | 5625 | ` *    An array to compare against` |
|         - | 5626 | ` *  $...` |
|         - | 5627 | ` *   More arrays to compare against` |
|         - | 5628 | ` * Return` |
|         - | 5629 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5630 | ` *  in all of the parameters.` |
|         - | 5631 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5632 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5633 | ` */` |
|        22 | 5634 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5635 | `{` |
|         - | 5636 | `	ph7_hashmap_node *pEntry;` |
|         - | 5637 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5638 | `	ph7_value *pArray;` |
|         - | 5639 | `	ph7_value *pVal;` |
|         - | 5640 | `	sxi32 rc;` |
|         - | 5641 | `	sxu32 n;` |
|         - | 5642 | `	int i;` |
|        26 | 5643 | `	if( nArg < 1 ){` |
|         4 | 5644 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5645 | `			"ArgumentCountError",` |
|         - | 5646 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5647 | `			nArg` |
|         - | 5648 | `			);` |
|         - | 5649 | `	}` |
|        23 | 5650 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5651 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5652 | `			"TypeError",` |
|         - | 5653 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5654 | `			ph7_type_name(apArg[0])` |
|         - | 5655 | `			);` |
|         - | 5656 | `	}` |
|        36 | 5657 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5658 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5659 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5660 | `				"TypeError",` |
|         - | 5661 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5662 | `				i + 1,` |
|         2 | 5663 | `				ph7_type_name(apArg[i])` |
|         - | 5664 | `				);` |
|         - | 5665 | `		}` |
|         9 | 5666 | `	}` |
|        17 | 5667 | `	if( nArg == 1 ){` |
|         - | 5668 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5669 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5670 | `		return PH7_OK;` |
|         - | 5671 | `	}` |
|         - | 5672 | `	/* Create a new array */` |
|        15 | 5673 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5674 | `	if( pArray == 0 ){` |
|       ! 0 | 5675 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5676 | `		return PH7_OK;` |
|         - | 5677 | `	}` |
|         - | 5678 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5679 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5680 | `	/* Perform the intersection */` |
|        15 | 5681 | `	pEntry = pSrc->pFirst;` |
|        15 | 5682 | `	n = pSrc->nEntry;` |
|        31 | 5683 | `	for(;;){` |
|        63 | 5684 | `		if( n < 1 ){` |
|        15 | 5685 | `			break;` |
|         - | 5686 | `		}` |
|         - | 5687 | `		/* Extract the node value */` |
|        49 | 5688 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5689 | `		if( pVal ){` |
|        79 | 5690 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5691 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5692 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5693 | `				/* Perform the lookup */` |
|        55 | 5694 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5695 | `				if( rc != SXRET_OK ){` |
|         - | 5696 | `					/* Value does not exist */` |
|        25 | 5697 | `					break;` |
|         - | 5698 | `				}` |
|        16 | 5699 | `			}` |
|        49 | 5700 | `			if( i >= nArg ){` |
|         - | 5701 | `				/* Perform the insertion */` |
|        25 | 5702 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5703 | `			}` |
|        24 | 5704 | `		}` |
|         - | 5705 | `		/* Point to the next entry */` |
|        49 | 5706 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5707 | `		n--;` |
|         1 | 5708 | `	}` |
|         - | 5709 | `	/* Return the freshly created array */` |
|        15 | 5710 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5711 | `	return PH7_OK;` |
|        15 | 5712 | `}` |
|         - | 5713 | `/*` |
|         - | 5714 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5715 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5716 | ` * Parameters` |
|         - | 5717 | ` *  $array1` |
|         - | 5718 | ` *    The array to compare from` |
|         - | 5719 | ` *  $array2` |
|         - | 5720 | ` *    An array to compare against` |
|         - | 5721 | ` *  $...` |
|         - | 5722 | ` *   More arrays to compare against` |
|         - | 5723 | ` * Return` |
|         - | 5724 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5725 | ` *  in all the arguments, with matching keys.` |
|         - | 5726 | ` */` |
|        22 | 5727 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5728 | `{` |
|         - | 5729 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5730 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5731 | `	ph7_value *pArray;` |
|         - | 5732 | `	ph7_value *pVal;` |
|         - | 5733 | `	sxi32 rc;` |
|         - | 5734 | `	sxu32 n;` |
|         - | 5735 | `	int i;` |
|        26 | 5736 | `	if( nArg < 1 ){` |
|         4 | 5737 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5738 | `			"ArgumentCountError",` |
|         - | 5739 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5740 | `			nArg` |
|         - | 5741 | `			);` |
|         - | 5742 | `	}` |
|        23 | 5743 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5744 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5745 | `			"TypeError",` |
|         - | 5746 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5747 | `			ph7_type_name(apArg[0])` |
|         - | 5748 | `			);` |
|         - | 5749 | `	}` |
|        36 | 5750 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5751 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5752 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5753 | `				"TypeError",` |
|         - | 5754 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5755 | `				i + 1,` |
|         2 | 5756 | `				ph7_type_name(apArg[i])` |
|         - | 5757 | `				);` |
|         - | 5758 | `		}` |
|         9 | 5759 | `	}` |
|        17 | 5760 | `	if( nArg == 1 ){` |
|         - | 5761 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5762 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5763 | `		return PH7_OK;` |
|         - | 5764 | `	}` |
|         - | 5765 | `	/* Create a new array */` |
|        15 | 5766 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5767 | `	if( pArray == 0 ){` |
|       ! 0 | 5768 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5769 | `		return PH7_OK;` |
|         - | 5770 | `	}` |
|         - | 5771 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5772 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5773 | `	/* Perform the intersection */` |
|        15 | 5774 | `	pEntry = pSrc->pFirst;` |
|        15 | 5775 | `	n = pSrc->nEntry;` |
|        15 | 5776 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5777 | `	for(;;){` |
|        47 | 5778 | `		if( n < 1 ){` |
|        15 | 5779 | `			break;` |
|         - | 5780 | `		}` |
|         - | 5781 | `		/* Extract the node value */` |
|        33 | 5782 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5783 | `		if( pVal ){` |
|        53 | 5784 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5785 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5786 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5787 | `				/* Perform a key lookup first */` |
|        37 | 5788 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5789 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5790 | `				}else{` |
|        23 | 5791 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5792 | `				}` |
|        37 | 5793 | `				if( rc != SXRET_OK ){` |
|         - | 5794 | `					/* No such key,break immediately */` |
|         7 | 5795 | `					break;` |
|         - | 5796 | `				}` |
|         - | 5797 | `				/* Perform the lookup */` |
|        31 | 5798 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5799 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5800 | `					/* Value does not exist */` |
|         6 | 5801 | `					break;` |
|         - | 5802 | `				}` |
|        11 | 5803 | `			}` |
|        33 | 5804 | `			if( i >= nArg ){` |
|         - | 5805 | `				/* Perform the insertion */` |
|        17 | 5806 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5807 | `			}` |
|        16 | 5808 | `		}` |
|         - | 5809 | `		/* Point to the next entry */` |
|        33 | 5810 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5811 | `		n--;` |
|         1 | 5812 | `	}` |
|         - | 5813 | `	/* Return the freshly created array */` |
|        15 | 5814 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5815 | `	return PH7_OK;` |
|        15 | 5816 | `}` |
|         - | 5817 | `/*` |
|         - | 5818 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5819 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5820 | ` * Parameters` |
|         - | 5821 | ` *  $array1` |
|         - | 5822 | ` *    The array to compare from` |
|         - | 5823 | ` *  $...` |
|         - | 5824 | ` *   More arrays to compare against` |
|         - | 5825 | ` * Return` |
|         - | 5826 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5827 | ` *  have keys that are present in all arguments.` |
|         - | 5828 | ` * Note that NULL is returned on failure.` |
|         - | 5829 | ` */` |
|        22 | 5830 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5831 | `{` |
|         - | 5832 | `	ph7_hashmap_node *pEntry;` |
|         - | 5833 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5834 | `	ph7_value *pArray;` |
|         - | 5835 | `	sxi32 rc;` |
|         - | 5836 | `	sxu32 n;` |
|         - | 5837 | `	int i;` |
|        26 | 5838 | `	if( nArg < 1 ){` |
|         4 | 5839 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5840 | `			"ArgumentCountError",` |
|         - | 5841 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5842 | `			nArg` |
|         - | 5843 | `			);` |
|         - | 5844 | `	}` |
|        23 | 5845 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5846 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5847 | `			"TypeError",` |
|         - | 5848 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5849 | `			ph7_type_name(apArg[0])` |
|         - | 5850 | `			);` |
|         - | 5851 | `	}` |
|        36 | 5852 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5853 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5854 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5855 | `				"TypeError",` |
|         - | 5856 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5857 | `				i + 1,` |
|         2 | 5858 | `				ph7_type_name(apArg[i])` |
|         - | 5859 | `				);` |
|         - | 5860 | `		}` |
|         9 | 5861 | `	}` |
|        17 | 5862 | `	if( nArg == 1 ){` |
|         - | 5863 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5864 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5865 | `		return PH7_OK;` |
|         - | 5866 | `	}` |
|         - | 5867 | `	/* Create a new array */` |
|        15 | 5868 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5869 | `	if( pArray == 0 ){` |
|       ! 0 | 5870 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5871 | `		return PH7_OK;` |
|         - | 5872 | `	}` |
|         - | 5873 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5874 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5875 | `	/* Perform the intersection */` |
|        15 | 5876 | `	pEntry = pSrc->pFirst;` |
|        15 | 5877 | `	n = pSrc->nEntry;` |
|        24 | 5878 | `	for(;;){` |
|        49 | 5879 | `		if( n < 1 ){` |
|        15 | 5880 | `			break;` |
|         - | 5881 | `		}` |
|        57 | 5882 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5883 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5884 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5885 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5886 | `				/* Blob lookup */` |
|        27 | 5887 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5888 | `			}else{` |
|         - | 5889 | `				/* Int key */` |
|        13 | 5890 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5891 | `			}` |
|        39 | 5892 | `			if( rc != SXRET_OK ){` |
|         - | 5893 | `				/* Key does not exist, break immediately */` |
|        17 | 5894 | `				break;` |
|         - | 5895 | `			}` |
|        12 | 5896 | `		}` |
|        35 | 5897 | `		if( i >= nArg ){` |
|         - | 5898 | `			/* Perform the insertion */` |
|        19 | 5899 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5900 | `		}` |
|         - | 5901 | `		/* Point to the next entry */` |
|        35 | 5902 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5903 | `		n--;` |
|         1 | 5904 | `	}` |
|         - | 5905 | `	/* Return the freshly created array */` |
|        15 | 5906 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5907 | `	return PH7_OK;` |
|        15 | 5908 | `}` |
|         - | 5909 | `/*` |
|         - | 5910 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5911 | ` *  Computes the intersection of arrays.` |
|         - | 5912 | ` * Parameters` |
|         - | 5913 | ` *  $array1` |
|         - | 5914 | ` *    The array to compare from` |
|         - | 5915 | ` *  $array2` |
|         - | 5916 | ` *    An array to compare against` |
|         - | 5917 | ` *  $...` |
|         - | 5918 | ` *   More arrays to compare against` |
|         - | 5919 | ` * $callback` |
|         - | 5920 | ` *  The callback comparison function.` |
|         - | 5921 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5922 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5923 | ` *  than the second.` |
|         - | 5924 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5925 | ` * Return` |
|         - | 5926 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5927 | ` *  in all of the parameters. .` |
|         - | 5928 | ` * Note that NULL is returned on failure.` |
|         - | 5929 | ` */` |
|        26 | 5930 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5931 | `{` |
|         - | 5932 | `	ph7_hashmap_node *pEntry;` |
|         - | 5933 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5934 | `	ph7_value *pCallback;` |
|         - | 5935 | `	ph7_value *pArray;` |
|         - | 5936 | `	ph7_value *pVal;` |
|         - | 5937 | `	sxi32 rc;` |
|         - | 5938 | `	sxu32 n;` |
|         - | 5939 | `	int i;` |
|         - | 5940 |  |
|         - | 5941 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5942 | `	if( nArg < 2 ){` |
|         4 | 5943 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5944 | `			"ArgumentCountError",` |
|         - | 5945 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5946 | `			nArg` |
|         - | 5947 | `			);` |
|         - | 5948 | `	}` |
|        29 | 5949 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5950 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5951 | `			"TypeError",` |
|         - | 5952 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5953 | `			ph7_type_name(apArg[0])` |
|         - | 5954 | `			);` |
|         - | 5955 | `	}` |
|         - | 5956 |  |
|        27 | 5957 | `	if( nArg == 2 ){` |
|         - | 5958 | `		/* Only the original array and the callback were provided. */` |
|         - | 5959 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5960 | `		 * validation ordering. */` |
|         3 | 5961 | `	} else {` |
|         - | 5962 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5963 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5964 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5965 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5966 | `					"TypeError",` |
|         - | 5967 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5968 | `					i + 1,` |
|         2 | 5969 | `					ph7_type_name(apArg[i])` |
|         - | 5970 | `					);` |
|         - | 5971 | `			}` |
|        13 | 5972 | `		}` |
|         - | 5973 | `	}` |
|         - | 5974 |  |
|         - | 5975 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5976 | `	pCallback = apArg[nArg - 1];` |
|         - | 5977 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5978 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5979 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5980 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5981 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5982 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5983 | `			 */` |
|         9 | 5984 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5985 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5986 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5987 | `					"TypeError",` |
|         - | 5988 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5989 | `					nArg` |
|         - | 5990 | `					);` |
|         - | 5991 | `			}` |
|         - | 5992 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 5993 | `			{` |
|         6 | 5994 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 5995 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 5996 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 5997 | `					int nMethodLen;` |
|         6 | 5998 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 5999 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6000 | `					if( pClass ){` |
|         - | 6001 | `						/* Class exists but method is missing. */` |
|         4 | 6002 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6003 | `							"TypeError",` |
|         - | 6004 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6005 | `							nArg,` |
|         1 | 6006 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6007 | `							zMethod` |
|         - | 6008 | `							);` |
|         - | 6009 | `					}` |
|         - | 6010 | `					/* Class not found */` |
|         - | 6011 | `					{` |
|         - | 6012 | `						int nName;` |
|         3 | 6013 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6014 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6015 | `							"TypeError",` |
|         - | 6016 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6017 | `							nArg,` |
|         1 | 6018 | `							zName` |
|         - | 6019 | `							);` |
|         - | 6020 | `					}` |
|         - | 6021 | `				}` |
|         - | 6022 | `			}` |
|         - | 6023 | `			/* Fallback message */` |
|       ! 0 | 6024 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6025 | `				"TypeError",` |
|         - | 6026 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6027 | `				nArg` |
|         - | 6028 | `				);` |
|         - | 6029 | `		}` |
|         6 | 6030 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6031 | `			int len;` |
|         3 | 6032 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6033 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6034 | `				"TypeError",` |
|         - | 6035 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6036 | `				nArg,` |
|         1 | 6037 | `				zName` |
|         - | 6038 | `				);` |
|         - | 6039 | `		}` |
|         4 | 6040 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6041 | `			"TypeError",` |
|         - | 6042 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6043 | `			nArg` |
|         - | 6044 | `			);` |
|         - | 6045 | `	}` |
|         - | 6046 |  |
|        11 | 6047 | `	if( nArg == 2 ){` |
|         - | 6048 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6049 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6050 | `		return PH7_OK;` |
|         - | 6051 | `	}` |
|         - | 6052 |  |
|         - | 6053 | `	/* Create a new array */` |
|         7 | 6054 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6055 | `	if( pArray == 0 ){` |
|       ! 0 | 6056 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6057 | `		return PH7_OK;` |
|         - | 6058 | `	}` |
|         - | 6059 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6060 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6061 | `	/* Perform the intersection */` |
|         7 | 6062 | `	pEntry = pSrc->pFirst;` |
|         7 | 6063 | `	n = pSrc->nEntry;` |
|         7 | 6064 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6065 | `	for(;;){` |
|        19 | 6066 | `		if( n < 1 ){` |
|         5 | 6067 | `			break;` |
|         - | 6068 | `		}` |
|         - | 6069 | `		/* Extract the node value */` |
|        15 | 6070 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6071 | `		if( pVal ){` |
|        23 | 6072 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6073 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6074 | `					/* ignore */` |
|       ! 0 | 6075 | `					continue;` |
|         - | 6076 | `				}` |
|         - | 6077 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6078 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6079 | `				/* Perform the lookup */` |
|        15 | 6080 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6081 | `				if( rc != SXRET_OK ){` |
|         - | 6082 | `					/* Value does not exist */` |
|         7 | 6083 | `					break;` |
|         - | 6084 | `				}` |
|         5 | 6085 | `			}` |
|        15 | 6086 | `			if( i >= (nArg-1) ){` |
|         - | 6087 | `				/* Perform the insertion */` |
|         9 | 6088 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6089 | `			}` |
|         7 | 6090 | `		}` |
|        15 | 6091 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6092 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6093 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6094 | `			return PH7_EXCEPTION;` |
|         - | 6095 | `		}` |
|         - | 6096 | `		/* Point to the next entry */` |
|        13 | 6097 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6098 | `		n--;` |
|         1 | 6099 | `	}` |
|         - | 6100 | `	/* Return the freshly created array */` |
|         5 | 6101 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6102 | `	return PH7_OK;` |
|        18 | 6103 | `}` |
|         - | 6104 | `/*` |
|         - | 6105 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6106 | ` *  Fill an array with values.` |
|         - | 6107 | ` * Parameters` |
|         - | 6108 | ` *  $start_index` |
|         - | 6109 | ` *    The first index of the returned array.` |
|         - | 6110 | ` *  $num` |
|         - | 6111 | ` *   Number of elements to insert.` |
|         - | 6112 | ` *  $value` |
|         - | 6113 | ` *    Value to use for filling.` |
|         - | 6114 | ` * Return` |
|         - | 6115 | ` *  The filled array or null on failure.` |
|         - | 6116 | ` */` |
|       244 | 6117 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6118 | `{` |
|         - | 6119 | `	ph7_value *pArray;` |
|         - | 6120 | `	int i,nEntry;` |
|         - | 6121 |  |
|         - | 6122 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 6123 | `	if( nArg != 3 ){` |
|         - | 6124 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6125 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6126 | `			"ArgumentCountError",` |
|         - | 6127 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6128 | `			nArg` |
|         - | 6129 | `			);` |
|         - | 6130 | `	}` |
|         - | 6131 |  |
|         - | 6132 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6133 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6134 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6135 | `	 * and NULLs are rejected outright. */` |
|       359 | 6136 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6137 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6138 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6139 | `			"TypeError",` |
|         - | 6140 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6141 | `			ph7_type_name(apArg[0])` |
|         - | 6142 | `			);` |
|         - | 6143 | `	}` |
|       242 | 6144 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6145 | `		int len;` |
|         8 | 6146 | `		sxu8 bReal = FALSE;` |
|         8 | 6147 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6148 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6149 | `			/* Non‑numeric string is an error. */` |
|         3 | 6150 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6151 | `				"TypeError",` |
|         - | 6152 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6153 | `				);` |
|         - | 6154 | `		}` |
|         5 | 6155 | `		if( bReal ){` |
|         - | 6156 | `			/* float-string -> deprecation warning */` |
|         4 | 6157 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6158 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6159 | `				zStr` |
|         - | 6160 | `				);` |
|         1 | 6161 | `		}` |
|         2 | 6162 | `	}` |
|         - | 6163 |  |
|         - | 6164 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6165 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6166 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6167 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6168 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6169 | `			"TypeError",` |
|         - | 6170 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6171 | `			ph7_type_name(apArg[1])` |
|         - | 6172 | `			);` |
|         - | 6173 | `	}` |
|       239 | 6174 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6175 | `		int len;` |
|         3 | 6176 | `		sxu8 bReal = FALSE;` |
|         3 | 6177 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6178 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6179 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6180 | `				"TypeError",` |
|         - | 6181 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6182 | `				);` |
|         - | 6183 | `		}` |
|       ! 0 | 6184 | `	}` |
|         - | 6185 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6186 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6187 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6188 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6189 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6190 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6191 | `		if( d != (double)i64 ){` |
|         7 | 6192 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6193 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6194 | `				d` |
|         - | 6195 | `				);` |
|         2 | 6196 | `		}` |
|         2 | 6197 | `	}` |
|         - | 6198 |  |
|         - | 6199 | `	/* Total number of entries to insert */` |
|       236 | 6200 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6201 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6202 | `	if( nEntry < 0 ){` |
|         3 | 6203 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6204 | `			"ValueError",` |
|         - | 6205 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6206 | `			);` |
|         - | 6207 | `	}` |
|         - | 6208 |  |
|         - | 6209 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6210 | `	if( nEntry == 0 ){` |
|         7 | 6211 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6212 | `		return PH7_OK;` |
|         - | 6213 | `	}` |
|         - | 6214 |  |
|         - | 6215 | `	/* Create a new array */` |
|       227 | 6216 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6217 | `	if( pArray == 0 ){` |
|       ! 0 | 6218 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6219 | `	}` |
|         - | 6220 |  |
|         - | 6221 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6222 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6223 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6224 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6225 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6226 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6227 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6228 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6229 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6230 | `		}` |
|   1058803 | 6231 | `	}` |
|         - | 6232 | `	/* Return the filled array */` |
|       227 | 6233 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6234 | `	return PH7_OK;` |
|       127 | 6235 | `}` |
|         - | 6236 | `/*` |
|         - | 6237 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6238 | ` *  Fill an array with values, specifying keys.` |
|         - | 6239 | ` * Parameters` |
|         - | 6240 | ` *  $input` |
|         - | 6241 | ` *   Array of values that will be used as key.` |
|         - | 6242 | ` *  $value` |
|         - | 6243 | ` *    Value to use for filling.` |
|         - | 6244 | ` * Return` |
|         - | 6245 | ` *  The filled array.` |
|         - | 6246 | ` * Throws` |
|         - | 6247 | ` *  ValueError if $input is not an array.` |
|         - | 6248 | ` */` |
|        26 | 6249 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6250 | `{` |
|         - | 6251 | `	ph7_hashmap_node *pEntry;` |
|         - | 6252 | `	ph7_hashmap *pSrc;` |
|         - | 6253 | `	ph7_value *pArray;` |
|         - | 6254 | `	sxu32 n;` |
|         - | 6255 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6256 | `	if( nArg != 2 ){` |
|        12 | 6257 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6258 | `			"ArgumentCountError",` |
|         - | 6259 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6260 | `			nArg` |
|         - | 6261 | `			);` |
|         - | 6262 | `	}` |
|         - | 6263 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6264 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6265 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6266 | `			"TypeError",` |
|         - | 6267 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6268 | `			ph7_type_name(apArg[0])` |
|         - | 6269 | `			);` |
|         - | 6270 | `	}` |
|         - | 6271 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6272 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6273 | `	/* Create a new array */` |
|        17 | 6274 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6275 | `	if( pArray == 0 ){` |
|       ! 0 | 6276 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6277 | `		return PH7_OK;` |
|         - | 6278 | `	}` |
|         - | 6279 | `	/* Perform the requested operation */` |
|        17 | 6280 | `	pEntry = pSrc->pFirst;` |
|        45 | 6281 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6282 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6283 | `		/* Point to the next entry */` |
|        29 | 6284 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6285 | `	}` |
|         - | 6286 | `	/* Return the filled array */` |
|        17 | 6287 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6288 | `	return PH7_OK;` |
|        18 | 6289 | `}` |
|         - | 6290 | `/*` |
|         - | 6291 | ` * array array_combine(array $keys,array $values)` |
|         - | 6292 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6293 | ` * Parameters` |
|         - | 6294 | ` *  $keys` |
|         - | 6295 | ` *    Array of keys to be used.` |
|         - | 6296 | ` * $values` |
|         - | 6297 | ` *   Array of values to be used.` |
|         - | 6298 | ` * Return` |
|         - | 6299 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6300 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6301 | ` *  not an array.` |
|         - | 6302 | ` */` |
|        18 | 6303 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6304 | `{` |
|         - | 6305 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6306 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6307 | `	ph7_value *pArray;` |
|         - | 6308 | `	sxu32 n;` |
|         - | 6309 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6310 | `	if( nArg != 2 ){` |
|         - | 6311 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6312 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6313 | `			"ArgumentCountError",` |
|         - | 6314 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6315 | `			nArg` |
|         - | 6316 | `			);` |
|         - | 6317 | `	}` |
|         - | 6318 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6319 | `	 * argument index in the error message. */` |
|        20 | 6320 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6321 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6322 | `			"TypeError",` |
|         - | 6323 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6324 | `			ph7_type_name(apArg[0])` |
|         - | 6325 | `			);` |
|         - | 6326 | `	}` |
|        17 | 6327 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6328 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6329 | `			"TypeError",` |
|         - | 6330 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6331 | `			ph7_type_name(apArg[1])` |
|         - | 6332 | `			);` |
|         - | 6333 | `	}` |
|         - | 6334 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6335 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6336 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6337 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6338 | `		/* Length mismatch -> ValueError */` |
|         3 | 6339 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6340 | `			"ValueError",` |
|         - | 6341 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6342 | `			);` |
|         - | 6343 | `	}` |
|         - | 6344 | `	/* Create a new array */` |
|        11 | 6345 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6346 | `	if( pArray == 0 ){` |
|       ! 0 | 6347 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6348 | `		return PH7_OK;` |
|         - | 6349 | `	}` |
|         - | 6350 | `	/* Perform the requested operation */` |
|        11 | 6351 | `	pKe = pKey->pFirst;` |
|        11 | 6352 | `	pVe = pValue->pFirst;` |
|        33 | 6353 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6354 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6355 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6356 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6357 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6358 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6359 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6360 | `		 * original array must not be mutated. */` |
|        23 | 6361 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6362 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6363 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6364 | `			if( pTmpKey ){` |
|         5 | 6365 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6366 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6367 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6368 | `				pKeyCopy = pTmpKey;` |
|         2 | 6369 | `			}` |
|         2 | 6370 | `		}` |
|        23 | 6371 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6372 | `		/* Point to the next entry */` |
|        23 | 6373 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6374 | `		pVe = pVe->pPrev;` |
|        12 | 6375 | `	}` |
|         - | 6376 | `	/* Return the filled array */` |
|        11 | 6377 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6378 | `	return PH7_OK;` |
|        14 | 6379 | `}` |
|         - | 6380 | `/*` |
|         - | 6381 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6382 | ` *  Return an array with elements in reverse order.` |
|         - | 6383 | ` * Parameters` |
|         - | 6384 | ` *  $array` |
|         - | 6385 | ` *   The input array.` |
|         - | 6386 | ` *  $preserve_keys (optional)` |
|         - | 6387 | ` *   If set to TRUE keys are preserved.` |
|         - | 6388 | ` * Return` |
|         - | 6389 | ` *  The reversed array.` |
|         - | 6390 | ` */` |
|        20 | 6391 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6392 | `{` |
|         - | 6393 | `	ph7_hashmap_node *pEntry;` |
|         - | 6394 | `	ph7_hashmap *pSrc;` |
|         - | 6395 | `	ph7_value *pArray;` |
|         - | 6396 | `	int bPreserve;` |
|         - | 6397 | `	sxu32 n;` |
|        23 | 6398 | `	if( nArg < 1 ){` |
|         4 | 6399 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6400 | `			"ArgumentCountError",` |
|         - | 6401 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6402 | `			nArg` |
|         - | 6403 | `			);` |
|         - | 6404 | `	}` |
|         - | 6405 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6406 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6407 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6408 | `			"TypeError",` |
|         - | 6409 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6410 | `			ph7_type_name(apArg[0])` |
|         - | 6411 | `			);` |
|         - | 6412 | `	}` |
|        17 | 6413 | `	bPreserve = FALSE;` |
|        17 | 6414 | `	if( nArg > 1 ){` |
|         7 | 6415 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6416 | `	}` |
|         - | 6417 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6418 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6419 | `	/* Create a new array */` |
|        17 | 6420 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6421 | `	if( pArray == 0 ){` |
|       ! 0 | 6422 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6423 | `		return PH7_OK;` |
|         - | 6424 | `	}` |
|         - | 6425 | `	/* Perform the requested operation */` |
|        17 | 6426 | `	pEntry = pSrc->pLast;` |
|        55 | 6427 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6428 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6429 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6430 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6431 | `		/* Point to the previous entry */` |
|        39 | 6432 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6433 | `	}` |
|        17 | 6434 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6435 | `	return PH7_OK;` |
|        13 | 6436 | `}` |
|         - | 6437 | `/*` |
|         - | 6438 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6439 | ` *  Removes duplicate values from an array.` |
|         - | 6440 | ` * Parameters` |
|         - | 6441 | ` *  $array` |
|         - | 6442 | ` *   The input array.` |
|         - | 6443 | ` *  $flags` |
|         - | 6444 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6445 | ` *   behavior using these values:` |
|         - | 6446 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6447 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6448 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6449 | ` * Return` |
|         - | 6450 | ` *  The filtered array.` |
|         - | 6451 | ` */` |
|        24 | 6452 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6453 | `{` |
|         - | 6454 | `	ph7_hashmap_node *pEntry;` |
|         - | 6455 | `	ph7_value *pNeedle;` |
|         - | 6456 | `	ph7_hashmap *pSrc;` |
|         - | 6457 | `	ph7_value *pArray;` |
|         - | 6458 | `	int bStrict;` |
|         - | 6459 | `	sxi32 rc;` |
|         - | 6460 | `	sxu32 n;` |
|        28 | 6461 | `	if( nArg < 1 ){` |
|         - | 6462 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6463 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6464 | `			"ArgumentCountError",` |
|         - | 6465 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6466 | `			);` |
|         - | 6467 | `	}` |
|        25 | 6468 | `	if( nArg > 2 ){` |
|         - | 6469 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6470 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6471 | `			"ArgumentCountError",` |
|         - | 6472 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6473 | `			nArg` |
|         - | 6474 | `			);` |
|         - | 6475 | `	}` |
|         - | 6476 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6477 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6478 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6479 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6480 | `			"TypeError",` |
|         - | 6481 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6482 | `			ph7_type_name(apArg[0])` |
|         - | 6483 | `			);` |
|         - | 6484 | `	}` |
|        19 | 6485 | `	bStrict = FALSE;` |
|         - | 6486 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6487 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6488 | `	/* Create a new array */` |
|        19 | 6489 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6490 | `	if( pArray == 0 ){` |
|       ! 0 | 6491 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6492 | `		return PH7_OK;` |
|         - | 6493 | `	}` |
|         - | 6494 | `	/* Perform the requested operation */` |
|        19 | 6495 | `	pEntry = pSrc->pFirst;` |
|        83 | 6496 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6497 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6498 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6499 | `		if( pNeedle ){` |
|        65 | 6500 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6501 | `		}` |
|        65 | 6502 | `		if( rc != SXRET_OK ){` |
|         - | 6503 | `			/* Perform the insertion */` |
|        37 | 6504 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6505 | `		}` |
|         - | 6506 | `		/* Point to the next entry */` |
|        65 | 6507 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6508 | `	}` |
|         - | 6509 | `	/* Return the freshly created array */` |
|        19 | 6510 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6511 | `	return PH7_OK;` |
|        16 | 6512 | `}` |
|         - | 6513 | `/*` |
|         - | 6514 | ` * array array_flip(array $input)` |
|         - | 6515 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6516 | ` * Parameter` |
|         - | 6517 | ` *  $input` |
|         - | 6518 | ` *   Input array.` |
|         - | 6519 | ` * Return` |
|         - | 6520 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6521 | ` */` |
|        34 | 6522 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6523 | `{` |
|         - | 6524 | `	ph7_hashmap_node *pEntry;` |
|         - | 6525 | `	ph7_hashmap *pSrc;` |
|         - | 6526 | `	ph7_value *pArray;` |
|         - | 6527 | `	ph7_value *pKey;` |
|         - | 6528 | `	ph7_value sVal;` |
|         - | 6529 | `	sxu32 n;` |
|         - | 6530 |  |
|         - | 6531 | `	/* PHP requires exactly one argument */` |
|        39 | 6532 | `	if( nArg != 1 ){` |
|         - | 6533 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6534 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6535 | `			"ArgumentCountError",` |
|         - | 6536 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6537 | `			nArg` |
|         - | 6538 | `			);` |
|         - | 6539 | `	}` |
|         - | 6540 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6541 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6542 | `		/* Type mismatch -> TypeError */` |
|         8 | 6543 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6544 | `			"TypeError",` |
|         - | 6545 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6546 | `			ph7_type_name(apArg[0])` |
|         - | 6547 | `			);` |
|         - | 6548 | `	}` |
|         - | 6549 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6550 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6551 | `	/* Create a new array */` |
|        27 | 6552 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6553 | `	if( pArray == 0 ){` |
|       ! 0 | 6554 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6555 | `		return PH7_OK;` |
|         - | 6556 | `	}` |
|         - | 6557 | `	/* Start processing */` |
|        27 | 6558 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6559 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6560 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6561 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6562 | `		if( pKey ){` |
|         - | 6563 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6564 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6565 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6566 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6567 | `					);` |
|     22236 | 6568 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6569 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6570 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6571 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6572 | `				}else{` |
|         - | 6573 | `					SyString sStr;` |
|      2227 | 6574 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6575 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6576 | `				}` |
|         - | 6577 | `				/* Perform the insertion */` |
|     22227 | 6578 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6579 | `				/* Safely release the value because each inserted entry` |
|         - | 6580 | `				 * has its own private copy of the value.` |
|         - | 6581 | `				 */` |
|     22227 | 6582 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6583 | `			}else{` |
|         - | 6584 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6585 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6586 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6587 | `					);` |
|         - | 6588 | `			}` |
|     11118 | 6589 | `		}` |
|         - | 6590 | `		/* Point to the next entry */` |
|     22237 | 6591 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6592 | `	}` |
|         - | 6593 | `	/* Return the freshly created array */` |
|        27 | 6594 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6595 | `	return PH7_OK;` |
|        22 | 6596 | `}` |
|         - | 6597 | `/*` |
|         - | 6598 | ` * number array_sum(array $array )` |
|         - | 6599 | ` *  Calculate the sum of values in an array.` |
|         - | 6600 | ` * Parameters` |
|         - | 6601 | ` *  $array: The input array.` |
|         - | 6602 | ` * Return` |
|         - | 6603 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6604 | ` */` |
|        24 | 6605 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6606 | `{` |
|         - | 6607 | `	ph7_hashmap_node *pEntry;` |
|         - | 6608 | `	ph7_value *pObj;` |
|        25 | 6609 | `	double dSum = 0;` |
|         - | 6610 | `	sxu32 n;` |
|        25 | 6611 | `	pEntry = pMap->pFirst;` |
|        91 | 6612 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6613 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6614 | `		if( pObj ){` |
|        67 | 6615 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6616 | `				dSum += pObj->rVal;` |
|        53 | 6617 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6618 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6619 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6620 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6621 | `					double dv = 0;` |
|        13 | 6622 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6623 | `					dSum += dv;` |
|         7 | 6624 | `				}` |
|        12 | 6625 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6626 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6627 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6628 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6629 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6630 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6631 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6632 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6633 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6634 | `			}` |
|         - | 6635 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6636 | `		}` |
|         - | 6637 | `		/* Point to the next entry */` |
|        67 | 6638 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6639 | `	}` |
|         - | 6640 | `	/* Return sum */` |
|        25 | 6641 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6642 | `}` |
|       680 | 6643 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6644 | `{` |
|         - | 6645 | `	ph7_hashmap_node *pEntry;` |
|         - | 6646 | `	ph7_value *pObj;` |
|       682 | 6647 | `	sxi64 nSum = 0;` |
|         - | 6648 | `	sxu32 n;` |
|       682 | 6649 | `	pEntry = pMap->pFirst;` |
|      4672 | 6650 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      3992 | 6651 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      3992 | 6652 | `		if( pObj ){` |
|      3992 | 6653 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3982 | 6654 | `				nSum += pObj->x.iVal;` |
|      2001 | 6655 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6656 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6657 | `					sxi64 nv = 0;` |
|         5 | 6658 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6659 | `					nSum += nv;` |
|         3 | 6660 | `				}` |
|         8 | 6661 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6662 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6663 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6664 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6665 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6666 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6667 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6668 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6669 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6670 | `			}` |
|         - | 6671 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      1995 | 6672 | `		}` |
|         - | 6673 | `		/* Point to the next entry */` |
|      3992 | 6674 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      1997 | 6675 | `	}` |
|         - | 6676 | `	/* Return sum */` |
|       682 | 6677 | `	ph7_result_int64(pCtx,nSum);` |
|       682 | 6678 | `}` |
|         - | 6679 | `/* number array_sum(array $array )` |
|         - | 6680 | ` * (See block-coment above)` |
|         - | 6681 | ` */` |
|       718 | 6682 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6683 | `{` |
|         - | 6684 | `	ph7_hashmap_node *pEntry;` |
|         - | 6685 | `	ph7_hashmap *pMap;` |
|         - | 6686 | `	ph7_value *pObj;` |
|       723 | 6687 | `	int useDouble = 0;` |
|         - | 6688 | `	sxu32 n;` |
|         - | 6689 | `	/* PHP requires exactly one argument */` |
|       723 | 6690 | `	if( nArg != 1 ){` |
|         8 | 6691 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6692 | `			"ArgumentCountError",` |
|         - | 6693 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6694 | `			nArg` |
|         - | 6695 | `			);` |
|         - | 6696 | `	}` |
|         - | 6697 | `	/* Make sure we are dealing with a valid hashmap */` |
|       717 | 6698 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6699 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6700 | `		char zBuf[64];` |
|         8 | 6701 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6702 | `			"TypeError",` |
|         - | 6703 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6704 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6705 | `			);` |
|         - | 6706 | `	}` |
|       712 | 6707 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       712 | 6708 | `	if( pMap->nEntry < 1 ){` |
|         - | 6709 | `		/* Nothing to compute,return 0 */` |
|         7 | 6710 | `		ph7_result_int(pCtx,0);` |
|         7 | 6711 | `		return PH7_OK;` |
|         - | 6712 | `	}` |
|         - | 6713 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6714 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6715 | `	 */` |
|       706 | 6716 | `	pEntry = pMap->pFirst;` |
|      4704 | 6717 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4024 | 6718 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4024 | 6719 | `		if( pObj ){` |
|      4024 | 6720 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6721 | `				useDouble = 1;` |
|        19 | 6722 | `				break;` |
|         - | 6723 | `			}` |
|      4006 | 6724 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6725 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6726 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6727 | `				sxu32 i;` |
|        23 | 6728 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6729 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6730 | `						useDouble = 1;` |
|         7 | 6731 | `						break;` |
|         - | 6732 | `					}` |
|         6 | 6733 | `				}` |
|        13 | 6734 | `				if( useDouble ){` |
|         7 | 6735 | `					break;` |
|         - | 6736 | `				}` |
|         3 | 6737 | `			}` |
|      1999 | 6738 | `		}` |
|      4000 | 6739 | `		pEntry = pEntry->pPrev;` |
|      2001 | 6740 | `	}` |
|       706 | 6741 | `	if( useDouble ){` |
|        25 | 6742 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6743 | `	}else{` |
|       682 | 6744 | `		Int64Sum(pCtx,pMap);` |
|         - | 6745 | `	}` |
|       706 | 6746 | `	return PH7_OK;` |
|       364 | 6747 | `}` |
|         - | 6748 | `/*` |
|         - | 6749 | ` * number array_product(array $array )` |
|         - | 6750 | ` *  Calculate the product of values in an array.` |
|         - | 6751 | ` * Parameters` |
|         - | 6752 | ` *  $array: The input array.` |
|         - | 6753 | ` * Return` |
|         - | 6754 | ` *  Returns the product of values as an integer or float.` |
|         - | 6755 | ` */` |
|         2 | 6756 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6757 | `{` |
|         - | 6758 | `	ph7_hashmap_node *pEntry;` |
|         - | 6759 | `	ph7_value *pObj;` |
|         - | 6760 | `	double dProd;` |
|         - | 6761 | `	sxu32 n;` |
|         3 | 6762 | `	pEntry = pMap->pFirst;` |
|         3 | 6763 | `	dProd = 1;` |
|         7 | 6764 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6765 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6766 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6767 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6768 | `				dProd *= pObj->rVal;` |
|         4 | 6769 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6770 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6771 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6772 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6773 | `					double dv = 0;` |
|       ! 0 | 6774 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6775 | `					dProd *= dv;` |
|       ! 0 | 6776 | `				}` |
|       ! 0 | 6777 | `			}` |
|         2 | 6778 | `		}` |
|         - | 6779 | `		/* Point to the next entry */` |
|         5 | 6780 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6781 | `	}` |
|         - | 6782 | `	/* Return product */` |
|         3 | 6783 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6784 | `}` |
|         2 | 6785 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6786 | `{` |
|         - | 6787 | `	ph7_hashmap_node *pEntry;` |
|         - | 6788 | `	ph7_value *pObj;` |
|         - | 6789 | `	sxi64 nProd;` |
|         - | 6790 | `	sxu32 n;` |
|         3 | 6791 | `	pEntry = pMap->pFirst;` |
|         3 | 6792 | `	nProd = 1;` |
|         9 | 6793 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6794 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6795 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6796 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6797 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6798 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6799 | `				nProd *= pObj->x.iVal;` |
|         3 | 6800 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6801 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6802 | `					sxi64 nv = 0;` |
|       ! 0 | 6803 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6804 | `					nProd *= nv;` |
|       ! 0 | 6805 | `				}` |
|       ! 0 | 6806 | `			}` |
|         3 | 6807 | `		}` |
|         - | 6808 | `		/* Point to the next entry */` |
|         7 | 6809 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6810 | `	}` |
|         - | 6811 | `	/* Return product */` |
|         3 | 6812 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6813 | `}` |
|         - | 6814 | `/* number array_product(array $array )` |
|         - | 6815 | ` * (See block-block comment above)` |
|         - | 6816 | ` */` |
|        18 | 6817 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6818 | `{` |
|         - | 6819 | `	ph7_hashmap *pMap;` |
|         - | 6820 | `	ph7_value *pObj;` |
|        19 | 6821 | `	if( nArg < 1 ){` |
|         - | 6822 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6823 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6824 | `		return PH7_OK;` |
|         - | 6825 | `	}` |
|         - | 6826 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6827 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6828 | `		char zBuf[64];` |
|        19 | 6829 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6830 | `			"TypeError",` |
|         - | 6831 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6832 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6833 | `			);` |
|         - | 6834 | `	}` |
|         7 | 6835 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6836 | `	if( pMap->nEntry < 1 ){` |
|         - | 6837 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6838 | `		ph7_result_int(pCtx,1);` |
|         3 | 6839 | `		return PH7_OK;` |
|         - | 6840 | `	}` |
|         - | 6841 | `	/* If the first element is of type float,then perform floating` |
|         - | 6842 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6843 | `	 */` |
|         5 | 6844 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6845 | `	if( pObj == 0 ){` |
|       ! 0 | 6846 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6847 | `		return PH7_OK;` |
|         - | 6848 | `	}` |
|         5 | 6849 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6850 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6851 | `	}else{` |
|         3 | 6852 | `		Int64Prod(pCtx,pMap);` |
|         - | 6853 | `	}` |
|         5 | 6854 | `	return PH7_OK;` |
|        10 | 6855 | `}` |
|         - | 6856 | `/*` |
|         - | 6857 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6858 | ` *  Pick one or more random entries out of an array.` |
|         - | 6859 | ` * Parameters` |
|         - | 6860 | ` * $input` |
|         - | 6861 | ` *  The input array.` |
|         - | 6862 | ` * $num_req` |
|         - | 6863 | ` *  Specifies how many entries you want to pick.` |
|         - | 6864 | ` * Return` |
|         - | 6865 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6866 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6867 | ` *  NULL is returned on failure.` |
|         - | 6868 | ` */` |
|        42 | 6869 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6870 | `{` |
|         - | 6871 | `	ph7_hashmap_node *pNode;` |
|         - | 6872 | `	ph7_hashmap *pMap;` |
|        43 | 6873 | `	int nItem = 1;` |
|        43 | 6874 | `	if( nArg < 1 ){` |
|         - | 6875 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6876 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6877 | `		return PH7_OK;` |
|         - | 6878 | `	}` |
|         - | 6879 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6880 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6881 | `		char zBuf[64];` |
|        10 | 6882 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6883 | `			"TypeError",` |
|         - | 6884 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6885 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6886 | `			);` |
|         - | 6887 | `	}` |
|         - | 6888 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6889 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6890 | `	if( nArg > 1 ){` |
|        29 | 6891 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6892 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6893 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6894 | `			char zBuf[64];` |
|        10 | 6895 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6896 | `				"TypeError",` |
|         - | 6897 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6898 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6899 | `				);` |
|         - | 6900 | `		}` |
|        23 | 6901 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6902 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6903 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6904 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6905 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6906 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6907 | `			int len;` |
|         9 | 6908 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6909 | `			sxi64 iLong; double dReal;` |
|         9 | 6910 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6911 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6912 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6913 | `					"TypeError",` |
|         - | 6914 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6915 | `					);` |
|         - | 6916 | `			}` |
|         - | 6917 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6918 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6919 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6920 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6921 | `			}` |
|         3 | 6922 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6923 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6924 | `			nItem = (int)iLong;` |
|         2 | 6925 | `		}else{` |
|        15 | 6926 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6927 | `		}` |
|         8 | 6928 | `	}` |
|         - | 6929 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6930 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6931 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6932 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6933 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6934 | `			"ValueError",` |
|         - | 6935 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6936 | `			);` |
|         - | 6937 | `	}` |
|         - | 6938 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6939 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6940 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6941 | `			"ValueError",` |
|         - | 6942 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6943 | `			);` |
|         - | 6944 | `	}` |
|        13 | 6945 | `	if( nItem < 2 ){` |
|         - | 6946 | `		sxu32 nEntry;` |
|         - | 6947 | `		/* Select a random number */` |
|         9 | 6948 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6949 | `		/* Extract the desired entry.` |
|         - | 6950 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6951 | `		 */` |
|         9 | 6952 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         5 | 6953 | `			pNode = pMap->pLast;` |
|         5 | 6954 | `			nEntry = pMap->nEntry - nEntry;` |
|         5 | 6955 | `			if( nEntry > 1 ){` |
|       ! 0 | 6956 | `				for(;;){` |
|       ! 0 | 6957 | `					if( nEntry == 0 ){` |
|       ! 0 | 6958 | `						break;` |
|         - | 6959 | `					}` |
|         - | 6960 | `					/* Point to the previous entry */` |
|       ! 0 | 6961 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6962 | `					nEntry--;` |
|       ! 0 | 6963 | `				}` |
|       ! 0 | 6964 | `			}` |
|         2 | 6965 | `		}else{` |
|         5 | 6966 | `			pNode = pMap->pFirst;` |
|         5 | 6967 | `			for(;;){` |
|         7 | 6968 | `				if( nEntry == 0 ){` |
|         5 | 6969 | `					break;` |
|         - | 6970 | `				}` |
|         - | 6971 | `				/* Point to the next entry */` |
|         3 | 6972 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         3 | 6973 | `				nEntry--;` |
|         1 | 6974 | `			}` |
|         - | 6975 | `		}` |
|         9 | 6976 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6977 | `			/* Int key */` |
|         7 | 6978 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6979 | `		}else{` |
|         - | 6980 | `			/* Blob key */` |
|         3 | 6981 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6982 | `		}` |
|         5 | 6983 | `	}else{` |
|         - | 6984 | `		ph7_value sKey,*pArray;` |
|         - | 6985 | `		ph7_hashmap *pDest;` |
|         - | 6986 | `		/* Create a new array */` |
|         5 | 6987 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 6988 | `		if( pArray == 0 ){` |
|       ! 0 | 6989 | `			ph7_result_null(pCtx);` |
|       ! 0 | 6990 | `			return PH7_OK;` |
|         - | 6991 | `		}` |
|         - | 6992 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 6993 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 6994 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 6995 | `		/* Copy the first n items */` |
|         5 | 6996 | `		pNode = pMap->pFirst;` |
|         5 | 6997 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 6998 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 6999 | `		}` |
|        15 | 7000 | `		while( nItem > 0){` |
|        11 | 7001 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7002 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7003 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7004 | `			/* Point to the next entry */` |
|        11 | 7005 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7006 | `			nItem--;` |
|         1 | 7007 | `		}` |
|         - | 7008 | `		/* Shuffle the array */` |
|         5 | 7009 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7010 | `		/* Rehash node */` |
|         5 | 7011 | `		HashmapSortRehash(pDest);` |
|         - | 7012 | `		/* Return the random array */` |
|         5 | 7013 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7014 | `	}` |
|        13 | 7015 | `	return PH7_OK;` |
|        22 | 7016 | `}` |
|         - | 7017 | `/*` |
|         - | 7018 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7019 | ` *  Split an array into chunks.` |
|         - | 7020 | ` * Parameters` |
|         - | 7021 | ` * $input` |
|         - | 7022 | ` *   The array to work on` |
|         - | 7023 | ` * $size` |
|         - | 7024 | ` *   The size of each chunk` |
|         - | 7025 | ` * $preserve_keys` |
|         - | 7026 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7027 | ` *   the chunk numerically.` |
|         - | 7028 | ` * Return` |
|         - | 7029 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7030 | ` *  zero, with each dimension containing size elements.` |
|         - | 7031 | ` */` |
|        42 | 7032 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7033 | `{` |
|         - | 7034 | `	ph7_value *pArray,*pChunk;` |
|         - | 7035 | `	ph7_hashmap_node *pEntry;` |
|         - | 7036 | `	ph7_hashmap *pMap;` |
|         - | 7037 | `	int bPreserve;` |
|         - | 7038 | `	sxu32 nChunk;` |
|         - | 7039 | `	sxu32 nSize;` |
|         - | 7040 | `	sxu32 n;` |
|         - | 7041 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 7042 | `	if( nArg < 2 ){` |
|         - | 7043 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 7044 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7045 | `			"ArgumentCountError",` |
|         - | 7046 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 7047 | `			nArg` |
|         - | 7048 | `			);` |
|         - | 7049 | `	}` |
|        45 | 7050 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7051 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7052 | `			"TypeError",` |
|         - | 7053 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7054 | `			ph7_type_name(apArg[0])` |
|         - | 7055 | `			);` |
|         - | 7056 | `	}` |
|         - | 7057 | `	/* Create a new array */` |
|        43 | 7058 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 7059 | `	if( pArray == 0 ){` |
|       ! 0 | 7060 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7061 | `		return PH7_OK;` |
|         - | 7062 | `	}` |
|         - | 7063 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 7064 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7065 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7066 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 7067 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 7068 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 7069 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7070 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7071 | `			"TypeError",` |
|         - | 7072 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7073 | `			ph7_type_name(apArg[1])` |
|         - | 7074 | `			);` |
|         - | 7075 | `	}` |
|         - | 7076 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7077 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7078 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 7079 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7080 | `		int len;` |
|         3 | 7081 | `		sxu8 bReal = FALSE;` |
|         3 | 7082 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7083 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7084 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7085 | `				"TypeError",` |
|         - | 7086 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7087 | `				);` |
|         - | 7088 | `		}` |
|       ! 0 | 7089 | `		if( bReal ){` |
|         - | 7090 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7091 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7092 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7093 | `				zStr` |
|         - | 7094 | `				);` |
|       ! 0 | 7095 | `		}` |
|       ! 0 | 7096 | `	}` |
|         - | 7097 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7098 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7099 | `	 * later via ph7_value_to_int. */` |
|        40 | 7100 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7101 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7102 | `		sxi64 i = (sxi64)d;` |
|         3 | 7103 | `		if( d != (double)i ){` |
|         4 | 7104 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7105 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7106 | `				d` |
|         - | 7107 | `				);` |
|         1 | 7108 | `		}` |
|         1 | 7109 | `	}` |
|         - | 7110 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7111 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7112 | `	{` |
|        40 | 7113 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 7114 | `		if( nSizeSigned < 1 ){` |
|         - | 7115 | `			/* size <= 0 -> ValueError */` |
|         6 | 7116 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7117 | `				"ValueError",` |
|         - | 7118 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7119 | `				);` |
|         - | 7120 | `		}` |
|        35 | 7121 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7122 | `	}` |
|        35 | 7123 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7124 | `		/* Return the whole array */` |
|         3 | 7125 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7126 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7127 | `		return PH7_OK;` |
|         - | 7128 | `	}` |
|        33 | 7129 | `	bPreserve = 0;` |
|        33 | 7130 | `	if( nArg > 2 ){` |
|         - | 7131 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7132 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7133 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7134 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7135 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7136 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7137 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7138 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7139 | `				"TypeError",` |
|         - | 7140 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7141 | `				ph7_type_name(apArg[2])` |
|         - | 7142 | `				);` |
|         - | 7143 | `		}` |
|        21 | 7144 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7145 | `	}` |
|         - | 7146 | `	/* Start processing */` |
|        27 | 7147 | `	pEntry = pMap->pFirst;` |
|        27 | 7148 | `	nChunk = 0;` |
|        27 | 7149 | `	pChunk = 0;` |
|        27 | 7150 | `	n = pMap->nEntry;` |
|        56 | 7151 | `	for( ;; ){` |
|       113 | 7152 | `		if( n < 1 ){` |
|         - | 7153 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7154 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7155 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7156 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7157 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7158 | `			 * exists. */` |
|        27 | 7159 | `			if( pChunk ){` |
|        27 | 7160 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7161 | `			}` |
|        27 | 7162 | `			break;` |
|         - | 7163 | `		}` |
|        87 | 7164 | `		if( nChunk < 1 ){` |
|        71 | 7165 | `			if( pChunk ){` |
|         - | 7166 | `				/* Put the first chunk */` |
|        45 | 7167 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7168 | `			}` |
|         - | 7169 | `			/* Create a new dimension */` |
|        71 | 7170 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7171 | `												   * will be automatically released as soon we return` |
|         - | 7172 | `												   * from this function */` |
|        71 | 7173 | `			if( pChunk == 0 ){` |
|       ! 0 | 7174 | `				break;` |
|         - | 7175 | `			}` |
|        71 | 7176 | `			nChunk = nSize;` |
|        35 | 7177 | `		}` |
|         - | 7178 | `		/* Insert the entry */` |
|        87 | 7179 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7180 | `		/* Point to the next entry */` |
|        87 | 7181 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7182 | `		nChunk--;` |
|        87 | 7183 | `		n--;` |
|         1 | 7184 | `	}` |
|         - | 7185 | `	/* Return the multidimensional array */` |
|        27 | 7186 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7187 | `	return PH7_OK;` |
|        26 | 7188 | `}` |
|         - | 7189 | `/*` |
|         - | 7190 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7191 | ` *  Pad array to the specified length with a value.` |
|         - | 7192 | ` * $input` |
|         - | 7193 | ` *   Initial array of values to pad.` |
|         - | 7194 | ` * $pad_size` |
|         - | 7195 | ` *   New size of the array.` |
|         - | 7196 | ` * $pad_value` |
|         - | 7197 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7198 | ` */` |
|         - | 7199 | `/*` |
|         - | 7200 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7201 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7202 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7203 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7204 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7205 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7206 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7207 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7208 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7209 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7210 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7211 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7212 | ` */` |
|        50 | 7213 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7214 | `	ph7_context *pCtx,` |
|         - | 7215 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7216 | `	int iArg,              /* 1-based argument position */` |
|         - | 7217 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7218 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7219 | `	)` |
|         1 | 7220 | `{` |
|        51 | 7221 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7222 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7223 | `			"ValueError",` |
|         - | 7224 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7225 | `			zFunc,iArg,zParam` |
|         - | 7226 | `			);` |
|         - | 7227 | `	}` |
|        37 | 7228 | `	return SXRET_OK;` |
|        26 | 7229 | `}` |
|        72 | 7230 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7231 | `{` |
|         - | 7232 | `	ph7_hashmap *pMap;` |
|         - | 7233 | `	ph7_value *pArray;` |
|         - | 7234 | `	sxi64 iLen,iAbs;` |
|         - | 7235 | `	int nEntry;` |
|         - | 7236 | `	sxi32 rc;` |
|        77 | 7237 | `	if( nArg != 3 ){` |
|        12 | 7238 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7239 | `			"ArgumentCountError",` |
|         - | 7240 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7241 | `			nArg` |
|         - | 7242 | `			);` |
|         - | 7243 | `	}` |
|        68 | 7244 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7245 | `		char zBuf[64];` |
|        14 | 7246 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7247 | `			"TypeError",` |
|         - | 7248 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7249 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7250 | `			);` |
|         - | 7251 | `	}` |
|         - | 7252 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7253 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7254 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7255 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        58 | 7256 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        56 | 7257 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7258 | `		char zBuf[64];` |
|         7 | 7259 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7260 | `			"TypeError",` |
|         - | 7261 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7262 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7263 | `			);` |
|         - | 7264 | `	}` |
|        55 | 7265 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7266 | `		int nStr;` |
|        11 | 7267 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7268 | `		sxi64 iLong; double dReal;` |
|        11 | 7269 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7270 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7271 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7272 | `				"TypeError",` |
|         - | 7273 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7274 | `				);` |
|         - | 7275 | `		}` |
|         7 | 7276 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7277 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7278 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7279 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7280 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7281 | `					"TypeError",` |
|         - | 7282 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7283 | `					);` |
|         - | 7284 | `			}` |
|         3 | 7285 | `			iLen = (sxi64)dReal;` |
|         3 | 7286 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7287 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7288 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7289 | `					zStr` |
|         - | 7290 | `					);` |
|       ! 0 | 7291 | `			}` |
|         2 | 7292 | `		}else{` |
|         5 | 7293 | `			iLen = iLong;` |
|         - | 7294 | `		}` |
|         4 | 7295 | `	}else{` |
|        45 | 7296 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7297 | `	}` |
|         - | 7298 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7299 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7300 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7301 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7302 | `	 * overflow). */` |
|        51 | 7303 | `	iAbs = iLen;` |
|        51 | 7304 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7305 | `		iAbs = -iAbs;` |
|         7 | 7306 | `	}` |
|        51 | 7307 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7308 | `	if( rc != SXRET_OK ){` |
|        15 | 7309 | `		return rc;` |
|         - | 7310 | `	}` |
|        37 | 7311 | `	nEntry = (int)iLen;` |
|         - | 7312 | `	/* Create a new array */` |
|        37 | 7313 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7314 | `	if( pArray == 0 ){` |
|       ! 0 | 7315 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7316 | `	}` |
|        37 | 7317 | `	if( nEntry < 0 ){` |
|        11 | 7318 | `		nEntry = -nEntry;` |
|        11 | 7319 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7320 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7321 | `			/* Insert given items first */` |
|        25 | 7322 | `			while( nEntry > 0 ){` |
|        19 | 7323 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7324 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7325 | `				}` |
|        19 | 7326 | `				nEntry--;` |
|         1 | 7327 | `			}` |
|         - | 7328 | `			/* Merge the two arrays */` |
|         7 | 7329 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7330 | `		}else{` |
|         5 | 7331 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7332 | `		}` |
|        32 | 7333 | `	}else if( nEntry > 0 ){` |
|        25 | 7334 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7335 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7336 | `			/* Merge the two arrays first */` |
|        19 | 7337 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7338 | `			/* Insert given items */` |
|       275 | 7339 | `			while( nEntry > 0 ){` |
|       257 | 7340 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7341 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7342 | `				}` |
|       257 | 7343 | `				nEntry--;` |
|         1 | 7344 | `			}` |
|        10 | 7345 | `		}else{` |
|         7 | 7346 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7347 | `		}` |
|        13 | 7348 | `	}else{` |
|         - | 7349 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7350 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7351 | `	}` |
|         - | 7352 | `	/* Return the new array */` |
|        37 | 7353 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7354 | `	return PH7_OK;` |
|        41 | 7355 | `}` |
|         - | 7356 | `/*` |
|         - | 7357 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7358 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7359 | ` * Parameters` |
|         - | 7360 | ` * $array` |
|         - | 7361 | ` *   The array in which elements are replaced.` |
|         - | 7362 | ` * $array1` |
|         - | 7363 | ` *   The array from which elements will be extracted.` |
|         - | 7364 | ` * ....` |
|         - | 7365 | ` *  More arrays from which elements will be extracted.` |
|         - | 7366 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7367 | ` * Return` |
|         - | 7368 | ` *  Returns an array.` |
|         - | 7369 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7370 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7371 | ` */` |
|        22 | 7372 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7373 | `{` |
|         - | 7374 | `	ph7_hashmap *pMap;` |
|         - | 7375 | `	ph7_value *pArray;` |
|         - | 7376 | `	int i;` |
|        26 | 7377 | `	if( nArg < 1 ){` |
|         3 | 7378 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7379 | `			"ArgumentCountError",` |
|         - | 7380 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7381 | `			);` |
|         - | 7382 | `	}` |
|        23 | 7383 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7384 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7385 | `			"TypeError",` |
|         - | 7386 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7387 | `			ph7_type_name(apArg[0])` |
|         - | 7388 | `			);` |
|         - | 7389 | `	}` |
|         - | 7390 | `	/* Create a new array */` |
|        20 | 7391 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7392 | `	if( pArray == 0 ){` |
|       ! 0 | 7393 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7394 | `		return PH7_OK;` |
|         - | 7395 | `	}` |
|         - | 7396 | `	/* Overwrite from the first array */` |
|        20 | 7397 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7398 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7399 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7400 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7401 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7402 | `			/* Type mismatch -> TypeError */` |
|         4 | 7403 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7404 | `				"TypeError",` |
|         - | 7405 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7406 | `				i + 1,` |
|         2 | 7407 | `				ph7_type_name(apArg[i])` |
|         - | 7408 | `				);` |
|         - | 7409 | `		}` |
|         - | 7410 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7411 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7412 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7413 | `	}` |
|         - | 7414 | `	/* Return the new array */` |
|        17 | 7415 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7416 | `	return PH7_OK;` |
|        15 | 7417 | `}` |
|         - | 7418 | `/*` |
|         - | 7419 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7420 | ` *  Filters elements of an array using a callback function.` |
|         - | 7421 | ` * Parameters` |
|         - | 7422 | ` *  $input` |
|         - | 7423 | ` *    The array to iterate over` |
|         - | 7424 | ` * $callback` |
|         - | 7425 | ` *    The callback function to use` |
|         - | 7426 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7427 | ` *    will be removed.` |
|         - | 7428 | ` * Return` |
|         - | 7429 | ` *  The filtered array.` |
|         - | 7430 | ` */` |
|        32 | 7431 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7432 | `{` |
|         - | 7433 | `	ph7_hashmap_node *pEntry;` |
|         - | 7434 | `	ph7_hashmap *pMap;` |
|         - | 7435 | `	ph7_value *pArray;` |
|         - | 7436 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7437 | `	ph7_value *pValue;` |
|         - | 7438 | `	sxi32 rc;` |
|         - | 7439 | `	int keep;` |
|         - | 7440 | `	sxu32 n;` |
|        34 | 7441 | `	if( nArg < 1 ){` |
|         - | 7442 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7443 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7444 | `		return PH7_OK;` |
|         - | 7445 | `	}` |
|         - | 7446 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7447 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7448 | `		char zBuf[64];` |
|        22 | 7449 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7450 | `			"TypeError",` |
|         - | 7451 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7452 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7453 | `			);` |
|         - | 7454 | `	}` |
|         - | 7455 | `	/* Create a new array */` |
|        20 | 7456 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7457 | `	if( pArray == 0 ){` |
|       ! 0 | 7458 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7459 | `		return PH7_OK;` |
|         - | 7460 | `	}` |
|         - | 7461 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7462 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7463 | `	pEntry = pMap->pFirst;` |
|        20 | 7464 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7465 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7466 | `	/* Perform the requested operation */` |
|        78 | 7467 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7468 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7469 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7470 | `		if( pValue == 0 ){` |
|         - | 7471 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7472 | `			keep = FALSE;` |
|        64 | 7473 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7474 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7475 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7476 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7477 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7478 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7479 | `					int len;` |
|         3 | 7480 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7481 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7482 | `						"TypeError",` |
|         - | 7483 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7484 | `						zName` |
|         - | 7485 | `						);` |
|       ! 0 | 7486 | `				}else{` |
|       ! 0 | 7487 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7488 | `						"TypeError",` |
|         - | 7489 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7490 | `						ph7_type_name(apArg[1])` |
|         - | 7491 | `						);` |
|         - | 7492 | `				}` |
|         - | 7493 | `			}` |
|        33 | 7494 | `			keep = FALSE;` |
|        33 | 7495 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7496 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7497 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7498 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7499 | `				return PH7_EXCEPTION;` |
|         - | 7500 | `			}` |
|        31 | 7501 | `			if( rc == SXRET_OK ){` |
|         - | 7502 | `				/* Perform a boolean cast */` |
|        31 | 7503 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7504 | `			}` |
|        31 | 7505 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7506 | `		}else{` |
|         - | 7507 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7508 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7509 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7510 | `			 */` |
|        29 | 7511 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7512 | `		}` |
|        59 | 7513 | `		if( keep ){` |
|         - | 7514 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7515 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7516 | `		}` |
|         - | 7517 | `		/* Point to the next entry */` |
|        59 | 7518 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7519 | `	}` |
|        15 | 7520 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7521 | `	return PH7_OK;` |
|        18 | 7522 | `}` |
|         - | 7523 | `/*` |
|         - | 7524 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7525 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7526 | ` * Parameters` |
|         - | 7527 | ` *  $callback` |
|         - | 7528 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7529 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7530 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7531 | ` *   are zipped together.` |
|         - | 7532 | ` *  $array` |
|         - | 7533 | ` *   The first array to run through the callback function.` |
|         - | 7534 | ` *  $arrays` |
|         - | 7535 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7536 | ` * Return` |
|         - | 7537 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7538 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7539 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7540 | ` *  padding shorter arrays with NULL.` |
|         - | 7541 | ` */` |
|        62 | 7542 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7543 | `{` |
|         - | 7544 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7545 | `	ph7_hashmap_node *pEntry;` |
|         - | 7546 | `	ph7_hashmap *pMap;` |
|         - | 7547 | `	ph7_vm *pVm;` |
|         - | 7548 | `	int bNullCallback;` |
|         - | 7549 | `	sxi32 rc;` |
|         - | 7550 | `	int i;` |
|         - | 7551 | `	sxu32 n;` |
|        67 | 7552 | `	if( nArg < 2 ){` |
|         8 | 7553 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7554 | `			"ArgumentCountError",` |
|         - | 7555 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7556 | `			nArg` |
|         - | 7557 | `			);` |
|         - | 7558 | `	}` |
|        62 | 7559 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        62 | 7560 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7561 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7562 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7563 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7564 | `				"TypeError",` |
|         - | 7565 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7566 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7567 | `				zFunc` |
|         - | 7568 | `				);` |
|         - | 7569 | `		}` |
|         3 | 7570 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7571 | `			"TypeError",` |
|         - | 7572 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7573 | `			"no array or string given"` |
|         - | 7574 | `			);` |
|         - | 7575 | `	}` |
|         - | 7576 | `	/* Every remaining argument must be an array */` |
|       121 | 7577 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7578 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7579 | `			if( i == 1 ){` |
|         4 | 7580 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7581 | `					"TypeError",` |
|         - | 7582 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7583 | `					ph7_type_name(apArg[1])` |
|         - | 7584 | `					);` |
|         - | 7585 | `			}` |
|       ! 0 | 7586 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7587 | `				"TypeError",` |
|         - | 7588 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7589 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7590 | `				);` |
|         - | 7591 | `		}` |
|        34 | 7592 | `	}` |
|        54 | 7593 | `	pVm = pCtx->pVm;` |
|         - | 7594 | `	/* Create a new array */` |
|        54 | 7595 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7596 | `	if( pArray == 0 ){` |
|       ! 0 | 7597 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7598 | `		return PH7_OK;` |
|         - | 7599 | `	}` |
|        54 | 7600 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7601 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7602 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7603 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7604 | `	if( nArg == 2 ){` |
|         - | 7605 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7606 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7607 | `		pEntry = pMap->pFirst;` |
|       134 | 7608 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7609 | `			/* Extract the node value */` |
|        96 | 7610 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7611 | `			if( pValue ){` |
|         - | 7612 | `				/* Extract the node key */` |
|        96 | 7613 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7614 | `				if( bNullCallback ){` |
|         - | 7615 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7616 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7617 | `				}else{` |
|         - | 7618 | `					/* Invoke the supplied callback */` |
|        86 | 7619 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7620 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7621 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7622 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7623 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7624 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7625 | `						return PH7_EXCEPTION;` |
|         - | 7626 | `					}` |
|         - | 7627 | `					/* Insert the callback return value */` |
|        82 | 7628 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7629 | `				}` |
|        92 | 7630 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7631 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7632 | `			}` |
|         - | 7633 | `			/* Point to the next entry */` |
|        92 | 7634 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7635 | `		}` |
|        21 | 7636 | `	}else{` |
|         - | 7637 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7638 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7639 | `		int nArrays = nArg - 1;` |
|         - | 7640 | `		ph7_hashmap_node **apCur;` |
|         - | 7641 | `		ph7_value **apCallArg;` |
|         - | 7642 | `		ph7_value sNull;` |
|        11 | 7643 | `		sxu32 nMax = 0;` |
|        11 | 7644 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7645 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7646 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7647 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7648 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7649 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7650 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7651 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7652 | `			return PH7_OK;` |
|         - | 7653 | `		}` |
|        11 | 7654 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7655 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7656 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7657 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7658 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7659 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7660 | `				nMax = pMap->nEntry;` |
|         6 | 7661 | `			}` |
|        12 | 7662 | `		}` |
|        35 | 7663 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7664 | `			ph7_value *pZip = 0;` |
|        25 | 7665 | `			if( bNullCallback ){` |
|         - | 7666 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7667 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7668 | `			}` |
|        79 | 7669 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7670 | `				ph7_value *pv = &sNull;` |
|        55 | 7671 | `				if( apCur[i] ){` |
|        53 | 7672 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7673 | `					if( pNodeVal ){` |
|        53 | 7674 | `						pv = pNodeVal;` |
|        26 | 7675 | `					}` |
|        53 | 7676 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7677 | `				}` |
|        55 | 7678 | `				if( bNullCallback ){` |
|         9 | 7679 | `					if( pZip ){` |
|         9 | 7680 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7681 | `					}` |
|         5 | 7682 | `				}else{` |
|        47 | 7683 | `					apCallArg[i] = pv;` |
|         - | 7684 | `				}` |
|        28 | 7685 | `			}` |
|        25 | 7686 | `			if( bNullCallback ){` |
|         5 | 7687 | `				if( pZip ){` |
|         5 | 7688 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7689 | `				}` |
|         3 | 7690 | `			}else{` |
|        21 | 7691 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7692 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7693 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7694 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7695 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7696 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7697 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7698 | `					return PH7_EXCEPTION;` |
|         - | 7699 | `				}` |
|        21 | 7700 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7701 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7702 | `			}` |
|        13 | 7703 | `		}` |
|        11 | 7704 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7705 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7706 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7707 | `	}` |
|        50 | 7708 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7709 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7710 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7711 | `	return PH7_OK;` |
|        36 | 7712 | `}` |
|         - | 7713 | `/*` |
|         - | 7714 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7715 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7716 | ` * Parameters` |
|         - | 7717 | ` *  $array` |
|         - | 7718 | ` *   The input array.` |
|         - | 7719 | ` *  $callback` |
|         - | 7720 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7721 | ` *  $initial` |
|         - | 7722 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7723 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7724 | ` * Return` |
|         - | 7725 | ` *  Returns the resulting value.` |
|         - | 7726 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7727 | ` */` |
|        34 | 7728 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7729 | `{` |
|         - | 7730 | `	ph7_hashmap_node *pEntry;` |
|         - | 7731 | `	ph7_hashmap *pMap;` |
|         - | 7732 | `	ph7_value *pValue;` |
|         - | 7733 | `	ph7_value sResult;` |
|         - | 7734 | `	sxi32 rc;` |
|         - | 7735 | `	sxu32 n;` |
|        39 | 7736 | `	if( nArg < 2 ){` |
|         8 | 7737 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7738 | `			"ArgumentCountError",` |
|         - | 7739 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7740 | `			nArg` |
|         - | 7741 | `			);` |
|         - | 7742 | `	}` |
|        35 | 7743 | `	if( nArg > 3 ){` |
|         4 | 7744 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7745 | `			"ArgumentCountError",` |
|         - | 7746 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7747 | `			nArg` |
|         - | 7748 | `			);` |
|         - | 7749 | `	}` |
|        33 | 7750 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7751 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7752 | `			"TypeError",` |
|         - | 7753 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7754 | `			ph7_type_name(apArg[0])` |
|         - | 7755 | `			);` |
|         - | 7756 | `	}` |
|        31 | 7757 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7758 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7759 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7760 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7761 | `				"TypeError",` |
|         - | 7762 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7763 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7764 | `				zFunc` |
|         - | 7765 | `				);` |
|         - | 7766 | `		}` |
|         9 | 7767 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7768 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7769 | `				"TypeError",` |
|         - | 7770 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7771 | `				"array callback must have exactly two members"` |
|         - | 7772 | `				);` |
|         - | 7773 | `		}` |
|         6 | 7774 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7775 | `			"TypeError",` |
|         - | 7776 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7777 | `			"no array or string given"` |
|         - | 7778 | `			);` |
|         - | 7779 | `	}` |
|         - | 7780 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7781 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7782 | `	/* Assume a NULL initial value */` |
|        19 | 7783 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7784 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7785 | `	if( nArg > 2 ){` |
|         - | 7786 | `		/* Set the initial value */` |
|        13 | 7787 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7788 | `	}` |
|         - | 7789 | `	/* Perform the requested operation */` |
|        19 | 7790 | `	pEntry = pMap->pFirst;` |
|        55 | 7791 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7792 | `		/* Extract the node value */` |
|        39 | 7793 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7794 | `		/* Invoke the supplied callback */` |
|        39 | 7795 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7796 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7797 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7798 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7799 | `			return PH7_EXCEPTION;` |
|         - | 7800 | `		}` |
|         - | 7801 | `		/* Point to the next entry */` |
|        37 | 7802 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7803 | `	}` |
|        17 | 7804 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7805 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7806 | `	return PH7_OK;` |
|        22 | 7807 | `}` |
|         - | 7808 | `/*` |
|         - | 7809 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7810 | ` *  Apply a user function to every member of an array.` |
|         - | 7811 | ` * Parameters` |
|         - | 7812 | ` *  $array` |
|         - | 7813 | ` *   The input array.` |
|         - | 7814 | ` *  $funcname` |
|         - | 7815 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7816 | ` *   the first, and the key/index second.` |
|         - | 7817 | ` * Note:` |
|         - | 7818 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7819 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7820 | ` *  be made in the original array itself.` |
|         - | 7821 | ` *  $userdata` |
|         - | 7822 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7823 | ` *   to the callback funcname.` |
|         - | 7824 | ` * Return` |
|         - | 7825 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7826 | ` */` |
|        38 | 7827 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7828 | `{` |
|         - | 7829 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7830 | `	ph7_hashmap_node *pEntry;` |
|         - | 7831 | `	ph7_hashmap *pMap;` |
|         - | 7832 | `	sxu32 n;` |
|        43 | 7833 | `	if( nArg < 2 ){` |
|         8 | 7834 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7835 | `			"ArgumentCountError",` |
|         - | 7836 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7837 | `			nArg` |
|         - | 7838 | `			);` |
|         - | 7839 | `	}` |
|        39 | 7840 | `	if( nArg > 3 ){` |
|         4 | 7841 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7842 | `			"ArgumentCountError",` |
|         - | 7843 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7844 | `			nArg` |
|         - | 7845 | `			);` |
|         - | 7846 | `	}` |
|        37 | 7847 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7848 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7849 | `			"TypeError",` |
|         - | 7850 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7851 | `			ph7_type_name(apArg[0])` |
|         - | 7852 | `			);` |
|         - | 7853 | `	}` |
|        35 | 7854 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7855 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7856 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7857 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7858 | `				"TypeError",` |
|         - | 7859 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7860 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7861 | `				zFunc` |
|         - | 7862 | `				);` |
|         - | 7863 | `		}` |
|        12 | 7864 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7865 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7866 | `				"TypeError",` |
|         - | 7867 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7868 | `				"array callback must have exactly two members"` |
|         - | 7869 | `				);` |
|         - | 7870 | `		}` |
|         6 | 7871 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7872 | `			"TypeError",` |
|         - | 7873 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7874 | `			"no array or string given"` |
|         - | 7875 | `			);` |
|         - | 7876 | `	}` |
|        21 | 7877 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7878 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7879 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7880 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7881 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7882 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7883 | `	/* Perform the desired operation */` |
|        21 | 7884 | `	pEntry = pMap->pFirst;` |
|        61 | 7885 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7886 | `		/* Extract the node value */` |
|        43 | 7887 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7888 | `		if( pValue ){` |
|         - | 7889 | `			sxi32 rcW;` |
|         - | 7890 | `			/* Extract the entry key */` |
|        43 | 7891 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7892 | `			/* Invoke the supplied callback */` |
|        43 | 7893 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7894 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7895 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7896 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7897 | `				return PH7_EXCEPTION;` |
|         - | 7898 | `			}` |
|        20 | 7899 | `		}` |
|         - | 7900 | `		/* Point to the next entry */` |
|        41 | 7901 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7902 | `	}` |
|         - | 7903 | `	/* All done, return TRUE */` |
|        19 | 7904 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7905 | `	return PH7_OK;` |
|        24 | 7906 | `}` |
|         - | 7907 | `/*` |
|         - | 7908 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7909 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7910 | ` */` |
|        22 | 7911 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7912 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7913 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7914 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7915 | `	int iNest             /* Nesting level */` |
|         - | 7916 | `	)` |
|         1 | 7917 | `{` |
|         - | 7918 | `	ph7_hashmap_node *pEntry;` |
|         - | 7919 | `	ph7_value *pValue,sKey;` |
|         - | 7920 | `	sxi32 rc;` |
|         - | 7921 | `	sxu32 n;` |
|         - | 7922 | `	/* Iterate through hashmap entries */` |
|        23 | 7923 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7924 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7925 | `	pEntry = pMap->pFirst;` |
|        59 | 7926 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7927 | `		/* Extract the node value */` |
|        37 | 7928 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7929 | `		if( pValue ){` |
|        37 | 7930 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7931 | `				if( iNest < 32 ){` |
|         - | 7932 | `					/* Recurse */` |
|        11 | 7933 | `					iNest++;` |
|        11 | 7934 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7935 | `					iNest--;` |
|        11 | 7936 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7937 | `						return PH7_EXCEPTION;` |
|         - | 7938 | `					}` |
|         5 | 7939 | `				}` |
|         6 | 7940 | `			}else{` |
|         - | 7941 | `				/* Extract the node key */` |
|        27 | 7942 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7943 | `				/* Invoke the supplied callback */` |
|        27 | 7944 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7945 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7946 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7947 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7948 | `					return PH7_EXCEPTION;` |
|         - | 7949 | `				}` |
|         - | 7950 | `			}` |
|        18 | 7951 | `		}` |
|         - | 7952 | `		/* Point to the next entry */` |
|        37 | 7953 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7954 | `	}` |
|        23 | 7955 | `	return PH7_OK;` |
|        12 | 7956 | `}` |
|         - | 7957 | `/*` |
|         - | 7958 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7959 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7960 | ` * Parameters` |
|         - | 7961 | ` *  $array` |
|         - | 7962 | ` *   The input array.` |
|         - | 7963 | ` *  $funcname` |
|         - | 7964 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7965 | ` *   the first, and the key/index second.` |
|         - | 7966 | ` * Note:` |
|         - | 7967 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7968 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7969 | ` *  be made in the original array itself.` |
|         - | 7970 | ` *  $userdata` |
|         - | 7971 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7972 | ` *   to the callback funcname.` |
|         - | 7973 | ` * Return` |
|         - | 7974 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7975 | ` */` |
|        30 | 7976 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7977 | `{` |
|         - | 7978 | `	ph7_hashmap *pMap;` |
|        35 | 7979 | `	if( nArg < 2 ){` |
|         8 | 7980 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7981 | `			"ArgumentCountError",` |
|         - | 7982 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7983 | `			nArg` |
|         - | 7984 | `			);` |
|         - | 7985 | `	}` |
|        31 | 7986 | `	if( nArg > 3 ){` |
|         4 | 7987 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7988 | `			"ArgumentCountError",` |
|         - | 7989 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 7990 | `			nArg` |
|         - | 7991 | `			);` |
|         - | 7992 | `	}` |
|        29 | 7993 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7994 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7995 | `			"TypeError",` |
|         - | 7996 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7997 | `			ph7_type_name(apArg[0])` |
|         - | 7998 | `			);` |
|         - | 7999 | `	}` |
|        27 | 8000 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8001 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8002 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8003 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8004 | `				"TypeError",` |
|         - | 8005 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8006 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8007 | `				zFunc` |
|         - | 8008 | `				);` |
|         - | 8009 | `		}` |
|        12 | 8010 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8011 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8012 | `				"TypeError",` |
|         - | 8013 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8014 | `				"array callback must have exactly two members"` |
|         - | 8015 | `				);` |
|         - | 8016 | `		}` |
|         6 | 8017 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8018 | `			"TypeError",` |
|         - | 8019 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8020 | `			"no array or string given"` |
|         - | 8021 | `			);` |
|         - | 8022 | `	}` |
|         - | 8023 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8024 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8025 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8026 | `	/* Perform the desired operation */` |
|        13 | 8027 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8028 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8029 | `		return PH7_EXCEPTION;` |
|         - | 8030 | `	}` |
|         - | 8031 | `	/* All done, return TRUE */` |
|        13 | 8032 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8033 | `	return PH7_OK;` |
|        20 | 8034 | `}` |
|         - | 8035 | `/*` |
|         - | 8036 | ` * bool array_is_list(array $array)` |
|         - | 8037 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8038 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8039 | ` * Return` |
|         - | 8040 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8041 | ` */` |
|         - | 8042 | `/*` |
|         - | 8043 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8044 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8045 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8046 | ` */` |
|       246 | 8047 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8048 | `{` |
|       247 | 8049 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       247 | 8050 | `	sxi64 iExpect = 0;` |
|         - | 8051 | `	sxu32 n;` |
|       555 | 8052 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       409 | 8053 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8054 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8055 | `			return 0;` |
|         - | 8056 | `		}` |
|       309 | 8057 | `		++iExpect;` |
|       309 | 8058 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       155 | 8059 | `	}` |
|       147 | 8060 | `	return 1;` |
|       124 | 8061 | `}` |
|        12 | 8062 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8063 | `{` |
|        13 | 8064 | `	if( nArg < 1 ){` |
|       ! 0 | 8065 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8066 | `			"ArgumentCountError",` |
|         - | 8067 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8068 | `			);` |
|         - | 8069 | `	}` |
|        13 | 8070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8071 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8072 | `			"TypeError",` |
|         - | 8073 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8074 | `			ph7_type_name(apArg[0])` |
|         - | 8075 | `			);` |
|         - | 8076 | `	}` |
|        13 | 8077 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8078 | `	return PH7_OK;` |
|         7 | 8079 | `}` |
|         - | 8080 | `/*` |
|         - | 8081 | ` * mixed array_first(array $array)` |
|         - | 8082 | ` * mixed array_last(array $array)` |
|         - | 8083 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8084 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8085 | ` *  untouched (unlike reset()/end()).` |
|         - | 8086 | ` */` |
|        20 | 8087 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8088 | `{` |
|         - | 8089 | `	ph7_hashmap *pMap;` |
|         - | 8090 | `	ph7_hashmap_node *pNode;` |
|         - | 8091 | `	ph7_value *pVal;` |
|        21 | 8092 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 8093 | `	if( nArg < 1 ){` |
|         4 | 8094 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8095 | `			"ArgumentCountError",` |
|         - | 8096 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8097 | `			zName` |
|         - | 8098 | `			);` |
|         - | 8099 | `	}` |
|        19 | 8100 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8101 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8102 | `			"TypeError",` |
|         - | 8103 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8104 | `			zName,` |
|         1 | 8105 | `			ph7_type_name(apArg[0])` |
|         - | 8106 | `			);` |
|         - | 8107 | `	}` |
|        17 | 8108 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8109 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8110 | `	if( pNode == 0 ){` |
|         - | 8111 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8112 | `		ph7_result_null(pCtx);` |
|         5 | 8113 | `		return PH7_OK;` |
|         - | 8114 | `	}` |
|        13 | 8115 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8116 | `	if( pVal ){` |
|        13 | 8117 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8118 | `	}else{` |
|       ! 0 | 8119 | `		ph7_result_null(pCtx);` |
|         - | 8120 | `	}` |
|        13 | 8121 | `	return PH7_OK;` |
|        11 | 8122 | `}` |
|        10 | 8123 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8124 | `{` |
|        11 | 8125 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8126 | `}` |
|        10 | 8127 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8128 | `{` |
|        11 | 8129 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8130 | `}` |
|         - | 8131 | `/*` |
|         - | 8132 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8133 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8134 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8135 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8136 | ` *  untouched.` |
|         - | 8137 | ` */` |
|        24 | 8138 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8139 | `{` |
|         - | 8140 | `	ph7_hashmap *pMap;` |
|         - | 8141 | `	ph7_hashmap_node *pNode;` |
|        25 | 8142 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        25 | 8143 | `	if( nArg < 1 ){` |
|         4 | 8144 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8145 | `			"ArgumentCountError",` |
|         - | 8146 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 8147 | `			zName` |
|         - | 8148 | `			);` |
|         - | 8149 | `	}` |
|        23 | 8150 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8151 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8152 | `			"TypeError",` |
|         - | 8153 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8154 | `			zName,` |
|         1 | 8155 | `			ph7_type_name(apArg[0])` |
|         - | 8156 | `			);` |
|         - | 8157 | `	}` |
|        21 | 8158 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8159 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8160 | `	if( pNode == 0 ){` |
|         - | 8161 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8162 | `		ph7_result_null(pCtx);` |
|         5 | 8163 | `		return PH7_OK;` |
|         - | 8164 | `	}` |
|        17 | 8165 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8166 | `	return PH7_OK;` |
|        13 | 8167 | `}` |
|        12 | 8168 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8169 | `{` |
|        13 | 8170 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8171 | `}` |
|        12 | 8172 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8173 | `{` |
|        13 | 8174 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8175 | `}` |
|         - | 8176 | `/*` |
|         - | 8177 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8178 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8179 | ` * array_column() for both the column value and the index key.` |
|         - | 8180 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8181 | ` * container or the key is absent.` |
|         - | 8182 | ` */` |
|        32 | 8183 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8184 | `{` |
|        33 | 8185 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8186 | `		ph7_hashmap_node *pNode;` |
|        25 | 8187 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8188 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8189 | `		}` |
|        11 | 8190 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8191 | `		ph7_value sName;` |
|         - | 8192 | `		const char *zName;` |
|         - | 8193 | `		ph7_value *pAttr;` |
|         - | 8194 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8195 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8196 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8197 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8198 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8199 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8200 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8201 | `		return pAttr;` |
|         - | 8202 | `	}` |
|         5 | 8203 | `	return 0;` |
|        17 | 8204 | `}` |
|         - | 8205 | `/*` |
|         - | 8206 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8207 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8208 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8209 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8210 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8211 | ` *  Each row may be an array or an object.` |
|         - | 8212 | ` */` |
|        12 | 8213 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8214 | `{` |
|         - | 8215 | `	ph7_hashmap_node *pNode;` |
|         - | 8216 | `	ph7_hashmap *pMap;` |
|         - | 8217 | `	ph7_value *pArray;` |
|         - | 8218 | `	ph7_value *pRow;` |
|         - | 8219 | `	ph7_value *pCol;` |
|         - | 8220 | `	ph7_value *pIdx;` |
|         - | 8221 | `	int bWantCol;` |
|         - | 8222 | `	int bWantIdx;` |
|         - | 8223 | `	sxu32 n;` |
|        13 | 8224 | `	if( nArg < 2 ){` |
|       ! 0 | 8225 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8226 | `			"ArgumentCountError",` |
|         - | 8227 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8228 | `			nArg` |
|         - | 8229 | `			);` |
|         - | 8230 | `	}` |
|        13 | 8231 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8232 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8233 | `			"TypeError",` |
|         - | 8234 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8235 | `			ph7_type_name(apArg[0])` |
|         - | 8236 | `			);` |
|         - | 8237 | `	}` |
|        13 | 8238 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8239 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8240 | `	if( pArray == 0 ){` |
|       ! 0 | 8241 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8242 | `		return PH7_OK;` |
|         - | 8243 | `	}` |
|         - | 8244 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8245 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8246 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8247 | `	pNode = pMap->pFirst;` |
|        33 | 8248 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8249 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8250 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8251 | `		if( pRow == 0 ){` |
|       ! 0 | 8252 | `			continue;` |
|         - | 8253 | `		}` |
|        21 | 8254 | `		if( bWantCol ){` |
|        19 | 8255 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8256 | `			if( pCol == 0 ){` |
|         - | 8257 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8258 | `				continue;` |
|         - | 8259 | `			}` |
|         9 | 8260 | `		}else{` |
|         3 | 8261 | `			pCol = pRow;` |
|         - | 8262 | `		}` |
|        19 | 8263 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8264 | `		if( pIdx ){` |
|        13 | 8265 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8266 | `		}else{` |
|         7 | 8267 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8268 | `		}` |
|        10 | 8269 | `	}` |
|        13 | 8270 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8271 | `	return PH7_OK;` |
|         7 | 8272 | `}` |
|         - | 8273 | `/*` |
|         - | 8274 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8275 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8276 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8277 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8278 | ` */` |
|        28 | 8279 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8280 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8281 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8282 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8283 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8284 | `	)` |
|         1 | 8285 | `{` |
|         - | 8286 | `	ph7_hashmap_node *pEntry;` |
|         - | 8287 | `	ph7_hashmap *pMap;` |
|         - | 8288 | `	ph7_value *pValue;` |
|         - | 8289 | `	ph7_value *apCbArg[2];` |
|         - | 8290 | `	ph7_value sKey;` |
|         - | 8291 | `	ph7_value sResult;` |
|         - | 8292 | `	sxi32 rc;` |
|         - | 8293 | `	sxu32 n;` |
|        29 | 8294 | `	*ppMatch = 0;` |
|        29 | 8295 | `	if( nArg < 2 ){` |
|       ! 0 | 8296 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8297 | `			"ArgumentCountError",` |
|         - | 8298 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8299 | `			zName,nArg` |
|         - | 8300 | `			);` |
|         - | 8301 | `	}` |
|        29 | 8302 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8303 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8304 | `			"TypeError",` |
|         - | 8305 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8306 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8307 | `			);` |
|         - | 8308 | `	}` |
|        29 | 8309 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8310 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8311 | `			"TypeError",` |
|         - | 8312 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8313 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8314 | `			);` |
|         - | 8315 | `	}` |
|        29 | 8316 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8317 | `	pEntry = pMap->pFirst;` |
|        29 | 8318 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8319 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8320 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8321 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8322 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8323 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8324 | `		if( pValue ){` |
|         - | 8325 | `			/* The callback receives ($value, $key). */` |
|        59 | 8326 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8327 | `			apCbArg[0] = pValue;` |
|        59 | 8328 | `			apCbArg[1] = &sKey;` |
|        59 | 8329 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8330 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8331 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8332 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8333 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8334 | `				return PH7_EXCEPTION;` |
|         - | 8335 | `			}` |
|        59 | 8336 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8337 | `				*ppMatch = pEntry;` |
|        15 | 8338 | `				break;` |
|         - | 8339 | `			}` |
|        22 | 8340 | `		}` |
|        45 | 8341 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8342 | `	}` |
|        29 | 8343 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8344 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8345 | `	return PH7_OK;` |
|        15 | 8346 | `}` |
|         - | 8347 | `/*` |
|         - | 8348 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8349 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8350 | ` *  is truthy, or NULL if none match.` |
|         - | 8351 | ` */` |
|         6 | 8352 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8353 | `{` |
|         - | 8354 | `	ph7_hashmap_node *pMatch;` |
|         - | 8355 | `	ph7_value *pVal;` |
|         - | 8356 | `	sxi32 rc;` |
|         7 | 8357 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8358 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8359 | `		return rc;` |
|         - | 8360 | `	}` |
|         7 | 8361 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8362 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8363 | `	}else{` |
|         3 | 8364 | `		ph7_result_null(pCtx);` |
|         - | 8365 | `	}` |
|         7 | 8366 | `	return PH7_OK;` |
|         4 | 8367 | `}` |
|         - | 8368 | `/*` |
|         - | 8369 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8370 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8371 | ` *  is truthy, or NULL if none match.` |
|         - | 8372 | ` */` |
|         6 | 8373 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8374 | `{` |
|         - | 8375 | `	ph7_hashmap_node *pMatch;` |
|         - | 8376 | `	sxi32 rc;` |
|         7 | 8377 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8378 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8379 | `		return rc;` |
|         - | 8380 | `	}` |
|         7 | 8381 | `	if( pMatch == 0 ){` |
|         3 | 8382 | `		ph7_result_null(pCtx);` |
|         6 | 8383 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8384 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8385 | `	}else{` |
|         4 | 8386 | `		ph7_result_string(pCtx,` |
|         2 | 8387 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8388 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8389 | `	}` |
|         7 | 8390 | `	return PH7_OK;` |
|         4 | 8391 | `}` |
|         - | 8392 | `/*` |
|         - | 8393 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8394 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8395 | ` *  FALSE for an empty array.` |
|         - | 8396 | ` */` |
|         8 | 8397 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8398 | `{` |
|         - | 8399 | `	ph7_hashmap_node *pMatch;` |
|         - | 8400 | `	sxi32 rc;` |
|         9 | 8401 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8402 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8403 | `		return rc;` |
|         - | 8404 | `	}` |
|         9 | 8405 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8406 | `	return PH7_OK;` |
|         5 | 8407 | `}` |
|         - | 8408 | `/*` |
|         - | 8409 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8410 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8411 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8412 | ` */` |
|         8 | 8413 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8414 | `{` |
|         - | 8415 | `	ph7_hashmap_node *pMatch;` |
|         - | 8416 | `	sxi32 rc;` |
|         9 | 8417 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8418 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8419 | `		return rc;` |
|         - | 8420 | `	}` |
|         9 | 8421 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8422 | `	return PH7_OK;` |
|         5 | 8423 | `}` |
|         - | 8424 | `/*` |
|         - | 8425 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8426 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8427 | ` */` |
|         - | 8428 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8429 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        78 | 8430 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8431 | `{` |
|        82 | 8432 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        39 | 8433 | `	(void)pVm;` |
|        82 | 8434 | `	p->nCount++;` |
|        82 | 8435 | `	if( p->pArray ){` |
|         - | 8436 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8437 | `		 * otherwise append with an auto-assigned int index. */` |
|        68 | 8438 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        32 | 8439 | `	}` |
|        82 | 8440 | `	return SXRET_OK;` |
|         4 | 8441 | `}` |
|         - | 8442 | `/*` |
|         - | 8443 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8444 | ` */` |
|        28 | 8445 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8446 | `{` |
|         - | 8447 | `	struct IterCollect sCol;` |
|         - | 8448 | `	ph7_value *pArray;` |
|         - | 8449 | `	sxi32 rc;` |
|        32 | 8450 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        32 | 8451 | `	pArray = ph7_context_new_array(pCtx);` |
|        32 | 8452 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        32 | 8453 | `	sCol.pArray = pArray;` |
|        32 | 8454 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        32 | 8455 | `	sCol.nCount = 0;` |
|        32 | 8456 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8457 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8458 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8459 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8460 | `		sxu32 n;` |
|         9 | 8461 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8462 | `			ph7_value sKey, *pVal;` |
|         7 | 8463 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8464 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8465 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8466 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8467 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8468 | `			pEntry = pEntry->pPrev;` |
|         4 | 8469 | `		}` |
|         3 | 8470 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8471 | `		return PH7_OK;` |
|         - | 8472 | `	}` |
|        30 | 8473 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        30 | 8474 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        28 | 8475 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8476 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8477 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8478 | `			ph7_type_name(apArg[0]));` |
|         - | 8479 | `	}` |
|        28 | 8480 | `	ph7_result_value(pCtx,pArray);` |
|        28 | 8481 | `	return PH7_OK;` |
|        18 | 8482 | `}` |
|         - | 8483 | `/*` |
|         - | 8484 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8485 | ` */` |
|         8 | 8486 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8487 | `{` |
|         - | 8488 | `	struct IterCollect sCol;` |
|         - | 8489 | `	sxi32 rc;` |
|         9 | 8490 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8491 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8492 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8493 | `		return PH7_OK;` |
|         - | 8494 | `	}` |
|         7 | 8495 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8496 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8497 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8498 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8499 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8500 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8501 | `			ph7_type_name(apArg[0]));` |
|         - | 8502 | `	}` |
|         7 | 8503 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8504 | `	return PH7_OK;` |
|         5 | 8505 | `}` |
|         - | 8506 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8507 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8508 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8509 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8510 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8511 | `{` |
|        33 | 8512 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8513 | `	ph7_value sResult;` |
|         - | 8514 | `	SySet aArg;` |
|         - | 8515 | `	sxi32 rc;` |
|         - | 8516 | `	int bContinue;` |
|        16 | 8517 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8518 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8519 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8520 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8521 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8522 | `		sxu32 n;` |
|        17 | 8523 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8524 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8525 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8526 | `			pEntry = pEntry->pPrev;` |
|         5 | 8527 | `		}` |
|         4 | 8528 | `	}` |
|        33 | 8529 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8530 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8531 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8532 | `	SySetRelease(&aArg);` |
|        33 | 8533 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8534 | `	p->nCount++;` |
|        31 | 8535 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8536 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8537 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8538 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8539 | `}` |
|         - | 8540 | `/*` |
|         - | 8541 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8542 | ` */` |
|        12 | 8543 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8544 | `{` |
|         - | 8545 | `	struct IterApply sApp;` |
|         - | 8546 | `	sxi32 rc;` |
|        13 | 8547 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8548 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8549 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8550 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8551 | `	}` |
|        13 | 8552 | `	sApp.pCallback = apArg[1];` |
|        13 | 8553 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8554 | `	sApp.nCount = 0;` |
|        13 | 8555 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8556 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8557 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8558 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8559 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8560 | `			ph7_type_name(apArg[0]));` |
|         - | 8561 | `	}` |
|        11 | 8562 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8563 | `	return PH7_OK;` |
|         7 | 8564 | `}` |
|         - | 8565 | `/*` |
|         - | 8566 | ` * Table of hashmap functions.` |
|         - | 8567 | ` */` |
|         - | 8568 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8569 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8570 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8571 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8572 | `	{"count",             ph7_hashmap_count },` |
|         - | 8573 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8574 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8575 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8576 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8577 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8578 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8579 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8580 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8581 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8582 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8583 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8584 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8585 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8586 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8587 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8588 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8589 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8590 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8591 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8592 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8593 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8594 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8595 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8596 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8597 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8598 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8599 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8600 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8601 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8602 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8603 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8604 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8605 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8606 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8607 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8608 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8609 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8610 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8611 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8612 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8613 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8614 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8615 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8616 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8617 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8618 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8619 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8620 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8621 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8622 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8623 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8624 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8625 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8626 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8627 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8628 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8629 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8630 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8631 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8632 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8633 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8634 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8635 | `	{"current",           ph7_hashmap_current },` |
|         - | 8636 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8637 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8638 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8639 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8640 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8641 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8642 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8643 | `};` |
|         - | 8644 | `/*` |
|         - | 8645 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8646 | ` */` |
|      3488 | 8647 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8648 | `{` |
|         - | 8649 | `	sxu32 n;` |
|    261605 | 8650 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    258117 | 8651 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    129061 | 8652 | `	}` |
|      3493 | 8653 | `}` |
|         - | 8654 | `/*` |
|         - | 8655 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8656 | ` * the BLOB given as the first argument.` |
|         - | 8657 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8658 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8659 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8660 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8661 | ` */` |
|         - | 8662 | `/*` |
|         - | 8663 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8664 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8665 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8666 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8667 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8668 | ` */` |
|       112 | 8669 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8670 | `{` |
|       115 | 8671 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8672 | `	ph7_value *pObj;` |
|       115 | 8673 | `	sxu32 n = 0;` |
|         - | 8674 | `	int isRef;` |
|       115 | 8675 | `	sxi32 rc = SXRET_OK;` |
|         - | 8676 | `	int i;` |
|       180 | 8677 | `	for(;;){` |
|       363 | 8678 | `		if( n >= pMap->nEntry ){` |
|       115 | 8679 | `			break;` |
|         - | 8680 | `		}` |
|       251 | 8681 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       251 | 8682 | `		isRef = (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0;` |
|       251 | 8683 | `		if( ShowType ){` |
|         - | 8684 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8685 | `			 * on the next line at the same indent (php). */` |
|       105 | 8686 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        71 | 8687 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        37 | 8688 | `			}` |
|        37 | 8689 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8690 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8691 | `			}else{` |
|        21 | 8692 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8693 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8694 | `			}` |
|        37 | 8695 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        37 | 8696 | `			if( pObj ){` |
|        37 | 8697 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        37 | 8698 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8699 | `					break;` |
|         - | 8700 | `				}` |
|        17 | 8701 | `			}` |
|        20 | 8702 | `		}else{` |
|         - | 8703 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8704 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8705 | `			 * php's extra blank line. References carry no marker. */` |
|      1184 | 8706 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|       970 | 8707 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       486 | 8708 | `			}` |
|       216 | 8709 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       103 | 8710 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        52 | 8711 | `			}else{` |
|       170 | 8712 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8713 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8714 | `			}` |
|       214 | 8715 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       121 | 8716 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8717 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8718 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8719 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8720 | `					break;` |
|         - | 8721 | `				}` |
|        13 | 8722 | `			}else{` |
|       192 | 8723 | `				if( pObj ){` |
|       192 | 8724 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        95 | 8725 | `				}` |
|       192 | 8726 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8727 | `			}` |
|         - | 8728 | `		}` |
|         - | 8729 | `		/* Point to the next entry */` |
|       251 | 8730 | `		n++;` |
|       251 | 8731 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8732 | `	}` |
|       115 | 8733 | `	return rc;` |
|         3 | 8734 | `}` |
|       108 | 8735 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8736 | `{` |
|         - | 8737 | `	sxi32 rc;` |
|         - | 8738 | `	int i;` |
|       110 | 8739 | `	if( nDepth > 31 ){` |
|         - | 8740 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8741 | `		/* Nesting limit reached */` |
|       ! 0 | 8742 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8743 | `		return SXERR_LIMIT;` |
|         - | 8744 | `	}` |
|       110 | 8745 | `	if( ShowType ){` |
|         - | 8746 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8747 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8748 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8749 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8750 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8751 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8752 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8753 | `		}` |
|        14 | 8754 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8755 | `		return rc;` |
|         - | 8756 | `	}` |
|         - | 8757 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|        97 | 8758 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       289 | 8759 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8760 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8761 | `	}` |
|        97 | 8762 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|        97 | 8763 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       289 | 8764 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8765 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8766 | `	}` |
|        97 | 8767 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        97 | 8768 | `	return rc;` |
|        56 | 8769 | `}` |
|         - | 8770 | `/*` |
|         - | 8771 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8772 | ` * retrieved entry.` |
|         - | 8773 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8774 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8775 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8776 | ` * a value different from PH7_OK.` |
|         - | 8777 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8778 | ` */` |
|     34078 | 8779 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8780 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8781 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8782 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8783 | `	)` |
|         5 | 8784 | `{` |
|         - | 8785 | `	ph7_hashmap_node *pEntry;` |
|         - | 8786 | `	ph7_value sKey,sValue;` |
|         - | 8787 | `	sxi32 rc;` |
|         - | 8788 | `	sxu32 n;` |
|         - | 8789 | `	/* Initialize walker parameter */` |
|     34083 | 8790 | `	rc = SXRET_OK;` |
|     34083 | 8791 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34083 | 8792 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34083 | 8793 | `	n = pMap->nEntry;` |
|     34083 | 8794 | `	pEntry = pMap->pFirst;` |
|         - | 8795 | `	/* Start the iteration process */` |
|     91670 | 8796 | `	for(;;){` |
|    183345 | 8797 | `		if( n < 1 ){` |
|     34083 | 8798 | `			break;` |
|         - | 8799 | `		}` |
|         - | 8800 | `		/* Extract a copy of the key and a copy the current value */` |
|    149267 | 8801 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    149267 | 8802 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8803 | `		/* Invoke the user callback */` |
|    149267 | 8804 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8805 | `		/* Release the copy of the key and the value */` |
|    149267 | 8806 | `		PH7_MemObjRelease(&sKey);` |
|    149267 | 8807 | `		PH7_MemObjRelease(&sValue);` |
|    149267 | 8808 | `		if( rc != PH7_OK ){` |
|         - | 8809 | `			/* Callback request an operation abort */` |
|       ! 0 | 8810 | `			return SXERR_ABORT;` |
|         - | 8811 | `		}` |
|         - | 8812 | `		/* Point to the next entry */` |
|    149267 | 8813 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    149267 | 8814 | `		n--;` |
|         5 | 8815 | `	}` |
|         - | 8816 | `	/* All done */` |
|     34083 | 8817 | `	return SXRET_OK;` |
|     17044 | 8818 | `}` |
|         - | 8819 |  |
