# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3942/4451 lines (88.56%)

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
|   7458272 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7458277 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7458277 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    628866 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    628871 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    628871 |   35 | `	sxu32 nH = 5381;` |
|    628871 |   36 | `	zEnd = &zIn[nLen];` |
|    711650 |   37 | `	for(;;){` |
|   1423305 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1220033 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1092921 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    951413 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    628871 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1940 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1945 |   54 | `	sxi64 iCount = 0;` |
|      1945 |   55 | `	if( !bRecursive ){` |
|      1771 |   56 | `		iCount = pMap->nEntry;` |
|       888 |   57 | `	}else{` |
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
|      1945 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3157094 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3157099 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3157099 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3157099 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3157099 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3157099 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3157099 |  112 | `	pNode->nHash = nHash;` |
|   3157099 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3157099 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3157099 |  115 | `	return pNode;` |
|   1578552 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    260486 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    260491 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    260491 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    260491 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    260491 |  133 | `	pNode->pMap  = &(*pMap);` |
|    260491 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    260491 |  135 | `	pNode->nHash = nHash;` |
|    260491 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    260491 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    260491 |  138 | `	pNode->nValIdx = nValIdx;` |
|    260491 |  139 | `	return pNode;` |
|    130248 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3417580 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3417585 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2949107 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2949107 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1474551 |  150 | `	}` |
|   3417585 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3417585 |  153 | `	if( pMap->pFirst == 0 ){` |
|     88643 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     88643 |  156 | `		pMap->pCur = pNode;` |
|     44324 |  157 | `	}else{` |
|   3328947 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3417585 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3417585 |  174 | `	++pMap->nEntry;` |
|   3417585 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7914 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7919 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7919 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7919 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7449 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3727 |  187 | `	}else{` |
|       472 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7919 |  190 | `	if( pNode->pNextCollide ){` |
|      5279 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2638 |  192 | `	}` |
|      7919 |  193 | `	if( pMap->pFirst == pNode ){` |
|       171 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        83 |  195 | `	}` |
|      7919 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       203 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        99 |  199 | `	}` |
|      7919 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7919 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7919 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       209 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       209 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       209 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       102 |  218 | `		}` |
|       102 |  219 | `	}` |
|      7919 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7681 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3838 |  222 | `	}` |
|      7919 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7919 |  224 | `	pMap->nEntry--;` |
|      7919 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|        99 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        99 |  228 | `		pMap->apBucket = 0;` |
|        99 |  229 | `		pMap->nSize = 0;` |
|        99 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        47 |  231 | `	}` |
|      7919 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3417580 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3417585 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     93769 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     93769 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     93769 |  245 | `		if( nNew < 1 ){` |
|     88643 |  246 | `			nNew = 16;` |
|     44319 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     93769 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     93769 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     93769 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     93769 |  260 | `		pMap->apBucket = apNew;` |
|     93769 |  261 | `		pMap->nSize = nNew;` |
|     93769 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     88643 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5131 |  267 | `		pEntry = pMap->pFirst;` |
|      5131 |  268 | `		n = 0;` |
|   2112643 |  269 | `		for( ;; ){` |
|   4225291 |  270 | `			if( n >= pMap->nEntry ){` |
|      5131 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4220165 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4220165 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4220165 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3597903 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3597903 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1798949 |  280 | `			}` |
|   4220165 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4220165 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4220165 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5131 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2563 |  288 | `	}` |
|   3328947 |  289 | `	return SXRET_OK;` |
|   1708795 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3157094 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3157099 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3157057 |  310 | `		if( pValue ){` |
|   3157051 |  311 | `			sSafeVal = *pValue;` |
|   3157051 |  312 | `			pValue = &sSafeVal;` |
|   1578523 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3157057 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3157057 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3157057 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3157051 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1578523 |  322 | `		}` |
|   3157057 |  323 | `		nIdx = pObj->nIdx;` |
|   1578531 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3157099 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3157099 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3157099 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3157099 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3157099 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3157099 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3157099 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3157099 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3157099 |  349 | `	return SXRET_OK;` |
|   1578552 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    260486 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    260491 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    214743 |  370 | `		if( pValue ){` |
|    214433 |  371 | `			sSafeVal = *pValue;` |
|    214433 |  372 | `			pValue = &sSafeVal;` |
|    107214 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    214743 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    214743 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    214743 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    214433 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    107214 |  382 | `		}` |
|    214743 |  383 | `		nIdx = pObj->nIdx;` |
|    107374 |  384 | `	}else{` |
|     45753 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    260491 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    260491 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    260491 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    260491 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     45753 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     22874 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    260491 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    260491 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    260491 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    260491 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    260491 |  409 | `	return SXRET_OK;` |
|    130248 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4287898 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4287903 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       725 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4287183 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4287183 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110564125 |  433 | `	for(;;){` |
| 221128255 |  434 | `		if( pNode == 0 ){` |
|   4282211 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216846044 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216843032 |  438 | `			&& pNode->nHash == nHash` |
| 108422501 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      4977 |  441 | `				if( ppNode ){` |
|      4955 |  442 | `					*ppNode = pNode;` |
|      2475 |  443 | `				}` |
|      4977 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841073 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4282211 |  450 | `	return SXERR_NOTFOUND;` |
|   2143954 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    402682 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    402687 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     34307 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    368385 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    368385 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    304184 |  475 | `	for(;;){` |
|    608373 |  476 | `		if( pNode == 0 ){` |
|    304101 |  477 | `			break;` |
|         - |  478 | `		}` |
|    304272 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    302761 |  480 | `			&& pNode->nHash == nHash` |
|    182817 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     64389 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     64289 |  484 | `				if( ppNode ){` |
|     64261 |  485 | `					*ppNode = pNode;` |
|     32128 |  486 | `				}` |
|     64289 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    239993 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    304101 |  493 | `	return SXERR_NOTFOUND;` |
|    201346 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    402814 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    402819 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    402819 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    402819 |  504 | `	int isNeg = FALSE, nDigit;` |
|    402819 |  505 | `	if( zIn >= zEnd ){` |
|        23 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    402797 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    402793 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    402793 |  516 | `	zDigit = zIn;` |
|    201828 |  517 | `	for(;;){` |
|    403661 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    403411 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    402543 |  523 | `			return FALSE;` |
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
|    201412 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    147182 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    147187 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    147187 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    142327 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  559 | `		}` |
|    142327 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    142313 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    142313 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|         7 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      4879 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        27 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        13 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      4879 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     73591 |  575 | `result:` |
|    147187 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     68403 |  578 | `		if( ppNode ){` |
|     68353 |  579 | `			*ppNode = pNode;` |
|     34174 |  580 | `		}` |
|     68403 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     78789 |  584 | `	return SXERR_NOTFOUND;` |
|     73596 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  588 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  589 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  590 | ` * via HashmapAppendIndexBusy.` |
|         - |  591 | ` */` |
|   2141468 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2141473 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       767 |  596 | `		pMap->bIntKeySeen = 1;` |
|       767 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       767 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|       767 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2140711 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2140465 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2140465 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1070230 |  609 | `	}` |
|   1070739 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   1015252 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1015257 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1015251 |  623 | `	return FALSE;` |
|    507631 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3371364 |  631 | `static sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3371369 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3371369 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3371369 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    214737 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         8 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|    214737 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       229 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    321761 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    107252 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  654 | `				/* Overwrite the old value */` |
|         - |  655 | `				ph7_value *pElem;` |
|       482 |  656 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       482 |  657 | `				if( pElem ){` |
|       482 |  658 | `					if( pVal ){` |
|       482 |  659 | `						PH7_MemObjStore(pVal,pElem);` |
|       243 |  660 | `					}else{` |
|         - |  661 | `						/* Nullify the entry */` |
|       ! 0 |  662 | `						PH7_MemObjToNull(pElem);` |
|         - |  663 | `					}` |
|       239 |  664 | `				}` |
|       482 |  665 | `				return SXRET_OK;` |
|         - |  666 | `		}` |
|    214031 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
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
|    213901 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    213901 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1578316 |  683 | `IntKey:` |
|   3156865 |  684 | `	if( pKey ){` |
|   2141647 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       259 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       129 |  688 | `		}` |
|   2141647 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  690 | `			/* Overwrite the old value */` |
|         - |  691 | `			ph7_value *pElem;` |
|       179 |  692 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       179 |  693 | `			if( pElem ){` |
|       179 |  694 | `				if( pVal ){` |
|       179 |  695 | `					PH7_MemObjStore(pVal,pElem);` |
|        90 |  696 | `				}else{` |
|         - |  697 | `					/* Nullify the entry */` |
|       ! 0 |  698 | `					PH7_MemObjToNull(pElem);` |
|         - |  699 | `				}` |
|        89 |  700 | `			}` |
|       179 |  701 | `			return SXRET_OK;` |
|         - |  702 | `		}` |
|   2141469 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2141467 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2141467 |  712 | `		if( rc == SXRET_OK ){` |
|   2141467 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070731 |  714 | `		}` |
|   1070736 |  715 | `	}else{` |
|   1015223 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1015221 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1015215 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1015215 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1015213 |  726 | `			++pMap->iNextIdx;` |
|    507604 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3156677 |  730 | `	return rc;` |
|   1685687 |  731 | `}` |
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
|     45800 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     45805 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     45805 |  766 | `	sxi32 rc = SXRET_OK;` |
|     45805 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     45765 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     45765 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     68642 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     22879 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     45753 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     45753 |  790 | `		return rc;` |
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
|     22905 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1445337 |  827 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1445342 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1445342 |  832 | `	return pObj;` |
|         5 |  833 | `}` |
|         - |  834 | `/*` |
|         - |  835 | ` * Insert a node in the given hashmap.` |
|         - |  836 | ` * If a node with the given key already exists in the database` |
|         - |  837 | ` * then this function overwrite the old value.` |
|         - |  838 | ` */` |
|       460 |  839 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  840 | `{` |
|         - |  841 | `	ph7_value *pObj;` |
|         - |  842 | `	sxi32 rc;` |
|         - |  843 | `	/* Extract the node value */` |
|       465 |  844 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       465 |  845 | `	if( pObj == 0 ){` |
|       ! 0 |  846 | `		return SXERR_EMPTY;` |
|         - |  847 | `	}` |
|       460 |  848 | `	if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|       464 |  849 | `	 \|\| PH7_VmSlotIsReferenced(pMap->pVm,pNode->nValIdx) ){` |
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
|       463 |  873 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  874 | `		/* Int64 key */` |
|       331 |  875 | `		if( !bPreserve ){` |
|         - |  876 | `			/* Assign an automatic index */` |
|       183 |  877 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        94 |  878 | `		}else{` |
|       149 |  879 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  880 | `		}` |
|       168 |  881 | `	}else{` |
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
|       463 |  892 | `	return rc;` |
|       235 |  893 | `}` |
|         - |  894 | `/*` |
|         - |  895 | ` * Compare two node values.` |
|         - |  896 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  897 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  898 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  899 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  900 | ` * documenation.` |
|         - |  901 | ` */` |
|     71804 |  902 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     71809 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     71809 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71809 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71809 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71809 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71809 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71809 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     71809 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     71809 |  921 | `	return rc;` |
|     35869 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     14000 |  927 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     14005 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     14005 |  932 | `	if( pEntry->pPrevCollide ){` |
|     11387 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5703 |  934 | `	}else{` |
|      2623 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     14005 |  937 | `	if( pEntry->pNextCollide ){` |
|      1147 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       583 |  939 | `	}` |
|     14005 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     14005 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     14005 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     14005 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     14005 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14005 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     11717 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5868 |  949 | `	}` |
|     14005 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     14005 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     14005 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     14005 |  956 | `		pMap->iNextIdx++;` |
|      7000 |  957 | `	}` |
|     14005 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     32996 |  966 | `static int HashmapFindValue(` |
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
|     33001 |  979 | `	pEntry = pMap->pFirst;` |
|     33001 |  980 | `	n = pMap->nEntry;` |
|     33001 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33001 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     78656 |  983 | `	for(;;){` |
|    157320 |  984 | `		if( n < 1 ){` |
|       115 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    157206 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    157206 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    157206 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    157206 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    157206 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    157206 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    157206 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    157206 | 1001 | `			if( rc == 0 ){` |
|     32887 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     32887 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     62158 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    124324 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    124324 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|       115 | 1014 | `	return SXERR_NOTFOUND;` |
|     16503 | 1015 | `}` |
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
|    664170 | 1201 | `static sxi32 HashmapDuplicateNode(` |
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
|    664170 | 1212 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    664172 | 1213 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
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
|    664167 | 1238 | `	sSafeVal = *pVal;` |
|         - | 1239 |  |
|    664167 | 1240 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1241 | `		/* Blob key insertion */` |
|      3965 | 1242 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      3965 | 1243 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      3965 | 1244 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      3965 | 1245 | `		PH7_MemObjRelease(&sKey);` |
|      1985 | 1246 | `	}else{` |
|         - | 1247 | `		/* Int key */` |
|    660207 | 1248 | `		if( iAction == 0 ){ /* Merge */` |
|    659961 | 1249 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    330227 | 1250 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1251 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1252 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1253 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1254 | `		}else{ /* Dup */` |
|       219 | 1255 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1256 | `		}` |
|         - | 1257 | `	}` |
|    664167 | 1258 | `	return rc;` |
|    332090 | 1259 | `}` |
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
|      2780 | 1272 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1273 | `{` |
|         - | 1274 | `	ph7_hashmap_node *pEntry;` |
|         - | 1275 | `	ph7_value *pVal;` |
|         - | 1276 | `	sxi32 rc;` |
|         - | 1277 | `	sxu32 n;` |
|      2785 | 1278 | `	if( pSrc == pDest ){` |
|         - | 1279 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1280 | `		 * Unlike the zend engine.` |
|         - | 1281 | `		 */` |
|       ! 0 | 1282 | `		return SXRET_OK;` |
|         - | 1283 | `	}` |
|         - | 1284 | `	/* Point to the first inserted entry in the source */` |
|      2785 | 1285 | `	pEntry = pSrc->pFirst;` |
|         - | 1286 | `	/* Perform the merge */` |
|    662801 | 1287 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1288 | `		/* Extract the node value */` |
|    660021 | 1289 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    660021 | 1290 | `		if( pVal ){` |
|         - | 1291 | `			/* Make a local copy of the value.` |
|         - | 1292 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1293 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1294 | `			 * to the old pool.` |
|         - | 1295 | `			 */` |
|    660021 | 1296 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    330013 | 1297 | `		}else{` |
|       ! 0 | 1298 | `			rc = SXRET_OK;` |
|         - | 1299 | `		}` |
|    660021 | 1300 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1301 | `			return rc;` |
|         - | 1302 | `		}` |
|         - | 1303 | `		/* Point to the next entry */` |
|    660021 | 1304 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    330013 | 1305 | `	}` |
|      2785 | 1306 | `	return SXRET_OK;` |
|      1395 | 1307 | `}` |
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
|      3868 | 1357 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1358 | `{` |
|         - | 1359 | `	ph7_hashmap_node *pEntry;` |
|         - | 1360 | `	ph7_value *pVal;` |
|         - | 1361 | `	sxi32 rc;` |
|         - | 1362 | `	sxu32 n;` |
|      3873 | 1363 | `	if( pSrc == pDest ){` |
|         - | 1364 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1365 | `		 * Unlike the zend engine.` |
|         - | 1366 | `		 */` |
|       ! 0 | 1367 | `		return SXRET_OK;` |
|         - | 1368 | `	}` |
|         - | 1369 | `	/* Point to the first inserted entry in the source */` |
|      3873 | 1370 | `	pEntry = pSrc->pFirst;` |
|         - | 1371 | `	/* Perform the duplication */` |
|      7983 | 1372 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1373 | `		/* Extract the node value */` |
|      4115 | 1374 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4115 | 1375 | `		if( pVal ){` |
|      4115 | 1376 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2060 | 1377 | `		}else{` |
|       ! 0 | 1378 | `			rc = SXRET_OK;` |
|         - | 1379 | `		}` |
|      4115 | 1380 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1381 | `			return rc;` |
|         - | 1382 | `		}` |
|         - | 1383 | `		/* Point to the next entry */` |
|      4115 | 1384 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2060 | 1385 | `	}` |
|      3873 | 1386 | `	return SXRET_OK;` |
|      1939 | 1387 | `}` |
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
|    233318 | 1463 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1464 | `{` |
|    233323 | 1465 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1466 | `	ph7_hashmap *pNew;` |
|         - | 1467 | `	ph7_value *pBacking;` |
|         - | 1468 | `	sxu32 nValIdx;` |
|         - | 1469 | `	int bValueInPool;` |
|    233323 | 1470 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    233323 | 1471 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1472 | `		/* Sole owner, no separation needed */` |
|    230641 | 1473 | `		return pMap;` |
|         - | 1474 | `	}` |
|      2687 | 1475 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1476 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1477 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1478 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1479 | `		return pMap;` |
|         - | 1480 | `	}` |
|         - | 1481 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1482 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1483 | `	 * frame is popped. */` |
|      2561 | 1484 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2561 | 1485 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2556 | 1486 | `		if( pBacking && pBacking != pValue` |
|      2532 | 1487 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2513 | 1488 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1489 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2513 | 1490 | `			pMap->iRef--;` |
|      2513 | 1491 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1492 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2467 | 1493 | `				pMap->iRef++;` |
|      2467 | 1494 | `				return pMap;` |
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
|        24 | 1525 | `	}` |
|         - | 1526 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1527 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1528 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1529 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1530 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1531 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1532 | `	 * pBacking in the backing-variable branch above). */` |
|        50 | 1533 | `	nValIdx = pValue->nIdx;` |
|        74 | 1534 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        48 | 1535 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        50 | 1536 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        50 | 1537 | `	if( pNew == 0 ){` |
|         - | 1538 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1539 | `		return pMap;` |
|         - | 1540 | `	}` |
|        50 | 1541 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1542 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1543 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1544 | `		return pMap;` |
|         - | 1545 | `	}` |
|        50 | 1546 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        50 | 1547 | `	pMap->iRef--;` |
|        50 | 1548 | `	if( bValueInPool ){` |
|         - | 1549 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        50 | 1550 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        50 | 1551 | `		if( pValue == 0 ){` |
|       ! 0 | 1552 | `			return pNew;` |
|         - | 1553 | `		}` |
|        24 | 1554 | `	}` |
|        50 | 1555 | `	pValue->x.pOther = pNew;` |
|        50 | 1556 | `	return pNew;` |
|    116664 | 1557 | `}` |
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
|      3746 | 1595 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1596 | `{` |
|         - | 1597 | `	ph7_hashmap_node *pEntry;` |
|      3751 | 1598 | `	sxi32 rc = SXRET_OK;` |
|         - | 1599 | `	ph7_value *pObj;` |
|         - | 1600 | `	sxu32 n;` |
|      3751 | 1601 | `	if( pLeft == pRight ){` |
|         - | 1602 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1603 | `		 * Unlike the zend engine.` |
|         - | 1604 | `		 */` |
|       ! 0 | 1605 | `		return SXRET_OK;` |
|         - | 1606 | `	}` |
|         - | 1607 | `	/* Perform the union */` |
|      3751 | 1608 | `	pEntry = pRight->pFirst;` |
|      3791 | 1609 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
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
|      3751 | 1643 | `	return SXRET_OK;` |
|      1878 | 1644 | `}` |
|         - | 1645 | `/*` |
|         - | 1646 | ` * Allocate a new hashmap.` |
|         - | 1647 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1648 | ` */` |
|    141940 | 1649 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1650 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1651 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1652 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1653 | `	)` |
|         5 | 1654 | `{` |
|         - | 1655 | `	ph7_hashmap *pMap;` |
|         - | 1656 | `	/* Allocate a new instance */` |
|    141945 | 1657 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    141945 | 1658 | `	if( pMap == 0 ){` |
|       ! 0 | 1659 | `		return 0;` |
|         - | 1660 | `	}` |
|         - | 1661 | `	/* Zero the structure */` |
|    141945 | 1662 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1663 | `	/* Fill in the structure */` |
|    141945 | 1664 | `	pMap->pVm = &(*pVm);` |
|    141945 | 1665 | `	pMap->iRef = 1;` |
|         - | 1666 | `	/* Default hash functions */` |
|    141945 | 1667 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    141945 | 1668 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    141945 | 1669 | `	return pMap;` |
|     70975 | 1670 | `}` |
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
|      3346 | 1691 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|      3351 | 1711 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3351 | 1712 | `	if( pMap == 0 ){` |
|       ! 0 | 1713 | `		return SXERR_MEM;` |
|         - | 1714 | `	}` |
|      3351 | 1715 | `	pVm->pGlobal = pMap;` |
|         - | 1716 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3351 | 1717 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3351 | 1718 | `	if( pObj == 0 ){` |
|       ! 0 | 1719 | `		return SXERR_MEM;` |
|         - | 1720 | `	}` |
|      3351 | 1721 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1722 | `	/* Record object index */` |
|      3351 | 1723 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1724 | `	/* Install the special $GLOBALS array */` |
|      3351 | 1725 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3351 | 1726 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1727 | `		return rc;` |
|         - | 1728 | `	}` |
|         - | 1729 | `	/* Install superglobals now */` |
|     36811 | 1730 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1731 | `		ph7_value *pSuper;` |
|         - | 1732 | `		/* Request an empty array */` |
|     33465 | 1733 | `		pSuper = ph7_new_array(&(*pVm));` |
|     33465 | 1734 | `		if( pSuper == 0 ){` |
|       ! 0 | 1735 | `			return SXERR_MEM;` |
|         - | 1736 | `		}` |
|         - | 1737 | `		/* Install */` |
|     33465 | 1738 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     33465 | 1739 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1740 | `			return rc;` |
|         - | 1741 | `		}` |
|         - | 1742 | `		/* Release the value now it have been installed */` |
|     33465 | 1743 | `		ph7_release_value(&(*pVm),pSuper);` |
|     16735 | 1744 | `	}` |
|         - | 1745 | `	/* Set some $_SERVER entries */` |
|      3351 | 1746 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1747 | `	/*` |
|         - | 1748 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1749 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1750 | `	 */` |
|      6697 | 1751 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1752 | `		"SCRIPT_FILENAME",` |
|      1673 | 1753 | `		pFile ? pFile->zString : ":Memory:",` |
|      3346 | 1754 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1755 | `		);` |
|         - | 1756 | `	/* All done,all super-global are installed now */` |
|      3351 | 1757 | `	return SXRET_OK;` |
|      1678 | 1758 | `}` |
|         - | 1759 | `/*` |
|         - | 1760 | ` * Release a hashmap.` |
|         - | 1761 | ` */` |
|    100338 | 1762 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1763 | `{` |
|         - | 1764 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    100343 | 1765 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1766 | `	sxu32 n;` |
|    100343 | 1767 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1768 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1769 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1770 | `		return SXRET_OK;` |
|         - | 1771 | `	}` |
|    100343 | 1772 | `	if( pMap->pActiveSteps ){` |
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
|    100343 | 1785 | `	n = 0;` |
|    100343 | 1786 | `	pEntry = pMap->pFirst;` |
|   1716886 | 1787 | `	for(;;){` |
|   3433777 | 1788 | `		if( n >= pMap->nEntry ){` |
|    100343 | 1789 | `			break;` |
|         - | 1790 | `		}` |
|   3333439 | 1791 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1792 | `		/* Remove the reference from the foreign table */` |
|   3333439 | 1793 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3333439 | 1794 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1795 | `			/* Restore the ph7_value to the free list */` |
|   3333379 | 1796 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1666687 | 1797 | `		}` |
|         - | 1798 | `		/* Release the node */` |
|   3333439 | 1799 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    187513 | 1800 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     93754 | 1801 | `		}` |
|   3333439 | 1802 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1803 | `		/* Point to the next entry */` |
|   3333439 | 1804 | `		pEntry = pNext;` |
|   3333439 | 1805 | `		n++;` |
|         5 | 1806 | `	}` |
|    100343 | 1807 | `	if( pMap->nEntry > 0 ){` |
|         - | 1808 | `		/* Release the hash bucket */` |
|     73819 | 1809 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     36907 | 1810 | `	}` |
|    100343 | 1811 | `	if( FreeDS ){` |
|         - | 1812 | `		/* Free the whole instance */` |
|    100317 | 1813 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     50161 | 1814 | `	}else{` |
|         - | 1815 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1816 | `		pMap->apBucket = 0;` |
|        28 | 1817 | `		pMap->iNextIdx = 0;` |
|        28 | 1818 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1819 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1820 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1821 | `	}` |
|    100343 | 1822 | `	return SXRET_OK;` |
|     50174 | 1823 | `}` |
|         - | 1824 | `/*` |
|         - | 1825 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1826 | ` * If the count reaches zero which mean no more variables` |
|         - | 1827 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1828 | ` */` |
|    831008 | 1829 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1830 | `{` |
|    831013 | 1831 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1832 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    831013 | 1833 | `	pMap->iRef--;` |
|    831013 | 1834 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    100297 | 1835 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     50146 | 1836 | `	}` |
|    831013 | 1837 | `}` |
|         - | 1838 | `/*` |
|         - | 1839 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1840 | ` * Write a pointer to the target node on success.` |
|         - | 1841 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1842 | ` */` |
|    147354 | 1843 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1844 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1845 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1846 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1847 | `	)` |
|         5 | 1848 | `{` |
|         - | 1849 | `	sxi32 rc;` |
|    147359 | 1850 | `	if( pMap->nEntry < 1 ){` |
|         - | 1851 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1852 | `		 */` |
|       177 | 1853 | `		return SXERR_NOTFOUND;` |
|         - | 1854 | `	}` |
|    147187 | 1855 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    147187 | 1856 | `	return rc;` |
|     73682 | 1857 | `}` |
|         - | 1858 | `/*` |
|         - | 1859 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1860 | ` * hashmap.` |
|         - | 1861 | ` * If a node with the given key already exists in the database` |
|         - | 1862 | ` * then this function overwrite the old value.` |
|         - | 1863 | ` */` |
|   2711158 | 1864 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2711163 | 1875 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2711163 | 1876 | `	return rc;` |
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
|     45790 | 1915 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1916 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1917 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1918 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1919 | `	)` |
|         5 | 1920 | `{` |
|         - | 1921 | `	sxi32 rc;` |
|     45795 | 1922 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1923 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1924 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1925 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1926 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1927 | `		return PH7_ABORT;` |
|         - | 1928 | `	}` |
|     45795 | 1929 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     45795 | 1930 | `	return rc;` |
|     22900 | 1931 | `}` |
|         - | 1932 | `/*` |
|         - | 1933 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1934 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1935 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1936 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1937 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1938 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1939 | ` */` |
|     18870 | 1940 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1941 | `{` |
|     18875 | 1942 | `	pStep->pCursor = pMap->pFirst;` |
|     18875 | 1943 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     18875 | 1944 | `	pMap->pActiveSteps = pStep;` |
|     18875 | 1945 | `}` |
|         - | 1946 | `/*` |
|         - | 1947 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1948 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1949 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1950 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1951 | ` */` |
|     18770 | 1952 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1953 | `{` |
|     18775 | 1954 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18775 | 1955 | `	while( *ppLink ){` |
|     18775 | 1956 | `		if( *ppLink == pStep ){` |
|     18775 | 1957 | `			*ppLink = pStep->pNextActive;` |
|     18775 | 1958 | `			pStep->pNextActive = 0;` |
|     18775 | 1959 | `			return;` |
|         - | 1960 | `		}` |
|       ! 0 | 1961 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1962 | `	}` |
|      9390 | 1963 | `}` |
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
|    589696 | 1984 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1985 | `{` |
|    589701 | 1986 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    589701 | 1987 | `	if( pEntry ){` |
|    589701 | 1988 | `		if( bStore ){` |
|    234087 | 1989 | `			PH7_MemObjStore(pEntry,pValue);` |
|    117046 | 1990 | `		}else{` |
|    355619 | 1991 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1992 | `		}` |
|    294777 | 1993 | `	}else{` |
|       ! 0 | 1994 | `		PH7_MemObjRelease(pValue);` |
|         - | 1995 | `	}` |
|    589701 | 1996 | `}` |
|         - | 1997 | `/*` |
|         - | 1998 | ` * Extract a node key.` |
|         - | 1999 | ` */` |
|    155542 | 2000 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2001 | `{` |
|         - | 2002 | `	/* Fill with the current key */` |
|    155547 | 2003 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    150389 | 2004 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2005 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2006 | `		}` |
|    150389 | 2007 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    150389 | 2008 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     75197 | 2009 | `	}else{` |
|      5163 | 2010 | `		SyBlobReset(&pKey->sBlob);` |
|      5163 | 2011 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5163 | 2012 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2013 | `	}` |
|    155547 | 2014 | `}` |
|         - | 2015 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2016 | `/*` |
|         - | 2017 | ` * Store the address of nodes value in the given container.` |
|         - | 2018 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2019 | ` * defined in 'builtin.c' for more information.` |
|         - | 2020 | ` */` |
|        12 | 2021 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 2022 | `{` |
|        13 | 2023 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2024 | `	ph7_value *pValue;` |
|         - | 2025 | `	sxu32 n;` |
|         - | 2026 | `	/* Initialize the container */` |
|        13 | 2027 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 2028 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2029 | `		/* Extract node value */` |
|        21 | 2030 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        21 | 2031 | `		if( pValue ){` |
|        21 | 2032 | `			SySetPut(pOut,(const void *)&pValue);` |
|        10 | 2033 | `		}` |
|         - | 2034 | `		/* Point to the next entry */` |
|        21 | 2035 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        11 | 2036 | `	}` |
|         - | 2037 | `	/* Total inserted entries */` |
|        13 | 2038 | `	return (int)SySetUsed(pOut);` |
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
|     35944 | 2065 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2066 | `{` |
|         - | 2067 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2068 | `    /* Prevent compiler warning */` |
|     35949 | 2069 | `	result.pNext = result.pPrev = 0;` |
|     35949 | 2070 | `	pTail = &result;` |
|    107895 | 2071 | `	while( pA && pB ){` |
|     71951 | 2072 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47674 | 2073 | `			pTail->pPrev = pA;` |
|     47674 | 2074 | `			pA->pNext = pTail;` |
|     47674 | 2075 | `			pTail = pA;` |
|     47674 | 2076 | `			pA = pA->pPrev;` |
|     23791 | 2077 | `		}else{` |
|     24282 | 2078 | `			pTail->pPrev = pB;` |
|     24282 | 2079 | `			pB->pNext = pTail;` |
|     24282 | 2080 | `			pTail = pB;` |
|     24282 | 2081 | `			pB = pB->pPrev;` |
|         - | 2082 | `		}` |
|         5 | 2083 | `	}` |
|     35949 | 2084 | `	if( pA ){` |
|     25355 | 2085 | `		pTail->pPrev = pA;` |
|     25355 | 2086 | `		pA->pNext = pTail;` |
|     23300 | 2087 | `	}else if( pB ){` |
|     10369 | 2088 | `		pTail->pPrev = pB;` |
|     10369 | 2089 | `		pB->pNext = pTail;` |
|      5161 | 2090 | `	}else{` |
|       235 | 2091 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2092 | `	}` |
|     35949 | 2093 | `	return result.pPrev;` |
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
|       750 | 2107 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2108 | `{` |
|         - | 2109 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2110 | `	sxu32 i;` |
|       755 | 2111 | `	SyZero(a,sizeof(a));` |
|         - | 2112 | `	/* Point to the first inserted entry */` |
|       755 | 2113 | `	pIn = pMap->pFirst;` |
|     14767 | 2114 | `	while( pIn ){` |
|     14017 | 2115 | `		p = pIn;` |
|     14017 | 2116 | `		pIn = p->pPrev;` |
|     14017 | 2117 | `		p->pPrev = 0;` |
|     26711 | 2118 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26711 | 2119 | `			if( a[i]==0 ){` |
|     14017 | 2120 | `				a[i] = p;` |
|     14017 | 2121 | `				break;` |
|       ! 0 | 2122 | `			}else{` |
|     12699 | 2123 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12699 | 2124 | `				a[i] = 0;` |
|         - | 2125 | `			}` |
|      6352 | 2126 | `		}` |
|     14017 | 2127 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2128 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2129 | `			 * But that is impossible.` |
|         - | 2130 | `			 */` |
|       ! 0 | 2131 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2132 | `		}` |
|         5 | 2133 | `	}` |
|       755 | 2134 | `	p = a[0];` |
|     24005 | 2135 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     23255 | 2136 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11630 | 2137 | `	}` |
|       755 | 2138 | `	p->pNext = 0;` |
|         - | 2139 | `	/* Reflect the change */` |
|       755 | 2140 | `	pMap->pFirst = p;` |
|         - | 2141 | `	/* Reset the loop cursor */` |
|       755 | 2142 | `	pMap->pCur = pMap->pFirst;` |
|       755 | 2143 | `	return SXRET_OK;` |
|         5 | 2144 | `}` |
|         - | 2145 | `/* SPDX-SnippetEnd */` |
|         - | 2146 | `/*` |
|         - | 2147 | ` * Node comparison callback.` |
|         - | 2148 | ` * used-by: [sort(),asort(),...]` |
|         - | 2149 | ` */` |
|     71674 | 2150 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2151 | `{` |
|         - | 2152 | `	ph7_value sA,sB;` |
|         - | 2153 | `	sxi32 iFlags;` |
|         - | 2154 | `	int rc;` |
|     71679 | 2155 | `	if( pCmpData == 0 ){` |
|         - | 2156 | `		/* Perform a standard comparison */` |
|     71655 | 2157 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     71655 | 2158 | `		return rc;` |
|         - | 2159 | `	}` |
|        25 | 2160 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2161 | `	/* Duplicate node values */` |
|        25 | 2162 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        25 | 2163 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        25 | 2164 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        25 | 2165 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        25 | 2166 | `	if( iFlags == 5 ){` |
|         - | 2167 | `		/* String cast */` |
|         - | 2168 | `		const char *zA,*zB;` |
|         - | 2169 | `		sxu32 nA,nB,nMin;` |
|        15 | 2170 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2171 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2172 | `		}` |
|        15 | 2173 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2174 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2175 | `		}` |
|         - | 2176 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        15 | 2177 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        15 | 2178 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        15 | 2179 | `		nA = SyBlobLength(&sA.sBlob);` |
|        15 | 2180 | `		nB = SyBlobLength(&sB.sBlob);` |
|        15 | 2181 | `		nMin = nA < nB ? nA : nB;` |
|        15 | 2182 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        15 | 2183 | `		if( rc == 0 ){` |
|         5 | 2184 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2185 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2186 | `		}` |
|         8 | 2187 | `	}else{` |
|         - | 2188 | `		/* Numeric cast */` |
|        11 | 2189 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2190 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2191 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2192 | `	}` |
|        25 | 2193 | `	PH7_MemObjRelease(&sA);` |
|        25 | 2194 | `	PH7_MemObjRelease(&sB);` |
|        25 | 2195 | `	return rc;` |
|     35804 | 2196 | `}` |
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
|        78 | 2283 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2284 | `{` |
|         - | 2285 | `	ph7_value sA,sB;` |
|         - | 2286 | `	sxi32 iFlags;` |
|         - | 2287 | `	int rc;` |
|        79 | 2288 | `	if( pCmpData == 0 ){` |
|         - | 2289 | `		/* Perform a standard comparison */` |
|        59 | 2290 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2291 | `		return -rc;` |
|         - | 2292 | `	}` |
|        21 | 2293 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2294 | `	/* Duplicate node values */` |
|        21 | 2295 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2296 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2297 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2298 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2299 | `	if( iFlags == 5 ){` |
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
|        11 | 2322 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2323 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2324 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2325 | `	}` |
|        21 | 2326 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2327 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2328 | `	return -rc;` |
|        40 | 2329 | `}` |
|         - | 2330 | `/*` |
|         - | 2331 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2332 | ` * used-by: [usort(),uasort()]` |
|         - | 2333 | ` */` |
|       110 | 2334 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2335 | `{` |
|         - | 2336 | `	ph7_value sResult,*pCallback;` |
|         - | 2337 | `	ph7_value *pV1,*pV2;` |
|         - | 2338 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2339 | `	sxi32 rc;` |
|         - | 2340 | `	/* Point to the desired callback */` |
|       113 | 2341 | `	pCallback = (ph7_value *)pCmpData;` |
|       113 | 2342 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2343 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2344 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2345 | `		return 0;` |
|         - | 2346 | `	}` |
|         - | 2347 | `	/* initialize the result value */` |
|       107 | 2348 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2349 | `	/* Extract nodes values */` |
|       107 | 2350 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       107 | 2351 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       107 | 2352 | `	apArg[0] = pV1;` |
|       107 | 2353 | `	apArg[1] = pV2;` |
|         - | 2354 | `	/* Invoke the callback */` |
|       107 | 2355 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       107 | 2356 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2357 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2358 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2359 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2360 | `		rc = 0;` |
|       102 | 2361 | `	}else if( rc != SXRET_OK ){` |
|         - | 2362 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2363 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2364 | `	}else{` |
|         - | 2365 | `		/* Extract callback result */` |
|        98 | 2366 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2367 | `			/* Perform an int cast */` |
|       ! 0 | 2368 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2369 | `		}` |
|        98 | 2370 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2371 | `	}` |
|       107 | 2372 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2373 | `	/* Callback result */` |
|       107 | 2374 | `	return rc;` |
|        58 | 2375 | `}` |
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
|        20 | 2442 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2443 | `{` |
|         - | 2444 | `	sxu32 n;` |
|        10 | 2445 | `	SXUNUSED(pB); /* cc warning */` |
|        10 | 2446 | `	SXUNUSED(pCmpData);` |
|         - | 2447 | `	/* Grab a random number */` |
|        21 | 2448 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2449 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2450 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2451 | `	 */` |
|        21 | 2452 | `	return n&1 ? 1 : -1;` |
|         1 | 2453 | `}` |
|         - | 2454 | `/*` |
|         - | 2455 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2456 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2457 | ` */` |
|       680 | 2458 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2459 | `{` |
|         - | 2460 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2461 | `	sxu32 i;` |
|         - | 2462 | `	/* Rehash all entries */` |
|       685 | 2463 | `	pLast = p = pMap->pFirst;` |
|       685 | 2464 | `	pMap->iNextIdx = 0;` |
|       685 | 2465 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|       685 | 2466 | `	i = 0;` |
|      7228 | 2467 | `	for( ;; ){` |
|     14461 | 2468 | `		if( i >= pMap->nEntry ){` |
|       685 | 2469 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       685 | 2470 | `			break;` |
|         - | 2471 | `		}` |
|     13781 | 2472 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2473 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2474 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2475 | `			/* Change key type */` |
|         5 | 2476 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2477 | `		}` |
|     13781 | 2478 | `		HashmapRehashIntNode(p);` |
|         - | 2479 | `		/* Point to the next entry */` |
|     13781 | 2480 | `		i++;` |
|     13781 | 2481 | `		pLast = p;` |
|     13781 | 2482 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2483 | `	}` |
|       685 | 2484 | `}` |
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
|      1016 | 2506 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2507 | `{` |
|         - | 2508 | `	ph7_hashmap *pMap;` |
|         - | 2509 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1021 | 2510 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2511 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2512 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2513 | `		return PH7_OK;` |
|         - | 2514 | `	}` |
|         - | 2515 | `	/* Point to the internal representation of the input hashmap */` |
|      1021 | 2516 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1021 | 2517 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1021 | 2518 | `	if( pMap->nEntry > 1 ){` |
|       661 | 2519 | `		sxi32 iCmpFlags = 0;` |
|       661 | 2520 | `		if( nArg > 1 ){` |
|         - | 2521 | `			/* Extract comparison flags */` |
|         3 | 2522 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2523 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2524 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2525 | `			}` |
|         1 | 2526 | `		}` |
|         - | 2527 | `		/* Do the merge sort */` |
|       661 | 2528 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2529 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       661 | 2530 | `		HashmapSortRehash(pMap);` |
|       328 | 2531 | `	}` |
|         - | 2532 | `	/* All done,return TRUE */` |
|      1021 | 2533 | `	ph7_result_bool(pCtx,1);` |
|      1021 | 2534 | `	return PH7_OK;` |
|       513 | 2535 | `}` |
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
|         2 | 2757 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2758 | `{` |
|         - | 2759 | `	ph7_hashmap *pMap;` |
|         - | 2760 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2761 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2762 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2763 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2764 | `		return PH7_OK;` |
|         - | 2765 | `	}` |
|         - | 2766 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2767 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2768 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2769 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2770 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2771 | `		if( nArg > 1 ){` |
|         - | 2772 | `			/* Extract comparison flags */` |
|       ! 0 | 2773 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2774 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2775 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2776 | `			}` |
|       ! 0 | 2777 | `		}` |
|         - | 2778 | `		/* Do the merge sort */` |
|         3 | 2779 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2780 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2781 | `		HashmapSortRehash(pMap);` |
|         1 | 2782 | `	}` |
|         - | 2783 | `	/* All done,return TRUE */` |
|         3 | 2784 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2785 | `	return PH7_OK;` |
|         2 | 2786 | `}` |
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
|        18 | 2801 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2802 | `{` |
|         - | 2803 | `	ph7_hashmap *pMap;` |
|         - | 2804 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 2805 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2806 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2807 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2808 | `		return PH7_OK;` |
|         - | 2809 | `	}` |
|         - | 2810 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 2811 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 2812 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 2813 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2814 | `		ph7_value *pCallback = 0;` |
|         - | 2815 | `		ProcNodeCmp xCmp;` |
|        21 | 2816 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        21 | 2817 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2818 | `			/* Point to the desired callback */` |
|        21 | 2819 | `			pCallback = apArg[1];` |
|        12 | 2820 | `		}else{` |
|         - | 2821 | `			/* Use the default comparison function */` |
|       ! 0 | 2822 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2823 | `		}` |
|         - | 2824 | `		/* Do the merge sort */` |
|        21 | 2825 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        21 | 2826 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2827 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        21 | 2828 | `		HashmapSortRehash(pMap);` |
|        21 | 2829 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2830 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2831 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2832 | `			return PH7_EXCEPTION;` |
|         - | 2833 | `		}` |
|         5 | 2834 | `	}` |
|         - | 2835 | `	/* All done,return TRUE */` |
|        12 | 2836 | `	ph7_result_bool(pCtx,1);` |
|        12 | 2837 | `	return PH7_OK;` |
|        12 | 2838 | `}` |
|         - | 2839 | `/*` |
|         - | 2840 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2841 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2842 | ` *  and maintain index association.` |
|         - | 2843 | ` * Parameters` |
|         - | 2844 | ` *  $array` |
|         - | 2845 | ` *   The input array.` |
|         - | 2846 | ` * $cmp_function` |
|         - | 2847 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2848 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2849 | ` *  to, or greater than the second.` |
|         - | 2850 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2851 | ` * Return` |
|         - | 2852 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2853 | ` */` |
|        10 | 2854 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2855 | `{` |
|         - | 2856 | `	ph7_hashmap *pMap;` |
|         - | 2857 | `	/* Make sure we are dealing with a valid hashmap */` |
|        11 | 2858 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2859 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2860 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2861 | `		return PH7_OK;` |
|         - | 2862 | `	}` |
|         - | 2863 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2864 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2865 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2866 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2867 | `		ph7_value *pCallback = 0;` |
|         - | 2868 | `		ProcNodeCmp xCmp;` |
|        11 | 2869 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2870 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2871 | `			/* Point to the desired callback */` |
|        11 | 2872 | `			pCallback = apArg[1];` |
|         6 | 2873 | `		}else{` |
|         - | 2874 | `			/* Use the default comparison function */` |
|       ! 0 | 2875 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2876 | `		}` |
|         - | 2877 | `		/* Do the merge sort */` |
|        11 | 2878 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2879 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2880 | `		/* Fix the last link broken by the merge */` |
|        23 | 2881 | `		while(pMap->pLast->pPrev){` |
|        13 | 2882 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2883 | `		}` |
|        11 | 2884 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2885 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2886 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2887 | `			return PH7_EXCEPTION;` |
|         - | 2888 | `		}` |
|         5 | 2889 | `	}` |
|         - | 2890 | `	/* All done,return TRUE */` |
|        11 | 2891 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2892 | `	return PH7_OK;` |
|         6 | 2893 | `}` |
|         - | 2894 | `/*` |
|         - | 2895 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2896 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2897 | ` *  function and maintain index association.` |
|         - | 2898 | ` * Parameters` |
|         - | 2899 | ` *  $array` |
|         - | 2900 | ` *   The input array.` |
|         - | 2901 | ` * $cmp_function` |
|         - | 2902 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2903 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2904 | ` *  to, or greater than the second.` |
|         - | 2905 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2906 | ` * Return` |
|         - | 2907 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2908 | ` */` |
|         2 | 2909 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2910 | `{` |
|         - | 2911 | `	ph7_hashmap *pMap;` |
|         - | 2912 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2913 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2914 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2915 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2916 | `		return PH7_OK;` |
|         - | 2917 | `	}` |
|         - | 2918 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2919 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2920 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2921 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2922 | `		ph7_value *pCallback = 0;` |
|         - | 2923 | `		ProcNodeCmp xCmp;` |
|         3 | 2924 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2925 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2926 | `			/* Point to the desired callback */` |
|         3 | 2927 | `			pCallback = apArg[1];` |
|         2 | 2928 | `		}else{` |
|         - | 2929 | `			/* Use the default comparison function */` |
|       ! 0 | 2930 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2931 | `		}` |
|         - | 2932 | `		/* Do the merge sort */` |
|         3 | 2933 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2934 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2935 | `		/* Fix the last link broken by the merge */` |
|         3 | 2936 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2937 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2938 | `		}` |
|         3 | 2939 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2940 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2941 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2942 | `			return PH7_EXCEPTION;` |
|         - | 2943 | `		}` |
|         1 | 2944 | `	}` |
|         - | 2945 | `	/* All done,return TRUE */` |
|         3 | 2946 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2947 | `	return PH7_OK;` |
|         2 | 2948 | `}` |
|         - | 2949 | `/*` |
|         - | 2950 | ` * bool shuffle(array &$array)` |
|         - | 2951 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2952 | ` * Parameters` |
|         - | 2953 | ` *  $array` |
|         - | 2954 | ` *   The input array.` |
|         - | 2955 | ` * Return` |
|         - | 2956 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2957 | ` *` |
|         - | 2958 | ` */` |
|         2 | 2959 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2960 | `{` |
|         - | 2961 | `	ph7_hashmap *pMap;` |
|         - | 2962 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2963 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2964 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2965 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2966 | `		return PH7_OK;` |
|         - | 2967 | `	}` |
|         - | 2968 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2969 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2970 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2971 | `	if( pMap->nEntry > 1 ){` |
|         - | 2972 | `		/* Do the merge sort */` |
|         3 | 2973 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2974 | `		/* Fix the last link broken by the merge */` |
|        10 | 2975 | `		while(pMap->pLast->pPrev){` |
|         8 | 2976 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2977 | `		}` |
|         1 | 2978 | `	}` |
|         - | 2979 | `	/* All done,return TRUE */` |
|         3 | 2980 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2981 | `	return PH7_OK;` |
|         2 | 2982 | `}` |
|         - | 2983 | `/*` |
|         - | 2984 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2985 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2986 | ` * Parameters` |
|         - | 2987 | ` *  $var` |
|         - | 2988 | ` *   The array or the object.` |
|         - | 2989 | ` * $mode` |
|         - | 2990 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2991 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2992 | ` *  all the elements of a multidimensional array.` |
|         - | 2993 | ` * Return` |
|         - | 2994 | ` *  Returns the number of elements in the array.` |
|         - | 2995 | ` */` |
|      1870 | 2996 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2997 | `{` |
|      1875 | 2998 | `	int bRecursive = FALSE;` |
|      1875 | 2999 | `	int bCycleDetected = FALSE;` |
|         - | 3000 | `	sxi64 iCount;` |
|      1875 | 3001 | `	if( nArg < 1 ){` |
|       ! 0 | 3002 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3003 | `			"ArgumentCountError",` |
|         - | 3004 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 3005 | `			);` |
|         - | 3006 | `	}` |
|      1875 | 3007 | `	if( nArg > 2 ){` |
|         4 | 3008 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3009 | `			"ArgumentCountError",` |
|         - | 3010 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 3011 | `			nArg` |
|         - | 3012 | `			);` |
|         - | 3013 | `	}` |
|         - | 3014 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 3015 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 3016 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1873 | 3017 | `	if( nArg > 1 ){` |
|        44 | 3018 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        44 | 3019 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        10 | 3020 | `			return PH7_VmThrowException(pCtx,` |
|         - | 3021 | `				"ValueError",` |
|         - | 3022 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 3023 | `				);` |
|         - | 3024 | `		}` |
|        34 | 3025 | `		bRecursive = iMode == 1;` |
|        16 | 3026 | `	}` |
|      1865 | 3027 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3028 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 3029 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 3030 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 3031 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 3032 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 3033 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3034 | `					"count",sizeof("count")-1);` |
|        59 | 3035 | `				if( pMeth ){` |
|         - | 3036 | `					ph7_value sResult;` |
|        59 | 3037 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 3038 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3039 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3040 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3041 | `					return PH7_OK;` |
|         - | 3042 | `				}` |
|       ! 0 | 3043 | `			}` |
|         1 | 3044 | `		}` |
|        22 | 3045 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3046 | `			"TypeError",` |
|         - | 3047 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3048 | `			ph7_type_name(apArg[0])` |
|         - | 3049 | `			);` |
|         - | 3050 | `	}` |
|         - | 3051 | `	/* Count */` |
|      1797 | 3052 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1797 | 3053 | `	if( bCycleDetected ){` |
|         3 | 3054 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3055 | `	}` |
|      1797 | 3056 | `	ph7_result_int64(pCtx,iCount);` |
|      1797 | 3057 | `	return PH7_OK;` |
|       940 | 3058 | `}` |
|         - | 3059 | `/*` |
|         - | 3060 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3061 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3062 | ` * Parameters` |
|         - | 3063 | ` * $key` |
|         - | 3064 | ` *   Value to check.` |
|         - | 3065 | ` * $search` |
|         - | 3066 | ` *  An array with keys to check.` |
|         - | 3067 | ` * Return` |
|         - | 3068 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3069 | ` */` |
|        90 | 3070 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3071 | `{` |
|         - | 3072 | `	sxi32 rc;` |
|        94 | 3073 | `	if( nArg != 2 ){` |
|         - | 3074 | `		/* PHP requires exactly two arguments */` |
|         4 | 3075 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3076 | `			"ArgumentCountError",` |
|         - | 3077 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3078 | `			nArg` |
|         - | 3079 | `			);` |
|         - | 3080 | `	}` |
|         - | 3081 | `	/* Make sure we are dealing with a valid hashmap */` |
|        92 | 3082 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3083 | `		/* Type mismatch -> TypeError */` |
|         8 | 3084 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3085 | `			"TypeError",` |
|         - | 3086 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3087 | `			ph7_type_name(apArg[1])` |
|         - | 3088 | `			);` |
|         - | 3089 | `	}` |
|         - | 3090 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        88 | 3091 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         - | 3092 | `		/* PH7_VmThrowDeprecatedFmt, not ph7_context_throw_error_format: the latter PREPENDS` |
|         - | 3093 | `		 * "array_key_exists(): " and php's message carries no such prefix. */` |
|         3 | 3094 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 3095 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3096 | `			"use an empty string instead"` |
|         - | 3097 | `			);` |
|        87 | 3098 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3099 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3100 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3101 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3102 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3103 | `				,rVal` |
|         - | 3104 | `				);` |
|         1 | 3105 | `		}` |
|         1 | 3106 | `	}` |
|         - | 3107 | `	/* Perform the lookup */` |
|        88 | 3108 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3109 | `	/* lookup result */` |
|        88 | 3110 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        88 | 3111 | `	return PH7_OK;` |
|        49 | 3112 | `}` |
|         - | 3113 | `/*` |
|         - | 3114 | ` * value array_pop(array $array)` |
|         - | 3115 | ` *   POP the last inserted element from the array.` |
|         - | 3116 | ` * Parameter` |
|         - | 3117 | ` *  The array to get the value from.` |
|         - | 3118 | ` * Return` |
|         - | 3119 | ` *  Poped value or NULL on failure.` |
|         - | 3120 | ` */` |
|       102 | 3121 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3122 | `{` |
|         - | 3123 | `	ph7_hashmap *pMap;` |
|         - | 3124 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       106 | 3125 | `	if( nArg != 1 ){` |
|         4 | 3126 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3127 | `			"ArgumentCountError",` |
|         - | 3128 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3129 | `			nArg` |
|         - | 3130 | `			);` |
|         - | 3131 | `	}` |
|         - | 3132 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3133 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3134 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3135 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3136 | `			"Error",` |
|         - | 3137 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3138 | `			);` |
|         - | 3139 | `	}` |
|         - | 3140 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3141 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3142 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3143 | `			"TypeError",` |
|         - | 3144 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3145 | `			ph7_type_name(apArg[0])` |
|         - | 3146 | `			);` |
|         - | 3147 | `	}` |
|        95 | 3148 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3149 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3150 | `	if( pMap->nEntry < 1 ){` |
|         - | 3151 | `		/* Nothing to pop,return NULL */` |
|         3 | 3152 | `		ph7_result_null(pCtx);` |
|         2 | 3153 | `	}else{` |
|        93 | 3154 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3155 | `		ph7_value *pObj;` |
|        93 | 3156 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3157 | `		if( pObj ){` |
|         - | 3158 | `			/* Node value */` |
|        93 | 3159 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3160 | `			/* Unlink the node */` |
|        93 | 3161 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3162 | `		}else{` |
|       ! 0 | 3163 | `			ph7_result_null(pCtx);` |
|         - | 3164 | `		}` |
|         - | 3165 | `		/* Reset the cursor */` |
|        93 | 3166 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3167 | `	}` |
|        95 | 3168 | `	return PH7_OK;` |
|        55 | 3169 | `}` |
|         - | 3170 | `/*` |
|         - | 3171 | ` * int array_push($array,$var,...)` |
|         - | 3172 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3173 | ` * Parameters` |
|         - | 3174 | ` *  array` |
|         - | 3175 | ` *    The input array.` |
|         - | 3176 | ` *  var` |
|         - | 3177 | ` *   On or more value to push.` |
|         - | 3178 | ` * Return` |
|         - | 3179 | ` *  New array count (including old items).` |
|         - | 3180 | ` */` |
|        22 | 3181 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3182 | `{` |
|         - | 3183 | `	ph7_hashmap *pMap;` |
|         - | 3184 | `	sxi32 rc;` |
|         - | 3185 | `	int i;` |
|        27 | 3186 | `	if( nArg < 1 ){` |
|       ! 0 | 3187 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3188 | `			"ArgumentCountError",` |
|         - | 3189 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3190 | `			nArg` |
|         - | 3191 | `			);` |
|         - | 3192 | `	}` |
|         - | 3193 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3194 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        27 | 3195 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3196 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3197 | `			"Error",` |
|         - | 3198 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3199 | `			);` |
|         - | 3200 | `	}` |
|         - | 3201 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3202 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3203 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3204 | `			"TypeError",` |
|         - | 3205 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3206 | `			ph7_type_name(apArg[0])` |
|         - | 3207 | `			);` |
|         - | 3208 | `	}` |
|         - | 3209 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3210 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3211 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3212 | `	/* Start pushing given values */` |
|        34 | 3213 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3214 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3215 | `		if( rc != SXRET_OK ){` |
|         3 | 3216 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3217 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3218 | `				return rc;` |
|         - | 3219 | `			}` |
|       ! 0 | 3220 | `			break;` |
|         - | 3221 | `		}` |
|         9 | 3222 | `	}` |
|         - | 3223 | `	/* Return the new count */` |
|        15 | 3224 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3225 | `	return PH7_OK;` |
|        16 | 3226 | `}` |
|         - | 3227 | `/*` |
|         - | 3228 | ` * value array_shift(array $array)` |
|         - | 3229 | ` *   Shift an element off the beginning of array.` |
|         - | 3230 | ` * Parameter` |
|         - | 3231 | ` *  The array to get the value from.` |
|         - | 3232 | ` * Return` |
|         - | 3233 | ` *  Shifted value or NULL on failure.` |
|         - | 3234 | ` */` |
|        44 | 3235 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3236 | `{` |
|         - | 3237 | `	ph7_hashmap *pMap;` |
|         - | 3238 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3239 | `	if( nArg != 1 ){` |
|         4 | 3240 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3241 | `			"ArgumentCountError",` |
|         - | 3242 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3243 | `			nArg` |
|         - | 3244 | `			);` |
|         - | 3245 | `	}` |
|         - | 3246 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3247 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3248 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3249 | `			"Error",` |
|         - | 3250 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3251 | `			);` |
|         - | 3252 | `	}` |
|         - | 3253 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3254 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3255 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3256 | `			"TypeError",` |
|         - | 3257 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3258 | `			ph7_type_name(apArg[0])` |
|         - | 3259 | `			);` |
|         - | 3260 | `	}` |
|         - | 3261 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3262 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3263 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3264 | `	if( pMap->nEntry < 1 ){` |
|         - | 3265 | `		/* Empty hashmap,return NULL */` |
|         3 | 3266 | `		ph7_result_null(pCtx);` |
|         2 | 3267 | `	}else{` |
|        39 | 3268 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3269 | `		ph7_value *pObj;` |
|         - | 3270 | `		sxu32 n;` |
|        39 | 3271 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3272 | `		if( pObj ){` |
|         - | 3273 | `			/* Node value */` |
|        39 | 3274 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3275 | `			/* Unlink the first node */` |
|        39 | 3276 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3277 | `		}else{` |
|       ! 0 | 3278 | `			ph7_result_null(pCtx);` |
|         - | 3279 | `		}` |
|         - | 3280 | `		/* Rehash all int keys */` |
|        39 | 3281 | `		n = pMap->nEntry;` |
|        39 | 3282 | `		pEntry = pMap->pFirst;` |
|        39 | 3283 | `		pMap->iNextIdx = 0;` |
|        39 | 3284 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3285 | `		for(;;){` |
|        99 | 3286 | `			if( n < 1 ){` |
|        39 | 3287 | `				break;` |
|         - | 3288 | `			}` |
|        65 | 3289 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3290 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3291 | `			}` |
|         - | 3292 | `			/* Point to the next entry */` |
|        65 | 3293 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3294 | `			n--;` |
|         5 | 3295 | `		}` |
|         - | 3296 | `		/* Reset the cursor */` |
|        39 | 3297 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3298 | `	}` |
|        41 | 3299 | `	return PH7_OK;` |
|        27 | 3300 | `}` |
|         - | 3301 | `/*` |
|         - | 3302 | ` * Extract the node cursor value.` |
|         - | 3303 | ` */` |
|      1094 | 3304 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3305 | `{` |
|      1095 | 3306 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3307 | `	ph7_value *pVal;` |
|      1095 | 3308 | `	if( pCur == 0 ){` |
|         - | 3309 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3310 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3311 | `		return PH7_OK;` |
|         - | 3312 | `	}` |
|      1057 | 3313 | `	if( iDirection != 0 ){` |
|       201 | 3314 | `		if( iDirection > 0 ){` |
|         - | 3315 | `			/* Point to the next entry */` |
|       199 | 3316 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3317 | `			pCur = pMap->pCur;` |
|       100 | 3318 | `		}else{` |
|         - | 3319 | `			/* Point to the previous entry */` |
|         3 | 3320 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3321 | `			pCur = pMap->pCur;` |
|         - | 3322 | `		}` |
|       201 | 3323 | `		if( pCur == 0 ){` |
|         - | 3324 | `			/* End of input reached,return FALSE */` |
|        83 | 3325 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3326 | `			return PH7_OK;` |
|         - | 3327 | `		}` |
|        59 | 3328 | `	}` |
|         - | 3329 | `	/* Point to the desired element */` |
|       975 | 3330 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3331 | `	if( pVal ){` |
|       975 | 3332 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3333 | `	}else{` |
|       ! 0 | 3334 | `		ph7_result_bool(pCtx,0);` |
|         - | 3335 | `	}` |
|       975 | 3336 | `	return PH7_OK;` |
|       548 | 3337 | `}` |
|         - | 3338 | `/*` |
|         - | 3339 | ` * value current(array $array)` |
|         - | 3340 | ` *  Return the current element in an array.` |
|         - | 3341 | ` * Parameter` |
|         - | 3342 | ` *  $input: The input array.` |
|         - | 3343 | ` * Return` |
|         - | 3344 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3345 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3346 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3347 | ` *  is empty, current() returns FALSE.` |
|         - | 3348 | ` */` |
|       302 | 3349 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3350 | `{` |
|       303 | 3351 | `	if( nArg < 1 ){` |
|         - | 3352 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3353 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3354 | `		return PH7_OK;` |
|         - | 3355 | `	}` |
|         - | 3356 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3357 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3358 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3359 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3360 | `		return PH7_OK;` |
|         - | 3361 | `	}` |
|       303 | 3362 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3363 | `	return PH7_OK;` |
|       152 | 3364 | `}` |
|         - | 3365 | `/*` |
|         - | 3366 | ` * value next(array $input)` |
|         - | 3367 | ` *  Advance the internal array pointer of an array.` |
|         - | 3368 | ` * Parameter` |
|         - | 3369 | ` *  $input: The input array.` |
|         - | 3370 | ` * Return` |
|         - | 3371 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3372 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3373 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3374 | ` */` |
|       198 | 3375 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3376 | `{` |
|       199 | 3377 | `	if( nArg < 1 ){` |
|         - | 3378 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3379 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3380 | `		return PH7_OK;` |
|         - | 3381 | `	}` |
|         - | 3382 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3383 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3384 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3385 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3386 | `		return PH7_OK;` |
|         - | 3387 | `	}` |
|       199 | 3388 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3389 | `	return PH7_OK;` |
|       100 | 3390 | `}` |
|         - | 3391 | `/*` |
|         - | 3392 | ` * value prev(array $input)` |
|         - | 3393 | ` *  Rewind the internal array pointer.` |
|         - | 3394 | ` * Parameter` |
|         - | 3395 | ` *  $input: The input array.` |
|         - | 3396 | ` * Return` |
|         - | 3397 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3398 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3399 | ` *  elements.` |
|         - | 3400 | ` */` |
|         2 | 3401 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3402 | `{` |
|         3 | 3403 | `	if( nArg < 1 ){` |
|         - | 3404 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3405 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3406 | `		return PH7_OK;` |
|         - | 3407 | `	}` |
|         - | 3408 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3409 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3410 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3411 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3412 | `		return PH7_OK;` |
|         - | 3413 | `	}` |
|         3 | 3414 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3415 | `	return PH7_OK;` |
|         2 | 3416 | `}` |
|         - | 3417 | `/*` |
|         - | 3418 | ` * value end(array $input)` |
|         - | 3419 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3420 | ` * Parameter` |
|         - | 3421 | ` *  $input: The input array.` |
|         - | 3422 | ` * Return` |
|         - | 3423 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3424 | ` */` |
|       348 | 3425 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3426 | `{` |
|         - | 3427 | `	ph7_hashmap *pMap;` |
|       349 | 3428 | `	if( nArg < 1 ){` |
|         - | 3429 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3430 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3431 | `		return PH7_OK;` |
|         - | 3432 | `	}` |
|         - | 3433 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3434 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3435 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3436 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3437 | `		return PH7_OK;` |
|         - | 3438 | `	}` |
|         - | 3439 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3440 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3441 | `	/* Point to the last node */` |
|       349 | 3442 | `	pMap->pCur = pMap->pLast;` |
|         - | 3443 | `	/* Return the last node value */` |
|       349 | 3444 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3445 | `	return PH7_OK;` |
|       175 | 3446 | `}` |
|         - | 3447 | `/*` |
|         - | 3448 | ` * value reset(array $array )` |
|         - | 3449 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3450 | ` * Parameter` |
|         - | 3451 | ` *  $input: The input array.` |
|         - | 3452 | ` * Return` |
|         - | 3453 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3454 | ` */` |
|       244 | 3455 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3456 | `{` |
|         - | 3457 | `	ph7_hashmap *pMap;` |
|       245 | 3458 | `	if( nArg < 1 ){` |
|         - | 3459 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3460 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3461 | `		return PH7_OK;` |
|         - | 3462 | `	}` |
|         - | 3463 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3464 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3465 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3466 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3467 | `		return PH7_OK;` |
|         - | 3468 | `	}` |
|         - | 3469 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3470 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3471 | `	/* Point to the first node */` |
|       245 | 3472 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3473 | `	/* Return the last node value if available */` |
|       245 | 3474 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3475 | `	return PH7_OK;` |
|       123 | 3476 | `}` |
|         - | 3477 | `/*` |
|         - | 3478 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3479 | ` * array_key_first() and array_key_last().` |
|         - | 3480 | ` */` |
|       672 | 3481 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3482 | `{` |
|       673 | 3483 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3484 | `		/* Key is integer */` |
|       283 | 3485 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3486 | `	}else{` |
|         - | 3487 | `		/* Key is blob */` |
|       586 | 3488 | `		ph7_result_string(pCtx,` |
|       390 | 3489 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3490 | `	}` |
|       673 | 3491 | `}` |
|         - | 3492 | `/*` |
|         - | 3493 | ` * value key(array $array)` |
|         - | 3494 | ` *   Fetch a key from an array` |
|         - | 3495 | ` * Parameter` |
|         - | 3496 | ` *  $input` |
|         - | 3497 | ` *   The input array.` |
|         - | 3498 | ` * Return` |
|         - | 3499 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3500 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3501 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3502 | ` *  is empty, key() returns NULL.` |
|         - | 3503 | ` */` |
|       776 | 3504 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3505 | `{` |
|         - | 3506 | `	ph7_hashmap_node *pCur;` |
|         - | 3507 | `	ph7_hashmap *pMap;` |
|       777 | 3508 | `	if( nArg < 1 ){` |
|         - | 3509 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3510 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3511 | `		return PH7_OK;` |
|         - | 3512 | `	}` |
|         - | 3513 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3514 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3515 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3516 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3517 | `		return PH7_OK;` |
|         - | 3518 | `	}` |
|       777 | 3519 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3520 | `	pCur = pMap->pCur;` |
|       777 | 3521 | `	if( pCur == 0 ){` |
|         - | 3522 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3523 | `		ph7_result_null(pCtx);` |
|       121 | 3524 | `		return PH7_OK;` |
|         - | 3525 | `	}` |
|       657 | 3526 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3527 | `	return PH7_OK;` |
|       389 | 3528 | `}` |
|         - | 3529 | `/*` |
|         - | 3530 | ` * array each(array $input)` |
|         - | 3531 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3532 | ` * Parameter` |
|         - | 3533 | ` *  $input` |
|         - | 3534 | ` *    The input array.` |
|         - | 3535 | ` * Return` |
|         - | 3536 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3537 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3538 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3539 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3540 | ` *  each() returns FALSE.` |
|         - | 3541 | ` */` |
|        22 | 3542 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3543 | `{` |
|         - | 3544 | `	ph7_hashmap_node *pCur;` |
|         - | 3545 | `	ph7_hashmap *pMap;` |
|         - | 3546 | `	ph7_value *pArray;` |
|         - | 3547 | `	ph7_value *pVal;` |
|         - | 3548 | `	ph7_value sKey;` |
|        23 | 3549 | `	if( nArg < 1 ){` |
|         - | 3550 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3551 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3552 | `		return PH7_OK;` |
|         - | 3553 | `	}` |
|         - | 3554 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3555 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3556 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3557 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3558 | `		return PH7_OK;` |
|         - | 3559 | `	}` |
|         - | 3560 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3561 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3562 | `	if( pMap->pCur == 0 ){` |
|         - | 3563 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3564 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3565 | `		return PH7_OK;` |
|         - | 3566 | `	}` |
|        15 | 3567 | `	pCur = pMap->pCur;` |
|         - | 3568 | `	/* Create a new array */` |
|        15 | 3569 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3570 | `	if( pArray == 0 ){` |
|       ! 0 | 3571 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3572 | `		return PH7_OK;` |
|         - | 3573 | `	}` |
|        15 | 3574 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3575 | `	/* Insert the current value */` |
|        15 | 3576 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3577 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3578 | `	/* Make the key */` |
|        15 | 3579 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3580 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3581 | `	}else{` |
|         9 | 3582 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3583 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3584 | `	}` |
|         - | 3585 | `	/* Insert the current key */` |
|        15 | 3586 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3587 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3588 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3589 | `	/* Advance the cursor */` |
|        15 | 3590 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3591 | `	/* Return the current entry */` |
|        15 | 3592 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3593 | `	return PH7_OK;` |
|        12 | 3594 | `}` |
|         - | 3595 | `/*` |
|         - | 3596 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3597 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3598 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3599 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3600 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3601 | ` */` |
|         - | 3602 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3603 | `/*` |
|         - | 3604 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3605 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3606 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3607 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3608 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3609 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3610 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3611 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3612 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3613 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3614 | ` */` |
|         - | 3615 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3616 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3617 | `/*` |
|         - | 3618 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3619 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3620 | ` */` |
|       ! 0 | 3621 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3622 | `{` |
|       ! 0 | 3623 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3624 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3625 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3626 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3627 | `		zBuf[n] = 0;` |
|       ! 0 | 3628 | `		return zBuf;` |
|         - | 3629 | `	}` |
|       ! 0 | 3630 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3631 | `}` |
|         - | 3632 | `/*` |
|         - | 3633 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3634 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3635 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3636 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3637 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3638 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3639 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3640 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3641 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3642 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3643 | ` */` |
|       156 | 3644 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3645 | `{` |
|       157 | 3646 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3647 | `	sxu64 uVal = 0;` |
|       157 | 3648 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3649 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3650 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3651 | `		bNeg = (z[0] == '-');` |
|         3 | 3652 | `		z++;` |
|         1 | 3653 | `	}` |
|       237 | 3654 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3655 | `		int d = z[0] - '0';` |
|         - | 3656 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3657 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3658 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3659 | `			bOverflow = 1;` |
|       ! 0 | 3660 | `		}else{` |
|        81 | 3661 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3662 | `		}` |
|        81 | 3663 | `		bDigit = 1;` |
|        81 | 3664 | `		z++;` |
|         1 | 3665 | `	}` |
|       157 | 3666 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3667 | `		bReal = 1;` |
|         3 | 3668 | `		z++;` |
|         5 | 3669 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3670 | `			bDigit = 1;` |
|         3 | 3671 | `			z++;` |
|         1 | 3672 | `		}` |
|         1 | 3673 | `	}` |
|         - | 3674 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3675 | `	if( !bDigit ){` |
|        61 | 3676 | `		return RANGE_IN_ERROR;` |
|         - | 3677 | `	}` |
|         - | 3678 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3679 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3680 | `		z++;` |
|         9 | 3681 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3682 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3683 | `			return RANGE_IN_ERROR;` |
|         - | 3684 | `		}` |
|         9 | 3685 | `		bReal = 1;` |
|        17 | 3686 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3687 | `	}` |
|         - | 3688 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3689 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3690 | `	if( z != zEnd ){` |
|        13 | 3691 | `		return RANGE_IN_ERROR;` |
|         - | 3692 | `	}` |
|        84 | 3693 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3694 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3695 | `		bReal = 1;` |
|        84 | 3696 | `	}` |
|        43 | 3697 | `	if( bReal ){` |
|        11 | 3698 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3699 | `		return RANGE_IN_DOUBLE;` |
|         - | 3700 | `	}` |
|         - | 3701 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3702 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3703 | `	return RANGE_IN_LONG;` |
|        58 | 3704 | `}` |
|         - | 3705 | `/*` |
|         - | 3706 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3707 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3708 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3709 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3710 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3711 | ` */` |
|       328 | 3712 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3713 | `{` |
|         - | 3714 | `	char zMsg[160];` |
|       329 | 3715 | `	*pRc = PH7_OK;` |
|       329 | 3716 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3717 | `		char zType[80];` |
|       ! 0 | 3718 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3719 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3720 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3721 | `		return FALSE;` |
|         - | 3722 | `	}` |
|       329 | 3723 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3724 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3725 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3726 | `			iArg,zName);` |
|         5 | 3727 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3728 | `		*pbNullCoerced = TRUE;` |
|         2 | 3729 | `	}` |
|       329 | 3730 | `	return TRUE;` |
|       165 | 3731 | `}` |
|         - | 3732 | `/*` |
|         - | 3733 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3734 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3735 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3736 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3737 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3738 | ` */` |
|        60 | 3739 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3740 | `{` |
|        61 | 3741 | `	*pRc = PH7_OK;` |
|        61 | 3742 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3743 | `		char zType[80];` |
|       ! 0 | 3744 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3745 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3746 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3747 | `		return RANGE_IN_ERROR;` |
|         - | 3748 | `	}` |
|        61 | 3749 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3750 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3751 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3752 | `		*pLong = 0;` |
|         3 | 3753 | `		return RANGE_IN_LONG;` |
|         - | 3754 | `	}` |
|        59 | 3755 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3756 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3757 | `		return RANGE_IN_DOUBLE;` |
|         - | 3758 | `	}` |
|        35 | 3759 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3760 | `		const char *zStr;` |
|         - | 3761 | `		int nLen;` |
|         - | 3762 | `		sxu8 iKind;` |
|         3 | 3763 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3764 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3765 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3766 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3767 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3768 | `		}` |
|         3 | 3769 | `		return iKind;` |
|         - | 3770 | `	}` |
|         - | 3771 | `	/* int / bool */` |
|        33 | 3772 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3773 | `	return RANGE_IN_LONG;` |
|        31 | 3774 | `}` |
|         - | 3775 | `/*` |
|         - | 3776 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3777 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3778 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3779 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3780 | ` */` |
|       296 | 3781 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3782 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3783 | `{` |
|         - | 3784 | `	char zMsg[160];` |
|         - | 3785 | `	double r;` |
|       297 | 3786 | `	*pRc = PH7_OK;` |
|       297 | 3787 | `	if( bNullCoerced ){` |
|         - | 3788 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3789 | `		*pLong = 0;` |
|         5 | 3790 | `		*pDouble = 0.0;` |
|         5 | 3791 | `		return RANGE_IN_LONG;` |
|         - | 3792 | `	}` |
|       293 | 3793 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3794 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3795 | `check_dval:` |
|        25 | 3796 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3797 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3798 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3799 | `			return RANGE_IN_ERROR;` |
|         - | 3800 | `		}` |
|        21 | 3801 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3802 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3803 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3804 | `			return RANGE_IN_ERROR;` |
|         - | 3805 | `		}` |
|        17 | 3806 | `		*pDouble = r;` |
|        17 | 3807 | `		return RANGE_IN_DOUBLE;` |
|         - | 3808 | `	}` |
|       273 | 3809 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3810 | `		const char *zStr;` |
|         - | 3811 | `		int nLen;` |
|         - | 3812 | `		sxu8 iKind;` |
|        81 | 3813 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3814 | `		if( nLen == 0 ){` |
|         7 | 3815 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3816 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3817 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3818 | `			*pLong = 0;` |
|         5 | 3819 | `			*pDouble = 0.0;` |
|        41 | 3820 | `			return RANGE_IN_LONG;` |
|         - | 3821 | `		}` |
|        77 | 3822 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3823 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3824 | `			r = *pDouble;` |
|         5 | 3825 | `			goto check_dval;` |
|         - | 3826 | `		}` |
|        73 | 3827 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3828 | `			*pDouble = (double)*pLong;` |
|        23 | 3829 | `			if( nLen == 1 ){` |
|         - | 3830 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3831 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3832 | `				return RANGE_IN_DIGIT;` |
|         - | 3833 | `			}` |
|        15 | 3834 | `			return RANGE_IN_LONG;` |
|         - | 3835 | `		}` |
|        51 | 3836 | `		if( nLen != 1 ){` |
|        10 | 3837 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3838 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3839 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3840 | `		}` |
|        51 | 3841 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3842 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3843 | `		*pLong = 0;` |
|        51 | 3844 | `		*pDouble = 0.0;` |
|        51 | 3845 | `		return RANGE_IN_STRING;` |
|         - | 3846 | `	}` |
|         - | 3847 | `	/* int / bool */` |
|       193 | 3848 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3849 | `	*pDouble = (double)*pLong;` |
|       193 | 3850 | `	return RANGE_IN_LONG;` |
|       149 | 3851 | `}` |
|         - | 3852 | `/*` |
|         - | 3853 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3854 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3855 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3856 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3857 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3858 | ` * exactly like php's two macros.` |
|         - | 3859 | ` */` |
|         6 | 3860 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3861 | `{` |
|        10 | 3862 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3863 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3864 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3865 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3866 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3867 | `}` |
|         6 | 3868 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3869 | `{` |
|         - | 3870 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3871 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3872 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3873 | `	const unsigned int nBuf = 1500;` |
|         7 | 3874 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3875 | `	if( zMsg == 0 ){` |
|       ! 0 | 3876 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3877 | `	}` |
|         7 | 3878 | `	snprintf(zMsg,nBuf,` |
|         - | 3879 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3880 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3881 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3882 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3883 | `}` |
|         - | 3884 | `/*` |
|         - | 3885 | ` * Set the element container to the next range element and append it to the` |
|         - | 3886 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3887 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3888 | ` * below stay one line per iteration.` |
|         - | 3889 | ` */` |
|      1680 | 3890 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3891 | `{` |
|      1681 | 3892 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3893 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3894 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3895 | `	}` |
|      1681 | 3896 | `	return PH7_OK;` |
|       841 | 3897 | `}` |
|        70 | 3898 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3899 | `{` |
|        71 | 3900 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3901 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3902 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3903 | `	}` |
|        71 | 3904 | `	return PH7_OK;` |
|        36 | 3905 | `}` |
|       168 | 3906 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3907 | `{` |
|       169 | 3908 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3909 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3910 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3911 | `	}` |
|       169 | 3912 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3913 | `	return PH7_OK;` |
|        85 | 3914 | `}` |
|         - | 3915 | `/*` |
|         - | 3916 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3917 | ` *  Create an array containing a range of elements.` |
|         - | 3918 | ` * Return` |
|         - | 3919 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3920 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3921 | ` */` |
|       166 | 3922 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3923 | `{` |
|         - | 3924 | `	ph7_value *pValue,*pArray;` |
|       167 | 3925 | `	sxi32 rc = PH7_OK;` |
|       167 | 3926 | `	int is_step_double = 0,is_step_negative = 0;` |
|       167 | 3927 | `	double step_double = 1.0;` |
|       167 | 3928 | `	sxi64 step = 1;` |
|         - | 3929 | `	sxu8 start_type,end_type;` |
|       167 | 3930 | `	sxi64 start_long = 0,end_long = 0;` |
|       167 | 3931 | `	double start_double = 0.0,end_double = 0.0;` |
|       167 | 3932 | `	unsigned char cStart = 0,cEnd = 0;` |
|       167 | 3933 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3934 | `	sxu32 i,size;` |
|         - | 3935 |  |
|         - | 3936 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       167 | 3937 | `	if( nArg > 3 ){` |
|         4 | 3938 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3939 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3940 | `	}` |
|       165 | 3941 | `	if( nArg < 2 ){` |
|         - | 3942 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3943 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3944 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3945 | `	}` |
|         - | 3946 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3947 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       165 | 3948 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 3949 | `		return rc;` |
|         - | 3950 | `	}` |
|       165 | 3951 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3952 | `		return rc;` |
|         - | 3953 | `	}` |
|       165 | 3954 | `	if( nArg > 2 ){` |
|        61 | 3955 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 3956 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 3957 | `			return rc;` |
|         - | 3958 | `		}` |
|        59 | 3959 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3960 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3961 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3962 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3963 | `			}` |
|        23 | 3964 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3965 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3966 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3967 | `			}` |
|         - | 3968 | `			/* We only want positive step values. */` |
|        21 | 3969 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3970 | `				is_step_negative = 1;` |
|       ! 0 | 3971 | `				step_double *= -1;` |
|       ! 0 | 3972 | `			}` |
|         - | 3973 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3974 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3975 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3976 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3977 | `				step = (sxi64)step_double;` |
|        19 | 3978 | `				if( (double)step != step_double ){` |
|        17 | 3979 | `					is_step_double = 1;` |
|         8 | 3980 | `				}` |
|        10 | 3981 | `			}else{` |
|         - | 3982 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3983 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3984 | `				is_step_double = 1;` |
|         - | 3985 | `			}` |
|        11 | 3986 | `		}else{` |
|         - | 3987 | `			/* We only want positive step values. */` |
|        35 | 3988 | `			if( step < 0 ){` |
|        11 | 3989 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3990 | `					/* -step would overflow */` |
|         4 | 3991 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3992 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3993 | `				}` |
|         9 | 3994 | `				is_step_negative = 1;` |
|         9 | 3995 | `				step = -step;` |
|         4 | 3996 | `			}` |
|        33 | 3997 | `			step_double = (double)step;` |
|         - | 3998 | `		}` |
|        53 | 3999 | `		if( step_double == 0.0 ){` |
|         7 | 4000 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4001 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 4002 | `		}` |
|        23 | 4003 | `	}` |
|       151 | 4004 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 4005 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 4006 | `		return rc;` |
|         - | 4007 | `	}` |
|       147 | 4008 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 4009 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 4010 | `		return rc;` |
|         - | 4011 | `	}` |
|         - | 4012 | `	/* Element container + result array */` |
|       143 | 4013 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 4014 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 4015 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 4016 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 4017 | `	}` |
|         - | 4018 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 4019 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 4020 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 4021 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 4022 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 4023 | `			 * and the range is numeric. */` |
|        15 | 4024 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 4025 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 4026 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4027 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 4028 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 4029 | `				}` |
|         7 | 4030 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4031 | `			}else{` |
|         9 | 4032 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4033 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4034 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4035 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4036 | `				}` |
|         9 | 4037 | `				start_type = RANGE_IN_LONG;` |
|         - | 4038 | `			}` |
|        15 | 4039 | `			goto handle_numeric_inputs;` |
|         - | 4040 | `		}` |
|        23 | 4041 | `		if( is_step_double ){` |
|         - | 4042 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4043 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4044 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4045 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4046 | `					" of characters, inputs converted to 0");` |
|         1 | 4047 | `			}` |
|         5 | 4048 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4049 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4050 | `			goto handle_numeric_inputs;` |
|         - | 4051 | `		}` |
|         - | 4052 | `		/* Generate an array of characters */` |
|        19 | 4053 | `		if( cStart > cEnd ){` |
|         - | 4054 | `			/* Decreasing char range */` |
|         - | 4055 | `			int iCur;` |
|         3 | 4056 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4057 | `				goto boundary_error;` |
|         - | 4058 | `			}` |
|        17 | 4059 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4060 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4061 | `					return rc;` |
|         - | 4062 | `				}` |
|         8 | 4063 | `			}` |
|        18 | 4064 | `		}else if( cEnd > cStart ){` |
|         - | 4065 | `			/* Increasing char range */` |
|         - | 4066 | `			int iCur;` |
|        15 | 4067 | `			if( is_step_negative ){` |
|         3 | 4068 | `				goto negative_step_error;` |
|         - | 4069 | `			}` |
|        13 | 4070 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4071 | `				goto boundary_error;` |
|         - | 4072 | `			}` |
|       163 | 4073 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4074 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4075 | `					return rc;` |
|         - | 4076 | `				}` |
|        77 | 4077 | `			}` |
|         6 | 4078 | `		}else{` |
|         3 | 4079 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4080 | `				return rc;` |
|         - | 4081 | `			}` |
|         - | 4082 | `		}` |
|        15 | 4083 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4084 | `		return PH7_OK;` |
|         - | 4085 | `	}` |
|        53 | 4086 | `handle_numeric_inputs:` |
|       133 | 4087 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4088 | `		/* Float range */` |
|         - | 4089 | `		double elem,calc;` |
|        25 | 4090 | `		if( start_double > end_double ){` |
|         - | 4091 | `			/* Decreasing float range */` |
|         7 | 4092 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4093 | `				goto boundary_error;` |
|         - | 4094 | `			}` |
|         7 | 4095 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4096 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4097 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4098 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4099 | `			}` |
|         5 | 4100 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4101 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4102 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4103 | `					return rc;` |
|         - | 4104 | `				}` |
|         8 | 4105 | `			}` |
|        21 | 4106 | `		}else if( end_double > start_double ){` |
|         - | 4107 | `			/* Increasing float range */` |
|        17 | 4108 | `			if( is_step_negative ){` |
|       ! 0 | 4109 | `				goto negative_step_error;` |
|         - | 4110 | `			}` |
|        17 | 4111 | `			if( end_double - start_double < step_double ){` |
|         3 | 4112 | `				goto boundary_error;` |
|         - | 4113 | `			}` |
|        15 | 4114 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4115 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4116 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4117 | `			}` |
|        11 | 4118 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4119 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4120 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4121 | `					return rc;` |
|         - | 4122 | `				}` |
|        28 | 4123 | `			}` |
|         6 | 4124 | `		}else{` |
|         3 | 4125 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4126 | `				return rc;` |
|         - | 4127 | `			}` |
|         - | 4128 | `		}` |
|         9 | 4129 | `	}else{` |
|         - | 4130 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4131 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4132 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4133 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4134 | `		sxu64 calc;` |
|       101 | 4135 | `		if( start_long > end_long ){` |
|         - | 4136 | `			/* Decreasing int range */` |
|        19 | 4137 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4138 | `				goto boundary_error;` |
|         - | 4139 | `			}` |
|        17 | 4140 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4141 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4142 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4143 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4144 | `			}` |
|        15 | 4145 | `			size = (sxu32)(calc + 1);` |
|       101 | 4146 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4147 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4148 | `					return rc;` |
|         - | 4149 | `				}` |
|        44 | 4150 | `			}` |
|        90 | 4151 | `		}else if( end_long > start_long ){` |
|         - | 4152 | `			/* Increasing int range */` |
|        77 | 4153 | `			if( is_step_negative ){` |
|         3 | 4154 | `				goto negative_step_error;` |
|         - | 4155 | `			}` |
|        75 | 4156 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4157 | `				goto boundary_error;` |
|         - | 4158 | `			}` |
|        73 | 4159 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4160 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4161 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4162 | `			}` |
|        69 | 4163 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4164 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4165 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4166 | `					return rc;` |
|         - | 4167 | `				}` |
|       795 | 4168 | `			}` |
|        35 | 4169 | `		}else{` |
|         7 | 4170 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4171 | `				return rc;` |
|         - | 4172 | `			}` |
|         - | 4173 | `		}` |
|         - | 4174 | `	}` |
|         - | 4175 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4176 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4177 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4178 | `	return PH7_OK;` |
|         2 | 4179 | `negative_step_error:` |
|         5 | 4180 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4181 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4182 | `boundary_error:` |
|         9 | 4183 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4184 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        84 | 4185 | `}` |
|         - | 4186 | `/*` |
|         - | 4187 | ` * array array_values(array $array)` |
|         - | 4188 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4189 | ` * Parameters` |
|         - | 4190 | ` *  $array` |
|         - | 4191 | ` *   The input array.` |
|         - | 4192 | ` * Return` |
|         - | 4193 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4194 | ` */` |
|        48 | 4195 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4196 | `{` |
|         - | 4197 | `	ph7_hashmap_node *pNode;` |
|         - | 4198 | `	ph7_hashmap *pMap;` |
|         - | 4199 | `	ph7_value *pArray;` |
|         - | 4200 | `	ph7_value *pObj;` |
|         - | 4201 | `	sxu32 n;` |
|        52 | 4202 | `	if( nArg != 1 ){` |
|         - | 4203 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4204 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4205 | `			"ArgumentCountError",` |
|         - | 4206 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4207 | `			nArg` |
|         - | 4208 | `			);` |
|         - | 4209 | `	}` |
|         - | 4210 | `	/* Make sure we are dealing with a valid hashmap */` |
|        49 | 4211 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4212 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4213 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4214 | `			"TypeError",` |
|         - | 4215 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4216 | `			ph7_type_name(apArg[0])` |
|         - | 4217 | `			);` |
|         - | 4218 | `	}` |
|         - | 4219 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4220 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4221 | `	/* Create a new array */` |
|        46 | 4222 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4223 | `	if( pArray == 0 ){` |
|       ! 0 | 4224 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4225 | `		return PH7_OK;` |
|         - | 4226 | `	}` |
|         - | 4227 | `	/* Perform the requested operation */` |
|        46 | 4228 | `	pNode = pMap->pFirst;` |
|       144 | 4229 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4230 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4231 | `		if( pObj ){` |
|         - | 4232 | `			/* perform the insertion */` |
|       100 | 4233 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4234 | `		}` |
|         - | 4235 | `		/* Point to the next entry */` |
|       100 | 4236 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4237 | `	}` |
|         - | 4238 | `	/* return the new array */` |
|        46 | 4239 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4240 | `	return PH7_OK;` |
|        28 | 4241 | `}` |
|         - | 4242 | `/*` |
|         - | 4243 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4244 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4245 | ` * Parameters` |
|         - | 4246 | ` *  $input` |
|         - | 4247 | ` *   An array containing keys to return.` |
|         - | 4248 | ` * $search_value` |
|         - | 4249 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4250 | ` * $strict` |
|         - | 4251 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4252 | ` * Return` |
|         - | 4253 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4254 | ` */` |
|       160 | 4255 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4256 | `{` |
|         - | 4257 | `	ph7_hashmap_node *pNode;` |
|         - | 4258 | `	ph7_hashmap *pMap;` |
|         - | 4259 | `	ph7_value *pArray;` |
|         - | 4260 | `	ph7_value sObj;` |
|         - | 4261 | `	ph7_value sVal;` |
|         - | 4262 | `	SyString sKey;` |
|         - | 4263 | `	int bStrict;` |
|         - | 4264 | `	sxi32 rc;` |
|         - | 4265 | `	sxu32 n;` |
|       164 | 4266 | `	if( nArg < 1 ){` |
|         - | 4267 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4268 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4269 | `			"ArgumentCountError",` |
|         - | 4270 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4271 | `			);` |
|         - | 4272 | `	}` |
|         - | 4273 | `	/* Make sure we are dealing with a valid hashmap */` |
|       164 | 4274 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4275 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4276 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4277 | `			"TypeError",` |
|         - | 4278 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4279 | `			ph7_type_name(apArg[0])` |
|         - | 4280 | `			);` |
|         - | 4281 | `	}` |
|         - | 4282 | `	/* Point to the internal representation of the input hashmap */` |
|       161 | 4283 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4284 | `	/* Create a new array */` |
|       161 | 4285 | `	pArray = ph7_context_new_array(pCtx);` |
|       161 | 4286 | `	if( pArray == 0 ){` |
|       ! 0 | 4287 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4288 | `		return PH7_OK;` |
|         - | 4289 | `	}` |
|       161 | 4290 | `	bStrict = FALSE;` |
|       161 | 4291 | `	if( nArg > 2 ){` |
|         - | 4292 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4293 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4294 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4295 | `				"TypeError",` |
|         - | 4296 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4297 | `				ph7_type_name(apArg[2])` |
|         - | 4298 | `				);` |
|         - | 4299 | `		}` |
|         9 | 4300 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4301 | `	}` |
|         - | 4302 | `	/* Perform the requested operation */` |
|       161 | 4303 | `	pNode = pMap->pFirst;` |
|       161 | 4304 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1463 | 4305 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1305 | 4306 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       185 | 4307 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        94 | 4308 | `		}else{` |
|      1122 | 4309 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4310 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4311 | `		}` |
|      1305 | 4312 | `		rc = 0;` |
|      1305 | 4313 | `		if( nArg > 1 ){` |
|        65 | 4314 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4315 | `			if( pValue ){` |
|         - | 4316 | `				ph7_value sNeedle;` |
|        65 | 4317 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4318 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4319 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4320 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4321 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4322 | `				 * corrupt every later comparison. */` |
|        65 | 4323 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4324 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4325 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4326 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4327 | `			}` |
|        32 | 4328 | `		}` |
|      1305 | 4329 | `		if( rc == 0 ){` |
|         - | 4330 | `			/* Perform the insertion */` |
|      1273 | 4331 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       635 | 4332 | `		}` |
|      1305 | 4333 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4334 | `		/* Point to the next entry */` |
|      1305 | 4335 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       654 | 4336 | `	}` |
|         - | 4337 | `	/* return the new array */` |
|       161 | 4338 | `	ph7_result_value(pCtx,pArray);` |
|       161 | 4339 | `	return PH7_OK;` |
|        84 | 4340 | `}` |
|         - | 4341 | `/*` |
|         - | 4342 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4343 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4344 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4345 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4346 | ` * Parameters` |
|         - | 4347 | ` *  $arr1` |
|         - | 4348 | ` *   First array` |
|         - | 4349 | ` *  $arr2` |
|         - | 4350 | ` *   Second array` |
|         - | 4351 | ` * Return` |
|         - | 4352 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4353 | ` * Note` |
|         - | 4354 | ` *  This function is a symisc eXtension.` |
|         - | 4355 | ` */` |
|         4 | 4356 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4357 | `{` |
|         - | 4358 | `	ph7_hashmap *p1,*p2;` |
|         - | 4359 | `	int rc;` |
|         5 | 4360 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4361 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4362 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4363 | `		return PH7_OK;` |
|         - | 4364 | `	}` |
|         - | 4365 | `	/* Point to the hashmaps */` |
|         5 | 4366 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4367 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4368 | `	rc = (p1 == p2);` |
|         - | 4369 | `	/* Same instance? */` |
|         5 | 4370 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4371 | `	return PH7_OK;` |
|         3 | 4372 | `}` |
|         - | 4373 | `/*` |
|         - | 4374 | ` * array array_merge(array ...$arrays)` |
|         - | 4375 | ` *  Merge one or more arrays.` |
|         - | 4376 | ` * Parameters` |
|         - | 4377 | ` *  ...$arrays` |
|         - | 4378 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4379 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4380 | ` * Return` |
|         - | 4381 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4382 | ` *  with no arguments.` |
|         - | 4383 | ` */` |
|      1056 | 4384 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4385 | `{` |
|         - | 4386 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4387 | `	ph7_value *pArray;` |
|         - | 4388 | `	int i;` |
|         - | 4389 | `	/* Create a new array */` |
|      1061 | 4390 | `	pArray = ph7_context_new_array(pCtx);` |
|      1061 | 4391 | `	if( pArray == 0 ){` |
|       ! 0 | 4392 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4393 | `		return PH7_OK;` |
|         - | 4394 | `	}` |
|         - | 4395 | `	/* Point to the internal representation of the hashmap */` |
|      1061 | 4396 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4397 | `	/* Start merging */` |
|      3163 | 4398 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4399 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2111 | 4400 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4401 | `			/* Type mismatch -> TypeError */` |
|         8 | 4402 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4403 | `				"TypeError",` |
|         - | 4404 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4405 | `				i + 1,` |
|         4 | 4406 | `				ph7_type_name(apArg[i])` |
|         - | 4407 | `				);` |
|       ! 0 | 4408 | `		}else{` |
|      2107 | 4409 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4410 | `			/* Merge the two hashmaps */` |
|      2107 | 4411 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4412 | `		}` |
|      1056 | 4413 | `	}` |
|         - | 4414 | `	/* Return the freshly created array */` |
|      1057 | 4415 | `	ph7_result_value(pCtx,pArray);` |
|      1057 | 4416 | `	return PH7_OK;` |
|       533 | 4417 | `}` |
|         - | 4418 | `/*` |
|         - | 4419 | ` * array array_copy(array $source)` |
|         - | 4420 | ` *  Make a blind copy of the target array.` |
|         - | 4421 | ` * Parameters` |
|         - | 4422 | ` *  $source` |
|         - | 4423 | ` *   Target array` |
|         - | 4424 | ` * Return` |
|         - | 4425 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4426 | ` * Note` |
|         - | 4427 | ` *  This function is a symisc eXtension.` |
|         - | 4428 | ` */` |
|        18 | 4429 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4430 | `{` |
|         - | 4431 | `	ph7_hashmap *pMap;` |
|         - | 4432 | `	ph7_value *pArray;` |
|        19 | 4433 | `	if( nArg < 1 ){` |
|         - | 4434 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4435 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4436 | `		return PH7_OK;` |
|         - | 4437 | `	}` |
|         - | 4438 | `	/* Create a new array */` |
|        19 | 4439 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4440 | `	if( pArray == 0 ){` |
|       ! 0 | 4441 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4442 | `		return PH7_OK;` |
|         - | 4443 | `	}` |
|         - | 4444 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4445 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4446 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4447 | `		/* Point to the internal representation of the source */` |
|        19 | 4448 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4449 | `		/* Perform the copy */` |
|        19 | 4450 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4451 | `	}else{` |
|         - | 4452 | `		/* Simple insertion */` |
|       ! 0 | 4453 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4454 | `	}` |
|         - | 4455 | `	/* Return the duplicated array */` |
|        19 | 4456 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4457 | `	return PH7_OK;` |
|        10 | 4458 | `}` |
|         - | 4459 | `/*` |
|         - | 4460 | ` * bool array_erase(array $source)` |
|         - | 4461 | ` *  Remove all elements from a given array.` |
|         - | 4462 | ` * Parameters` |
|         - | 4463 | ` *  $source` |
|         - | 4464 | ` *   Target array` |
|         - | 4465 | ` * Return` |
|         - | 4466 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4467 | ` * Note` |
|         - | 4468 | ` *  This function is a symisc eXtension.` |
|         - | 4469 | ` */` |
|        26 | 4470 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4471 | `{` |
|         - | 4472 | `	ph7_hashmap *pMap;` |
|        28 | 4473 | `	if( nArg < 1 ){` |
|         - | 4474 | `		/* Missing arguments */` |
|       ! 0 | 4475 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4476 | `		return PH7_OK;` |
|         - | 4477 | `	}` |
|         - | 4478 | `	/* Point to the target hashmap */` |
|        28 | 4479 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4480 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4481 | `	/* Erase */` |
|        28 | 4482 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4483 | `	return PH7_OK;` |
|        15 | 4484 | `}` |
|         - | 4485 | `/*` |
|         - | 4486 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4487 | ` *  Extract a slice of the array.` |
|         - | 4488 | ` * Parameters` |
|         - | 4489 | ` *  $array` |
|         - | 4490 | ` *    The input array.` |
|         - | 4491 | ` * $offset` |
|         - | 4492 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4493 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4494 | ` * $length (optional, nullable)` |
|         - | 4495 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4496 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4497 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4498 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4499 | ` * $preserve_keys (optional)` |
|         - | 4500 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4501 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4502 | ` * Return` |
|         - | 4503 | ` *   The new slice.` |
|         - | 4504 | ` */` |
|        46 | 4505 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4506 | `{` |
|         - | 4507 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4508 | `	ph7_hashmap_node *pCur;` |
|         - | 4509 | `	ph7_value *pArray;` |
|         - | 4510 | `	int iLength,iOfft;` |
|         - | 4511 | `	int bPreserve;` |
|         - | 4512 | `	sxi32 rc;` |
|        51 | 4513 | `	if( nArg < 2 ){` |
|       ! 0 | 4514 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4515 | `			"ArgumentCountError",` |
|         - | 4516 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4517 | `			nArg` |
|         - | 4518 | `			);` |
|         - | 4519 | `	}` |
|        51 | 4520 | `	if( nArg > 4 ){` |
|         4 | 4521 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4522 | `			"ArgumentCountError",` |
|         - | 4523 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4524 | `			nArg` |
|         - | 4525 | `			);` |
|         - | 4526 | `	}` |
|        49 | 4527 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4528 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4529 | `			"TypeError",` |
|         - | 4530 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4531 | `			ph7_type_name(apArg[0])` |
|         - | 4532 | `			);` |
|         - | 4533 | `	}` |
|         - | 4534 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4535 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4536 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4537 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4538 | `			"TypeError",` |
|         - | 4539 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4540 | `			ph7_type_name(apArg[1])` |
|         - | 4541 | `			);` |
|         - | 4542 | `	}` |
|         - | 4543 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4544 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4545 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4546 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4547 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4548 | `				"TypeError",` |
|         - | 4549 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4550 | `				ph7_type_name(apArg[2])` |
|         - | 4551 | `				);` |
|         - | 4552 | `		}` |
|         8 | 4553 | `	}` |
|         - | 4554 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4555 | `	if( nArg > 3 ){` |
|         7 | 4556 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4557 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4558 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4559 | `				"TypeError",` |
|         - | 4560 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4561 | `				ph7_type_name(apArg[3])` |
|         - | 4562 | `				);` |
|         - | 4563 | `		}` |
|         2 | 4564 | `	}` |
|         - | 4565 | `	/* Point the internal representation of the target array */` |
|        43 | 4566 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        43 | 4567 | `	bPreserve = FALSE;` |
|         - | 4568 | `	/* Get the offset */` |
|         - | 4569 | `	{` |
|        43 | 4570 | `		sxi64 iTmp = 0;` |
|        43 | 4571 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        43 | 4572 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4573 | `			return rcArg;` |
|         - | 4574 | `		}` |
|        43 | 4575 | `		iOfft = (int)iTmp;` |
|         - | 4576 | `	}` |
|        43 | 4577 | `	if( iOfft < 0 ){` |
|         5 | 4578 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4579 | `		if( iOfft < 0 ){` |
|         3 | 4580 | `			iOfft = 0;` |
|         1 | 4581 | `		}` |
|         2 | 4582 | `	}` |
|        43 | 4583 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4584 | `		/* Offset past end of array, return empty array */` |
|         5 | 4585 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4586 | `		if( pArray == 0 ){` |
|       ! 0 | 4587 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4588 | `			return PH7_OK;` |
|         - | 4589 | `		}` |
|         5 | 4590 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4591 | `		return PH7_OK;` |
|         - | 4592 | `	}` |
|         - | 4593 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        39 | 4594 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        39 | 4595 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        17 | 4596 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        17 | 4597 | `		if( iLength < 0 ){` |
|         5 | 4598 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4599 | `		}` |
|        17 | 4600 | `		if( iLength < 0 ){` |
|         3 | 4601 | `			iLength = 0;` |
|         1 | 4602 | `		}` |
|        17 | 4603 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4604 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4605 | `		}` |
|         8 | 4606 | `	}` |
|        39 | 4607 | `	if( nArg > 3 ){` |
|         5 | 4608 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4609 | `	}` |
|         - | 4610 | `	/* Create a new array */` |
|        39 | 4611 | `	pArray = ph7_context_new_array(pCtx);` |
|        39 | 4612 | `	if( pArray == 0 ){` |
|       ! 0 | 4613 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4614 | `		return PH7_OK;` |
|         - | 4615 | `	}` |
|        39 | 4616 | `	if( iLength < 1 ){` |
|         - | 4617 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4618 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4619 | `		return PH7_OK;` |
|         - | 4620 | `	}` |
|         - | 4621 | `	/* Point to the desired entry */` |
|        35 | 4622 | `	pCur = pSrc->pFirst;` |
|        29 | 4623 | `	for(;;){` |
|        63 | 4624 | `		if( iOfft < 1 ){` |
|        35 | 4625 | `			break;` |
|         - | 4626 | `		}` |
|         - | 4627 | `		/* Point to the next entry */` |
|        33 | 4628 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4629 | `		iOfft--;` |
|         5 | 4630 | `	}` |
|         - | 4631 | `	/* Point to the internal representation of the hashmap */` |
|        35 | 4632 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        54 | 4633 | `	for(;;){` |
|       113 | 4634 | `		if( iLength < 1 ){` |
|        35 | 4635 | `			break;` |
|         - | 4636 | `		}` |
|         - | 4637 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4638 | `		{` |
|        83 | 4639 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        83 | 4640 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4641 | `		}` |
|        83 | 4642 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4643 | `			break;` |
|         - | 4644 | `		}` |
|         - | 4645 | `		/* Point to the next entry */` |
|        83 | 4646 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        83 | 4647 | `		iLength--;` |
|         5 | 4648 | `	}` |
|         - | 4649 | `	/* Return the freshly created array */` |
|        35 | 4650 | `	ph7_result_value(pCtx,pArray);` |
|        35 | 4651 | `	return PH7_OK;` |
|        28 | 4652 | `}` |
|         - | 4653 | `/*` |
|         - | 4654 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4655 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4656 | ` * beginning (becomes the new pFirst).` |
|         - | 4657 | ` */` |
|        38 | 4658 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4659 | `{` |
|         - | 4660 | `	ph7_hashmap_node *pNode;` |
|         - | 4661 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4662 | `	pNode = pMap->pLast;` |
|        39 | 4663 | `	if( pNode == 0 ){` |
|       ! 0 | 4664 | `		return;` |
|         - | 4665 | `	}` |
|        39 | 4666 | `	if( pNode->pNext == 0 ){` |
|         - | 4667 | `		/* Only node in the list, nothing to move */` |
|         5 | 4668 | `		return;` |
|         - | 4669 | `	}` |
|        35 | 4670 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4671 | `		/* Already in the correct position */` |
|         9 | 4672 | `		return;` |
|         - | 4673 | `	}` |
|         - | 4674 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4675 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4676 | `	pMap->pLast->pPrev = 0;` |
|         - | 4677 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4678 | `	if( pAfter == 0 ){` |
|         - | 4679 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4680 | `		pNode->pNext = 0;` |
|         3 | 4681 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4682 | `		if( pMap->pFirst ){` |
|         3 | 4683 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4684 | `		}` |
|         3 | 4685 | `		pMap->pFirst = pNode;` |
|         2 | 4686 | `	}else{` |
|        25 | 4687 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4688 | `		pNode->pPrev = pOldNext;` |
|        25 | 4689 | `		pNode->pNext = pAfter;` |
|        25 | 4690 | `		pAfter->pPrev = pNode;` |
|        25 | 4691 | `		if( pOldNext ){` |
|        25 | 4692 | `			pOldNext->pNext = pNode;` |
|        13 | 4693 | `		}else{` |
|       ! 0 | 4694 | `			pMap->pLast = pNode;` |
|         - | 4695 | `		}` |
|         - | 4696 | `	}` |
|        20 | 4697 | `}` |
|         - | 4698 | `/*` |
|         - | 4699 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4700 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4701 | ` * Parameters` |
|         - | 4702 | ` *  $array` |
|         - | 4703 | ` *    The input array.` |
|         - | 4704 | ` *  $offset` |
|         - | 4705 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4706 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4707 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4708 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4709 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4710 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4711 | ` *  $length (optional)` |
|         - | 4712 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4713 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4714 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4715 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4716 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4717 | ` *  $replacement (optional)` |
|         - | 4718 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4719 | ` *    with elements from this array.` |
|         - | 4720 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4721 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4722 | ` *    offset.` |
|         - | 4723 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4724 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4725 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4726 | ` * Return` |
|         - | 4727 | ` *   A new array consisting of the extracted elements.` |
|         - | 4728 | ` */` |
|        64 | 4729 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4730 | `{` |
|         - | 4731 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4732 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4733 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4734 | `	int iLength,iOfft,i;` |
|         - | 4735 | `	sxi32 rc;` |
|        66 | 4736 | `	if( nArg < 2 ){` |
|       ! 0 | 4737 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4738 | `			"ArgumentCountError",` |
|         - | 4739 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4740 | `			nArg` |
|         - | 4741 | `			);` |
|         - | 4742 | `	}` |
|        66 | 4743 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4744 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4745 | `			"TypeError",` |
|         - | 4746 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4747 | `			ph7_type_name(apArg[0])` |
|         - | 4748 | `			);` |
|         - | 4749 | `	}` |
|         - | 4750 | `	/* Point to the internal representation of the target array */` |
|        63 | 4751 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4752 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4753 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4754 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4755 | `	if( iOfft < 0 ){` |
|         9 | 4756 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4757 | `		if( iOfft < 0 ){` |
|         3 | 4758 | `			iOfft = 0;` |
|         2 | 4759 | `		}` |
|        59 | 4760 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4761 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4762 | `	}` |
|         - | 4763 | `	/* Get the length and clamp to valid range.` |
|         - | 4764 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4765 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4766 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4767 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4768 | `		if( iLength < 0 ){` |
|         7 | 4769 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4770 | `			if( iLength < 0 ){` |
|         3 | 4771 | `				iLength = 0;` |
|         1 | 4772 | `			}` |
|         3 | 4773 | `		}` |
|        45 | 4774 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4775 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4776 | `		}` |
|        22 | 4777 | `	}` |
|         - | 4778 | `	/* Create the result array for removed elements */` |
|        63 | 4779 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4780 | `	if( pArray == 0 ){` |
|       ! 0 | 4781 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4782 | `		return PH7_OK;` |
|         - | 4783 | `	}` |
|         - | 4784 | `	/* Get replacement array if provided */` |
|        63 | 4785 | `	pRep = 0;` |
|        63 | 4786 | `	if( nArg > 3 ){` |
|        27 | 4787 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4788 | `			/* Perform an array cast */` |
|         3 | 4789 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4790 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4791 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4792 | `			}` |
|         2 | 4793 | `		}else{` |
|        25 | 4794 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4795 | `		}` |
|        27 | 4796 | `		if( pRep ){` |
|         - | 4797 | `			/* Reset the loop cursor */` |
|        27 | 4798 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4799 | `		}` |
|        13 | 4800 | `	}` |
|         - | 4801 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4802 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4803 | `	/* Navigate to the offset position */` |
|        63 | 4804 | `	pCur = pSrc->pFirst;` |
|       131 | 4805 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4806 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4807 | `	}` |
|         - | 4808 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4809 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4810 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4811 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4812 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4813 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4814 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4815 | `		pPrev = pCur->pPrev;` |
|        79 | 4816 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4817 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4818 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4819 | `			break;` |
|         - | 4820 | `		}` |
|        79 | 4821 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4822 | `	}` |
|         - | 4823 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4824 | `	if( pRep ){` |
|         - | 4825 | `		ph7_value sSafeVal;` |
|        78 | 4826 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4827 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4828 | `			if( pRvalue ){` |
|         - | 4829 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4830 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4831 | `				 * since it points into that same pool. */` |
|        39 | 4832 | `				sSafeVal = *pRvalue;` |
|        39 | 4833 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4834 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4835 | `					pNewNode = pSrc->pLast;` |
|        39 | 4836 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4837 | `					pInsertAfter = pNewNode;` |
|        19 | 4838 | `				}` |
|        19 | 4839 | `			}` |
|         1 | 4840 | `		}` |
|        13 | 4841 | `	}` |
|         - | 4842 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4843 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4844 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4845 | `	 * and removals left gaps. */` |
|         - | 4846 | `	{` |
|        63 | 4847 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4848 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4849 | `		pSrc->iNextIdx = 0;` |
|       233 | 4850 | `		while( n > 0 ){` |
|       171 | 4851 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4852 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4853 | `			}` |
|       171 | 4854 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4855 | `			n--;` |
|         1 | 4856 | `		}` |
|        63 | 4857 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4858 | `	}` |
|         - | 4859 | `	/* Return the freshly created array */` |
|        63 | 4860 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4861 | `	return PH7_OK;` |
|        34 | 4862 | `}` |
|         - | 4863 | `/*` |
|         - | 4864 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4865 | ` *  Checks if a value exists in an array.` |
|         - | 4866 | ` * Parameters` |
|         - | 4867 | ` *  $needle` |
|         - | 4868 | ` *   The searched value.` |
|         - | 4869 | ` *   Note:` |
|         - | 4870 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4871 | ` * $haystack` |
|         - | 4872 | ` *  The target array.` |
|         - | 4873 | ` * $strict` |
|         - | 4874 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4875 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4876 | ` */` |
|     32804 | 4877 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4878 | `{` |
|         - | 4879 | `	ph7_value *pNeedle;` |
|         - | 4880 | `	int bStrict;` |
|         - | 4881 | `	int rc;` |
|     32809 | 4882 | `	if( nArg < 2 ){` |
|         - | 4883 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4884 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4885 | `		return PH7_OK;` |
|         - | 4886 | `	}` |
|     32809 | 4887 | `	pNeedle = apArg[0];` |
|     32809 | 4888 | `	bStrict = 0;` |
|     32809 | 4889 | `	if( nArg > 2 ){` |
|        53 | 4890 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4891 | `	}` |
|     32809 | 4892 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4893 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4894 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4895 | `		/* Set the comparison result */` |
|       ! 0 | 4896 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4897 | `		return PH7_OK;` |
|         - | 4898 | `	}` |
|         - | 4899 | `	/* Perform the lookup */` |
|     32809 | 4900 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4901 | `	/* Lookup result */` |
|     32809 | 4902 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32809 | 4903 | `	return PH7_OK;` |
|     16407 | 4904 | `}` |
|         - | 4905 | `/*` |
|         - | 4906 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4907 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4908 | ` * Parameters` |
|         - | 4909 | ` * $needle` |
|         - | 4910 | ` *   The searched value.` |
|         - | 4911 | ` * $haystack` |
|         - | 4912 | ` *   The array.` |
|         - | 4913 | ` * $strict` |
|         - | 4914 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4915 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4916 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4917 | ` * Return` |
|         - | 4918 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4919 | ` */` |
|        26 | 4920 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4921 | `{` |
|         - | 4922 | `	ph7_hashmap_node *pEntry;` |
|         - | 4923 | `	ph7_value *pVal,sNeedle;` |
|         - | 4924 | `	ph7_hashmap *pMap;` |
|         - | 4925 | `	ph7_value sVal;` |
|         - | 4926 | `	int bStrict;` |
|         - | 4927 | `	sxu32 n;` |
|         - | 4928 | `	int rc;` |
|        28 | 4929 | `	if( nArg < 2 ){` |
|         - | 4930 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4931 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4932 | `			"ArgumentCountError",` |
|         - | 4933 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 4934 | `			nArg` |
|         - | 4935 | `			);` |
|         - | 4936 | `	}` |
|        28 | 4937 | `	bStrict = FALSE;` |
|        28 | 4938 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4939 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4940 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4941 | `			"TypeError",` |
|         - | 4942 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4943 | `			ph7_type_name(apArg[1])` |
|         - | 4944 | `			);` |
|         - | 4945 | `	}` |
|        25 | 4946 | `	if( nArg > 2 ){` |
|         - | 4947 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        11 | 4948 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4949 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4950 | `				"TypeError",` |
|         - | 4951 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4952 | `				ph7_type_name(apArg[2])` |
|         - | 4953 | `				);` |
|         - | 4954 | `		}` |
|        11 | 4955 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4956 | `	}` |
|         - | 4957 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4958 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4959 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4960 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4961 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4962 | `	pEntry = pMap->pFirst;` |
|        25 | 4963 | `	n = pMap->nEntry;` |
|        28 | 4964 | `	for(;;){` |
|        57 | 4965 | `		if( !n ){` |
|         9 | 4966 | `			break;` |
|         - | 4967 | `		}` |
|         - | 4968 | `		/* Extract node value */` |
|        49 | 4969 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4970 | `		if( pVal ){` |
|         - | 4971 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4972 | `			 * can change their type.` |
|         - | 4973 | `			 */` |
|        49 | 4974 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4975 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4976 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4977 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4978 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4979 | `			if( rc == 0 ){` |
|         - | 4980 | `				/* Match found,return key */` |
|        17 | 4981 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4982 | `					/* INT key */` |
|        11 | 4983 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4984 | `				}else{` |
|         7 | 4985 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4986 | `					/* Blob key */` |
|         7 | 4987 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4988 | `				}` |
|        17 | 4989 | `				return PH7_OK;` |
|         - | 4990 | `			}` |
|        16 | 4991 | `		}` |
|         - | 4992 | `		/* Point to the next entry */` |
|        33 | 4993 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4994 | `		n--;` |
|         1 | 4995 | `	}` |
|         - | 4996 | `	/* No such value,return FALSE */` |
|         9 | 4997 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4998 | `	return PH7_OK;` |
|        15 | 4999 | `}` |
|         - | 5000 | `/*` |
|         - | 5001 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 5002 | ` *  Computes the difference of arrays.` |
|         - | 5003 | ` * Parameters` |
|         - | 5004 | ` *  $array1` |
|         - | 5005 | ` *    The array to compare from` |
|         - | 5006 | ` *  $array2` |
|         - | 5007 | ` *    An array to compare against` |
|         - | 5008 | ` *  $...` |
|         - | 5009 | ` *   More arrays to compare against` |
|         - | 5010 | ` * Return` |
|         - | 5011 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5012 | ` *  are not present in any of the other arrays.` |
|         - | 5013 | ` */` |
|        20 | 5014 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5015 | `{` |
|         - | 5016 | `	ph7_hashmap_node *pEntry;` |
|         - | 5017 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5018 | `	ph7_value *pArray;` |
|         - | 5019 | `	ph7_value *pVal;` |
|         - | 5020 | `	sxi32 rc;` |
|         - | 5021 | `	sxu32 n;` |
|         - | 5022 | `	int i;` |
|         - | 5023 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 5024 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 5025 | `	 * debugging difficult. */` |
|        23 | 5026 | `	if( nArg < 1 ){` |
|       ! 0 | 5027 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5028 | `			"ArgumentCountError",` |
|         - | 5029 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5030 | `			nArg` |
|         - | 5031 | `			);` |
|         - | 5032 | `	}` |
|        23 | 5033 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5034 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5035 | `			"TypeError",` |
|         - | 5036 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5037 | `			ph7_type_name(apArg[0])` |
|         - | 5038 | `			);` |
|         - | 5039 | `	}` |
|        36 | 5040 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5041 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5042 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5043 | `				"TypeError",` |
|         - | 5044 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5045 | `				i + 1,` |
|         2 | 5046 | `				ph7_type_name(apArg[i])` |
|         - | 5047 | `				);` |
|         - | 5048 | `		}` |
|         9 | 5049 | `	}` |
|        17 | 5050 | `	if( nArg == 1 ){` |
|         - | 5051 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5052 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5053 | `		return PH7_OK;` |
|         - | 5054 | `	}` |
|         - | 5055 | `	/* Create a new array */` |
|        15 | 5056 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5057 | `	if( pArray == 0 ){` |
|       ! 0 | 5058 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5059 | `		return PH7_OK;` |
|         - | 5060 | `	}` |
|         - | 5061 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5062 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5063 | `	/* Perform the diff */` |
|        15 | 5064 | `	pEntry = pSrc->pFirst;` |
|        15 | 5065 | `	n = pSrc->nEntry;` |
|        27 | 5066 | `	for(;;){` |
|        55 | 5067 | `		if( n < 1 ){` |
|        15 | 5068 | `			break;` |
|         - | 5069 | `		}` |
|         - | 5070 | `		/* Extract the node value */` |
|        41 | 5071 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5072 | `		if( pVal ){` |
|        69 | 5073 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5074 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5075 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5076 | `				/* Perform the lookup */` |
|        45 | 5077 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5078 | `				if( rc == SXRET_OK ){` |
|         - | 5079 | `					/* Value exist */` |
|        17 | 5080 | `					break;` |
|         - | 5081 | `				}` |
|        15 | 5082 | `			}` |
|        41 | 5083 | `			if( i >= nArg ){` |
|         - | 5084 | `				/* Perform the insertion */` |
|        25 | 5085 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5086 | `			}` |
|        20 | 5087 | `		}` |
|         - | 5088 | `		/* Point to the next entry */` |
|        41 | 5089 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5090 | `		n--;` |
|         1 | 5091 | `	}` |
|         - | 5092 | `	/* Return the freshly created array */` |
|        15 | 5093 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5094 | `	return PH7_OK;` |
|        13 | 5095 | `}` |
|         - | 5096 | `/*` |
|         - | 5097 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5098 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5099 | ` * Parameters` |
|         - | 5100 | ` *  $array1` |
|         - | 5101 | ` *    The array to compare from` |
|         - | 5102 | ` *  $array2` |
|         - | 5103 | ` *    An array to compare against` |
|         - | 5104 | ` *  $...` |
|         - | 5105 | ` *   More arrays to compare against.` |
|         - | 5106 | ` * $callback` |
|         - | 5107 | ` *  The callback comparison function.` |
|         - | 5108 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5109 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5110 | ` *  than the second.` |
|         - | 5111 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5112 | ` * Return` |
|         - | 5113 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5114 | ` *  are not present in any of the other arrays.` |
|         - | 5115 | ` */` |
|        20 | 5116 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5117 | `{` |
|         - | 5118 | `	ph7_hashmap_node *pEntry;` |
|         - | 5119 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5120 | `	ph7_value *pCallback;` |
|         - | 5121 | `	ph7_value *pArray;` |
|         - | 5122 | `	ph7_value *pVal;` |
|         - | 5123 | `	sxi32 rc;` |
|         - | 5124 | `	sxu32 n;` |
|         - | 5125 | `	int i;` |
|         - | 5126 |  |
|         - | 5127 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5128 | `	if( nArg < 2 ){` |
|       ! 0 | 5129 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5130 | `			"ArgumentCountError",` |
|         - | 5131 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5132 | `			nArg` |
|         - | 5133 | `			);` |
|         - | 5134 | `	}` |
|        25 | 5135 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5136 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5137 | `			"TypeError",` |
|         - | 5138 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5139 | `			ph7_type_name(apArg[0])` |
|         - | 5140 | `			);` |
|         - | 5141 | `	}` |
|         - | 5142 |  |
|        23 | 5143 | `	if( nArg == 2 ){` |
|         - | 5144 | `		/* Only the original array and the callback were provided. */` |
|         - | 5145 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5146 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5147 | `		 * validation order.` |
|         - | 5148 | `		 */` |
|         4 | 5149 | `	} else {` |
|         - | 5150 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5151 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5152 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5153 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5154 | `					"TypeError",` |
|         - | 5155 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5156 | `					i + 1,` |
|         6 | 5157 | `					ph7_type_name(apArg[i])` |
|         - | 5158 | `					);` |
|         - | 5159 | `			}` |
|         7 | 5160 | `		}` |
|         - | 5161 | `	}` |
|         - | 5162 |  |
|         - | 5163 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5164 | `	pCallback = apArg[nArg - 1];` |
|         - | 5165 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5166 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5167 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5168 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5169 | `				"TypeError",` |
|         - | 5170 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5171 | `				nArg` |
|         - | 5172 | `				);` |
|         - | 5173 | `		}` |
|         6 | 5174 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5175 | `			int len;` |
|         3 | 5176 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5177 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5178 | `				"TypeError",` |
|         - | 5179 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5180 | `				nArg,` |
|         1 | 5181 | `				zName` |
|         - | 5182 | `				);` |
|         - | 5183 | `		}` |
|         4 | 5184 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5185 | `			"TypeError",` |
|         - | 5186 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5187 | `			nArg` |
|         - | 5188 | `			);` |
|         - | 5189 | `	}` |
|         - | 5190 |  |
|         7 | 5191 | `	if( nArg == 2 ){` |
|         - | 5192 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5193 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5194 | `		return PH7_OK;` |
|         - | 5195 | `	}` |
|         - | 5196 |  |
|         - | 5197 | `	/* Create a new array */` |
|         5 | 5198 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5199 | `	if( pArray == 0 ){` |
|       ! 0 | 5200 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5201 | `		return PH7_OK;` |
|         - | 5202 | `	}` |
|         - | 5203 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5204 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5205 | `	/* Perform the diff */` |
|         5 | 5206 | `	pEntry = pSrc->pFirst;` |
|         5 | 5207 | `	n = pSrc->nEntry;` |
|         5 | 5208 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5209 | `	for(;;){` |
|        11 | 5210 | `		if( n < 1 ){` |
|         3 | 5211 | `			break;` |
|         - | 5212 | `		}` |
|         - | 5213 | `		/* Extract the node value */` |
|         9 | 5214 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5215 | `		if( pVal ){` |
|        15 | 5216 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5217 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5218 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5219 | `				/* Perform the lookup */` |
|         9 | 5220 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5221 | `				if( rc == SXRET_OK ){` |
|         - | 5222 | `					/* Value exist */` |
|         3 | 5223 | `					break;` |
|         - | 5224 | `				}` |
|         4 | 5225 | `			}` |
|         9 | 5226 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5227 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5228 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5229 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5230 | `				return PH7_EXCEPTION;` |
|         - | 5231 | `			}` |
|         7 | 5232 | `			if( i >= (nArg - 1)){` |
|         - | 5233 | `				/* Perform the insertion */` |
|         5 | 5234 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5235 | `			}` |
|         3 | 5236 | `		}` |
|         - | 5237 | `		/* Point to the next entry */` |
|         7 | 5238 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5239 | `		n--;` |
|         1 | 5240 | `	}` |
|         - | 5241 | `	/* Return the freshly created array */` |
|         3 | 5242 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5243 | `	return PH7_OK;` |
|        15 | 5244 | `}` |
|         - | 5245 | `/*` |
|         - | 5246 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5247 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5248 | ` * Parameters` |
|         - | 5249 | ` *  $array1` |
|         - | 5250 | ` *    The array to compare from` |
|         - | 5251 | ` *  $array2` |
|         - | 5252 | ` *    An array to compare against` |
|         - | 5253 | ` *  $...` |
|         - | 5254 | ` *   More arrays to compare against` |
|         - | 5255 | ` * Return` |
|         - | 5256 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5257 | ` *  are not present in any of the other arrays.` |
|         - | 5258 | ` */` |
|        20 | 5259 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5260 | `{` |
|         - | 5261 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5262 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5263 | `	ph7_value *pArray;` |
|         - | 5264 | `	ph7_value *pVal;` |
|         - | 5265 | `	sxi32 rc;` |
|         - | 5266 | `	sxu32 n;` |
|         - | 5267 | `	int i;` |
|         - | 5268 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5269 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5270 | `	 * accompanying integration tests to pass. */` |
|        24 | 5271 | `	if( nArg < 1 ){` |
|       ! 0 | 5272 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5273 | `			"ArgumentCountError",` |
|         - | 5274 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5275 | `			nArg` |
|         - | 5276 | `			);` |
|         - | 5277 | `	}` |
|        24 | 5278 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5279 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5280 | `			"TypeError",` |
|         - | 5281 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5282 | `			ph7_type_name(apArg[0])` |
|         - | 5283 | `			);` |
|         - | 5284 | `	}` |
|        37 | 5285 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5286 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5287 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5288 | `				"TypeError",` |
|         - | 5289 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5290 | `				i + 1,` |
|         4 | 5291 | `				ph7_type_name(apArg[i])` |
|         - | 5292 | `				);` |
|         - | 5293 | `		}` |
|        10 | 5294 | `	}` |
|        15 | 5295 | `	if( nArg == 1 ){` |
|         - | 5296 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5297 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5298 | `		return PH7_OK;` |
|         - | 5299 | `	}` |
|         - | 5300 | `	/* Create a new array */` |
|        13 | 5301 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5302 | `	if( pArray == 0 ){` |
|       ! 0 | 5303 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5304 | `		return PH7_OK;` |
|         - | 5305 | `	}` |
|         - | 5306 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5307 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5308 | `	/* Perform the diff */` |
|        13 | 5309 | `	pEntry = pSrc->pFirst;` |
|        13 | 5310 | `	n = pSrc->nEntry;` |
|        13 | 5311 | `	pN1 = pN2 = 0;` |
|        34 | 5312 | `	for(;;){` |
|         - | 5313 | `		int keep;` |
|        41 | 5314 | `		if( n < 1 ){` |
|        13 | 5315 | `			break;` |
|         - | 5316 | `		}` |
|         - | 5317 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5318 | `		keep = 1;` |
|        47 | 5319 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5320 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5321 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5322 | `			/* Perform a key lookup first */` |
|        33 | 5323 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5324 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5325 | `			}else{` |
|        21 | 5326 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5327 | `			}` |
|        33 | 5328 | `			if( rc != SXRET_OK ){` |
|         - | 5329 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5330 | `				continue;` |
|         - | 5331 | `			}` |
|         - | 5332 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5333 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5334 | `			if( pVal ){` |
|         - | 5335 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5336 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5337 | `				if( pVal2 ){` |
|         - | 5338 | `					ph7_value sV1,sV2;` |
|         - | 5339 | `					sxi32 cmp;` |
|         - | 5340 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5341 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5342 | `					 * null element used to come back bool(false) in the` |
|         - | 5343 | `					 * caller's array). */` |
|        17 | 5344 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5345 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5346 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5347 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5348 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5349 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5350 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5351 | `					if( cmp == 0 ){` |
|         - | 5352 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5353 | `						keep = 0;` |
|        15 | 5354 | `						break;` |
|         - | 5355 | `					}` |
|         1 | 5356 | `				}` |
|         1 | 5357 | `			}` |
|         2 | 5358 | `		}` |
|        29 | 5359 | `		if( keep ){` |
|         - | 5360 | `			/* Perform the insertion */` |
|        15 | 5361 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5362 | `		}` |
|         - | 5363 | `		/* Point to the next entry */` |
|        29 | 5364 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5365 | `		n--;` |
|         1 | 5366 | `	}` |
|         - | 5367 | `	/* Return the freshly created array */` |
|        13 | 5368 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5369 | `	return PH7_OK;` |
|        14 | 5370 | `}` |
|         - | 5371 | `/*` |
|         - | 5372 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5373 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5374 | ` *  by a user supplied callback function.` |
|         - | 5375 | ` * Parameters` |
|         - | 5376 | ` *  $array1` |
|         - | 5377 | ` *    The array to compare from` |
|         - | 5378 | ` *  $array2` |
|         - | 5379 | ` *    An array to compare against` |
|         - | 5380 | ` *  $...` |
|         - | 5381 | ` *   More arrays to compare against.` |
|         - | 5382 | ` *  $key_compare_func` |
|         - | 5383 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5384 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5385 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5386 | ` * Return` |
|         - | 5387 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5388 | ` *  are not present in any of the other arrays.` |
|         - | 5389 | ` */` |
|        22 | 5390 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5391 | `{` |
|         - | 5392 | `	ph7_hashmap_node *pEntry;` |
|         - | 5393 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5394 | `	ph7_value *pCallback;` |
|         - | 5395 | `	ph7_value *pArray;` |
|         - | 5396 | `	sxi32 rc;` |
|         - | 5397 | `	sxu32 n;` |
|         - | 5398 | `	int i;` |
|         - | 5399 |  |
|         - | 5400 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5401 | `	if( nArg < 2 ){` |
|       ! 0 | 5402 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5403 | `			"ArgumentCountError",` |
|         - | 5404 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5405 | `			nArg` |
|         - | 5406 | `			);` |
|         - | 5407 | `	}` |
|        26 | 5408 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5409 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5410 | `			"TypeError",` |
|         - | 5411 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5412 | `			ph7_type_name(apArg[0])` |
|         - | 5413 | `			);` |
|         - | 5414 | `	}` |
|         - | 5415 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5416 | `	 * expected to be a callback. */` |
|        38 | 5417 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5418 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5419 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5420 | `				"TypeError",` |
|         - | 5421 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5422 | `				i + 1,` |
|         2 | 5423 | `				ph7_type_name(apArg[i])` |
|         - | 5424 | `				);` |
|         - | 5425 | `		}` |
|         9 | 5426 | `	}` |
|         - | 5427 | `	/* Point to the callback value */` |
|        22 | 5428 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5429 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5430 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5431 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5432 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5433 | `		 * string given" which we also reproduce. */` |
|         9 | 5434 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5435 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5436 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5437 | `				"TypeError",` |
|         - | 5438 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5439 | `				nArg` |
|         - | 5440 | `				);` |
|         - | 5441 | `		}` |
|         6 | 5442 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5443 | `			/* neither array nor string */` |
|         8 | 5444 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5445 | `				"TypeError",` |
|         - | 5446 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5447 | `				nArg` |
|         - | 5448 | `				);` |
|         - | 5449 | `		}` |
|         - | 5450 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5451 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5452 | `			"TypeError",` |
|         - | 5453 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5454 | `			nArg,` |
|       ! 0 | 5455 | `			ph7_type_name(pCallback)` |
|         - | 5456 | `			);` |
|         - | 5457 | `	}` |
|        13 | 5458 | `	if( nArg == 2 ){` |
|         - | 5459 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5460 | `		 * input array. */` |
|         3 | 5461 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5462 | `		return PH7_OK;` |
|         - | 5463 | `	}` |
|         - | 5464 | `	/* Create a new array */` |
|        11 | 5465 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5466 | `	if( pArray == 0 ){` |
|       ! 0 | 5467 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5468 | `		return PH7_OK;` |
|         - | 5469 | `	}` |
|         - | 5470 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5471 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5472 | `	/* Perform the diff */` |
|        11 | 5473 | `	pEntry = pSrc->pFirst;` |
|        11 | 5474 | `	n = pSrc->nEntry;` |
|        21 | 5475 | `	for(;;){` |
|         - | 5476 | `		int keep;` |
|        27 | 5477 | `		if( n < 1 ){` |
|         9 | 5478 | `			break;` |
|         - | 5479 | `		}` |
|        19 | 5480 | `		keep = 1;` |
|        31 | 5481 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5482 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5483 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5484 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5485 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5486 | `			while( pIt ){` |
|         - | 5487 | `				/* build temporary key values for callback */` |
|         - | 5488 | `				ph7_value key1, key2, result;` |
|         - | 5489 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5490 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5491 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5492 | `				}else{` |
|         - | 5493 | `					SyString sStr;` |
|        33 | 5494 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5495 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5496 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5497 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5498 | `				}` |
|        33 | 5499 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5500 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5501 | `				}else{` |
|         - | 5502 | `					SyString sStr;` |
|        33 | 5503 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5504 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5505 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5506 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5507 | `				}` |
|        33 | 5508 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5509 | `				/* call user callback with (key1, key2) */` |
|         - | 5510 | `				{` |
|         - | 5511 | `					ph7_value *apK[2];` |
|        33 | 5512 | `					apK[0] = &key1;` |
|        33 | 5513 | `					apK[1] = &key2;` |
|        33 | 5514 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5515 | `				}` |
|        33 | 5516 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5517 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5518 | `					 * array_uintersect (which signal back from` |
|         - | 5519 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5520 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5521 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5522 | `					PH7_MemObjRelease(&result);` |
|         3 | 5523 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5524 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5525 | `					return PH7_EXCEPTION;` |
|         - | 5526 | `				}` |
|        31 | 5527 | `				if( rc == SXRET_OK ){` |
|        31 | 5528 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5529 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5530 | `					}` |
|        31 | 5531 | `					if( result.x.iVal == 0 ){` |
|         - | 5532 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5533 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5534 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5535 | `						if( pVal1 && pVal2 ){` |
|         - | 5536 | `							ph7_value sV1,sV2;` |
|         - | 5537 | `							sxi32 cmp;` |
|         - | 5538 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5539 | `							 * place and these are LIVE array elements. */` |
|        13 | 5540 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5541 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5542 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5543 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5544 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5545 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5546 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5547 | `							if( cmp == 0 ){` |
|         9 | 5548 | `								keep = 0;` |
|         9 | 5549 | `								PH7_MemObjRelease(&result);` |
|         - | 5550 | `								/* release keys too before breaking */` |
|         9 | 5551 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5552 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5553 | `								break;` |
|         - | 5554 | `							}` |
|         2 | 5555 | `						}` |
|         2 | 5556 | `					}` |
|        11 | 5557 | `				}` |
|        23 | 5558 | `				PH7_MemObjRelease(&result);` |
|        23 | 5559 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5560 | `				PH7_MemObjRelease(&key2);` |
|         - | 5561 | `				/* move to next node */` |
|        23 | 5562 | `				pIt = pIt->pPrev;` |
|        23 | 5563 | `				if( keep == 0 ) break;` |
|         1 | 5564 | `			}` |
|        21 | 5565 | `			if( keep == 0 ) break;` |
|         7 | 5566 | `		}` |
|        17 | 5567 | `		if( keep ){` |
|         - | 5568 | `			/* Perform the insertion */` |
|         9 | 5569 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5570 | `		}` |
|         - | 5571 | `		/* Point to the next entry */` |
|        17 | 5572 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5573 | `		n--;` |
|         1 | 5574 | `	}` |
|         - | 5575 | `	/* Return the freshly created array */` |
|         9 | 5576 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5577 | `	return PH7_OK;` |
|        15 | 5578 | `}` |
|         - | 5579 | `/*` |
|         - | 5580 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5581 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5582 | ` * Parameters` |
|         - | 5583 | ` *  $array1` |
|         - | 5584 | ` *    The array to compare from` |
|         - | 5585 | ` *  $array2` |
|         - | 5586 | ` *    An array to compare against` |
|         - | 5587 | ` *  $...` |
|         - | 5588 | ` *   More arrays to compare against` |
|         - | 5589 | ` * Return` |
|         - | 5590 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5591 | ` *  in any of the other arrays.` |
|         - | 5592 | ` * Note that NULL is returned on failure.` |
|         - | 5593 | ` */` |
|        12 | 5594 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5595 | `{` |
|         - | 5596 | `	ph7_hashmap_node *pEntry;` |
|         - | 5597 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5598 | `	ph7_value *pArray;` |
|         - | 5599 | `	sxi32 rc;` |
|         - | 5600 | `	sxu32 n;` |
|         - | 5601 | `	int i;` |
|         - | 5602 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5603 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5604 | `	 * helpers. */` |
|        15 | 5605 | `	if( nArg < 1 ){` |
|       ! 0 | 5606 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5607 | `			"ArgumentCountError",` |
|         - | 5608 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5609 | `			nArg` |
|         - | 5610 | `			);` |
|         - | 5611 | `	}` |
|        15 | 5612 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5613 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5614 | `			"TypeError",` |
|         - | 5615 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5616 | `			ph7_type_name(apArg[0])` |
|         - | 5617 | `			);` |
|         - | 5618 | `	}` |
|        20 | 5619 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5620 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5621 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5622 | `				"TypeError",` |
|         - | 5623 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5624 | `				i + 1,` |
|         2 | 5625 | `				ph7_type_name(apArg[i])` |
|         - | 5626 | `				);` |
|         - | 5627 | `		}` |
|         5 | 5628 | `	}` |
|         9 | 5629 | `	if( nArg == 1 ){` |
|         - | 5630 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5631 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5632 | `		return PH7_OK;` |
|         - | 5633 | `	}` |
|         - | 5634 | `	/* Create a new array */` |
|         7 | 5635 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5636 | `	if( pArray == 0 ){` |
|       ! 0 | 5637 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5638 | `		return PH7_OK;` |
|         - | 5639 | `	}` |
|         - | 5640 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5641 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5642 | `	/* Perfrom the diff */` |
|         7 | 5643 | `	pEntry = pSrc->pFirst;` |
|         7 | 5644 | `	n = pSrc->nEntry;` |
|        12 | 5645 | `	for(;;){` |
|        25 | 5646 | `		if( n < 1 ){` |
|         7 | 5647 | `			break;` |
|         - | 5648 | `		}` |
|        31 | 5649 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5650 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5651 | `				/* ignore */` |
|       ! 0 | 5652 | `				continue;` |
|         - | 5653 | `			}` |
|        23 | 5654 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5655 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5656 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5657 | `				/* Blob lookup */` |
|        17 | 5658 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5659 | `			}else{` |
|         - | 5660 | `				/* Int lookup */` |
|         7 | 5661 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5662 | `			}` |
|        23 | 5663 | `			if( rc == SXRET_OK ){` |
|         - | 5664 | `				/* Key exists,break immediately */` |
|        11 | 5665 | `				break;` |
|         - | 5666 | `			}` |
|         7 | 5667 | `		}` |
|        19 | 5668 | `		if( i >= nArg ){` |
|         - | 5669 | `			/* Perform the insertion */` |
|         9 | 5670 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5671 | `		}` |
|         - | 5672 | `		/* Point to the next entry */` |
|        19 | 5673 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5674 | `		n--;` |
|         1 | 5675 | `	}` |
|         - | 5676 | `	/* Return the freshly created array */` |
|         7 | 5677 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5678 | `	return PH7_OK;` |
|         9 | 5679 | `}` |
|         - | 5680 | `/*` |
|         - | 5681 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5682 | ` *  Computes the intersection of arrays.` |
|         - | 5683 | ` * Parameters` |
|         - | 5684 | ` *  $array1` |
|         - | 5685 | ` *    The array to compare from` |
|         - | 5686 | ` *  $array2` |
|         - | 5687 | ` *    An array to compare against` |
|         - | 5688 | ` *  $...` |
|         - | 5689 | ` *   More arrays to compare against` |
|         - | 5690 | ` * Return` |
|         - | 5691 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5692 | ` *  in all of the parameters.` |
|         - | 5693 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5694 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5695 | ` */` |
|        20 | 5696 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5697 | `{` |
|         - | 5698 | `	ph7_hashmap_node *pEntry;` |
|         - | 5699 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5700 | `	ph7_value *pArray;` |
|         - | 5701 | `	ph7_value *pVal;` |
|         - | 5702 | `	sxi32 rc;` |
|         - | 5703 | `	sxu32 n;` |
|         - | 5704 | `	int i;` |
|        23 | 5705 | `	if( nArg < 1 ){` |
|       ! 0 | 5706 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5707 | `			"ArgumentCountError",` |
|         - | 5708 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5709 | `			nArg` |
|         - | 5710 | `			);` |
|         - | 5711 | `	}` |
|        23 | 5712 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5713 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5714 | `			"TypeError",` |
|         - | 5715 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5716 | `			ph7_type_name(apArg[0])` |
|         - | 5717 | `			);` |
|         - | 5718 | `	}` |
|        36 | 5719 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5720 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5721 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5722 | `				"TypeError",` |
|         - | 5723 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5724 | `				i + 1,` |
|         2 | 5725 | `				ph7_type_name(apArg[i])` |
|         - | 5726 | `				);` |
|         - | 5727 | `		}` |
|         9 | 5728 | `	}` |
|        17 | 5729 | `	if( nArg == 1 ){` |
|         - | 5730 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5731 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5732 | `		return PH7_OK;` |
|         - | 5733 | `	}` |
|         - | 5734 | `	/* Create a new array */` |
|        15 | 5735 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5736 | `	if( pArray == 0 ){` |
|       ! 0 | 5737 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5738 | `		return PH7_OK;` |
|         - | 5739 | `	}` |
|         - | 5740 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5741 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5742 | `	/* Perform the intersection */` |
|        15 | 5743 | `	pEntry = pSrc->pFirst;` |
|        15 | 5744 | `	n = pSrc->nEntry;` |
|        31 | 5745 | `	for(;;){` |
|        63 | 5746 | `		if( n < 1 ){` |
|        15 | 5747 | `			break;` |
|         - | 5748 | `		}` |
|         - | 5749 | `		/* Extract the node value */` |
|        49 | 5750 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5751 | `		if( pVal ){` |
|        79 | 5752 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5753 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5754 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5755 | `				/* Perform the lookup */` |
|        55 | 5756 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5757 | `				if( rc != SXRET_OK ){` |
|         - | 5758 | `					/* Value does not exist */` |
|        25 | 5759 | `					break;` |
|         - | 5760 | `				}` |
|        16 | 5761 | `			}` |
|        49 | 5762 | `			if( i >= nArg ){` |
|         - | 5763 | `				/* Perform the insertion */` |
|        25 | 5764 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5765 | `			}` |
|        24 | 5766 | `		}` |
|         - | 5767 | `		/* Point to the next entry */` |
|        49 | 5768 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5769 | `		n--;` |
|         1 | 5770 | `	}` |
|         - | 5771 | `	/* Return the freshly created array */` |
|        15 | 5772 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5773 | `	return PH7_OK;` |
|        13 | 5774 | `}` |
|         - | 5775 | `/*` |
|         - | 5776 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5777 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5778 | ` * Parameters` |
|         - | 5779 | ` *  $array1` |
|         - | 5780 | ` *    The array to compare from` |
|         - | 5781 | ` *  $array2` |
|         - | 5782 | ` *    An array to compare against` |
|         - | 5783 | ` *  $...` |
|         - | 5784 | ` *   More arrays to compare against` |
|         - | 5785 | ` * Return` |
|         - | 5786 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5787 | ` *  in all the arguments, with matching keys.` |
|         - | 5788 | ` */` |
|        20 | 5789 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5790 | `{` |
|         - | 5791 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5792 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5793 | `	ph7_value *pArray;` |
|         - | 5794 | `	ph7_value *pVal;` |
|         - | 5795 | `	sxi32 rc;` |
|         - | 5796 | `	sxu32 n;` |
|         - | 5797 | `	int i;` |
|        23 | 5798 | `	if( nArg < 1 ){` |
|       ! 0 | 5799 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5800 | `			"ArgumentCountError",` |
|         - | 5801 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5802 | `			nArg` |
|         - | 5803 | `			);` |
|         - | 5804 | `	}` |
|        23 | 5805 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5806 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5807 | `			"TypeError",` |
|         - | 5808 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5809 | `			ph7_type_name(apArg[0])` |
|         - | 5810 | `			);` |
|         - | 5811 | `	}` |
|        36 | 5812 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5813 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5814 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5815 | `				"TypeError",` |
|         - | 5816 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5817 | `				i + 1,` |
|         2 | 5818 | `				ph7_type_name(apArg[i])` |
|         - | 5819 | `				);` |
|         - | 5820 | `		}` |
|         9 | 5821 | `	}` |
|        17 | 5822 | `	if( nArg == 1 ){` |
|         - | 5823 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5824 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5825 | `		return PH7_OK;` |
|         - | 5826 | `	}` |
|         - | 5827 | `	/* Create a new array */` |
|        15 | 5828 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5829 | `	if( pArray == 0 ){` |
|       ! 0 | 5830 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5831 | `		return PH7_OK;` |
|         - | 5832 | `	}` |
|         - | 5833 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5834 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5835 | `	/* Perform the intersection */` |
|        15 | 5836 | `	pEntry = pSrc->pFirst;` |
|        15 | 5837 | `	n = pSrc->nEntry;` |
|        15 | 5838 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5839 | `	for(;;){` |
|        47 | 5840 | `		if( n < 1 ){` |
|        15 | 5841 | `			break;` |
|         - | 5842 | `		}` |
|         - | 5843 | `		/* Extract the node value */` |
|        33 | 5844 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5845 | `		if( pVal ){` |
|        53 | 5846 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5847 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5848 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5849 | `				/* Perform a key lookup first */` |
|        37 | 5850 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5851 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5852 | `				}else{` |
|        23 | 5853 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5854 | `				}` |
|        37 | 5855 | `				if( rc != SXRET_OK ){` |
|         - | 5856 | `					/* No such key,break immediately */` |
|         7 | 5857 | `					break;` |
|         - | 5858 | `				}` |
|         - | 5859 | `				/* Perform the lookup */` |
|        31 | 5860 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5861 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5862 | `					/* Value does not exist */` |
|         6 | 5863 | `					break;` |
|         - | 5864 | `				}` |
|        11 | 5865 | `			}` |
|        33 | 5866 | `			if( i >= nArg ){` |
|         - | 5867 | `				/* Perform the insertion */` |
|        17 | 5868 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5869 | `			}` |
|        16 | 5870 | `		}` |
|         - | 5871 | `		/* Point to the next entry */` |
|        33 | 5872 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5873 | `		n--;` |
|         1 | 5874 | `	}` |
|         - | 5875 | `	/* Return the freshly created array */` |
|        15 | 5876 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5877 | `	return PH7_OK;` |
|        13 | 5878 | `}` |
|         - | 5879 | `/*` |
|         - | 5880 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5881 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5882 | ` * Parameters` |
|         - | 5883 | ` *  $array1` |
|         - | 5884 | ` *    The array to compare from` |
|         - | 5885 | ` *  $...` |
|         - | 5886 | ` *   More arrays to compare against` |
|         - | 5887 | ` * Return` |
|         - | 5888 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5889 | ` *  have keys that are present in all arguments.` |
|         - | 5890 | ` * Note that NULL is returned on failure.` |
|         - | 5891 | ` */` |
|        20 | 5892 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5893 | `{` |
|         - | 5894 | `	ph7_hashmap_node *pEntry;` |
|         - | 5895 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5896 | `	ph7_value *pArray;` |
|         - | 5897 | `	sxi32 rc;` |
|         - | 5898 | `	sxu32 n;` |
|         - | 5899 | `	int i;` |
|        23 | 5900 | `	if( nArg < 1 ){` |
|       ! 0 | 5901 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5902 | `			"ArgumentCountError",` |
|         - | 5903 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5904 | `			nArg` |
|         - | 5905 | `			);` |
|         - | 5906 | `	}` |
|        23 | 5907 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5908 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5909 | `			"TypeError",` |
|         - | 5910 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5911 | `			ph7_type_name(apArg[0])` |
|         - | 5912 | `			);` |
|         - | 5913 | `	}` |
|        36 | 5914 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5915 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5916 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5917 | `				"TypeError",` |
|         - | 5918 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5919 | `				i + 1,` |
|         2 | 5920 | `				ph7_type_name(apArg[i])` |
|         - | 5921 | `				);` |
|         - | 5922 | `		}` |
|         9 | 5923 | `	}` |
|        17 | 5924 | `	if( nArg == 1 ){` |
|         - | 5925 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5926 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5927 | `		return PH7_OK;` |
|         - | 5928 | `	}` |
|         - | 5929 | `	/* Create a new array */` |
|        15 | 5930 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5931 | `	if( pArray == 0 ){` |
|       ! 0 | 5932 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5933 | `		return PH7_OK;` |
|         - | 5934 | `	}` |
|         - | 5935 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5936 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5937 | `	/* Perform the intersection */` |
|        15 | 5938 | `	pEntry = pSrc->pFirst;` |
|        15 | 5939 | `	n = pSrc->nEntry;` |
|        24 | 5940 | `	for(;;){` |
|        49 | 5941 | `		if( n < 1 ){` |
|        15 | 5942 | `			break;` |
|         - | 5943 | `		}` |
|        57 | 5944 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5945 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5946 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5947 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5948 | `				/* Blob lookup */` |
|        27 | 5949 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5950 | `			}else{` |
|         - | 5951 | `				/* Int key */` |
|        13 | 5952 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5953 | `			}` |
|        39 | 5954 | `			if( rc != SXRET_OK ){` |
|         - | 5955 | `				/* Key does not exist, break immediately */` |
|        17 | 5956 | `				break;` |
|         - | 5957 | `			}` |
|        12 | 5958 | `		}` |
|        35 | 5959 | `		if( i >= nArg ){` |
|         - | 5960 | `			/* Perform the insertion */` |
|        19 | 5961 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5962 | `		}` |
|         - | 5963 | `		/* Point to the next entry */` |
|        35 | 5964 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5965 | `		n--;` |
|         1 | 5966 | `	}` |
|         - | 5967 | `	/* Return the freshly created array */` |
|        15 | 5968 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5969 | `	return PH7_OK;` |
|        13 | 5970 | `}` |
|         - | 5971 | `/*` |
|         - | 5972 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5973 | ` *  Computes the intersection of arrays.` |
|         - | 5974 | ` * Parameters` |
|         - | 5975 | ` *  $array1` |
|         - | 5976 | ` *    The array to compare from` |
|         - | 5977 | ` *  $array2` |
|         - | 5978 | ` *    An array to compare against` |
|         - | 5979 | ` *  $...` |
|         - | 5980 | ` *   More arrays to compare against` |
|         - | 5981 | ` * $callback` |
|         - | 5982 | ` *  The callback comparison function.` |
|         - | 5983 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5984 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5985 | ` *  than the second.` |
|         - | 5986 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5987 | ` * Return` |
|         - | 5988 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5989 | ` *  in all of the parameters. .` |
|         - | 5990 | ` * Note that NULL is returned on failure.` |
|         - | 5991 | ` */` |
|        24 | 5992 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5993 | `{` |
|         - | 5994 | `	ph7_hashmap_node *pEntry;` |
|         - | 5995 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5996 | `	ph7_value *pCallback;` |
|         - | 5997 | `	ph7_value *pArray;` |
|         - | 5998 | `	ph7_value *pVal;` |
|         - | 5999 | `	sxi32 rc;` |
|         - | 6000 | `	sxu32 n;` |
|         - | 6001 | `	int i;` |
|         - | 6002 |  |
|         - | 6003 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 6004 | `	if( nArg < 2 ){` |
|       ! 0 | 6005 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6006 | `			"ArgumentCountError",` |
|         - | 6007 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 6008 | `			nArg` |
|         - | 6009 | `			);` |
|         - | 6010 | `	}` |
|        29 | 6011 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6012 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6013 | `			"TypeError",` |
|         - | 6014 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6015 | `			ph7_type_name(apArg[0])` |
|         - | 6016 | `			);` |
|         - | 6017 | `	}` |
|         - | 6018 |  |
|        27 | 6019 | `	if( nArg == 2 ){` |
|         - | 6020 | `		/* Only the original array and the callback were provided. */` |
|         - | 6021 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 6022 | `		 * validation ordering. */` |
|         3 | 6023 | `	} else {` |
|         - | 6024 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 6025 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 6026 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 6027 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6028 | `					"TypeError",` |
|         - | 6029 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6030 | `					i + 1,` |
|         2 | 6031 | `					ph7_type_name(apArg[i])` |
|         - | 6032 | `					);` |
|         - | 6033 | `			}` |
|        13 | 6034 | `		}` |
|         - | 6035 | `	}` |
|         - | 6036 |  |
|         - | 6037 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6038 | `	pCallback = apArg[nArg - 1];` |
|         - | 6039 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6040 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6041 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6042 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6043 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6044 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6045 | `			 */` |
|         9 | 6046 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6047 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6048 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6049 | `					"TypeError",` |
|         - | 6050 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6051 | `					nArg` |
|         - | 6052 | `					);` |
|         - | 6053 | `			}` |
|         - | 6054 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6055 | `			{` |
|         6 | 6056 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6057 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6058 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6059 | `					int nMethodLen;` |
|         6 | 6060 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6061 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6062 | `					if( pClass ){` |
|         - | 6063 | `						/* Class exists but method is missing. */` |
|         4 | 6064 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6065 | `							"TypeError",` |
|         - | 6066 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6067 | `							nArg,` |
|         1 | 6068 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6069 | `							zMethod` |
|         - | 6070 | `							);` |
|         - | 6071 | `					}` |
|         - | 6072 | `					/* Class not found */` |
|         - | 6073 | `					{` |
|         - | 6074 | `						int nName;` |
|         3 | 6075 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6076 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6077 | `							"TypeError",` |
|         - | 6078 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6079 | `							nArg,` |
|         1 | 6080 | `							zName` |
|         - | 6081 | `							);` |
|         - | 6082 | `					}` |
|         - | 6083 | `				}` |
|         - | 6084 | `			}` |
|         - | 6085 | `			/* Fallback message */` |
|       ! 0 | 6086 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6087 | `				"TypeError",` |
|         - | 6088 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6089 | `				nArg` |
|         - | 6090 | `				);` |
|         - | 6091 | `		}` |
|         6 | 6092 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6093 | `			int len;` |
|         3 | 6094 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6095 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6096 | `				"TypeError",` |
|         - | 6097 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6098 | `				nArg,` |
|         1 | 6099 | `				zName` |
|         - | 6100 | `				);` |
|         - | 6101 | `		}` |
|         4 | 6102 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6103 | `			"TypeError",` |
|         - | 6104 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6105 | `			nArg` |
|         - | 6106 | `			);` |
|         - | 6107 | `	}` |
|         - | 6108 |  |
|        11 | 6109 | `	if( nArg == 2 ){` |
|         - | 6110 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6111 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6112 | `		return PH7_OK;` |
|         - | 6113 | `	}` |
|         - | 6114 |  |
|         - | 6115 | `	/* Create a new array */` |
|         7 | 6116 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6117 | `	if( pArray == 0 ){` |
|       ! 0 | 6118 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6119 | `		return PH7_OK;` |
|         - | 6120 | `	}` |
|         - | 6121 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6122 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6123 | `	/* Perform the intersection */` |
|         7 | 6124 | `	pEntry = pSrc->pFirst;` |
|         7 | 6125 | `	n = pSrc->nEntry;` |
|         7 | 6126 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6127 | `	for(;;){` |
|        19 | 6128 | `		if( n < 1 ){` |
|         5 | 6129 | `			break;` |
|         - | 6130 | `		}` |
|         - | 6131 | `		/* Extract the node value */` |
|        15 | 6132 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6133 | `		if( pVal ){` |
|        23 | 6134 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6135 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6136 | `					/* ignore */` |
|       ! 0 | 6137 | `					continue;` |
|         - | 6138 | `				}` |
|         - | 6139 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6140 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6141 | `				/* Perform the lookup */` |
|        15 | 6142 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6143 | `				if( rc != SXRET_OK ){` |
|         - | 6144 | `					/* Value does not exist */` |
|         7 | 6145 | `					break;` |
|         - | 6146 | `				}` |
|         5 | 6147 | `			}` |
|        15 | 6148 | `			if( i >= (nArg-1) ){` |
|         - | 6149 | `				/* Perform the insertion */` |
|         9 | 6150 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6151 | `			}` |
|         7 | 6152 | `		}` |
|        15 | 6153 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6154 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6155 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6156 | `			return PH7_EXCEPTION;` |
|         - | 6157 | `		}` |
|         - | 6158 | `		/* Point to the next entry */` |
|        13 | 6159 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6160 | `		n--;` |
|         1 | 6161 | `	}` |
|         - | 6162 | `	/* Return the freshly created array */` |
|         5 | 6163 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6164 | `	return PH7_OK;` |
|        17 | 6165 | `}` |
|         - | 6166 | `/*` |
|         - | 6167 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6168 | ` *  Fill an array with values.` |
|         - | 6169 | ` * Parameters` |
|         - | 6170 | ` *  $start_index` |
|         - | 6171 | ` *    The first index of the returned array.` |
|         - | 6172 | ` *  $num` |
|         - | 6173 | ` *   Number of elements to insert.` |
|         - | 6174 | ` *  $value` |
|         - | 6175 | ` *    Value to use for filling.` |
|         - | 6176 | ` * Return` |
|         - | 6177 | ` *  The filled array or null on failure.` |
|         - | 6178 | ` */` |
|       240 | 6179 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6180 | `{` |
|         - | 6181 | `	ph7_value *pArray;` |
|         - | 6182 | `	int i,nEntry;` |
|         - | 6183 |  |
|         - | 6184 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6185 | `	if( nArg != 3 ){` |
|         - | 6186 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6187 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6188 | `			"ArgumentCountError",` |
|         - | 6189 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6190 | `			nArg` |
|         - | 6191 | `			);` |
|         - | 6192 | `	}` |
|         - | 6193 |  |
|         - | 6194 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6195 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6196 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6197 | `	 * and NULLs are rejected outright. */` |
|       357 | 6198 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6199 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6200 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6201 | `			"TypeError",` |
|         - | 6202 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6203 | `			ph7_type_name(apArg[0])` |
|         - | 6204 | `			);` |
|         - | 6205 | `	}` |
|       242 | 6206 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6207 | `		int len;` |
|         8 | 6208 | `		sxu8 bReal = FALSE;` |
|         8 | 6209 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6210 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6211 | `			/* Non‑numeric string is an error. */` |
|         3 | 6212 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6213 | `				"TypeError",` |
|         - | 6214 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6215 | `				);` |
|         - | 6216 | `		}` |
|         5 | 6217 | `		if( bReal ){` |
|         - | 6218 | `			/* float-string -> deprecation warning */` |
|         4 | 6219 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6220 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6221 | `				zStr` |
|         - | 6222 | `				);` |
|         1 | 6223 | `		}` |
|         2 | 6224 | `	}` |
|         - | 6225 |  |
|         - | 6226 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6227 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6228 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6229 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6230 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6231 | `			"TypeError",` |
|         - | 6232 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6233 | `			ph7_type_name(apArg[1])` |
|         - | 6234 | `			);` |
|         - | 6235 | `	}` |
|       239 | 6236 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6237 | `		int len;` |
|         3 | 6238 | `		sxu8 bReal = FALSE;` |
|         3 | 6239 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6240 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6241 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6242 | `				"TypeError",` |
|         - | 6243 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6244 | `				);` |
|         - | 6245 | `		}` |
|       ! 0 | 6246 | `	}` |
|         - | 6247 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6248 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6249 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6250 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6251 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6252 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6253 | `		if( d != (double)i64 ){` |
|         7 | 6254 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6255 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6256 | `				d` |
|         - | 6257 | `				);` |
|         2 | 6258 | `		}` |
|         2 | 6259 | `	}` |
|         - | 6260 |  |
|         - | 6261 | `	/* Total number of entries to insert */` |
|       236 | 6262 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6263 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6264 | `	if( nEntry < 0 ){` |
|         3 | 6265 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6266 | `			"ValueError",` |
|         - | 6267 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6268 | `			);` |
|         - | 6269 | `	}` |
|         - | 6270 |  |
|         - | 6271 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6272 | `	if( nEntry == 0 ){` |
|         7 | 6273 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6274 | `		return PH7_OK;` |
|         - | 6275 | `	}` |
|         - | 6276 |  |
|         - | 6277 | `	/* Create a new array */` |
|       227 | 6278 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6279 | `	if( pArray == 0 ){` |
|       ! 0 | 6280 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6281 | `	}` |
|         - | 6282 |  |
|         - | 6283 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6284 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6285 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6286 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6287 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6288 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6289 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6290 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6291 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6292 | `		}` |
|   1058803 | 6293 | `	}` |
|         - | 6294 | `	/* Return the filled array */` |
|       227 | 6295 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6296 | `	return PH7_OK;` |
|       124 | 6297 | `}` |
|         - | 6298 | `/*` |
|         - | 6299 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6300 | ` *  Fill an array with values, specifying keys.` |
|         - | 6301 | ` * Parameters` |
|         - | 6302 | ` *  $input` |
|         - | 6303 | ` *   Array of values that will be used as key.` |
|         - | 6304 | ` *  $value` |
|         - | 6305 | ` *    Value to use for filling.` |
|         - | 6306 | ` * Return` |
|         - | 6307 | ` *  The filled array.` |
|         - | 6308 | ` * Throws` |
|         - | 6309 | ` *  ValueError if $input is not an array.` |
|         - | 6310 | ` */` |
|        22 | 6311 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6312 | `{` |
|         - | 6313 | `	ph7_hashmap_node *pEntry;` |
|         - | 6314 | `	ph7_hashmap *pSrc;` |
|         - | 6315 | `	ph7_value *pArray;` |
|         - | 6316 | `	sxu32 n;` |
|         - | 6317 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6318 | `	if( nArg != 2 ){` |
|         4 | 6319 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6320 | `			"ArgumentCountError",` |
|         - | 6321 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6322 | `			nArg` |
|         - | 6323 | `			);` |
|         - | 6324 | `	}` |
|         - | 6325 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6326 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6327 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6328 | `			"TypeError",` |
|         - | 6329 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6330 | `			ph7_type_name(apArg[0])` |
|         - | 6331 | `			);` |
|         - | 6332 | `	}` |
|         - | 6333 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6334 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6335 | `	/* Create a new array */` |
|        17 | 6336 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6337 | `	if( pArray == 0 ){` |
|       ! 0 | 6338 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6339 | `		return PH7_OK;` |
|         - | 6340 | `	}` |
|         - | 6341 | `	/* Perform the requested operation */` |
|        17 | 6342 | `	pEntry = pSrc->pFirst;` |
|        45 | 6343 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6344 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6345 | `		/* Point to the next entry */` |
|        29 | 6346 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6347 | `	}` |
|         - | 6348 | `	/* Return the filled array */` |
|        17 | 6349 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6350 | `	return PH7_OK;` |
|        14 | 6351 | `}` |
|         - | 6352 | `/*` |
|         - | 6353 | ` * array array_combine(array $keys,array $values)` |
|         - | 6354 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6355 | ` * Parameters` |
|         - | 6356 | ` *  $keys` |
|         - | 6357 | ` *    Array of keys to be used.` |
|         - | 6358 | ` * $values` |
|         - | 6359 | ` *   Array of values to be used.` |
|         - | 6360 | ` * Return` |
|         - | 6361 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6362 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6363 | ` *  not an array.` |
|         - | 6364 | ` */` |
|        16 | 6365 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6366 | `{` |
|         - | 6367 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6368 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6369 | `	ph7_value *pArray;` |
|         - | 6370 | `	sxu32 n;` |
|         - | 6371 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6372 | `	if( nArg != 2 ){` |
|         - | 6373 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6374 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6375 | `			"ArgumentCountError",` |
|         - | 6376 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6377 | `			nArg` |
|         - | 6378 | `			);` |
|         - | 6379 | `	}` |
|         - | 6380 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6381 | `	 * argument index in the error message. */` |
|        20 | 6382 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6383 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6384 | `			"TypeError",` |
|         - | 6385 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6386 | `			ph7_type_name(apArg[0])` |
|         - | 6387 | `			);` |
|         - | 6388 | `	}` |
|        17 | 6389 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6390 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6391 | `			"TypeError",` |
|         - | 6392 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6393 | `			ph7_type_name(apArg[1])` |
|         - | 6394 | `			);` |
|         - | 6395 | `	}` |
|         - | 6396 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6397 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6398 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6399 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6400 | `		/* Length mismatch -> ValueError */` |
|         3 | 6401 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6402 | `			"ValueError",` |
|         - | 6403 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6404 | `			);` |
|         - | 6405 | `	}` |
|         - | 6406 | `	/* Create a new array */` |
|        11 | 6407 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6408 | `	if( pArray == 0 ){` |
|       ! 0 | 6409 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6410 | `		return PH7_OK;` |
|         - | 6411 | `	}` |
|         - | 6412 | `	/* Perform the requested operation */` |
|        11 | 6413 | `	pKe = pKey->pFirst;` |
|        11 | 6414 | `	pVe = pValue->pFirst;` |
|        33 | 6415 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6416 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6417 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6418 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6419 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6420 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6421 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6422 | `		 * original array must not be mutated. */` |
|        23 | 6423 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6424 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6425 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6426 | `			if( pTmpKey ){` |
|         5 | 6427 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6428 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6429 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6430 | `				pKeyCopy = pTmpKey;` |
|         2 | 6431 | `			}` |
|         2 | 6432 | `		}` |
|        23 | 6433 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6434 | `		/* Point to the next entry */` |
|        23 | 6435 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6436 | `		pVe = pVe->pPrev;` |
|        12 | 6437 | `	}` |
|         - | 6438 | `	/* Return the filled array */` |
|        11 | 6439 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6440 | `	return PH7_OK;` |
|        12 | 6441 | `}` |
|         - | 6442 | `/*` |
|         - | 6443 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6444 | ` *  Return an array with elements in reverse order.` |
|         - | 6445 | ` * Parameters` |
|         - | 6446 | ` *  $array` |
|         - | 6447 | ` *   The input array.` |
|         - | 6448 | ` *  $preserve_keys (optional)` |
|         - | 6449 | ` *   If set to TRUE keys are preserved.` |
|         - | 6450 | ` * Return` |
|         - | 6451 | ` *  The reversed array.` |
|         - | 6452 | ` */` |
|        18 | 6453 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6454 | `{` |
|         - | 6455 | `	ph7_hashmap_node *pEntry;` |
|         - | 6456 | `	ph7_hashmap *pSrc;` |
|         - | 6457 | `	ph7_value *pArray;` |
|         - | 6458 | `	int bPreserve;` |
|         - | 6459 | `	sxu32 n;` |
|        20 | 6460 | `	if( nArg < 1 ){` |
|       ! 0 | 6461 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6462 | `			"ArgumentCountError",` |
|         - | 6463 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6464 | `			nArg` |
|         - | 6465 | `			);` |
|         - | 6466 | `	}` |
|         - | 6467 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6468 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6469 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6470 | `			"TypeError",` |
|         - | 6471 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6472 | `			ph7_type_name(apArg[0])` |
|         - | 6473 | `			);` |
|         - | 6474 | `	}` |
|        17 | 6475 | `	bPreserve = FALSE;` |
|        17 | 6476 | `	if( nArg > 1 ){` |
|         7 | 6477 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6478 | `	}` |
|         - | 6479 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6480 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6481 | `	/* Create a new array */` |
|        17 | 6482 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6483 | `	if( pArray == 0 ){` |
|       ! 0 | 6484 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6485 | `		return PH7_OK;` |
|         - | 6486 | `	}` |
|         - | 6487 | `	/* Perform the requested operation */` |
|        17 | 6488 | `	pEntry = pSrc->pLast;` |
|        55 | 6489 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6490 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6491 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6492 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6493 | `		/* Point to the previous entry */` |
|        39 | 6494 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6495 | `	}` |
|        17 | 6496 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6497 | `	return PH7_OK;` |
|        11 | 6498 | `}` |
|         - | 6499 | `/*` |
|         - | 6500 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6501 | ` *  Removes duplicate values from an array.` |
|         - | 6502 | ` * Parameters` |
|         - | 6503 | ` *  $array` |
|         - | 6504 | ` *   The input array.` |
|         - | 6505 | ` *  $flags` |
|         - | 6506 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6507 | ` *   behavior using these values:` |
|         - | 6508 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6509 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6510 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6511 | ` * Return` |
|         - | 6512 | ` *  The filtered array.` |
|         - | 6513 | ` */` |
|        22 | 6514 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6515 | `{` |
|         - | 6516 | `	ph7_hashmap_node *pEntry;` |
|         - | 6517 | `	ph7_value *pNeedle;` |
|         - | 6518 | `	ph7_hashmap *pSrc;` |
|         - | 6519 | `	ph7_value *pArray;` |
|         - | 6520 | `	int bStrict;` |
|         - | 6521 | `	sxi32 rc;` |
|         - | 6522 | `	sxu32 n;` |
|        25 | 6523 | `	if( nArg < 1 ){` |
|         - | 6524 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6525 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6526 | `			"ArgumentCountError",` |
|         - | 6527 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6528 | `			);` |
|         - | 6529 | `	}` |
|        25 | 6530 | `	if( nArg > 2 ){` |
|         - | 6531 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6532 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6533 | `			"ArgumentCountError",` |
|         - | 6534 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6535 | `			nArg` |
|         - | 6536 | `			);` |
|         - | 6537 | `	}` |
|         - | 6538 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6539 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6540 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6541 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6542 | `			"TypeError",` |
|         - | 6543 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6544 | `			ph7_type_name(apArg[0])` |
|         - | 6545 | `			);` |
|         - | 6546 | `	}` |
|        19 | 6547 | `	bStrict = FALSE;` |
|         - | 6548 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6549 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6550 | `	/* Create a new array */` |
|        19 | 6551 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6552 | `	if( pArray == 0 ){` |
|       ! 0 | 6553 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6554 | `		return PH7_OK;` |
|         - | 6555 | `	}` |
|         - | 6556 | `	/* Perform the requested operation */` |
|        19 | 6557 | `	pEntry = pSrc->pFirst;` |
|        83 | 6558 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6559 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6560 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6561 | `		if( pNeedle ){` |
|        65 | 6562 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6563 | `		}` |
|        65 | 6564 | `		if( rc != SXRET_OK ){` |
|         - | 6565 | `			/* Perform the insertion */` |
|        37 | 6566 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6567 | `		}` |
|         - | 6568 | `		/* Point to the next entry */` |
|        65 | 6569 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6570 | `	}` |
|         - | 6571 | `	/* Return the freshly created array */` |
|        19 | 6572 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6573 | `	return PH7_OK;` |
|        14 | 6574 | `}` |
|         - | 6575 | `/*` |
|         - | 6576 | ` * array array_flip(array $input)` |
|         - | 6577 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6578 | ` * Parameter` |
|         - | 6579 | ` *  $input` |
|         - | 6580 | ` *   Input array.` |
|         - | 6581 | ` * Return` |
|         - | 6582 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6583 | ` */` |
|        30 | 6584 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6585 | `{` |
|         - | 6586 | `	ph7_hashmap_node *pEntry;` |
|         - | 6587 | `	ph7_hashmap *pSrc;` |
|         - | 6588 | `	ph7_value *pArray;` |
|         - | 6589 | `	ph7_value *pKey;` |
|         - | 6590 | `	ph7_value sVal;` |
|         - | 6591 | `	sxu32 n;` |
|         - | 6592 |  |
|         - | 6593 | `	/* PHP requires exactly one argument */` |
|        33 | 6594 | `	if( nArg != 1 ){` |
|         - | 6595 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6596 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6597 | `			"ArgumentCountError",` |
|         - | 6598 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6599 | `			nArg` |
|         - | 6600 | `			);` |
|         - | 6601 | `	}` |
|         - | 6602 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6603 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6604 | `		/* Type mismatch -> TypeError */` |
|         4 | 6605 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6606 | `			"TypeError",` |
|         - | 6607 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6608 | `			ph7_type_name(apArg[0])` |
|         - | 6609 | `			);` |
|         - | 6610 | `	}` |
|         - | 6611 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6612 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6613 | `	/* Create a new array */` |
|        27 | 6614 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6615 | `	if( pArray == 0 ){` |
|       ! 0 | 6616 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6617 | `		return PH7_OK;` |
|         - | 6618 | `	}` |
|         - | 6619 | `	/* Start processing */` |
|        27 | 6620 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6621 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6622 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6623 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6624 | `		if( pKey ){` |
|         - | 6625 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6626 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6627 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6628 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6629 | `					);` |
|     22236 | 6630 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6631 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6632 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6633 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6634 | `				}else{` |
|         - | 6635 | `					SyString sStr;` |
|      2227 | 6636 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6637 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6638 | `				}` |
|         - | 6639 | `				/* Perform the insertion */` |
|     22227 | 6640 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6641 | `				/* Safely release the value because each inserted entry` |
|         - | 6642 | `				 * has its own private copy of the value.` |
|         - | 6643 | `				 */` |
|     22227 | 6644 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6645 | `			}else{` |
|         - | 6646 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6647 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6648 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6649 | `					);` |
|         - | 6650 | `			}` |
|     11118 | 6651 | `		}` |
|         - | 6652 | `		/* Point to the next entry */` |
|     22237 | 6653 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6654 | `	}` |
|         - | 6655 | `	/* Return the freshly created array */` |
|        27 | 6656 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6657 | `	return PH7_OK;` |
|        18 | 6658 | `}` |
|         - | 6659 | `/*` |
|         - | 6660 | ` * number array_sum(array $array )` |
|         - | 6661 | ` *  Calculate the sum of values in an array.` |
|         - | 6662 | ` * Parameters` |
|         - | 6663 | ` *  $array: The input array.` |
|         - | 6664 | ` * Return` |
|         - | 6665 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6666 | ` */` |
|        24 | 6667 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6668 | `{` |
|         - | 6669 | `	ph7_hashmap_node *pEntry;` |
|         - | 6670 | `	ph7_value *pObj;` |
|        26 | 6671 | `	double dSum = 0;` |
|         - | 6672 | `	sxu32 n;` |
|        26 | 6673 | `	pEntry = pMap->pFirst;` |
|        92 | 6674 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6675 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6676 | `		if( pObj ){` |
|        68 | 6677 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6678 | `				dSum += pObj->rVal;` |
|        54 | 6679 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6680 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6681 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6682 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6683 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6684 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6685 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6686 | `						"Addition is not supported on type string");` |
|        14 | 6687 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6688 | `					double dv = 0;` |
|        13 | 6689 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6690 | `					dSum += dv;` |
|         8 | 6691 | `				}` |
|        12 | 6692 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6693 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6694 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6695 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6696 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6697 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6698 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6699 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6700 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6701 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6702 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6703 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6704 | `			}` |
|         - | 6705 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6706 | `		}` |
|         - | 6707 | `		/* Point to the next entry */` |
|        68 | 6708 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6709 | `	}` |
|         - | 6710 | `	/* Return sum */` |
|        26 | 6711 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6712 | `}` |
|       688 | 6713 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6714 | `{` |
|         - | 6715 | `	ph7_hashmap_node *pEntry;` |
|         - | 6716 | `	ph7_value *pObj;` |
|       690 | 6717 | `	sxi64 nSum = 0;` |
|         - | 6718 | `	sxu32 n;` |
|       690 | 6719 | `	pEntry = pMap->pFirst;` |
|      4702 | 6720 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6721 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6722 | `		if( pObj ){` |
|      4014 | 6723 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6724 | `				nSum += pObj->x.iVal;` |
|      2018 | 6725 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6726 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6727 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6728 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6729 | `						"Addition is not supported on type string");` |
|        10 | 6730 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6731 | `					sxi64 nv = 0;` |
|         8 | 6732 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6733 | `					nSum += nv;` |
|         5 | 6734 | `				}` |
|        17 | 6735 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6736 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6737 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6738 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6739 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6740 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6741 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6742 | `					"Addition is not supported on type %s",` |
|         2 | 6743 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6744 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6745 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6746 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6747 | `			}` |
|         - | 6748 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6749 | `		}` |
|         - | 6750 | `		/* Point to the next entry */` |
|      4014 | 6751 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6752 | `	}` |
|         - | 6753 | `	/* Return sum */` |
|       690 | 6754 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6755 | `}` |
|         - | 6756 | `/* number array_sum(array $array )` |
|         - | 6757 | ` * (See block-coment above)` |
|         - | 6758 | ` */` |
|       724 | 6759 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6760 | `{` |
|         - | 6761 | `	ph7_hashmap_node *pEntry;` |
|         - | 6762 | `	ph7_hashmap *pMap;` |
|         - | 6763 | `	ph7_value *pObj;` |
|       728 | 6764 | `	int useDouble = 0;` |
|         - | 6765 | `	sxu32 n;` |
|         - | 6766 | `	/* PHP requires exactly one argument */` |
|       728 | 6767 | `	if( nArg != 1 ){` |
|         4 | 6768 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6769 | `			"ArgumentCountError",` |
|         - | 6770 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6771 | `			nArg` |
|         - | 6772 | `			);` |
|         - | 6773 | `	}` |
|         - | 6774 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6775 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6776 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6777 | `		char zBuf[64];` |
|         8 | 6778 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6779 | `			"TypeError",` |
|         - | 6780 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6781 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6782 | `			);` |
|         - | 6783 | `	}` |
|       720 | 6784 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6785 | `	if( pMap->nEntry < 1 ){` |
|         - | 6786 | `		/* Nothing to compute,return 0 */` |
|         7 | 6787 | `		ph7_result_int(pCtx,0);` |
|         7 | 6788 | `		return PH7_OK;` |
|         - | 6789 | `	}` |
|         - | 6790 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6791 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6792 | `	 */` |
|       714 | 6793 | `	pEntry = pMap->pFirst;` |
|      4734 | 6794 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6795 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6796 | `		if( pObj ){` |
|      4046 | 6797 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6798 | `				useDouble = 1;` |
|        20 | 6799 | `				break;` |
|         - | 6800 | `			}` |
|      4028 | 6801 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6802 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6803 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6804 | `				sxu32 i;` |
|        32 | 6805 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6806 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6807 | `						useDouble = 1;` |
|         7 | 6808 | `						break;` |
|         - | 6809 | `					}` |
|         9 | 6810 | `				}` |
|        18 | 6811 | `				if( useDouble ){` |
|         7 | 6812 | `					break;` |
|         - | 6813 | `				}` |
|         5 | 6814 | `			}` |
|      2010 | 6815 | `		}` |
|      4022 | 6816 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6817 | `	}` |
|       714 | 6818 | `	if( useDouble ){` |
|        26 | 6819 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6820 | `	}else{` |
|       690 | 6821 | `		Int64Sum(pCtx,pMap);` |
|         - | 6822 | `	}` |
|       714 | 6823 | `	return PH7_OK;` |
|       366 | 6824 | `}` |
|         - | 6825 | `/*` |
|         - | 6826 | ` * number array_product(array $array )` |
|         - | 6827 | ` *  Calculate the product of values in an array.` |
|         - | 6828 | ` * Parameters` |
|         - | 6829 | ` *  $array: The input array.` |
|         - | 6830 | ` * Return` |
|         - | 6831 | ` *  Returns the product of values as an integer or float.` |
|         - | 6832 | ` */` |
|         2 | 6833 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6834 | `{` |
|         - | 6835 | `	ph7_hashmap_node *pEntry;` |
|         - | 6836 | `	ph7_value *pObj;` |
|         - | 6837 | `	double dProd;` |
|         - | 6838 | `	sxu32 n;` |
|         3 | 6839 | `	pEntry = pMap->pFirst;` |
|         3 | 6840 | `	dProd = 1;` |
|         7 | 6841 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6842 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6843 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6844 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6845 | `				dProd *= pObj->rVal;` |
|         4 | 6846 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6847 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6848 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6849 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6850 | `					double dv = 0;` |
|       ! 0 | 6851 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6852 | `					dProd *= dv;` |
|       ! 0 | 6853 | `				}` |
|       ! 0 | 6854 | `			}` |
|         2 | 6855 | `		}` |
|         - | 6856 | `		/* Point to the next entry */` |
|         5 | 6857 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6858 | `	}` |
|         - | 6859 | `	/* Return product */` |
|         3 | 6860 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6861 | `}` |
|         2 | 6862 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6863 | `{` |
|         - | 6864 | `	ph7_hashmap_node *pEntry;` |
|         - | 6865 | `	ph7_value *pObj;` |
|         - | 6866 | `	sxi64 nProd;` |
|         - | 6867 | `	sxu32 n;` |
|         3 | 6868 | `	pEntry = pMap->pFirst;` |
|         3 | 6869 | `	nProd = 1;` |
|         9 | 6870 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6871 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6872 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6873 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6874 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6875 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6876 | `				nProd *= pObj->x.iVal;` |
|         3 | 6877 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6878 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6879 | `					sxi64 nv = 0;` |
|       ! 0 | 6880 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6881 | `					nProd *= nv;` |
|       ! 0 | 6882 | `				}` |
|       ! 0 | 6883 | `			}` |
|         3 | 6884 | `		}` |
|         - | 6885 | `		/* Point to the next entry */` |
|         7 | 6886 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6887 | `	}` |
|         - | 6888 | `	/* Return product */` |
|         3 | 6889 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6890 | `}` |
|         - | 6891 | `/* number array_product(array $array )` |
|         - | 6892 | ` * (See block-block comment above)` |
|         - | 6893 | ` */` |
|        16 | 6894 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6895 | `{` |
|         - | 6896 | `	ph7_hashmap *pMap;` |
|         - | 6897 | `	ph7_value *pObj;` |
|        17 | 6898 | `	if( nArg < 1 ){` |
|         - | 6899 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6900 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6901 | `		return PH7_OK;` |
|         - | 6902 | `	}` |
|         - | 6903 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 6904 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6905 | `		char zBuf[64];` |
|        16 | 6906 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6907 | `			"TypeError",` |
|         - | 6908 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 6909 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6910 | `			);` |
|         - | 6911 | `	}` |
|         7 | 6912 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6913 | `	if( pMap->nEntry < 1 ){` |
|         - | 6914 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6915 | `		ph7_result_int(pCtx,1);` |
|         3 | 6916 | `		return PH7_OK;` |
|         - | 6917 | `	}` |
|         - | 6918 | `	/* If the first element is of type float,then perform floating` |
|         - | 6919 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6920 | `	 */` |
|         5 | 6921 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6922 | `	if( pObj == 0 ){` |
|       ! 0 | 6923 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6924 | `		return PH7_OK;` |
|         - | 6925 | `	}` |
|         5 | 6926 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6927 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6928 | `	}else{` |
|         3 | 6929 | `		Int64Prod(pCtx,pMap);` |
|         - | 6930 | `	}` |
|         5 | 6931 | `	return PH7_OK;` |
|         9 | 6932 | `}` |
|         - | 6933 | `/*` |
|         - | 6934 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6935 | ` *  Pick one or more random entries out of an array.` |
|         - | 6936 | ` * Parameters` |
|         - | 6937 | ` * $input` |
|         - | 6938 | ` *  The input array.` |
|         - | 6939 | ` * $num_req` |
|         - | 6940 | ` *  Specifies how many entries you want to pick.` |
|         - | 6941 | ` * Return` |
|         - | 6942 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6943 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6944 | ` *  NULL is returned on failure.` |
|         - | 6945 | ` */` |
|        36 | 6946 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6947 | `{` |
|         - | 6948 | `	ph7_hashmap_node *pNode;` |
|         - | 6949 | `	ph7_hashmap *pMap;` |
|        37 | 6950 | `	int nItem = 1;` |
|        37 | 6951 | `	if( nArg < 1 ){` |
|         - | 6952 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6953 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6954 | `		return PH7_OK;` |
|         - | 6955 | `	}` |
|         - | 6956 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 6957 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6958 | `		char zBuf[64];` |
|        10 | 6959 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6960 | `			"TypeError",` |
|         - | 6961 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6962 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6963 | `			);` |
|         - | 6964 | `	}` |
|         - | 6965 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6966 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 6967 | `	if( nArg > 1 ){` |
|        23 | 6968 | `		ph7_value *pNum = apArg[1];` |
|        22 | 6969 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 6970 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6971 | `			char zBuf[64];` |
|       ! 0 | 6972 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6973 | `				"TypeError",` |
|         - | 6974 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 6975 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6976 | `				);` |
|         - | 6977 | `		}` |
|        23 | 6978 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6979 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6980 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6981 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6982 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6983 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6984 | `			int len;` |
|         9 | 6985 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6986 | `			sxi64 iLong; double dReal;` |
|         9 | 6987 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6988 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6989 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6990 | `					"TypeError",` |
|         - | 6991 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6992 | `					);` |
|         - | 6993 | `			}` |
|         - | 6994 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6995 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6996 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6997 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6998 | `			}` |
|         3 | 6999 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 7000 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 7001 | `			nItem = (int)iLong;` |
|         2 | 7002 | `		}else{` |
|        15 | 7003 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 7004 | `		}` |
|         8 | 7005 | `	}` |
|         - | 7006 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 7007 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7008 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 7009 | `	if( pMap->nEntry < 1 ){` |
|         5 | 7010 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7011 | `			"ValueError",` |
|         - | 7012 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 7013 | `			);` |
|         - | 7014 | `	}` |
|         - | 7015 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 7016 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 7017 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7018 | `			"ValueError",` |
|         - | 7019 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 7020 | `			);` |
|         - | 7021 | `	}` |
|        13 | 7022 | `	if( nItem < 2 ){` |
|         - | 7023 | `		sxu32 nEntry;` |
|         - | 7024 | `		/* Select a random number */` |
|         9 | 7025 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 7026 | `		/* Extract the desired entry.` |
|         - | 7027 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 7028 | `		 */` |
|         9 | 7029 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         4 | 7030 | `			pNode = pMap->pLast;` |
|         4 | 7031 | `			nEntry = pMap->nEntry - nEntry;` |
|         4 | 7032 | `			if( nEntry > 1 ){` |
|       ! 0 | 7033 | `				for(;;){` |
|       ! 0 | 7034 | `					if( nEntry == 0 ){` |
|       ! 0 | 7035 | `						break;` |
|         - | 7036 | `					}` |
|         - | 7037 | `					/* Point to the previous entry */` |
|       ! 0 | 7038 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7039 | `					nEntry--;` |
|       ! 0 | 7040 | `				}` |
|       ! 0 | 7041 | `			}` |
|         2 | 7042 | `		}else{` |
|         6 | 7043 | `			pNode = pMap->pFirst;` |
|         4 | 7044 | `			for(;;){` |
|         9 | 7045 | `				if( nEntry == 0 ){` |
|         6 | 7046 | `					break;` |
|         - | 7047 | `				}` |
|         - | 7048 | `				/* Point to the next entry */` |
|         4 | 7049 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         4 | 7050 | `				nEntry--;` |
|         1 | 7051 | `			}` |
|         - | 7052 | `		}` |
|         9 | 7053 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7054 | `			/* Int key */` |
|         7 | 7055 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7056 | `		}else{` |
|         - | 7057 | `			/* Blob key */` |
|         3 | 7058 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7059 | `		}` |
|         5 | 7060 | `	}else{` |
|         - | 7061 | `		ph7_value sKey,*pArray;` |
|         - | 7062 | `		ph7_hashmap *pDest;` |
|         - | 7063 | `		/* Create a new array */` |
|         5 | 7064 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7065 | `		if( pArray == 0 ){` |
|       ! 0 | 7066 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7067 | `			return PH7_OK;` |
|         - | 7068 | `		}` |
|         - | 7069 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7070 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7071 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7072 | `		/* Copy the first n items */` |
|         5 | 7073 | `		pNode = pMap->pFirst;` |
|         5 | 7074 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7075 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7076 | `		}` |
|        15 | 7077 | `		while( nItem > 0){` |
|        11 | 7078 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7079 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7080 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7081 | `			/* Point to the next entry */` |
|        11 | 7082 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7083 | `			nItem--;` |
|         1 | 7084 | `		}` |
|         - | 7085 | `		/* Shuffle the array */` |
|         5 | 7086 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7087 | `		/* Rehash node */` |
|         5 | 7088 | `		HashmapSortRehash(pDest);` |
|         - | 7089 | `		/* Return the random array */` |
|         5 | 7090 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7091 | `	}` |
|        13 | 7092 | `	return PH7_OK;` |
|        19 | 7093 | `}` |
|         - | 7094 | `/*` |
|         - | 7095 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7096 | ` *  Split an array into chunks.` |
|         - | 7097 | ` * Parameters` |
|         - | 7098 | ` * $input` |
|         - | 7099 | ` *   The array to work on` |
|         - | 7100 | ` * $size` |
|         - | 7101 | ` *   The size of each chunk` |
|         - | 7102 | ` * $preserve_keys` |
|         - | 7103 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7104 | ` *   the chunk numerically.` |
|         - | 7105 | ` * Return` |
|         - | 7106 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7107 | ` *  zero, with each dimension containing size elements.` |
|         - | 7108 | ` */` |
|        36 | 7109 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7110 | `{` |
|         - | 7111 | `	ph7_value *pArray,*pChunk;` |
|         - | 7112 | `	ph7_hashmap_node *pEntry;` |
|         - | 7113 | `	ph7_hashmap *pMap;` |
|         - | 7114 | `	int bPreserve;` |
|         - | 7115 | `	sxu32 nChunk;` |
|         - | 7116 | `	sxu32 nSize;` |
|         - | 7117 | `	sxu32 n;` |
|         - | 7118 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7119 | `	if( nArg < 2 ){` |
|         - | 7120 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7121 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7122 | `			"ArgumentCountError",` |
|         - | 7123 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7124 | `			nArg` |
|         - | 7125 | `			);` |
|         - | 7126 | `	}` |
|        41 | 7127 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7128 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7129 | `			"TypeError",` |
|         - | 7130 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7131 | `			ph7_type_name(apArg[0])` |
|         - | 7132 | `			);` |
|         - | 7133 | `	}` |
|         - | 7134 | `	/* Create a new array */` |
|        38 | 7135 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7136 | `	if( pArray == 0 ){` |
|       ! 0 | 7137 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7138 | `		return PH7_OK;` |
|         - | 7139 | `	}` |
|         - | 7140 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7141 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7142 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7143 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7144 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7145 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7146 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7147 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7148 | `			"TypeError",` |
|         - | 7149 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7150 | `			ph7_type_name(apArg[1])` |
|         - | 7151 | `			);` |
|         - | 7152 | `	}` |
|         - | 7153 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7154 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7155 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7156 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7157 | `		int len;` |
|         3 | 7158 | `		sxu8 bReal = FALSE;` |
|         3 | 7159 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7160 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7161 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7162 | `				"TypeError",` |
|         - | 7163 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7164 | `				);` |
|         - | 7165 | `		}` |
|       ! 0 | 7166 | `		if( bReal ){` |
|         - | 7167 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7168 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7169 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7170 | `				zStr` |
|         - | 7171 | `				);` |
|       ! 0 | 7172 | `		}` |
|       ! 0 | 7173 | `	}` |
|         - | 7174 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7175 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7176 | `	 * later via ph7_value_to_int. */` |
|        35 | 7177 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7178 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7179 | `		sxi64 i = (sxi64)d;` |
|         3 | 7180 | `		if( d != (double)i ){` |
|         4 | 7181 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7182 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7183 | `				d` |
|         - | 7184 | `				);` |
|         1 | 7185 | `		}` |
|         1 | 7186 | `	}` |
|         - | 7187 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7188 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7189 | `	{` |
|        35 | 7190 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7191 | `		if( nSizeSigned < 1 ){` |
|         - | 7192 | `			/* size <= 0 -> ValueError */` |
|         6 | 7193 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7194 | `				"ValueError",` |
|         - | 7195 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7196 | `				);` |
|         - | 7197 | `		}` |
|        29 | 7198 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7199 | `	}` |
|        29 | 7200 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7201 | `		/* Return the whole array */` |
|         3 | 7202 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7203 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7204 | `		return PH7_OK;` |
|         - | 7205 | `	}` |
|        27 | 7206 | `	bPreserve = 0;` |
|        27 | 7207 | `	if( nArg > 2 ){` |
|         - | 7208 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7209 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7210 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7211 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7212 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7213 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7214 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7215 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7216 | `				"TypeError",` |
|         - | 7217 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7218 | `				ph7_type_name(apArg[2])` |
|         - | 7219 | `				);` |
|         - | 7220 | `		}` |
|        21 | 7221 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7222 | `	}` |
|         - | 7223 | `	/* Start processing */` |
|        27 | 7224 | `	pEntry = pMap->pFirst;` |
|        27 | 7225 | `	nChunk = 0;` |
|        27 | 7226 | `	pChunk = 0;` |
|        27 | 7227 | `	n = pMap->nEntry;` |
|        56 | 7228 | `	for( ;; ){` |
|       113 | 7229 | `		if( n < 1 ){` |
|         - | 7230 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7231 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7232 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7233 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7234 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7235 | `			 * exists. */` |
|        27 | 7236 | `			if( pChunk ){` |
|        27 | 7237 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7238 | `			}` |
|        27 | 7239 | `			break;` |
|         - | 7240 | `		}` |
|        87 | 7241 | `		if( nChunk < 1 ){` |
|        71 | 7242 | `			if( pChunk ){` |
|         - | 7243 | `				/* Put the first chunk */` |
|        45 | 7244 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7245 | `			}` |
|         - | 7246 | `			/* Create a new dimension */` |
|        71 | 7247 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7248 | `												   * will be automatically released as soon we return` |
|         - | 7249 | `												   * from this function */` |
|        71 | 7250 | `			if( pChunk == 0 ){` |
|       ! 0 | 7251 | `				break;` |
|         - | 7252 | `			}` |
|        71 | 7253 | `			nChunk = nSize;` |
|        35 | 7254 | `		}` |
|         - | 7255 | `		/* Insert the entry */` |
|        87 | 7256 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7257 | `		/* Point to the next entry */` |
|        87 | 7258 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7259 | `		nChunk--;` |
|        87 | 7260 | `		n--;` |
|         1 | 7261 | `	}` |
|         - | 7262 | `	/* Return the multidimensional array */` |
|        27 | 7263 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7264 | `	return PH7_OK;` |
|        23 | 7265 | `}` |
|         - | 7266 | `/*` |
|         - | 7267 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7268 | ` *  Pad array to the specified length with a value.` |
|         - | 7269 | ` * $input` |
|         - | 7270 | ` *   Initial array of values to pad.` |
|         - | 7271 | ` * $pad_size` |
|         - | 7272 | ` *   New size of the array.` |
|         - | 7273 | ` * $pad_value` |
|         - | 7274 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7275 | ` */` |
|         - | 7276 | `/*` |
|         - | 7277 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7278 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7279 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7280 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7281 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7282 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7283 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7284 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7285 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7286 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7287 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7288 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7289 | ` */` |
|        50 | 7290 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7291 | `	ph7_context *pCtx,` |
|         - | 7292 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7293 | `	int iArg,              /* 1-based argument position */` |
|         - | 7294 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7295 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7296 | `	)` |
|         1 | 7297 | `{` |
|        51 | 7298 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7299 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7300 | `			"ValueError",` |
|         - | 7301 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7302 | `			zFunc,iArg,zParam` |
|         - | 7303 | `			);` |
|         - | 7304 | `	}` |
|        37 | 7305 | `	return SXRET_OK;` |
|        26 | 7306 | `}` |
|        62 | 7307 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7308 | `{` |
|         - | 7309 | `	ph7_hashmap *pMap;` |
|         - | 7310 | `	ph7_value *pArray;` |
|         - | 7311 | `	sxi64 iLen,iAbs;` |
|         - | 7312 | `	int nEntry;` |
|         - | 7313 | `	sxi32 rc;` |
|        65 | 7314 | `	if( nArg != 3 ){` |
|         4 | 7315 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7316 | `			"ArgumentCountError",` |
|         - | 7317 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7318 | `			nArg` |
|         - | 7319 | `			);` |
|         - | 7320 | `	}` |
|        62 | 7321 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7322 | `		char zBuf[64];` |
|        11 | 7323 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7324 | `			"TypeError",` |
|         - | 7325 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7326 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7327 | `			);` |
|         - | 7328 | `	}` |
|         - | 7329 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7330 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7331 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7332 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7333 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7334 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7335 | `		char zBuf[64];` |
|       ! 0 | 7336 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7337 | `			"TypeError",` |
|         - | 7338 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7339 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7340 | `			);` |
|         - | 7341 | `	}` |
|        55 | 7342 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7343 | `		int nStr;` |
|        11 | 7344 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7345 | `		sxi64 iLong; double dReal;` |
|        11 | 7346 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7347 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7348 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7349 | `				"TypeError",` |
|         - | 7350 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7351 | `				);` |
|         - | 7352 | `		}` |
|         7 | 7353 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7354 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7355 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7356 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7357 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7358 | `					"TypeError",` |
|         - | 7359 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7360 | `					);` |
|         - | 7361 | `			}` |
|         3 | 7362 | `			iLen = (sxi64)dReal;` |
|         3 | 7363 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7364 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7365 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7366 | `					zStr` |
|         - | 7367 | `					);` |
|       ! 0 | 7368 | `			}` |
|         2 | 7369 | `		}else{` |
|         5 | 7370 | `			iLen = iLong;` |
|         - | 7371 | `		}` |
|         4 | 7372 | `	}else{` |
|        45 | 7373 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7374 | `	}` |
|         - | 7375 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7376 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7377 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7378 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7379 | `	 * overflow). */` |
|        51 | 7380 | `	iAbs = iLen;` |
|        51 | 7381 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7382 | `		iAbs = -iAbs;` |
|         7 | 7383 | `	}` |
|        51 | 7384 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7385 | `	if( rc != SXRET_OK ){` |
|        15 | 7386 | `		return rc;` |
|         - | 7387 | `	}` |
|        37 | 7388 | `	nEntry = (int)iLen;` |
|         - | 7389 | `	/* Create a new array */` |
|        37 | 7390 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7391 | `	if( pArray == 0 ){` |
|       ! 0 | 7392 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7393 | `	}` |
|        37 | 7394 | `	if( nEntry < 0 ){` |
|        11 | 7395 | `		nEntry = -nEntry;` |
|        11 | 7396 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7397 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7398 | `			/* Insert given items first */` |
|        25 | 7399 | `			while( nEntry > 0 ){` |
|        19 | 7400 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7401 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7402 | `				}` |
|        19 | 7403 | `				nEntry--;` |
|         1 | 7404 | `			}` |
|         - | 7405 | `			/* Merge the two arrays */` |
|         7 | 7406 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7407 | `		}else{` |
|         5 | 7408 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7409 | `		}` |
|        32 | 7410 | `	}else if( nEntry > 0 ){` |
|        25 | 7411 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7412 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7413 | `			/* Merge the two arrays first */` |
|        19 | 7414 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7415 | `			/* Insert given items */` |
|       275 | 7416 | `			while( nEntry > 0 ){` |
|       257 | 7417 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7418 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7419 | `				}` |
|       257 | 7420 | `				nEntry--;` |
|         1 | 7421 | `			}` |
|        10 | 7422 | `		}else{` |
|         7 | 7423 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7424 | `		}` |
|        13 | 7425 | `	}else{` |
|         - | 7426 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7427 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7428 | `	}` |
|         - | 7429 | `	/* Return the new array */` |
|        37 | 7430 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7431 | `	return PH7_OK;` |
|        34 | 7432 | `}` |
|         - | 7433 | `/*` |
|         - | 7434 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7435 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7436 | ` * Parameters` |
|         - | 7437 | ` * $array` |
|         - | 7438 | ` *   The array in which elements are replaced.` |
|         - | 7439 | ` * $array1` |
|         - | 7440 | ` *   The array from which elements will be extracted.` |
|         - | 7441 | ` * ....` |
|         - | 7442 | ` *  More arrays from which elements will be extracted.` |
|         - | 7443 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7444 | ` * Return` |
|         - | 7445 | ` *  Returns an array.` |
|         - | 7446 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7447 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7448 | ` */` |
|        20 | 7449 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7450 | `{` |
|         - | 7451 | `	ph7_hashmap *pMap;` |
|         - | 7452 | `	ph7_value *pArray;` |
|         - | 7453 | `	int i;` |
|        23 | 7454 | `	if( nArg < 1 ){` |
|       ! 0 | 7455 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7456 | `			"ArgumentCountError",` |
|         - | 7457 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7458 | `			);` |
|         - | 7459 | `	}` |
|        23 | 7460 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7461 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7462 | `			"TypeError",` |
|         - | 7463 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7464 | `			ph7_type_name(apArg[0])` |
|         - | 7465 | `			);` |
|         - | 7466 | `	}` |
|         - | 7467 | `	/* Create a new array */` |
|        20 | 7468 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7469 | `	if( pArray == 0 ){` |
|       ! 0 | 7470 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7471 | `		return PH7_OK;` |
|         - | 7472 | `	}` |
|         - | 7473 | `	/* Overwrite from the first array */` |
|        20 | 7474 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7475 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7476 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7477 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7478 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7479 | `			/* Type mismatch -> TypeError */` |
|         4 | 7480 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7481 | `				"TypeError",` |
|         - | 7482 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7483 | `				i + 1,` |
|         2 | 7484 | `				ph7_type_name(apArg[i])` |
|         - | 7485 | `				);` |
|         - | 7486 | `		}` |
|         - | 7487 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7488 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7489 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7490 | `	}` |
|         - | 7491 | `	/* Return the new array */` |
|        17 | 7492 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7493 | `	return PH7_OK;` |
|        13 | 7494 | `}` |
|         - | 7495 | `/*` |
|         - | 7496 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7497 | ` *  Filters elements of an array using a callback function.` |
|         - | 7498 | ` * Parameters` |
|         - | 7499 | ` *  $input` |
|         - | 7500 | ` *    The array to iterate over` |
|         - | 7501 | ` * $callback` |
|         - | 7502 | ` *    The callback function to use` |
|         - | 7503 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7504 | ` *    will be removed.` |
|         - | 7505 | ` * Return` |
|         - | 7506 | ` *  The filtered array.` |
|         - | 7507 | ` */` |
|        30 | 7508 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7509 | `{` |
|         - | 7510 | `	ph7_hashmap_node *pEntry;` |
|         - | 7511 | `	ph7_hashmap *pMap;` |
|         - | 7512 | `	ph7_value *pArray;` |
|         - | 7513 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7514 | `	ph7_value *pValue;` |
|         - | 7515 | `	sxi32 rc;` |
|         - | 7516 | `	int keep;` |
|         - | 7517 | `	sxu32 n;` |
|        32 | 7518 | `	if( nArg < 1 ){` |
|         - | 7519 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7520 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7521 | `		return PH7_OK;` |
|         - | 7522 | `	}` |
|         - | 7523 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7524 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7525 | `		char zBuf[64];` |
|        19 | 7526 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7527 | `			"TypeError",` |
|         - | 7528 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7529 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7530 | `			);` |
|         - | 7531 | `	}` |
|         - | 7532 | `	/* Create a new array */` |
|        20 | 7533 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7534 | `	if( pArray == 0 ){` |
|       ! 0 | 7535 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7536 | `		return PH7_OK;` |
|         - | 7537 | `	}` |
|         - | 7538 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7539 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7540 | `	pEntry = pMap->pFirst;` |
|        20 | 7541 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7542 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7543 | `	/* Perform the requested operation */` |
|        78 | 7544 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7545 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7546 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7547 | `		if( pValue == 0 ){` |
|         - | 7548 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7549 | `			keep = FALSE;` |
|        64 | 7550 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7551 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7552 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7553 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7554 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7555 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7556 | `					int len;` |
|         3 | 7557 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7558 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7559 | `						"TypeError",` |
|         - | 7560 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7561 | `						zName` |
|         - | 7562 | `						);` |
|       ! 0 | 7563 | `				}else{` |
|       ! 0 | 7564 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7565 | `						"TypeError",` |
|         - | 7566 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7567 | `						ph7_type_name(apArg[1])` |
|         - | 7568 | `						);` |
|         - | 7569 | `				}` |
|         - | 7570 | `			}` |
|        33 | 7571 | `			keep = FALSE;` |
|        33 | 7572 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7573 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7574 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7575 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7576 | `				return PH7_EXCEPTION;` |
|         - | 7577 | `			}` |
|        31 | 7578 | `			if( rc == SXRET_OK ){` |
|         - | 7579 | `				/* Perform a boolean cast */` |
|        31 | 7580 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7581 | `			}` |
|        31 | 7582 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7583 | `		}else{` |
|         - | 7584 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7585 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7586 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7587 | `			 */` |
|        29 | 7588 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7589 | `		}` |
|        59 | 7590 | `		if( keep ){` |
|         - | 7591 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7592 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7593 | `		}` |
|         - | 7594 | `		/* Point to the next entry */` |
|        59 | 7595 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7596 | `	}` |
|        15 | 7597 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7598 | `	return PH7_OK;` |
|        17 | 7599 | `}` |
|         - | 7600 | `/*` |
|         - | 7601 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7602 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7603 | ` * Parameters` |
|         - | 7604 | ` *  $callback` |
|         - | 7605 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7606 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7607 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7608 | ` *   are zipped together.` |
|         - | 7609 | ` *  $array` |
|         - | 7610 | ` *   The first array to run through the callback function.` |
|         - | 7611 | ` *  $arrays` |
|         - | 7612 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7613 | ` * Return` |
|         - | 7614 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7615 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7616 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7617 | ` *  padding shorter arrays with NULL.` |
|         - | 7618 | ` */` |
|        58 | 7619 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7620 | `{` |
|         - | 7621 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7622 | `	ph7_hashmap_node *pEntry;` |
|         - | 7623 | `	ph7_hashmap *pMap;` |
|         - | 7624 | `	ph7_vm *pVm;` |
|         - | 7625 | `	int bNullCallback;` |
|         - | 7626 | `	sxi32 rc;` |
|         - | 7627 | `	int i;` |
|         - | 7628 | `	sxu32 n;` |
|        62 | 7629 | `	if( nArg < 2 ){` |
|       ! 0 | 7630 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7631 | `			"ArgumentCountError",` |
|         - | 7632 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7633 | `			nArg` |
|         - | 7634 | `			);` |
|         - | 7635 | `	}` |
|        62 | 7636 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        62 | 7637 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7638 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7639 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7640 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7641 | `				"TypeError",` |
|         - | 7642 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7643 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7644 | `				zFunc` |
|         - | 7645 | `				);` |
|         - | 7646 | `		}` |
|         3 | 7647 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7648 | `			"TypeError",` |
|         - | 7649 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7650 | `			"no array or string given"` |
|         - | 7651 | `			);` |
|         - | 7652 | `	}` |
|         - | 7653 | `	/* Every remaining argument must be an array */` |
|       121 | 7654 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7655 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7656 | `			if( i == 1 ){` |
|         4 | 7657 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7658 | `					"TypeError",` |
|         - | 7659 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7660 | `					ph7_type_name(apArg[1])` |
|         - | 7661 | `					);` |
|         - | 7662 | `			}` |
|       ! 0 | 7663 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7664 | `				"TypeError",` |
|         - | 7665 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7666 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7667 | `				);` |
|         - | 7668 | `		}` |
|        34 | 7669 | `	}` |
|        54 | 7670 | `	pVm = pCtx->pVm;` |
|         - | 7671 | `	/* Create a new array */` |
|        54 | 7672 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7673 | `	if( pArray == 0 ){` |
|       ! 0 | 7674 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7675 | `		return PH7_OK;` |
|         - | 7676 | `	}` |
|        54 | 7677 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7678 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7679 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7680 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7681 | `	if( nArg == 2 ){` |
|         - | 7682 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7683 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7684 | `		pEntry = pMap->pFirst;` |
|       134 | 7685 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7686 | `			/* Extract the node value */` |
|        96 | 7687 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7688 | `			if( pValue ){` |
|         - | 7689 | `				/* Extract the node key */` |
|        96 | 7690 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7691 | `				if( bNullCallback ){` |
|         - | 7692 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7693 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7694 | `				}else{` |
|         - | 7695 | `					/* Invoke the supplied callback */` |
|        86 | 7696 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7697 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7698 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7699 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7700 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7701 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7702 | `						return PH7_EXCEPTION;` |
|         - | 7703 | `					}` |
|         - | 7704 | `					/* Insert the callback return value */` |
|        82 | 7705 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7706 | `				}` |
|        92 | 7707 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7708 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7709 | `			}` |
|         - | 7710 | `			/* Point to the next entry */` |
|        92 | 7711 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7712 | `		}` |
|        21 | 7713 | `	}else{` |
|         - | 7714 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7715 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7716 | `		int nArrays = nArg - 1;` |
|         - | 7717 | `		ph7_hashmap_node **apCur;` |
|         - | 7718 | `		ph7_value **apCallArg;` |
|         - | 7719 | `		ph7_value sNull;` |
|        11 | 7720 | `		sxu32 nMax = 0;` |
|        11 | 7721 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7722 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7723 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7724 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7725 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7726 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7727 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7728 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7729 | `			return PH7_OK;` |
|         - | 7730 | `		}` |
|        11 | 7731 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7732 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7733 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7734 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7735 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7736 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7737 | `				nMax = pMap->nEntry;` |
|         6 | 7738 | `			}` |
|        12 | 7739 | `		}` |
|        35 | 7740 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7741 | `			ph7_value *pZip = 0;` |
|        25 | 7742 | `			if( bNullCallback ){` |
|         - | 7743 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7744 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7745 | `			}` |
|        79 | 7746 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7747 | `				ph7_value *pv = &sNull;` |
|        55 | 7748 | `				if( apCur[i] ){` |
|        53 | 7749 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7750 | `					if( pNodeVal ){` |
|        53 | 7751 | `						pv = pNodeVal;` |
|        26 | 7752 | `					}` |
|        53 | 7753 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7754 | `				}` |
|        55 | 7755 | `				if( bNullCallback ){` |
|         9 | 7756 | `					if( pZip ){` |
|         9 | 7757 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7758 | `					}` |
|         5 | 7759 | `				}else{` |
|        47 | 7760 | `					apCallArg[i] = pv;` |
|         - | 7761 | `				}` |
|        28 | 7762 | `			}` |
|        25 | 7763 | `			if( bNullCallback ){` |
|         5 | 7764 | `				if( pZip ){` |
|         5 | 7765 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7766 | `				}` |
|         3 | 7767 | `			}else{` |
|        21 | 7768 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7769 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7770 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7771 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7772 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7773 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7774 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7775 | `					return PH7_EXCEPTION;` |
|         - | 7776 | `				}` |
|        21 | 7777 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7778 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7779 | `			}` |
|        13 | 7780 | `		}` |
|        11 | 7781 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7782 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7783 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7784 | `	}` |
|        50 | 7785 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7786 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7787 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7788 | `	return PH7_OK;` |
|        33 | 7789 | `}` |
|         - | 7790 | `/*` |
|         - | 7791 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7792 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7793 | ` * Parameters` |
|         - | 7794 | ` *  $array` |
|         - | 7795 | ` *   The input array.` |
|         - | 7796 | ` *  $callback` |
|         - | 7797 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7798 | ` *  $initial` |
|         - | 7799 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7800 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7801 | ` * Return` |
|         - | 7802 | ` *  Returns the resulting value.` |
|         - | 7803 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7804 | ` */` |
|        30 | 7805 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7806 | `{` |
|         - | 7807 | `	ph7_hashmap_node *pEntry;` |
|         - | 7808 | `	ph7_hashmap *pMap;` |
|         - | 7809 | `	ph7_value *pValue;` |
|         - | 7810 | `	ph7_value sResult;` |
|         - | 7811 | `	sxi32 rc;` |
|         - | 7812 | `	sxu32 n;` |
|        35 | 7813 | `	if( nArg < 2 ){` |
|       ! 0 | 7814 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7815 | `			"ArgumentCountError",` |
|         - | 7816 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7817 | `			nArg` |
|         - | 7818 | `			);` |
|         - | 7819 | `	}` |
|        35 | 7820 | `	if( nArg > 3 ){` |
|         4 | 7821 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7822 | `			"ArgumentCountError",` |
|         - | 7823 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7824 | `			nArg` |
|         - | 7825 | `			);` |
|         - | 7826 | `	}` |
|        33 | 7827 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7828 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7829 | `			"TypeError",` |
|         - | 7830 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7831 | `			ph7_type_name(apArg[0])` |
|         - | 7832 | `			);` |
|         - | 7833 | `	}` |
|        31 | 7834 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7835 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7836 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7837 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7838 | `				"TypeError",` |
|         - | 7839 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7840 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7841 | `				zFunc` |
|         - | 7842 | `				);` |
|         - | 7843 | `		}` |
|         9 | 7844 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7845 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7846 | `				"TypeError",` |
|         - | 7847 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7848 | `				"array callback must have exactly two members"` |
|         - | 7849 | `				);` |
|         - | 7850 | `		}` |
|         6 | 7851 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7852 | `			"TypeError",` |
|         - | 7853 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7854 | `			"no array or string given"` |
|         - | 7855 | `			);` |
|         - | 7856 | `	}` |
|         - | 7857 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7858 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7859 | `	/* Assume a NULL initial value */` |
|        19 | 7860 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7861 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7862 | `	if( nArg > 2 ){` |
|         - | 7863 | `		/* Set the initial value */` |
|        13 | 7864 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7865 | `	}` |
|         - | 7866 | `	/* Perform the requested operation */` |
|        19 | 7867 | `	pEntry = pMap->pFirst;` |
|        55 | 7868 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7869 | `		/* Extract the node value */` |
|        39 | 7870 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7871 | `		/* Invoke the supplied callback */` |
|        39 | 7872 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7873 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7874 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7875 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7876 | `			return PH7_EXCEPTION;` |
|         - | 7877 | `		}` |
|         - | 7878 | `		/* Point to the next entry */` |
|        37 | 7879 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7880 | `	}` |
|        17 | 7881 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7882 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7883 | `	return PH7_OK;` |
|        20 | 7884 | `}` |
|         - | 7885 | `/*` |
|         - | 7886 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7887 | ` *  Apply a user function to every member of an array.` |
|         - | 7888 | ` * Parameters` |
|         - | 7889 | ` *  $array` |
|         - | 7890 | ` *   The input array.` |
|         - | 7891 | ` *  $funcname` |
|         - | 7892 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7893 | ` *   the first, and the key/index second.` |
|         - | 7894 | ` * Note:` |
|         - | 7895 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7896 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7897 | ` *  be made in the original array itself.` |
|         - | 7898 | ` *  $userdata` |
|         - | 7899 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7900 | ` *   to the callback funcname.` |
|         - | 7901 | ` * Return` |
|         - | 7902 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7903 | ` */` |
|        34 | 7904 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7905 | `{` |
|         - | 7906 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7907 | `	ph7_hashmap_node *pEntry;` |
|         - | 7908 | `	ph7_hashmap *pMap;` |
|         - | 7909 | `	sxu32 n;` |
|        39 | 7910 | `	if( nArg < 2 ){` |
|       ! 0 | 7911 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7912 | `			"ArgumentCountError",` |
|         - | 7913 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7914 | `			nArg` |
|         - | 7915 | `			);` |
|         - | 7916 | `	}` |
|        39 | 7917 | `	if( nArg > 3 ){` |
|         4 | 7918 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7919 | `			"ArgumentCountError",` |
|         - | 7920 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7921 | `			nArg` |
|         - | 7922 | `			);` |
|         - | 7923 | `	}` |
|        37 | 7924 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7925 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7926 | `			"TypeError",` |
|         - | 7927 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7928 | `			ph7_type_name(apArg[0])` |
|         - | 7929 | `			);` |
|         - | 7930 | `	}` |
|        35 | 7931 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7932 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7933 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7934 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7935 | `				"TypeError",` |
|         - | 7936 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7937 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7938 | `				zFunc` |
|         - | 7939 | `				);` |
|         - | 7940 | `		}` |
|        12 | 7941 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7942 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7943 | `				"TypeError",` |
|         - | 7944 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7945 | `				"array callback must have exactly two members"` |
|         - | 7946 | `				);` |
|         - | 7947 | `		}` |
|         6 | 7948 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7949 | `			"TypeError",` |
|         - | 7950 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7951 | `			"no array or string given"` |
|         - | 7952 | `			);` |
|         - | 7953 | `	}` |
|        21 | 7954 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7955 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7956 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7957 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7958 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7959 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7960 | `	/* Perform the desired operation */` |
|        21 | 7961 | `	pEntry = pMap->pFirst;` |
|        61 | 7962 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7963 | `		/* Extract the node value */` |
|        43 | 7964 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7965 | `		if( pValue ){` |
|         - | 7966 | `			sxi32 rcW;` |
|         - | 7967 | `			/* Extract the entry key */` |
|        43 | 7968 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7969 | `			/* Invoke the supplied callback */` |
|        43 | 7970 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7971 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7972 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7973 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7974 | `				return PH7_EXCEPTION;` |
|         - | 7975 | `			}` |
|        20 | 7976 | `		}` |
|         - | 7977 | `		/* Point to the next entry */` |
|        41 | 7978 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7979 | `	}` |
|         - | 7980 | `	/* All done, return TRUE */` |
|        19 | 7981 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7982 | `	return PH7_OK;` |
|        22 | 7983 | `}` |
|         - | 7984 | `/*` |
|         - | 7985 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7986 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7987 | ` */` |
|        22 | 7988 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7989 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7990 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7991 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7992 | `	int iNest             /* Nesting level */` |
|         - | 7993 | `	)` |
|         1 | 7994 | `{` |
|         - | 7995 | `	ph7_hashmap_node *pEntry;` |
|         - | 7996 | `	ph7_value *pValue,sKey;` |
|         - | 7997 | `	sxi32 rc;` |
|         - | 7998 | `	sxu32 n;` |
|         - | 7999 | `	/* Iterate through hashmap entries */` |
|        23 | 8000 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 8001 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 8002 | `	pEntry = pMap->pFirst;` |
|        59 | 8003 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8004 | `		/* Extract the node value */` |
|        37 | 8005 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 8006 | `		if( pValue ){` |
|        37 | 8007 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 8008 | `				if( iNest < 32 ){` |
|         - | 8009 | `					/* Recurse */` |
|        11 | 8010 | `					iNest++;` |
|        11 | 8011 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 8012 | `					iNest--;` |
|        11 | 8013 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 8014 | `						return PH7_EXCEPTION;` |
|         - | 8015 | `					}` |
|         5 | 8016 | `				}` |
|         6 | 8017 | `			}else{` |
|         - | 8018 | `				/* Extract the node key */` |
|        27 | 8019 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 8020 | `				/* Invoke the supplied callback */` |
|        27 | 8021 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 8022 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 8023 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 8024 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8025 | `					return PH7_EXCEPTION;` |
|         - | 8026 | `				}` |
|         - | 8027 | `			}` |
|        18 | 8028 | `		}` |
|         - | 8029 | `		/* Point to the next entry */` |
|        37 | 8030 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8031 | `	}` |
|        23 | 8032 | `	return PH7_OK;` |
|        12 | 8033 | `}` |
|         - | 8034 | `/*` |
|         - | 8035 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8036 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8037 | ` * Parameters` |
|         - | 8038 | ` *  $array` |
|         - | 8039 | ` *   The input array.` |
|         - | 8040 | ` *  $funcname` |
|         - | 8041 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8042 | ` *   the first, and the key/index second.` |
|         - | 8043 | ` * Note:` |
|         - | 8044 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8045 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8046 | ` *  be made in the original array itself.` |
|         - | 8047 | ` *  $userdata` |
|         - | 8048 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8049 | ` *   to the callback funcname.` |
|         - | 8050 | ` * Return` |
|         - | 8051 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8052 | ` */` |
|        26 | 8053 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8054 | `{` |
|         - | 8055 | `	ph7_hashmap *pMap;` |
|        31 | 8056 | `	if( nArg < 2 ){` |
|       ! 0 | 8057 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8058 | `			"ArgumentCountError",` |
|         - | 8059 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8060 | `			nArg` |
|         - | 8061 | `			);` |
|         - | 8062 | `	}` |
|        31 | 8063 | `	if( nArg > 3 ){` |
|         4 | 8064 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8065 | `			"ArgumentCountError",` |
|         - | 8066 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8067 | `			nArg` |
|         - | 8068 | `			);` |
|         - | 8069 | `	}` |
|        29 | 8070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8071 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8072 | `			"TypeError",` |
|         - | 8073 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8074 | `			ph7_type_name(apArg[0])` |
|         - | 8075 | `			);` |
|         - | 8076 | `	}` |
|        27 | 8077 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8078 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8079 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8080 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8081 | `				"TypeError",` |
|         - | 8082 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8083 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8084 | `				zFunc` |
|         - | 8085 | `				);` |
|         - | 8086 | `		}` |
|        12 | 8087 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8088 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8089 | `				"TypeError",` |
|         - | 8090 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8091 | `				"array callback must have exactly two members"` |
|         - | 8092 | `				);` |
|         - | 8093 | `		}` |
|         6 | 8094 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8095 | `			"TypeError",` |
|         - | 8096 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8097 | `			"no array or string given"` |
|         - | 8098 | `			);` |
|         - | 8099 | `	}` |
|         - | 8100 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8101 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8102 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8103 | `	/* Perform the desired operation */` |
|        13 | 8104 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8105 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8106 | `		return PH7_EXCEPTION;` |
|         - | 8107 | `	}` |
|         - | 8108 | `	/* All done, return TRUE */` |
|        13 | 8109 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8110 | `	return PH7_OK;` |
|        18 | 8111 | `}` |
|         - | 8112 | `/*` |
|         - | 8113 | ` * bool array_is_list(array $array)` |
|         - | 8114 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8115 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8116 | ` * Return` |
|         - | 8117 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8118 | ` */` |
|         - | 8119 | `/*` |
|         - | 8120 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8121 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8122 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8123 | ` */` |
|       246 | 8124 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8125 | `{` |
|       247 | 8126 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       247 | 8127 | `	sxi64 iExpect = 0;` |
|         - | 8128 | `	sxu32 n;` |
|       555 | 8129 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       409 | 8130 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8131 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8132 | `			return 0;` |
|         - | 8133 | `		}` |
|       309 | 8134 | `		++iExpect;` |
|       309 | 8135 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       155 | 8136 | `	}` |
|       147 | 8137 | `	return 1;` |
|       124 | 8138 | `}` |
|        12 | 8139 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8140 | `{` |
|        13 | 8141 | `	if( nArg < 1 ){` |
|       ! 0 | 8142 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8143 | `			"ArgumentCountError",` |
|         - | 8144 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8145 | `			);` |
|         - | 8146 | `	}` |
|        13 | 8147 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8148 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8149 | `			"TypeError",` |
|         - | 8150 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8151 | `			ph7_type_name(apArg[0])` |
|         - | 8152 | `			);` |
|         - | 8153 | `	}` |
|        13 | 8154 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8155 | `	return PH7_OK;` |
|         7 | 8156 | `}` |
|         - | 8157 | `/*` |
|         - | 8158 | ` * mixed array_first(array $array)` |
|         - | 8159 | ` * mixed array_last(array $array)` |
|         - | 8160 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8161 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8162 | ` *  untouched (unlike reset()/end()).` |
|         - | 8163 | ` */` |
|        18 | 8164 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8165 | `{` |
|         - | 8166 | `	ph7_hashmap *pMap;` |
|         - | 8167 | `	ph7_hashmap_node *pNode;` |
|         - | 8168 | `	ph7_value *pVal;` |
|        19 | 8169 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8170 | `	if( nArg < 1 ){` |
|       ! 0 | 8171 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8172 | `			"ArgumentCountError",` |
|         - | 8173 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8174 | `			zName` |
|         - | 8175 | `			);` |
|         - | 8176 | `	}` |
|        19 | 8177 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8178 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8179 | `			"TypeError",` |
|         - | 8180 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8181 | `			zName,` |
|         1 | 8182 | `			ph7_type_name(apArg[0])` |
|         - | 8183 | `			);` |
|         - | 8184 | `	}` |
|        17 | 8185 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8186 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8187 | `	if( pNode == 0 ){` |
|         - | 8188 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8189 | `		ph7_result_null(pCtx);` |
|         5 | 8190 | `		return PH7_OK;` |
|         - | 8191 | `	}` |
|        13 | 8192 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8193 | `	if( pVal ){` |
|        13 | 8194 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8195 | `	}else{` |
|       ! 0 | 8196 | `		ph7_result_null(pCtx);` |
|         - | 8197 | `	}` |
|        13 | 8198 | `	return PH7_OK;` |
|        10 | 8199 | `}` |
|         8 | 8200 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8201 | `{` |
|         9 | 8202 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8203 | `}` |
|        10 | 8204 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8205 | `{` |
|        11 | 8206 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8207 | `}` |
|         - | 8208 | `/*` |
|         - | 8209 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8210 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8211 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8212 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8213 | ` *  untouched.` |
|         - | 8214 | ` */` |
|        22 | 8215 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8216 | `{` |
|         - | 8217 | `	ph7_hashmap *pMap;` |
|         - | 8218 | `	ph7_hashmap_node *pNode;` |
|        23 | 8219 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8220 | `	if( nArg < 1 ){` |
|       ! 0 | 8221 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8222 | `			"ArgumentCountError",` |
|         - | 8223 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8224 | `			zName` |
|         - | 8225 | `			);` |
|         - | 8226 | `	}` |
|        23 | 8227 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8228 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8229 | `			"TypeError",` |
|         - | 8230 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8231 | `			zName,` |
|         1 | 8232 | `			ph7_type_name(apArg[0])` |
|         - | 8233 | `			);` |
|         - | 8234 | `	}` |
|        21 | 8235 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8236 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8237 | `	if( pNode == 0 ){` |
|         - | 8238 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8239 | `		ph7_result_null(pCtx);` |
|         5 | 8240 | `		return PH7_OK;` |
|         - | 8241 | `	}` |
|        17 | 8242 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8243 | `	return PH7_OK;` |
|        12 | 8244 | `}` |
|        10 | 8245 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8246 | `{` |
|        11 | 8247 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8248 | `}` |
|        12 | 8249 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8250 | `{` |
|        13 | 8251 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8252 | `}` |
|         - | 8253 | `/*` |
|         - | 8254 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8255 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8256 | ` * array_column() for both the column value and the index key.` |
|         - | 8257 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8258 | ` * container or the key is absent.` |
|         - | 8259 | ` */` |
|        32 | 8260 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8261 | `{` |
|        33 | 8262 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8263 | `		ph7_hashmap_node *pNode;` |
|        25 | 8264 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8265 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8266 | `		}` |
|        11 | 8267 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8268 | `		ph7_value sName;` |
|         - | 8269 | `		const char *zName;` |
|         - | 8270 | `		ph7_value *pAttr;` |
|         - | 8271 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8272 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8273 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8274 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8275 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8276 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8277 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8278 | `		return pAttr;` |
|         - | 8279 | `	}` |
|         5 | 8280 | `	return 0;` |
|        17 | 8281 | `}` |
|         - | 8282 | `/*` |
|         - | 8283 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8284 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8285 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8286 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8287 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8288 | ` *  Each row may be an array or an object.` |
|         - | 8289 | ` */` |
|        12 | 8290 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8291 | `{` |
|         - | 8292 | `	ph7_hashmap_node *pNode;` |
|         - | 8293 | `	ph7_hashmap *pMap;` |
|         - | 8294 | `	ph7_value *pArray;` |
|         - | 8295 | `	ph7_value *pRow;` |
|         - | 8296 | `	ph7_value *pCol;` |
|         - | 8297 | `	ph7_value *pIdx;` |
|         - | 8298 | `	int bWantCol;` |
|         - | 8299 | `	int bWantIdx;` |
|         - | 8300 | `	sxu32 n;` |
|        13 | 8301 | `	if( nArg < 2 ){` |
|       ! 0 | 8302 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8303 | `			"ArgumentCountError",` |
|         - | 8304 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8305 | `			nArg` |
|         - | 8306 | `			);` |
|         - | 8307 | `	}` |
|        13 | 8308 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8309 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8310 | `			"TypeError",` |
|         - | 8311 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8312 | `			ph7_type_name(apArg[0])` |
|         - | 8313 | `			);` |
|         - | 8314 | `	}` |
|        13 | 8315 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8316 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8317 | `	if( pArray == 0 ){` |
|       ! 0 | 8318 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8319 | `		return PH7_OK;` |
|         - | 8320 | `	}` |
|         - | 8321 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8322 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8323 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8324 | `	pNode = pMap->pFirst;` |
|        33 | 8325 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8326 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8327 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8328 | `		if( pRow == 0 ){` |
|       ! 0 | 8329 | `			continue;` |
|         - | 8330 | `		}` |
|        21 | 8331 | `		if( bWantCol ){` |
|        19 | 8332 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8333 | `			if( pCol == 0 ){` |
|         - | 8334 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8335 | `				continue;` |
|         - | 8336 | `			}` |
|         9 | 8337 | `		}else{` |
|         3 | 8338 | `			pCol = pRow;` |
|         - | 8339 | `		}` |
|        19 | 8340 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8341 | `		if( pIdx ){` |
|        13 | 8342 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8343 | `		}else{` |
|         7 | 8344 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8345 | `		}` |
|        10 | 8346 | `	}` |
|        13 | 8347 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8348 | `	return PH7_OK;` |
|         7 | 8349 | `}` |
|         - | 8350 | `/*` |
|         - | 8351 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8352 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8353 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8354 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8355 | ` */` |
|        28 | 8356 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8357 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8358 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8359 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8360 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8361 | `	)` |
|         1 | 8362 | `{` |
|         - | 8363 | `	ph7_hashmap_node *pEntry;` |
|         - | 8364 | `	ph7_hashmap *pMap;` |
|         - | 8365 | `	ph7_value *pValue;` |
|         - | 8366 | `	ph7_value *apCbArg[2];` |
|         - | 8367 | `	ph7_value sKey;` |
|         - | 8368 | `	ph7_value sResult;` |
|         - | 8369 | `	sxi32 rc;` |
|         - | 8370 | `	sxu32 n;` |
|        29 | 8371 | `	*ppMatch = 0;` |
|        29 | 8372 | `	if( nArg < 2 ){` |
|       ! 0 | 8373 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8374 | `			"ArgumentCountError",` |
|         - | 8375 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8376 | `			zName,nArg` |
|         - | 8377 | `			);` |
|         - | 8378 | `	}` |
|        29 | 8379 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8380 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8381 | `			"TypeError",` |
|         - | 8382 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8383 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8384 | `			);` |
|         - | 8385 | `	}` |
|        29 | 8386 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8387 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8388 | `			"TypeError",` |
|         - | 8389 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8390 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8391 | `			);` |
|         - | 8392 | `	}` |
|        29 | 8393 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8394 | `	pEntry = pMap->pFirst;` |
|        29 | 8395 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8396 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8397 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8398 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8399 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8400 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8401 | `		if( pValue ){` |
|         - | 8402 | `			/* The callback receives ($value, $key). */` |
|        59 | 8403 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8404 | `			apCbArg[0] = pValue;` |
|        59 | 8405 | `			apCbArg[1] = &sKey;` |
|        59 | 8406 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8407 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8408 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8409 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8410 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8411 | `				return PH7_EXCEPTION;` |
|         - | 8412 | `			}` |
|        59 | 8413 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8414 | `				*ppMatch = pEntry;` |
|        15 | 8415 | `				break;` |
|         - | 8416 | `			}` |
|        22 | 8417 | `		}` |
|        45 | 8418 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8419 | `	}` |
|        29 | 8420 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8421 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8422 | `	return PH7_OK;` |
|        15 | 8423 | `}` |
|         - | 8424 | `/*` |
|         - | 8425 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8426 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8427 | ` *  is truthy, or NULL if none match.` |
|         - | 8428 | ` */` |
|         6 | 8429 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8430 | `{` |
|         - | 8431 | `	ph7_hashmap_node *pMatch;` |
|         - | 8432 | `	ph7_value *pVal;` |
|         - | 8433 | `	sxi32 rc;` |
|         7 | 8434 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8435 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8436 | `		return rc;` |
|         - | 8437 | `	}` |
|         7 | 8438 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8439 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8440 | `	}else{` |
|         3 | 8441 | `		ph7_result_null(pCtx);` |
|         - | 8442 | `	}` |
|         7 | 8443 | `	return PH7_OK;` |
|         4 | 8444 | `}` |
|         - | 8445 | `/*` |
|         - | 8446 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8447 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8448 | ` *  is truthy, or NULL if none match.` |
|         - | 8449 | ` */` |
|         6 | 8450 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8451 | `{` |
|         - | 8452 | `	ph7_hashmap_node *pMatch;` |
|         - | 8453 | `	sxi32 rc;` |
|         7 | 8454 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8455 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8456 | `		return rc;` |
|         - | 8457 | `	}` |
|         7 | 8458 | `	if( pMatch == 0 ){` |
|         3 | 8459 | `		ph7_result_null(pCtx);` |
|         6 | 8460 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8461 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8462 | `	}else{` |
|         4 | 8463 | `		ph7_result_string(pCtx,` |
|         2 | 8464 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8465 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8466 | `	}` |
|         7 | 8467 | `	return PH7_OK;` |
|         4 | 8468 | `}` |
|         - | 8469 | `/*` |
|         - | 8470 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8471 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8472 | ` *  FALSE for an empty array.` |
|         - | 8473 | ` */` |
|         8 | 8474 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8475 | `{` |
|         - | 8476 | `	ph7_hashmap_node *pMatch;` |
|         - | 8477 | `	sxi32 rc;` |
|         9 | 8478 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8479 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8480 | `		return rc;` |
|         - | 8481 | `	}` |
|         9 | 8482 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8483 | `	return PH7_OK;` |
|         5 | 8484 | `}` |
|         - | 8485 | `/*` |
|         - | 8486 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8487 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8488 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8489 | ` */` |
|         8 | 8490 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8491 | `{` |
|         - | 8492 | `	ph7_hashmap_node *pMatch;` |
|         - | 8493 | `	sxi32 rc;` |
|         9 | 8494 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8495 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8496 | `		return rc;` |
|         - | 8497 | `	}` |
|         9 | 8498 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8499 | `	return PH7_OK;` |
|         5 | 8500 | `}` |
|         - | 8501 | `/*` |
|         - | 8502 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8503 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8504 | ` */` |
|         - | 8505 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8506 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8507 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8508 | `{` |
|        84 | 8509 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8510 | `	(void)pVm;` |
|        84 | 8511 | `	p->nCount++;` |
|        84 | 8512 | `	if( p->pArray ){` |
|         - | 8513 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8514 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8515 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8516 | `	}` |
|        84 | 8517 | `	return SXRET_OK;` |
|         4 | 8518 | `}` |
|         - | 8519 | `/*` |
|         - | 8520 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8521 | ` */` |
|        30 | 8522 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8523 | `{` |
|         - | 8524 | `	struct IterCollect sCol;` |
|         - | 8525 | `	ph7_value *pArray;` |
|         - | 8526 | `	sxi32 rc;` |
|        34 | 8527 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8528 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8529 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8530 | `	sCol.pArray = pArray;` |
|        34 | 8531 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8532 | `	sCol.nCount = 0;` |
|        34 | 8533 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8534 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8535 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8536 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8537 | `		sxu32 n;` |
|         9 | 8538 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8539 | `			ph7_value sKey, *pVal;` |
|         7 | 8540 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8541 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8542 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8543 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8544 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8545 | `			pEntry = pEntry->pPrev;` |
|         4 | 8546 | `		}` |
|         3 | 8547 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8548 | `		return PH7_OK;` |
|         - | 8549 | `	}` |
|        32 | 8550 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8551 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8552 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8553 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8554 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8555 | `			ph7_type_name(apArg[0]));` |
|         - | 8556 | `	}` |
|        30 | 8557 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8558 | `	return PH7_OK;` |
|        19 | 8559 | `}` |
|         - | 8560 | `/*` |
|         - | 8561 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8562 | ` */` |
|         8 | 8563 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8564 | `{` |
|         - | 8565 | `	struct IterCollect sCol;` |
|         - | 8566 | `	sxi32 rc;` |
|         9 | 8567 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8568 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8569 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8570 | `		return PH7_OK;` |
|         - | 8571 | `	}` |
|         7 | 8572 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8573 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8574 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8575 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8576 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8577 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8578 | `			ph7_type_name(apArg[0]));` |
|         - | 8579 | `	}` |
|         7 | 8580 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8581 | `	return PH7_OK;` |
|         5 | 8582 | `}` |
|         - | 8583 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8584 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8585 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8586 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8587 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8588 | `{` |
|        33 | 8589 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8590 | `	ph7_value sResult;` |
|         - | 8591 | `	SySet aArg;` |
|         - | 8592 | `	sxi32 rc;` |
|         - | 8593 | `	int bContinue;` |
|        16 | 8594 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8595 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8596 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8597 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8598 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8599 | `		sxu32 n;` |
|        17 | 8600 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8601 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8602 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8603 | `			pEntry = pEntry->pPrev;` |
|         5 | 8604 | `		}` |
|         4 | 8605 | `	}` |
|        33 | 8606 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8607 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8608 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8609 | `	SySetRelease(&aArg);` |
|        33 | 8610 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8611 | `	p->nCount++;` |
|        31 | 8612 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8613 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8614 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8615 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8616 | `}` |
|         - | 8617 | `/*` |
|         - | 8618 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8619 | ` */` |
|        12 | 8620 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8621 | `{` |
|         - | 8622 | `	struct IterApply sApp;` |
|         - | 8623 | `	sxi32 rc;` |
|        13 | 8624 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8625 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8626 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8627 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8628 | `	}` |
|        13 | 8629 | `	sApp.pCallback = apArg[1];` |
|        13 | 8630 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8631 | `	sApp.nCount = 0;` |
|        13 | 8632 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8633 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8634 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8635 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8636 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8637 | `			ph7_type_name(apArg[0]));` |
|         - | 8638 | `	}` |
|        11 | 8639 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8640 | `	return PH7_OK;` |
|         7 | 8641 | `}` |
|         - | 8642 | `/*` |
|         - | 8643 | ` * Table of hashmap functions.` |
|         - | 8644 | ` */` |
|         - | 8645 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8646 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8647 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8648 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8649 | `	{"count",             ph7_hashmap_count },` |
|         - | 8650 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8651 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8652 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8653 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8654 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8655 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8656 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8657 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8658 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8659 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8660 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8661 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8662 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8663 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8664 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8665 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8666 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8667 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8668 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8669 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8670 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8671 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8672 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8673 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8674 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8675 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8676 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8677 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8678 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8679 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8680 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8681 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8682 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8683 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8684 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8685 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8686 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8687 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8688 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8689 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8690 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8691 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8692 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8693 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8694 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8695 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8696 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8697 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8698 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8699 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8700 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8701 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8702 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8703 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8704 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8705 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8706 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8707 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8708 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8709 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8710 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8711 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8712 | `	{"current",           ph7_hashmap_current },` |
|         - | 8713 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8714 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8715 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8716 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8717 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8718 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8719 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8720 | `};` |
|         - | 8721 | `/*` |
|         - | 8722 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8723 | ` */` |
|      3338 | 8724 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8725 | `{` |
|         - | 8726 | `	sxu32 n;` |
|    250355 | 8727 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    247017 | 8728 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    123511 | 8729 | `	}` |
|      3343 | 8730 | `}` |
|         - | 8731 | `/*` |
|         - | 8732 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8733 | ` * the BLOB given as the first argument.` |
|         - | 8734 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8735 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8736 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8737 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8738 | ` */` |
|         - | 8739 | `/*` |
|         - | 8740 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8741 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8742 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8743 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8744 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8745 | ` */` |
|       120 | 8746 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8747 | `{` |
|       123 | 8748 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8749 | `	ph7_value *pObj;` |
|       123 | 8750 | `	sxu32 n = 0;` |
|         - | 8751 | `	int isRef;` |
|       123 | 8752 | `	sxi32 rc = SXRET_OK;` |
|         - | 8753 | `	int i;` |
|       195 | 8754 | `	for(;;){` |
|       393 | 8755 | `		if( n >= pMap->nEntry ){` |
|       123 | 8756 | `			break;` |
|         - | 8757 | `		}` |
|       273 | 8758 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 8759 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 8760 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       541 | 8761 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       270 | 8762 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       273 | 8763 | `		if( ShowType ){` |
|         - | 8764 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8765 | `			 * on the next line at the same indent (php). */` |
|       105 | 8766 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        71 | 8767 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        37 | 8768 | `			}` |
|        37 | 8769 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8770 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8771 | `			}else{` |
|        21 | 8772 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8773 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8774 | `			}` |
|        37 | 8775 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        37 | 8776 | `			if( pObj ){` |
|        37 | 8777 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        37 | 8778 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8779 | `					break;` |
|         - | 8780 | `				}` |
|        17 | 8781 | `			}` |
|        20 | 8782 | `		}else{` |
|         - | 8783 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8784 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8785 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8786 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8787 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8788 | `			}` |
|       238 | 8789 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8790 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8791 | `			}else{` |
|       170 | 8792 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8793 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8794 | `			}` |
|       236 | 8795 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8796 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8797 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8798 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8799 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8800 | `					break;` |
|         - | 8801 | `				}` |
|        13 | 8802 | `			}else{` |
|       214 | 8803 | `				if( pObj ){` |
|       214 | 8804 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8805 | `				}` |
|       214 | 8806 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8807 | `			}` |
|         - | 8808 | `		}` |
|         - | 8809 | `		/* Point to the next entry */` |
|       273 | 8810 | `		n++;` |
|       273 | 8811 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8812 | `	}` |
|       123 | 8813 | `	return rc;` |
|         3 | 8814 | `}` |
|       116 | 8815 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8816 | `{` |
|         - | 8817 | `	sxi32 rc;` |
|         - | 8818 | `	int i;` |
|       118 | 8819 | `	if( nDepth > 31 ){` |
|         - | 8820 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8821 | `		/* Nesting limit reached */` |
|       ! 0 | 8822 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8823 | `		return SXERR_LIMIT;` |
|         - | 8824 | `	}` |
|       118 | 8825 | `	if( ShowType ){` |
|         - | 8826 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8827 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8828 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8829 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8830 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8831 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8832 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8833 | `		}` |
|        14 | 8834 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8835 | `		return rc;` |
|         - | 8836 | `	}` |
|         - | 8837 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8838 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8839 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8840 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8841 | `	}` |
|       105 | 8842 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8843 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8844 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8845 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8846 | `	}` |
|       105 | 8847 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8848 | `	return rc;` |
|        60 | 8849 | `}` |
|         - | 8850 | `/*` |
|         - | 8851 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8852 | ` * retrieved entry.` |
|         - | 8853 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8854 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8855 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8856 | ` * a value different from PH7_OK.` |
|         - | 8857 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8858 | ` */` |
|     33718 | 8859 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8860 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8861 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8862 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8863 | `	)` |
|         5 | 8864 | `{` |
|         - | 8865 | `	ph7_hashmap_node *pEntry;` |
|         - | 8866 | `	ph7_value sKey,sValue;` |
|         - | 8867 | `	sxi32 rc;` |
|         - | 8868 | `	sxu32 n;` |
|         - | 8869 | `	/* Initialize walker parameter */` |
|     33723 | 8870 | `	rc = SXRET_OK;` |
|     33723 | 8871 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     33723 | 8872 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     33723 | 8873 | `	n = pMap->nEntry;` |
|     33723 | 8874 | `	pEntry = pMap->pFirst;` |
|         - | 8875 | `	/* Start the iteration process */` |
|     91919 | 8876 | `	for(;;){` |
|    183843 | 8877 | `		if( n < 1 ){` |
|     33723 | 8878 | `			break;` |
|         - | 8879 | `		}` |
|         - | 8880 | `		/* Extract a copy of the key and a copy the current value */` |
|    150125 | 8881 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    150125 | 8882 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8883 | `		/* Invoke the user callback */` |
|    150125 | 8884 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8885 | `		/* Release the copy of the key and the value */` |
|    150125 | 8886 | `		PH7_MemObjRelease(&sKey);` |
|    150125 | 8887 | `		PH7_MemObjRelease(&sValue);` |
|    150125 | 8888 | `		if( rc != PH7_OK ){` |
|         - | 8889 | `			/* Callback request an operation abort */` |
|       ! 0 | 8890 | `			return SXERR_ABORT;` |
|         - | 8891 | `		}` |
|         - | 8892 | `		/* Point to the next entry */` |
|    150125 | 8893 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    150125 | 8894 | `		n--;` |
|         5 | 8895 | `	}` |
|         - | 8896 | `	/* All done */` |
|     33723 | 8897 | `	return SXRET_OK;` |
|     16864 | 8898 | `}` |
|         - | 8899 |  |
