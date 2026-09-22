# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1122/1224 lines (91.67%)

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
|   9138395 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   9138400 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   9138400 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|   5740850 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|   5740855 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|   5740855 |   35 | `	sxu32 nH = 5381;` |
|   5740855 |   36 | `	zEnd = &zIn[nLen];` |
|   6457859 |   37 | `	for(;;){` |
|  12915723 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   8589669 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   7457625 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   7316123 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|   5740855 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      2582 |   52 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      2587 |   54 | `	sxi64 iCount = 0;` |
|      2587 |   55 | `	if( !bRecursive ){` |
|      2413 |   56 | `		iCount = pMap->nEntry;` |
|      1209 |   57 | `	}else{` |
|         - |   58 | `		/* Recursive hashmap walk */` |
|       175 |   59 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|         - |   60 | `		ph7_value *pElem;` |
|       175 |   61 | `		sxu32 n = 0;` |
|         - |   62 | `		/* Mark this map as being counted */` |
|       175 |   63 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|       215 |   64 | `		for(;;){` |
|       431 |   65 | `			if( n >= pMap->nEntry ){` |
|       175 |   66 | `				break;` |
|         - |   67 | `			}` |
|         - |   68 | `			/* Point to the element value */` |
|       257 |   69 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|       257 |   70 | `			if( pElem ){` |
|       257 |   71 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
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
|       128 |   82 | `			}` |
|         - |   83 | `			/* Point to the next entry */` |
|       257 |   84 | `			pEntry = pEntry->pNext;` |
|       257 |   85 | `			++n;` |
|         1 |   86 | `		}` |
|         - |   87 | `		/* Clear the counting flag */` |
|       175 |   88 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|         - |   89 | `		/* Update count */` |
|       175 |   90 | `		iCount += pMap->nEntry;` |
|         - |   91 | `	}` |
|      2587 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   4399658 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   4399663 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   4399663 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   4399663 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   4399663 |  110 | `	pNode->pMap  = &(*pMap);` |
|   4399663 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   4399663 |  112 | `	pNode->nHash = nHash;` |
|   4399663 |  113 | `	pNode->xKey.iKey = iKey;` |
|   4399663 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   4399663 |  115 | `	return pNode;` |
|   2199834 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|   3104354 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|   3104359 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3104359 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|   3104359 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|   3104359 |  133 | `	pNode->pMap  = &(*pMap);` |
|   3104359 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   3104359 |  135 | `	pNode->nHash = nHash;` |
|   3104359 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   3104359 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   3104359 |  138 | `	pNode->nValIdx = nValIdx;` |
|   3104359 |  139 | `	return pNode;` |
|   1552182 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   7504012 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   7504017 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   3988953 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   3988953 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1994477 |  150 | `	}` |
|   7504017 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   7504017 |  153 | `	if( pMap->pFirst == 0 ){` |
|   1346433 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|   1346433 |  156 | `		pMap->pCur = pNode;` |
|    673219 |  157 | `	}else{` |
|   6157589 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   7504017 |  160 | `	if( pMap->pActiveSteps ){` |
|         - |  161 | `		/* Re-arm any live foreach cursor parked past the end: php's by-ref` |
|         - |  162 | `		 * foreach iterates the LIVE array, so an element appended while the` |
|         - |  163 | `		 * loop stands on the last node (worklist idiom), or after the body` |
|         - |  164 | `		 * emptied the map, is still visited. A registered step with a NULL` |
|         - |  165 | `		 * cursor is always mid-loop — natural exhaustion unregisters before` |
|         - |  166 | `		 * the loop ends. */` |
|         - |  167 | `		ph7_foreach_step *pStep;` |
|        34 |  168 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        18 |  169 | `			if( pStep->pCursor == 0 ){` |
|        14 |  170 | `				pStep->pCursor = pNode;` |
|         6 |  171 | `			}` |
|        10 |  172 | `		}` |
|         8 |  173 | `	}` |
|   7504017 |  174 | `	++pMap->nEntry;` |
|   7504017 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7376 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7381 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7381 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7381 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      6845 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3425 |  187 | `	}else{` |
|       539 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7381 |  190 | `	if( pNode->pNextCollide ){` |
|      4827 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2412 |  192 | `	}` |
|      7381 |  193 | `	if( pMap->pFirst == pNode ){` |
|       247 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|       121 |  195 | `	}` |
|      7381 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       283 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       139 |  199 | `	}` |
|      7381 |  200 | `	if( pMap->pActiveSteps ){` |
|         - |  201 | `		/* Advance any live foreach cursor parked on this node (delete during` |
|         - |  202 | `		 * live-map iteration: by-ref foreach, $GLOBALS, snapshot fallbacks). */` |
|         - |  203 | `		ph7_foreach_step *pStep;` |
|        29 |  204 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        15 |  205 | `			if( pStep->pCursor == pNode ){` |
|         3 |  206 | `				pStep->pCursor = pNode->pPrev; /* Reverse link */` |
|         1 |  207 | `			}` |
|         8 |  208 | `		}` |
|         7 |  209 | `	}` |
|         - |  210 | `	/* Unlink from the map list */` |
|      7381 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7381 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       215 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       215 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       215 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       105 |  218 | `		}` |
|       105 |  219 | `	}` |
|      7381 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7133 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3564 |  222 | `	}` |
|      7381 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7381 |  224 | `	pMap->nEntry--;` |
|      7381 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       113 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       113 |  228 | `		pMap->apBucket = 0;` |
|       113 |  229 | `		pMap->nSize = 0;` |
|       113 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        54 |  231 | `	}` |
|      7381 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   7504012 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   7504017 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   1352835 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   1352835 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|   1352835 |  245 | `		if( nNew < 1 ){` |
|   1346433 |  246 | `			nNew = 16;` |
|    673214 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|   1352835 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   1352835 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|   1352835 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|   1352835 |  260 | `		pMap->apBucket = apNew;` |
|   1352835 |  261 | `		pMap->nSize = nNew;` |
|   1352835 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   1346433 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      6407 |  267 | `		pEntry = pMap->pFirst;` |
|      6407 |  268 | `		n = 0;` |
|   2588337 |  269 | `		for( ;; ){` |
|   5176679 |  270 | `			if( n >= pMap->nEntry ){` |
|      6407 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   5170277 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   5170277 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   5170277 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   4446733 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   4446733 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2223364 |  280 | `			}` |
|   5170277 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   5170277 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   5170277 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      6407 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      3201 |  288 | `	}` |
|   6157589 |  289 | `	return SXRET_OK;` |
|   3752011 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   4399658 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   4399663 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   4399615 |  310 | `		if( pValue ){` |
|   4399599 |  311 | `			sSafeVal = *pValue;` |
|   4399599 |  312 | `			pValue = &sSafeVal;` |
|   2199797 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   4399615 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   4399615 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   4399615 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   4399599 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   2199797 |  322 | `		}` |
|   4399615 |  323 | `		nIdx = pObj->nIdx;` |
|   2199810 |  324 | `	}else{` |
|        50 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   4399663 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   4399663 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   4399663 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   4399663 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        50 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        24 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   4399663 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   4399663 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   4399663 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   4399663 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   4399663 |  349 | `	return SXRET_OK;` |
|   2199834 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|   3104354 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|   3104359 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3050613 |  370 | `		if( pValue ){` |
|   3050547 |  371 | `			sSafeVal = *pValue;` |
|   3050547 |  372 | `			pValue = &sSafeVal;` |
|   1525271 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|   3050613 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3050613 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|   3050613 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|   3050547 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|   1525271 |  382 | `		}` |
|   3050613 |  383 | `		nIdx = pObj->nIdx;` |
|   1525309 |  384 | `	}else{` |
|     53751 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|   3104359 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|   3104359 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   3104359 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|   3104359 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     53751 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     26873 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3104359 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3104359 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|   3104359 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|   3104359 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|   3104359 |  409 | `	return SXRET_OK;` |
|   1552182 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4721473 |  416 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4721478 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|      1207 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4720276 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4720276 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110798440 |  433 | `	for(;;){` |
| 221596854 |  434 | `		if( pNode == 0 ){` |
|   4308998 |  435 | `			break;` |
|         - |  436 | `		}` |
| 217287856 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 217284827 |  438 | `			&& pNode->nHash == nHash` |
| 108846554 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|    411283 |  441 | `				if( ppNode ){` |
|    411249 |  442 | `					*ppNode = pNode;` |
|    205622 |  443 | `				}` |
|    411283 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216876581 |  447 | `		pNode = pNode->pNextCollide;` |
|         3 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4308998 |  450 | `	return SXERR_NOTFOUND;` |
|   2360746 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|   3293276 |  457 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|   3293281 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|    656785 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|   2636501 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|   2636501 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|   1674248 |  475 | `	for(;;){` |
|   3348501 |  476 | `		if( pNode == 0 ){` |
|   2542641 |  477 | `			break;` |
|         - |  478 | `		}` |
|    805860 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    804338 |  480 | `			&& pNode->nHash == nHash` |
|    448391 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     93971 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     93865 |  484 | `				if( ppNode ){` |
|     93833 |  485 | `					*ppNode = pNode;` |
|     46914 |  486 | `				}` |
|     93865 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    712005 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|   2542641 |  493 | `	return SXERR_NOTFOUND;` |
|   1646643 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|   3293722 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|   3293727 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|   3293727 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|   3293727 |  504 | `	int isNeg = FALSE, nDigit;` |
|   3293727 |  505 | `	if( zIn >= zEnd ){` |
|       100 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|         - |  508 | `	/* php's rule (_zend_handle_numeric_str_ex), byte for byte:` |
|         - |  509 | `	 *   - a leading '-' is allowed, a leading '+' is NOT ('+1' stays a` |
|         - |  510 | `	 *     string key)` |
|         - |  511 | `	 *   - after the sign, a leading '0' disqualifies the key unless the` |
|         - |  512 | `	 *     WHOLE key is the single digit "0" -- so "00", "01", "-0" and` |
|         - |  513 | `	 *     "-01" all stay string keys. The length is measured over the key,` |
|         - |  514 | `	 *     sign included, which is what makes "-0" fail.` |
|         - |  515 | `	 * PH7 tested the leading zero BEFORE skipping the sign and accepted '+',` |
|         - |  516 | `	 * so $a['-0'], $a['+0'], $a['+1'] and $a['-01'] canonicalised onto the` |
|         - |  517 | `	 * integer keys 0/0/1/-1 -- silently COLLIDING with a genuine 0/1/-1 entry` |
|         - |  518 | `	 * ($a = ['-0'=>'a','0'=>'d'] kept one element where php keeps two) and` |
|         - |  519 | `	 * carrying the wrong key through array_keys/array_flip/json_decode/` |
|         - |  520 | `	 * serialize/array_count_values alike. */` |
|   3293631 |  521 | `	if( zIn[0] == '-' && &zIn[1] < zEnd ){` |
|        91 |  522 | `		isNeg = TRUE;` |
|        91 |  523 | `		zIn++;` |
|        44 |  524 | `	}` |
|   3293631 |  525 | `	if( zIn < zEnd && zIn[0] == '0' && SyBlobLength(pKey) > 1 ){` |
|         - |  526 | `		/* Leading zero: octal-looking, signed zero, or just padded */` |
|       147 |  527 | `		return FALSE;` |
|         - |  528 | `	}` |
|   3293487 |  529 | `	zDigit = zIn;` |
|   1647772 |  530 | `	for(;;){` |
|   3295549 |  531 | `		if( zIn >= zEnd ){` |
|       582 |  532 | `			break;` |
|         - |  533 | `		}` |
|   3294971 |  534 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  535 | `			/* Key does not look like a decimal number */` |
|   3292909 |  536 | `			return FALSE;` |
|         - |  537 | `		}` |
|      2066 |  538 | `		zIn++;` |
|         4 |  539 | `	}` |
|         - |  540 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  541 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  542 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  543 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       582 |  544 | `	nDigit = (int)(zEnd - zDigit);` |
|       582 |  545 | `	if( nDigit < 1 ){` |
|         - |  546 | `		/* A lone "-" (the digit loop rejects it first; kept defensive) */` |
|       ! 0 |  547 | `		return FALSE;` |
|         - |  548 | `	}` |
|       601 |  549 | `	if( nDigit > 19 \|\|` |
|       307 |  550 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|        22 |  551 | `		return FALSE;` |
|         - |  552 | `	}` |
|       562 |  553 | `	return TRUE;` |
|   1646866 |  554 | `}` |
|         - |  555 | `/*` |
|         - |  556 | ` * TRUE when this key value lands on an INTEGER key — the same fold HashmapLookup` |
|         - |  557 | ` * and HashmapInsert perform below, exposed so a DIAGNOSTIC can name the key the` |
|         - |  558 | ` * way the lookup saw it rather than the way it was written ($a["10"] misses the` |
|         - |  559 | `` * integer key 10, so php's warning says `Undefined array key 10`, unquoted).`` |
|         - |  560 | ` * A non-integer key is left as a STRING with its blob ready to print — including` |
|         - |  561 | ` * the NULL key, which folds to "" exactly as the lookup folds it.` |
|         - |  562 | ` */` |
|        60 |  563 | `PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey)` |
|         5 |  564 | `{` |
|        65 |  565 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|        53 |  566 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  567 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|         3 |  568 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  569 | `		}` |
|        53 |  570 | `		return HashmapIsIntKey(&pKey->sBlob) ? TRUE : FALSE;` |
|         - |  571 | `	}` |
|         - |  572 | `	/* int / float / BOOL all reach an integer key ($a[false] is $a[0]) */` |
|        14 |  573 | `	return TRUE;` |
|        35 |  574 | `}` |
|         - |  575 | `/*` |
|         - |  576 | ` * Check if a given key exists in the given hashmap.` |
|         - |  577 | ` * Write a pointer to the target node on success.` |
|         - |  578 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  579 | ` */` |
|    600378 |  580 | `static sxi32 HashmapLookup(` |
|         - |  581 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  582 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  583 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  584 | `	)` |
|         5 |  585 | `{` |
|    600383 |  586 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  587 | `	sxi32 rc;` |
|    600383 |  588 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    189219 |  589 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  590 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|        32 |  591 | `			PH7_MemObjToString(&(*pKey));` |
|        15 |  592 | `		}` |
|    189219 |  593 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  594 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  595 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  596 | `			 * to an integer lookup for key 0. */` |
|    189143 |  597 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    189143 |  598 | `			goto result;` |
|         - |  599 | `		}` |
|        38 |  600 | `	}` |
|         - |  601 | `	/* Perform an int lookup */` |
|    411245 |  602 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  603 | `		/* Force an integer cast */` |
|        96 |  604 | `		PH7_MemObjToInteger(pKey);` |
|        46 |  605 | `	}` |
|         - |  606 | `	/* Perform an int lookup */` |
|    411245 |  607 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|    300189 |  608 | `result:` |
|    600383 |  609 | `	if( rc == SXRET_OK ){` |
|         - |  610 | `		/* Node found */` |
|    504171 |  611 | `		if( ppNode ){` |
|    504097 |  612 | `			*ppNode = pNode;` |
|    252046 |  613 | `		}` |
|    504171 |  614 | `		return SXRET_OK;` |
|         - |  615 | `	}` |
|         - |  616 | `	/* No such entry */` |
|     96217 |  617 | `	return SXERR_NOTFOUND;` |
|    300194 |  618 | `}` |
|         - |  619 | `/*` |
|         - |  620 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  621 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  622 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  623 | ` * via HashmapAppendIndexBusy.` |
|         - |  624 | ` */` |
|   2154822 |  625 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  626 | `{` |
|   2154827 |  627 | `	if( !pMap->bIntKeySeen ){` |
|         - |  628 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|      1297 |  629 | `		pMap->bIntKeySeen = 1;` |
|      1297 |  630 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|      1297 |  631 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  632 | `			pMap->iNextIdx++;` |
|       ! 0 |  633 | `		}` |
|      1297 |  634 | `		return;` |
|         - |  635 | `	}` |
|   2153535 |  636 | `	if( iKey >= pMap->iNextIdx ){` |
|   2153222 |  637 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  638 | `		/* Make sure the automatic index is not reserved */` |
|   2153222 |  639 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  640 | `			pMap->iNextIdx++;` |
|       ! 0 |  641 | `		}` |
|   1076613 |  642 | `	}` |
|   1077416 |  643 | `}` |
|         - |  644 | `/*` |
|         - |  645 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  646 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  647 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  648 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  649 | ` */` |
|   2239732 |  650 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  651 | `{` |
|   2239737 |  652 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  653 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  654 | `		return TRUE;` |
|         - |  655 | `	}` |
|   2239731 |  656 | `	return FALSE;` |
|   1119871 |  657 | `}` |
|         - |  658 | `/*` |
|         - |  659 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  660 | ` * hashmap.` |
|         - |  661 | ` * If a node with the given key already exists in the database` |
|         - |  662 | ` * then this function overwrite the old value.` |
|         - |  663 | ` */` |
|   7445036 |  664 | `PH7_PRIVATE sxi32 HashmapInsert(` |
|         - |  665 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  666 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  667 | `	ph7_value *pVal    /* Node value */` |
|         - |  668 | `	)` |
|         5 |  669 | `{` |
|   7445041 |  670 | `	ph7_hashmap_node *pNode = 0;` |
|   7445041 |  671 | `	sxi32 rc = SXRET_OK;` |
|   7445041 |  672 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|   3050703 |  673 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  674 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  675 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  676 | `			 * path and filed it under 0). */` |
|         7 |  677 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  678 | `		}` |
|   3050703 |  679 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       474 |  680 | `			goto IntKey;` |
|         - |  681 | `		}` |
|         - |  682 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  683 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  684 | `		 * overwriting nothing and bumping the auto-index). */` |
|   4575347 |  685 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   1525114 |  686 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  687 | `				/* Overwrite the old value */` |
|         - |  688 | `				ph7_value *pElem;` |
|       497 |  689 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       497 |  690 | `				if( pElem ){` |
|       497 |  691 | `					if( pVal ){` |
|       497 |  692 | `						PH7_MemObjStore(pVal,pElem);` |
|       251 |  693 | `					}else{` |
|         - |  694 | `						/* Nullify the entry */` |
|       ! 0 |  695 | `						PH7_MemObjToNull(pElem);` |
|         - |  696 | `					}` |
|       246 |  697 | `				}` |
|       497 |  698 | `				return SXRET_OK;` |
|         - |  699 | `		}` |
|   3049741 |  700 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  701 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  702 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       165 |  703 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  704 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  705 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  706 | `				return SXRET_OK;` |
|         - |  707 | `			}` |
|       246 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       162 |  709 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        81 |  710 | `				pVal,SXU32_HIGH);` |
|         - |  711 | `		}` |
|         - |  712 | `		/* Perform a blob-key insertion */` |
|   3049579 |  713 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   3049579 |  714 | `		return rc;` |
|         - |  715 | `	}` |
|   2197169 |  716 | `IntKey:` |
|   4394813 |  717 | `	if( pKey ){` |
|   2155117 |  718 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  719 | `			/* Force an integer cast */` |
|       489 |  720 | `			PH7_MemObjToInteger(pKey);` |
|       242 |  721 | `		}` |
|   2155117 |  722 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  723 | `			/* Overwrite the old value */` |
|         - |  724 | `			ph7_value *pElem;` |
|       301 |  725 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       301 |  726 | `			if( pElem ){` |
|       301 |  727 | `				if( pVal ){` |
|       301 |  728 | `					PH7_MemObjStore(pVal,pElem);` |
|       152 |  729 | `				}else{` |
|         - |  730 | `					/* Nullify the entry */` |
|       ! 0 |  731 | `					PH7_MemObjToNull(pElem);` |
|         - |  732 | `				}` |
|       149 |  733 | `			}` |
|       301 |  734 | `			return SXRET_OK;` |
|         - |  735 | `		}` |
|   2154819 |  736 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  737 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  738 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  739 | `			char zKey[24];` |
|         3 |  740 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  741 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  742 | `		}` |
|         - |  743 | `		/* Perform a 64-bit-int-key insertion */` |
|   2154817 |  744 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2154817 |  745 | `		if( rc == SXRET_OK ){` |
|   2154817 |  746 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1077406 |  747 | `		}` |
|   1077411 |  748 | `	}else{` |
|   2239701 |  749 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  750 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  751 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  752 | `		}` |
|   2239699 |  753 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  754 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  755 | `		}` |
|         - |  756 | `		/* Assign an automatic index */` |
|   2239693 |  757 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   2239693 |  758 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   2239691 |  759 | `			++pMap->iNextIdx;` |
|   1119843 |  760 | `		}` |
|         - |  761 | `	}` |
|         - |  762 | `	/* Insertion result */` |
|   4394505 |  763 | `	return rc;` |
|   3722523 |  764 | `}` |
|         - |  765 | `/*` |
|         - |  766 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  767 | ` * hashmap.` |
|         - |  768 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  769 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  770 | ` * The insertion by reference is triggered when the following` |
|         - |  771 | ` * expression is encountered.` |
|         - |  772 | ` * $var = 10;` |
|         - |  773 | ` *  $a = array(&var);` |
|         - |  774 | ` * OR` |
|         - |  775 | ` *  $a[] =& $var;` |
|         - |  776 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  777 | ` * over it's contents.` |
|         - |  778 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  779 | ` * removed when the foreign ph7_value is unset.` |
|         - |  780 | ` * Example:` |
|         - |  781 | ` *  $var = 10;` |
|         - |  782 | ` *  $a[] =& $var;` |
|         - |  783 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  784 | ` *  //Unset the foreign ph7_value now` |
|         - |  785 | ` *  unset($var);` |
|         - |  786 | ` *  echo count($a); //0` |
|         - |  787 | ` * Note that this is a PH7 eXtension.` |
|         - |  788 | ` * Refer to the official documentation for more information.` |
|         - |  789 | ` * If a node with the given key already exists in the database` |
|         - |  790 | ` * then this function overwrite the old value.` |
|         - |  791 | ` */` |
|     53808 |  792 | `static sxi32 HashmapInsertByRef(` |
|         - |  793 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  794 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  795 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  796 | `	)` |
|         5 |  797 | `{` |
|     53813 |  798 | `	ph7_hashmap_node *pNode = 0;` |
|     53813 |  799 | `	sxi32 rc = SXRET_OK;` |
|     53813 |  800 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|     53767 |  801 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  802 | ``			/* Force a string cast. NULL casts to "": `$a[null] =& $x` binds under the`` |
|         - |  803 | `			 * EMPTY STRING key, symmetric with HashmapInsert (the by-value path). */` |
|         3 |  804 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  805 | `		}` |
|     53767 |  806 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  807 | `			goto IntKey;` |
|         - |  808 | `		}` |
|         - |  809 | ``		/* An empty key is a REAL key: `$a[""] =& $x` binds (and OVERWRITES an existing`` |
|         - |  810 | `		 * "" element) under "", it does NOT auto-index. The legacy path turned "" into` |
|         - |  811 | ``		 * the next integer slot — `$a[""] =& $x` filed under 0 and a second write added`` |
|         - |  812 | `		 * a duplicate rather than rebinding. A genuine auto-index caller passes` |
|         - |  813 | `		 * pKey == 0 (a literal null pointer), handled at IntKey below, never a "" blob. */` |
|     80645 |  814 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     26880 |  815 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  816 | `				/* Overwrite */` |
|        16 |  817 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        16 |  818 | `				pNode->nValIdx = nRefIdx;` |
|         - |  819 | `				/* Install in the reference table */` |
|        16 |  820 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        16 |  821 | `				return SXRET_OK;` |
|         - |  822 | `		}` |
|         - |  823 | `		/* Perform a blob-key insertion */` |
|     53751 |  824 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     53751 |  825 | `		return rc;` |
|         - |  826 | `	}` |
|        23 |  827 | `IntKey:` |
|        50 |  828 | `	if( pKey ){` |
|        12 |  829 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  830 | `			/* Force an integer cast */` |
|         3 |  831 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  832 | `		}` |
|        12 |  833 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  834 | `			/* Overwrite */` |
|       ! 0 |  835 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  836 | `			pNode->nValIdx = nRefIdx;` |
|         - |  837 | `			/* Install in the reference table */` |
|       ! 0 |  838 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  839 | `			return SXRET_OK;` |
|         - |  840 | `		}` |
|         - |  841 | `		/* Perform a 64-bit-int-key insertion */` |
|        12 |  842 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|        12 |  843 | `		if( rc == SXRET_OK ){` |
|        12 |  844 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         5 |  845 | `		}` |
|         7 |  846 | `	}else{` |
|        40 |  847 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  848 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  849 | `		}` |
|         - |  850 | `		/* Assign an automatic index */` |
|        40 |  851 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        40 |  852 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        40 |  853 | `			++pMap->iNextIdx;` |
|        19 |  854 | `		}` |
|         - |  855 | `	}` |
|         - |  856 | `	/* Insertion result */` |
|        50 |  857 | `	return rc;` |
|     26909 |  858 | `}` |
|         - |  859 | `/*` |
|         - |  860 | ` * Extract node value.` |
|         - |  861 | ` */` |
|   1696444 |  862 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  863 | `{` |
|         - |  864 | `	/* Point to the desired object */` |
|         - |  865 | `	ph7_value *pObj;` |
|   1696449 |  866 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1696449 |  867 | `	return pObj;` |
|         5 |  868 | `}` |
|         - |  869 | `/*` |
|         - |  870 | ` * Insert a node in the given hashmap.` |
|         - |  871 | ` * If a node with the given key already exists in the database` |
|         - |  872 | ` * then this function overwrite the old value.` |
|         - |  873 | ` */` |
|      1270 |  874 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  875 | `{` |
|         - |  876 | `	ph7_value *pObj;` |
|         - |  877 | `	sxi32 rc;` |
|         - |  878 | `	/* Extract the node value */` |
|      1275 |  879 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|      1275 |  880 | `	if( pObj == 0 ){` |
|       ! 0 |  881 | `		return SXERR_EMPTY;` |
|         - |  882 | `	}` |
|      1270 |  883 | `	if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|      1274 |  884 | `	 \|\| PH7_VmSlotIsReferenced(pMap->pVm,pNode->nValIdx) ){` |
|         - |  885 | `		/* A referenced element keeps its reference through the copy (php: array_slice()` |
|         - |  886 | ``		 * of an array holding `$r = &$a[1]` still var_dumps that element as &int(2)).`` |
|         - |  887 | `		 * Same rule HashmapDuplicateNode applies for array_merge()/spread. */` |
|         3 |  888 | `		sxu32 nRefIdx = pNode->nValIdx;` |
|         - |  889 | `		ph7_value sKey;` |
|         3 |  890 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         3 |  891 | `			if( !bPreserve ){` |
|         3 |  892 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  893 | `			}` |
|       ! 0 |  894 | `			PH7_MemObjInitFromInt(pMap->pVm,&sKey,pNode->xKey.iKey);` |
|       ! 0 |  895 | `		}else{` |
|       ! 0 |  896 | `			if( !bPreserve ){` |
|       ! 0 |  897 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  898 | `			}` |
|       ! 0 |  899 | `			PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       ! 0 |  900 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|       ! 0 |  901 | `				SyBlobLength(&pNode->xKey.sKey));` |
|         - |  902 | `		}` |
|       ! 0 |  903 | `		rc = HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|       ! 0 |  904 | `		PH7_MemObjRelease(&sKey);` |
|       ! 0 |  905 | `		return rc;` |
|         - |  906 | `	}` |
|         - |  907 | `	/* Preserve key */` |
|      1273 |  908 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  909 | `		/* Int64 key */` |
|      1117 |  910 | `		if( !bPreserve ){` |
|         - |  911 | `			/* Assign an automatic index */` |
|       349 |  912 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       177 |  913 | `		}else{` |
|       773 |  914 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  915 | `		}` |
|       561 |  916 | `	}else{` |
|         - |  917 | `		/* Blob key */` |
|       158 |  918 | `		if( !bPreserve ){` |
|         - |  919 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  920 | `			 * original string key entirely */` |
|        35 |  921 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  922 | `		}else{` |
|       185 |  923 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        61 |  924 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  925 | `		}` |
|         - |  926 | `	}` |
|      1273 |  927 | `	return rc;` |
|       640 |  928 | `}` |
|         - |  929 | `/*` |
|         - |  930 | ` * Compare two node values.` |
|         - |  931 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  932 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  933 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  934 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  935 | ` * documenation.` |
|         - |  936 | ` */` |
|     88177 |  937 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  938 | `{` |
|         - |  939 | `	ph7_value sObj1,sObj2;` |
|         - |  940 | `	sxi32 rc;` |
|     88182 |  941 | `	if( pLeft == pRight ){` |
|         - |  942 | `		/*` |
|         - |  943 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  944 | `		 * below for more information on this sceanario.` |
|         - |  945 | `		 */` |
|       ! 0 |  946 | `		return 0;` |
|         - |  947 | `	}` |
|         - |  948 | `	/* Do the comparison */` |
|     88182 |  949 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     88182 |  950 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     88182 |  951 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     88182 |  952 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     88182 |  953 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     88182 |  954 | `	PH7_MemObjRelease(&sObj1);` |
|     88182 |  955 | `	PH7_MemObjRelease(&sObj2);` |
|     88182 |  956 | `	return rc;` |
|     43945 |  957 | `}` |
|         - |  958 | `/*` |
|         - |  959 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  960 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  961 | ` */` |
|     18466 |  962 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  963 | `{` |
|     18471 |  964 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  965 | `	sxu32 nBucket;` |
|         - |  966 | `	/* Remove old collision links */` |
|     18471 |  967 | `	if( pEntry->pPrevCollide ){` |
|     13038 |  968 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      6444 |  969 | `	}else{` |
|      5438 |  970 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  971 | `	}` |
|     18471 |  972 | `	if( pEntry->pNextCollide ){` |
|      1405 |  973 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       684 |  974 | `	}` |
|     18471 |  975 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  976 | `	/* Compute the new hash */` |
|     18471 |  977 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     18471 |  978 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     18471 |  979 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  980 | `	/* Link to the new bucket */` |
|     18471 |  981 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     18471 |  982 | `	if( pMap->apBucket[nBucket] ){` |
|     13409 |  983 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      6626 |  984 | `	}` |
|     18471 |  985 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     18471 |  986 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  987 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  988 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  989 | `	 * the no-overflow invariant uniform). */` |
|     18471 |  990 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     18471 |  991 | `		pMap->iNextIdx++;` |
|      9233 |  992 | `	}` |
|     18471 |  993 | `}` |
|         - |  994 | `/*` |
|         - |  995 | ` * Perform a linear search on a given hashmap.` |
|         - |  996 | ` * Write a pointer to the target node on success.` |
|         - |  997 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  998 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  999 | ` * for more information.` |
|         - | 1000 | ` */` |
|     35866 | 1001 | `PH7_PRIVATE int HashmapFindValue(` |
|         - | 1002 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1003 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - | 1004 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - | 1005 | `	int bStrict      /* TRUE for strict comparison */` |
|         - | 1006 | `	)` |
|         5 | 1007 | `{` |
|         - | 1008 | `	ph7_hashmap_node *pEntry;` |
|         - | 1009 | `	ph7_value sVal,*pVal;` |
|         - | 1010 | `	ph7_value sNeedle;` |
|         - | 1011 | `	sxi32 rc;` |
|         - | 1012 | `	sxu32 n;` |
|         - | 1013 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     35871 | 1014 | `	pEntry = pMap->pFirst;` |
|     35871 | 1015 | `	n = pMap->nEntry;` |
|     35871 | 1016 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     35871 | 1017 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     88306 | 1018 | `	for(;;){` |
|    176615 | 1019 | `		if( n < 1 ){` |
|        38 | 1020 | `			break;` |
|         - | 1021 | `		}` |
|         - | 1022 | `		/* Extract node value */` |
|    176581 | 1023 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    176581 | 1024 | `		if( pVal ){` |
|         - | 1025 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - | 1026 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - | 1027 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - | 1028 | `			 * so null needles/values take the same path as everything else` |
|         - | 1029 | `			 * (the historical null-to-null shortcut here made` |
|         - | 1030 | `			 * in_array(null, [""]) false where php says true). */` |
|    176581 | 1031 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    176581 | 1032 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    176581 | 1033 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    176581 | 1034 | `			PH7_MemObjRelease(&sVal);` |
|    176581 | 1035 | `			PH7_MemObjRelease(&sNeedle);` |
|    176581 | 1036 | `			if( rc == 0 ){` |
|     35837 | 1037 | `				if( ppNode ){` |
|       ! 0 | 1038 | `					*ppNode = pEntry;` |
|       ! 0 | 1039 | `				}` |
|         - | 1040 | `				/* Match found*/` |
|     35837 | 1041 | `				return SXRET_OK;` |
|         - | 1042 | `			}` |
|     70373 | 1043 | `		}` |
|         - | 1044 | `		/* Point to the next entry */` |
|    140749 | 1045 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    140749 | 1046 | `		n--;` |
|         5 | 1047 | `	}` |
|         - | 1048 | `	/* No such entry */` |
|        38 | 1049 | `	return SXERR_NOTFOUND;` |
|     17938 | 1050 | `}` |
|         - | 1051 | `/*` |
|         - | 1052 | ` * The element comparison array_diff()/array_intersect() and their _assoc pair` |
|         - | 1053 | ` * use, which is NOT the engine's value comparison: php's manual defines all four` |
|         - | 1054 | ` * as` |
|         - | 1055 | ` *     (string)$elem1 === (string)$elem2` |
|         - | 1056 | ` * a PURE string comparison — not numeric-string aware, so array_diff(["10"],` |
|         - | 1057 | ` * ["1e1"]) keeps "10" — where PHL used to call PH7_MemObjCmp with bStrict. That` |
|         - | 1058 | ` * made no int ever match its own decimal string, so array_diff([1,2,3],` |
|         - | 1059 | ` * ["1","2"]) answered the whole first array instead of [2=>3].` |
|         - | 1060 | ` *` |
|         - | 1061 | ` * bUserVisible picks the coercion: TRUE emits php's user-visible diagnostics (an` |
|         - | 1062 | ` * ARRAY element warns "Array to string conversion", an object with no` |
|         - | 1063 | ` * __toString() throws the catchable "could not be converted to string" Error,` |
|         - | 1064 | ` * reported through *pRc so the builtin answers the throw instead of a result),` |
|         - | 1065 | ` * FALSE renders silently. The two diff families need different answers there:` |
|         - | 1066 | ` * the _assoc pair converts LAZILY, only when a key matched, so it coerces` |
|         - | 1067 | ` * user-visibly right here; array_diff/array_intersect convert every element of` |
|         - | 1068 | ` * every input array up front (php sorts them), so those pre-pass with` |
|         - | 1069 | ` * HashmapStringifyElems and compare silently afterwards — which is what makes` |
|         - | 1070 | ` * the warning COUNT and the "throws even though an earlier element matched"` |
|         - | 1071 | ` * behaviour come out php-exact.` |
|         - | 1072 | ` *` |
|         - | 1073 | ` * Both operands are coerced on COPIES: these are live array elements, and a` |
|         - | 1074 | ` * diff must not rewrite the caller's array.` |
|         - | 1075 | ` */` |
|       438 | 1076 | `PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc)` |
|         2 | 1077 | `{` |
|         - | 1078 | `	ph7_value sA,sB;` |
|       440 | 1079 | `	int bEq = FALSE;` |
|         - | 1080 | `	sxi32 rc;` |
|       440 | 1081 | `	*pRc = SXRET_OK;` |
|         - | 1082 | `	/* Two fast paths that need no rendering at all, because each type's string` |
|         - | 1083 | `	 * form is canonical and injective: two STRINGS already ARE their string form,` |
|         - | 1084 | `	 * and two INTS are string-equal exactly when they are equal. Without them` |
|         - | 1085 | `	 * array_diff() over a pair of integer ranges formatted both operands of every` |
|         - | 1086 | `	 * one of its O(n*m) comparisons (~4x slower than the strict compare it` |
|         - | 1087 | `	 * replaced). A value carrying MEMOBJ_INT alongside MEMOBJ_REAL is an integral` |
|         - | 1088 | `	 * FLOAT, whose "1" can equal an int's — the mask sends it down the slow path` |
|         - | 1089 | `	 * rather than comparing rVal-derived iVal, and bools/null/resources likewise. */` |
|       440 | 1090 | `	if( (pA->iFlags & MEMOBJ_STRING) && (pB->iFlags & MEMOBJ_STRING) ){` |
|        93 | 1091 | `		return SyBlobLength(&pA->sBlob) == SyBlobLength(&pB->sBlob)` |
|       112 | 1092 | `		    && ( SyBlobLength(&pA->sBlob) == 0` |
|        42 | 1093 | `		      \|\| SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),` |
|        42 | 1094 | `		                  SyBlobLength(&pA->sBlob)) == 0 );` |
|         - | 1095 | `	}` |
|       368 | 1096 | `	if( (pA->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|       347 | 1097 | `	 && (pB->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT ){` |
|       179 | 1098 | `		return pA->x.iVal == pB->x.iVal;` |
|         - | 1099 | `	}` |
|       192 | 1100 | `	PH7_MemObjInit(pA->pVm,&sA);` |
|       192 | 1101 | `	PH7_MemObjInit(pA->pVm,&sB);` |
|       192 | 1102 | `	PH7_MemObjLoad(pA,&sA);` |
|       192 | 1103 | `	PH7_MemObjLoad(pB,&sB);` |
|       192 | 1104 | `	rc = bUserVisible ? PH7_MemObjToStringUV(&sA) : PH7_MemObjToString(&sA);` |
|       192 | 1105 | `	if( rc == SXRET_OK ){` |
|       192 | 1106 | `		rc = bUserVisible ? PH7_MemObjToStringUV(&sB) : PH7_MemObjToString(&sB);` |
|        95 | 1107 | `	}` |
|       192 | 1108 | `	if( rc != SXRET_OK ){` |
|         3 | 1109 | `		*pRc = rc;` |
|       191 | 1110 | `	}else if( SyBlobLength(&sA.sBlob) == SyBlobLength(&sB.sBlob) ){` |
|       251 | 1111 | `		bEq = SyBlobLength(&sA.sBlob) == 0` |
|       168 | 1112 | `		   \|\| SyMemcmp(SyBlobData(&sA.sBlob),SyBlobData(&sB.sBlob),SyBlobLength(&sA.sBlob)) == 0;` |
|        84 | 1113 | `	}` |
|       192 | 1114 | `	PH7_MemObjRelease(&sA);` |
|       192 | 1115 | `	PH7_MemObjRelease(&sB);` |
|       192 | 1116 | `	return bEq;` |
|       221 | 1117 | `}` |
|         - | 1118 | `/*` |
|         - | 1119 | ` * Run the USER-VISIBLE string coercion over every element of pMap once, in` |
|         - | 1120 | ` * insertion order, discarding the result: php's array_diff/array_intersect sort` |
|         - | 1121 | ` * each input array, which converts every element exactly once, so this is where` |
|         - | 1122 | ` * their "Array to string conversion" warnings and their not-stringable-object` |
|         - | 1123 | ` * Error come from. Doing it as a pre-pass is what lets` |
|         - | 1124 | ` * array_diff([1,2],[1,new P()]) throw the way php's does even though the first` |
|         - | 1125 | ` * element already matched. Returns the throw status, SXRET_OK otherwise.` |
|         - | 1126 | ` */` |
|       182 | 1127 | `PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap)` |
|         2 | 1128 | `{` |
|       184 | 1129 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       184 | 1130 | `	sxu32 n = pMap->nEntry;` |
|       530 | 1131 | `	while( n > 0 && pEntry ){` |
|       356 | 1132 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       356 | 1133 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1134 | `			ph7_value sTmp;` |
|         - | 1135 | `			sxi32 rc;` |
|       246 | 1136 | `			PH7_MemObjInit(pMap->pVm,&sTmp);` |
|       246 | 1137 | `			PH7_MemObjLoad(pVal,&sTmp);` |
|       246 | 1138 | `			rc = PH7_MemObjToStringUV(&sTmp);` |
|       246 | 1139 | `			PH7_MemObjRelease(&sTmp);` |
|       246 | 1140 | `			if( rc != SXRET_OK ){` |
|         9 | 1141 | `				return rc;` |
|         - | 1142 | `			}` |
|       118 | 1143 | `		}` |
|       348 | 1144 | `		pEntry = pEntry->pPrev; /* Reverse link — insertion order */` |
|       348 | 1145 | `		n--;` |
|         2 | 1146 | `	}` |
|       176 | 1147 | `	return SXRET_OK;` |
|        93 | 1148 | `}` |
|         - | 1149 | `/*` |
|         - | 1150 | ` * Perform a linear search on a given hashmap, comparing values the way` |
|         - | 1151 | ` * array_diff()/array_intersect() do (see HashmapValueStrEq). Writes a pointer to` |
|         - | 1152 | ` * the target node on success; SXERR_NOTFOUND otherwise, with *pRc carrying the` |
|         - | 1153 | ` * status of a coercion that threw.` |
|         - | 1154 | ` */` |
|       204 | 1155 | `PH7_PRIVATE int HashmapFindStringValue(` |
|         - | 1156 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - | 1157 | `	ph7_value *pNeedle,  /* Lookup value */` |
|         - | 1158 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success */` |
|         - | 1159 | `	sxi32 *pRc           /* OUT: coercion status */` |
|         - | 1160 | `	)` |
|         2 | 1161 | `{` |
|       206 | 1162 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       206 | 1163 | `	sxu32 n = pMap->nEntry;` |
|       206 | 1164 | `	*pRc = SXRET_OK;` |
|       446 | 1165 | `	while( n > 0 && pEntry ){` |
|       346 | 1166 | `		ph7_value *pVal = HashmapExtractNodeValue(pEntry);` |
|       346 | 1167 | `		if( pVal ){` |
|       346 | 1168 | `			if( HashmapValueStrEq(pNeedle,pVal,/*bUserVisible*/0,pRc) ){` |
|       106 | 1169 | `				if( ppNode ){` |
|       ! 0 | 1170 | `					*ppNode = pEntry;` |
|       ! 0 | 1171 | `				}` |
|       106 | 1172 | `				return SXRET_OK;` |
|         - | 1173 | `			}` |
|       242 | 1174 | `			if( *pRc != SXRET_OK ){` |
|       ! 0 | 1175 | `				return SXERR_NOTFOUND;` |
|         - | 1176 | `			}` |
|       120 | 1177 | `		}` |
|       242 | 1178 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       242 | 1179 | `		n--;` |
|         2 | 1180 | `	}` |
|       102 | 1181 | `	return SXERR_NOTFOUND;` |
|       104 | 1182 | `}` |
|         - | 1183 | `/*` |
|         - | 1184 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - | 1185 | ` * for values comparison.` |
|         - | 1186 | ` * Write a pointer to the target node on success.` |
|         - | 1187 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1188 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - | 1189 | ` * for more information.` |
|         - | 1190 | ` */` |
|        40 | 1191 | `PH7_PRIVATE int HashmapFindValueByCallback(` |
|         - | 1192 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - | 1193 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - | 1194 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - | 1195 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - | 1196 | `	)` |
|         3 | 1197 | `{` |
|         - | 1198 | `	ph7_hashmap_node *pEntry;` |
|         - | 1199 | `	ph7_value sResult,*pVal;` |
|         - | 1200 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - | 1201 | `	sxi32 rc;` |
|         - | 1202 | `	sxu32 n;` |
|        43 | 1203 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - | 1204 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - | 1205 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 | 1206 | `		return SXERR_NOTFOUND;` |
|         - | 1207 | `	}` |
|         - | 1208 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        43 | 1209 | `	pEntry = pMap->pFirst;` |
|        43 | 1210 | `	n = pMap->nEntry;` |
|         - | 1211 | `	/* Store callback result here */` |
|        43 | 1212 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - | 1213 | `	/* First argument to the callback */` |
|        43 | 1214 | `	apArg[0] = pNeedle;` |
|        42 | 1215 | `	for(;;){` |
|        87 | 1216 | `		if( n < 1 ){` |
|        19 | 1217 | `			break;` |
|         - | 1218 | `		}` |
|         - | 1219 | `		/* Extract node value */` |
|        71 | 1220 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        71 | 1221 | `		if( pVal ){` |
|         - | 1222 | `			/* Invoke the user callback */` |
|        71 | 1223 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        71 | 1224 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        71 | 1225 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 1226 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1227 | `				 * and report no match for the rest of the run. */` |
|         5 | 1228 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1229 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1230 | `				return SXERR_NOTFOUND;` |
|         - | 1231 | `			}` |
|        67 | 1232 | `			if( rc == SXRET_OK ){` |
|         - | 1233 | `				/* Extract callback result */` |
|        67 | 1234 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1235 | `					/* Perform an int cast */` |
|       ! 0 | 1236 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1237 | `				}` |
|        67 | 1238 | `				rc = (sxi32)sResult.x.iVal;` |
|        67 | 1239 | `				PH7_MemObjRelease(&sResult);` |
|        67 | 1240 | `				if( rc == 0 ){` |
|         - | 1241 | `					/* Match found*/` |
|        23 | 1242 | `					if( ppNode ){` |
|       ! 0 | 1243 | `						*ppNode = pEntry;` |
|       ! 0 | 1244 | `					}` |
|        23 | 1245 | `					return SXRET_OK;` |
|         - | 1246 | `				}` |
|        22 | 1247 | `			}` |
|        22 | 1248 | `		}` |
|         - | 1249 | `		/* Point to the next entry */` |
|        47 | 1250 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 1251 | `		n--;` |
|         3 | 1252 | `	}` |
|         - | 1253 | `	/* No such entry */` |
|        19 | 1254 | `	return SXERR_NOTFOUND;` |
|        23 | 1255 | `}` |
|         - | 1256 | `/*` |
|         - | 1257 | ` * Compare two hashmaps.` |
|         - | 1258 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1259 | ` * Note on array comparison operators.` |
|         - | 1260 | ` *  According to the PHP language reference manual.` |
|         - | 1261 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1262 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1263 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1264 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1265 | ` *                          order and of the same types.` |
|         - | 1266 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1267 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1268 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1269 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1270 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1271 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1272 | ` * <?php` |
|         - | 1273 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1274 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1275 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1276 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1277 | ` * var_dump($c);` |
|         - | 1278 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1279 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1280 | ` * var_dump($c);` |
|         - | 1281 | ` * ?>` |
|         - | 1282 | ` * When executed, this script will print the following:` |
|         - | 1283 | ` * Union of $a and $b:` |
|         - | 1284 | ` * array(3) {` |
|         - | 1285 | ` *  ["a"]=>` |
|         - | 1286 | ` *  string(5) "apple"` |
|         - | 1287 | ` *  ["b"]=>` |
|         - | 1288 | ` * string(6) "banana"` |
|         - | 1289 | ` *  ["c"]=>` |
|         - | 1290 | ` * string(6) "cherry"` |
|         - | 1291 | ` * }` |
|         - | 1292 | ` * Union of $b and $a:` |
|         - | 1293 | ` * array(3) {` |
|         - | 1294 | ` * ["a"]=>` |
|         - | 1295 | ` * string(4) "pear"` |
|         - | 1296 | ` * ["b"]=>` |
|         - | 1297 | ` * string(10) "strawberry"` |
|         - | 1298 | ` * ["c"]=>` |
|         - | 1299 | ` * string(6) "cherry"` |
|         - | 1300 | ` * }` |
|         - | 1301 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1302 | ` */` |
|        72 | 1303 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1304 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1305 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1306 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1307 | `	)` |
|         4 | 1308 | `{` |
|         - | 1309 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1310 | `	sxi32 rc;` |
|         - | 1311 | `	sxu32 n;` |
|        76 | 1312 | `	if( pLeft == pRight ){` |
|         - | 1313 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1314 | `		 * Unlike the zend engine.` |
|         - | 1315 | `		 */` |
|         7 | 1316 | `		return 0;` |
|         - | 1317 | `	}` |
|        70 | 1318 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1319 | `		/* Must have the same number of entries */` |
|         8 | 1320 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1321 | `	}` |
|        64 | 1322 | `	if( bStrict ){` |
|         - | 1323 | `		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the` |
|         - | 1324 | `		 * same key/value pairs, with identical key types, in the same insertion` |
|         - | 1325 | `		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per` |
|         - | 1326 | `		 * this file's forward-iteration convention) in lockstep and compare each` |
|         - | 1327 | `		 * position's key then value. (Loose '==' below stays order-insensitive,` |
|         - | 1328 | `		 * matching each left key by lookup into the right map.) */` |
|        44 | 1329 | `		ph7_hashmap_node *pLs = pLeft->pFirst;` |
|        44 | 1330 | `		ph7_hashmap_node *pRs = pRight->pFirst;` |
|      1108 | 1331 | `		for( n = pLeft->nEntry ; n > 0 ; n-- ){` |
|         - | 1332 | `			/* Keys must match in type and value at this position */` |
|      1080 | 1333 | `			if( pLs->iType != pRs->iType ){` |
|       ! 0 | 1334 | `				return 1;` |
|         - | 1335 | `			}` |
|      1080 | 1336 | `			if( pLs->iType == HASHMAP_INT_NODE ){` |
|      1050 | 1337 | `				if( pLs->xKey.iKey != pRs->xKey.iKey ){` |
|         3 | 1338 | `					return 1;` |
|         - | 1339 | `				}` |
|       525 | 1340 | `			}else{` |
|        31 | 1341 | `				SyBlob *pLk = &pLs->xKey.sKey;` |
|        31 | 1342 | `				SyBlob *pRk = &pRs->xKey.sKey;` |
|        30 | 1343 | `				if( SyBlobLength(pLk) != SyBlobLength(pRk)` |
|        31 | 1344 | `				 \|\| (SyBlobLength(pLk) > 0` |
|        30 | 1345 | `				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){` |
|         7 | 1346 | `					return 1;` |
|         - | 1347 | `				}` |
|         - | 1348 | `			}` |
|         - | 1349 | `			/* Values must be strictly identical */` |
|      1072 | 1350 | `			if( HashmapNodeCmp(pLs,pRs,TRUE) != 0 ){` |
|         7 | 1351 | `				return 1;` |
|         - | 1352 | `			}` |
|      1066 | 1353 | `			pLs = pLs->pPrev; /* Reverse link = insertion order */` |
|      1066 | 1354 | `			pRs = pRs->pPrev;` |
|       534 | 1355 | `		}` |
|        30 | 1356 | `		return 0; /* Same pairs, same order */` |
|         - | 1357 | `	}` |
|         - | 1358 | `	/* Point to the first inserted entry of the left hashmap */` |
|        21 | 1359 | `	pLe = pLeft->pFirst;` |
|        21 | 1360 | `	pRe = 0; /* cc warning */` |
|         - | 1361 | `	/* Perform the comparison */` |
|        21 | 1362 | `	n = pLeft->nEntry;` |
|        20 | 1363 | `	for(;;){` |
|        43 | 1364 | `		if( n < 1 ){` |
|        16 | 1365 | `			break;` |
|         - | 1366 | `		}` |
|        29 | 1367 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1368 | `			/* Int key */` |
|        21 | 1369 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        12 | 1370 | `		}else{` |
|         9 | 1371 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1372 | `			/* Blob key */` |
|         9 | 1373 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1374 | `		}` |
|        29 | 1375 | `		if( rc != SXRET_OK ){` |
|         - | 1376 | `			/* No such entry in the right side */` |
|       ! 0 | 1377 | `			return 1;` |
|         - | 1378 | `		}` |
|        29 | 1379 | `		rc = 0;` |
|        29 | 1380 | `		if( bStrict ){` |
|         - | 1381 | `			/* Make sure,the keys are of the same type */` |
|       ! 0 | 1382 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1383 | `				rc = 1;` |
|       ! 0 | 1384 | `			}` |
|       ! 0 | 1385 | `		}` |
|        29 | 1386 | `		if( !rc ){` |
|         - | 1387 | `			/* Compare nodes */` |
|        29 | 1388 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        13 | 1389 | `		}` |
|        29 | 1390 | `		if( rc != 0 ){` |
|         - | 1391 | `			/* Nodes key/value differ */` |
|         6 | 1392 | `			return rc;` |
|         - | 1393 | `		}` |
|         - | 1394 | `		/* Point to the next entry */` |
|        24 | 1395 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        24 | 1396 | `		n--;` |
|         2 | 1397 | `	}` |
|        16 | 1398 | `	return 0; /* Hashmaps are equals */` |
|        40 | 1399 | `}` |
|         - | 1400 | `/*` |
|         - | 1401 | ` * Duplicate a hashmap node.` |
|         - | 1402 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1403 | ` */` |
|    767754 | 1404 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1405 | `	ph7_hashmap *pDest,` |
|         - | 1406 | `	ph7_hashmap_node *pEntry,` |
|         - | 1407 | `	ph7_value *pVal,` |
|         - | 1408 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1409 | `	)` |
|         5 | 1410 | `{` |
|         - | 1411 | `	ph7_value sSafeVal;` |
|         - | 1412 | `	ph7_value sKey;` |
|         - | 1413 | `	sxi32 rc;` |
|         - | 1414 |  |
|    767754 | 1415 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    767756 | 1416 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
|         - | 1417 | ``		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points`` |
|         - | 1418 | `		 * at an outside slot) or, the case PH7 missed, an element somebody took a` |
|         - | 1419 | ``		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through`` |
|         - | 1420 | `		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep` |
|         - | 1421 | ``		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */`` |
|         9 | 1422 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         9 | 1423 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1424 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1425 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1426 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1427 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1428 | `		}else{` |
|         7 | 1429 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         7 | 1430 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         3 | 1431 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1432 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1433 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1434 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1435 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1436 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1437 | `			}` |
|         - | 1438 | `		}` |
|         9 | 1439 | `		return rc;` |
|         - | 1440 | `	}` |
|    767751 | 1441 | `	sSafeVal = *pVal;` |
|         - | 1442 |  |
|    767751 | 1443 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1444 | `		/* Blob key insertion */` |
|      4923 | 1445 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4923 | 1446 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4923 | 1447 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4923 | 1448 | `		PH7_MemObjRelease(&sKey);` |
|      2464 | 1449 | `	}else{` |
|         - | 1450 | `		/* Int key */` |
|    762833 | 1451 | `		if( iAction == 0 ){ /* Merge */` |
|    758479 | 1452 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    383596 | 1453 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1454 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1455 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1456 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1457 | `		}else{ /* Dup */` |
|      4329 | 1458 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1459 | `		}` |
|         - | 1460 | `	}` |
|    767751 | 1461 | `	return rc;` |
|    383882 | 1462 | `}` |
|         - | 1463 | `/*` |
|         - | 1464 | ` * Merge two hashmaps.` |
|         - | 1465 | ` * Note on the merge process` |
|         - | 1466 | ` * According to the PHP language reference manual.` |
|         - | 1467 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1468 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1469 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1470 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1471 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1472 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1473 | ` *  keys starting from zero in the result array.` |
|         - | 1474 | ` */` |
|      3014 | 1475 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1476 | `{` |
|         - | 1477 | `	ph7_hashmap_node *pEntry;` |
|         - | 1478 | `	ph7_value *pVal;` |
|         - | 1479 | `	sxi32 rc;` |
|         - | 1480 | `	sxu32 n;` |
|      3019 | 1481 | `	if( pSrc == pDest ){` |
|         - | 1482 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1483 | `		 * Unlike the zend engine.` |
|         - | 1484 | `		 */` |
|       ! 0 | 1485 | `		return SXRET_OK;` |
|         - | 1486 | `	}` |
|         - | 1487 | `	/* Point to the first inserted entry in the source */` |
|      3019 | 1488 | `	pEntry = pSrc->pFirst;` |
|         - | 1489 | `	/* Perform the merge */` |
|    761557 | 1490 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1491 | `		/* Extract the node value */` |
|    758543 | 1492 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    758543 | 1493 | `		if( pVal ){` |
|         - | 1494 | `			/* Make a local copy of the value.` |
|         - | 1495 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1496 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1497 | `			 * to the old pool.` |
|         - | 1498 | `			 */` |
|    758543 | 1499 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    379274 | 1500 | `		}else{` |
|       ! 0 | 1501 | `			rc = SXRET_OK;` |
|         - | 1502 | `		}` |
|    758543 | 1503 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1504 | `			return rc;` |
|         - | 1505 | `		}` |
|         - | 1506 | `		/* Point to the next entry */` |
|    758543 | 1507 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    379274 | 1508 | `	}` |
|      3019 | 1509 | `	return SXRET_OK;` |
|      1512 | 1510 | `}` |
|         - | 1511 | `/*` |
|         - | 1512 | ` * Overwrite entries with the same key.` |
|         - | 1513 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1514 | ` *  According to the PHP language reference manual.` |
|         - | 1515 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1516 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1517 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1518 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1519 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1520 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1521 | ` *  overwriting the previous values.` |
|         - | 1522 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1523 | ` *  by whatever type is in the second array.` |
|         - | 1524 | ` */` |
|        34 | 1525 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1526 | `{` |
|         - | 1527 | `	ph7_hashmap_node *pEntry;` |
|         - | 1528 | `	ph7_value *pVal;` |
|         - | 1529 | `	sxi32 rc;` |
|         - | 1530 | `	sxu32 n;` |
|        36 | 1531 | `	if( pSrc == pDest ){` |
|         - | 1532 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1533 | `		 * Unlike the zend engine.` |
|         - | 1534 | `		 */` |
|       ! 0 | 1535 | `		return SXRET_OK;` |
|         - | 1536 | `	}` |
|         - | 1537 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1538 | `	pEntry = pSrc->pFirst;` |
|         - | 1539 | `	/* Perform the merge */` |
|        80 | 1540 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1541 | `		/* Extract the node value */` |
|        46 | 1542 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1543 | `		if( pVal ){` |
|        46 | 1544 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1545 | `		}else{` |
|       ! 0 | 1546 | `			rc = SXRET_OK;` |
|         - | 1547 | `		}` |
|        46 | 1548 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1549 | `			return rc;` |
|         - | 1550 | `		}` |
|         - | 1551 | `		/* Point to the next entry */` |
|        46 | 1552 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1553 | `	}` |
|        36 | 1554 | `	return SXRET_OK;` |
|        19 | 1555 | `}` |
|         - | 1556 | `/*` |
|         - | 1557 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1558 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1559 | ` */` |
|      8626 | 1560 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1561 | `{` |
|         - | 1562 | `	ph7_hashmap_node *pEntry;` |
|         - | 1563 | `	ph7_value *pVal;` |
|         - | 1564 | `	sxi32 rc;` |
|         - | 1565 | `	sxu32 n;` |
|      8631 | 1566 | `	if( pSrc == pDest ){` |
|         - | 1567 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1568 | `		 * Unlike the zend engine.` |
|         - | 1569 | `		 */` |
|       ! 0 | 1570 | `		return SXRET_OK;` |
|         - | 1571 | `	}` |
|         - | 1572 | `	/* Point to the first inserted entry in the source */` |
|      8631 | 1573 | `	pEntry = pSrc->pFirst;` |
|         - | 1574 | `	/* Perform the duplication */` |
|     17803 | 1575 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1576 | `		/* Extract the node value */` |
|      9177 | 1577 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      9177 | 1578 | `		if( pVal ){` |
|      9177 | 1579 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      4591 | 1580 | `		}else{` |
|       ! 0 | 1581 | `			rc = SXRET_OK;` |
|         - | 1582 | `		}` |
|      9177 | 1583 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1584 | `			return rc;` |
|         - | 1585 | `		}` |
|         - | 1586 | `		/* Point to the next entry */` |
|      9177 | 1587 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      4591 | 1588 | `	}` |
|      8631 | 1589 | `	return SXRET_OK;` |
|      4318 | 1590 | `}` |
|         - | 1591 | `/*` |
|         - | 1592 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1593 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1594 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1595 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1596 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1597 | ` * this instead of PH7_HashmapDup.` |
|         - | 1598 | ` */` |
|        12 | 1599 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1600 | `{` |
|         - | 1601 | `	ph7_hashmap_node *pEntry;` |
|         - | 1602 | `	ph7_value *pVal;` |
|         - | 1603 | `	sxi32 rc;` |
|         - | 1604 | `	sxu32 n;` |
|        13 | 1605 | `	if( pSrc == pDest ){` |
|       ! 0 | 1606 | `		return SXRET_OK;` |
|         - | 1607 | `	}` |
|        13 | 1608 | `	pEntry = pSrc->pFirst;` |
|       917 | 1609 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1610 | `		/* Extract the node value (resolves foreign references) */` |
|       905 | 1611 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       904 | 1612 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       586 | 1613 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1614 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1615 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1616 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1617 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1618 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1619 | `			pVal = 0;` |
|         2 | 1620 | `		}` |
|       905 | 1621 | `		if( pVal ){` |
|       901 | 1622 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1345 | 1623 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       448 | 1624 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       449 | 1625 | `			}else{` |
|         5 | 1626 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1627 | `			}` |
|       901 | 1628 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1629 | `				return rc;` |
|         - | 1630 | `			}` |
|       450 | 1631 | `		}` |
|         - | 1632 | `		/* Point to the next entry */` |
|       905 | 1633 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       453 | 1634 | `	}` |
|        13 | 1635 | `	return SXRET_OK;` |
|         7 | 1636 | `}` |
|         - | 1637 | `/*` |
|         - | 1638 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1639 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1640 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1641 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1642 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1643 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1644 | ` * iterate-a-snapshot semantic.` |
|         - | 1645 | ` */` |
|        50 | 1646 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1647 | `{` |
|         - | 1648 | `	ph7_foreach_step *pStep;` |
|        53 | 1649 | `	sxi32 nRef = 0;` |
|       103 | 1650 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        53 | 1651 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1652 | `			nRef++;` |
|        21 | 1653 | `		}` |
|        28 | 1654 | `	}` |
|        53 | 1655 | `	return nRef;` |
|         3 | 1656 | `}` |
|         - | 1657 | `/*` |
|         - | 1658 | ` * Copy-on-write separation for arrays.` |
|         - | 1659 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1660 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1661 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1662 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1663 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1664 | ` * on the live map the loop is walking, like php.` |
|         - | 1665 | ` */` |
|    296278 | 1666 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1667 | `{` |
|    296283 | 1668 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1669 | `	ph7_hashmap *pNew;` |
|         - | 1670 | `	ph7_value *pBacking;` |
|         - | 1671 | `	sxu32 nValIdx;` |
|         - | 1672 | `	int bValueInPool;` |
|    296283 | 1673 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    296283 | 1674 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1675 | `		/* Sole owner, no separation needed */` |
|    292927 | 1676 | `		return pMap;` |
|         - | 1677 | `	}` |
|      3361 | 1678 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1679 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1680 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1681 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       172 | 1682 | `		return pMap;` |
|         - | 1683 | `	}` |
|         - | 1684 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1685 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1686 | `	 * frame is popped. */` |
|      3191 | 1687 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      3191 | 1688 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      3186 | 1689 | `		if( pBacking && pBacking != pValue` |
|      3161 | 1690 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      3141 | 1691 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1692 | `			/* Undo the stack ref to reveal true sharing count */` |
|      3141 | 1693 | `			pMap->iRef--;` |
|      3141 | 1694 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1695 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2989 | 1696 | `				pMap->iRef++;` |
|      2989 | 1697 | `				return pMap;` |
|         - | 1698 | `			}` |
|       154 | 1699 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|       154 | 1700 | `			if( pNew == 0 ){` |
|       ! 0 | 1701 | `				pMap->iRef++;` |
|       ! 0 | 1702 | `				return pMap;` |
|         - | 1703 | `			}` |
|       154 | 1704 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1705 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1706 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1707 | `				pMap->iRef++;` |
|       ! 0 | 1708 | `				return pMap;` |
|         - | 1709 | `			}` |
|       154 | 1710 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|       154 | 1711 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1712 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1713 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1714 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1715 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1716 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1717 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1718 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|       154 | 1719 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|       154 | 1720 | `			if( pBacking ){` |
|       154 | 1721 | `				pBacking->x.pOther = pNew;` |
|        76 | 1722 | `			}` |
|         - | 1723 | `			/* Update the stack value to match */` |
|       154 | 1724 | `			pValue->x.pOther = pNew;` |
|       154 | 1725 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|       154 | 1726 | `			return pNew;` |
|         - | 1727 | `		}` |
|        25 | 1728 | `	}` |
|         - | 1729 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1730 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1731 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1732 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1733 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1734 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1735 | `	 * pBacking in the backing-variable branch above). */` |
|        53 | 1736 | `	nValIdx = pValue->nIdx;` |
|        78 | 1737 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        50 | 1738 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        53 | 1739 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        53 | 1740 | `	if( pNew == 0 ){` |
|         - | 1741 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1742 | `		return pMap;` |
|         - | 1743 | `	}` |
|        53 | 1744 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1745 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1746 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1747 | `		return pMap;` |
|         - | 1748 | `	}` |
|        53 | 1749 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        53 | 1750 | `	pMap->iRef--;` |
|        53 | 1751 | `	if( bValueInPool ){` |
|         - | 1752 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        53 | 1753 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        53 | 1754 | `		if( pValue == 0 ){` |
|       ! 0 | 1755 | `			return pNew;` |
|         - | 1756 | `		}` |
|        25 | 1757 | `	}` |
|        53 | 1758 | `	pValue->x.pOther = pNew;` |
|        53 | 1759 | `	return pNew;` |
|    148144 | 1760 | `}` |
|         - | 1761 | `/*` |
|         - | 1762 | ` * Perform the union of two hashmaps.` |
|         - | 1763 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1764 | ` * with a variable holding an array as follows:` |
|         - | 1765 | ` * <?php` |
|         - | 1766 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1767 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1768 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1769 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1770 | ` * var_dump($c);` |
|         - | 1771 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1772 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1773 | ` * var_dump($c);` |
|         - | 1774 | ` * ?>` |
|         - | 1775 | ` * When executed, this script will print the following:` |
|         - | 1776 | ` * Union of $a and $b:` |
|         - | 1777 | ` * array(3) {` |
|         - | 1778 | ` *  ["a"]=>` |
|         - | 1779 | ` *  string(5) "apple"` |
|         - | 1780 | ` *  ["b"]=>` |
|         - | 1781 | ` * string(6) "banana"` |
|         - | 1782 | ` *  ["c"]=>` |
|         - | 1783 | ` * string(6) "cherry"` |
|         - | 1784 | ` * }` |
|         - | 1785 | ` * Union of $b and $a:` |
|         - | 1786 | ` * array(3) {` |
|         - | 1787 | ` * ["a"]=>` |
|         - | 1788 | ` * string(4) "pear"` |
|         - | 1789 | ` * ["b"]=>` |
|         - | 1790 | ` * string(10) "strawberry"` |
|         - | 1791 | ` * ["c"]=>` |
|         - | 1792 | ` * string(6) "cherry"` |
|         - | 1793 | ` * }` |
|         - | 1794 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1795 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1796 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1797 | ` */` |
|      4468 | 1798 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1799 | `{` |
|         - | 1800 | `	ph7_hashmap_node *pEntry;` |
|      4473 | 1801 | `	sxi32 rc = SXRET_OK;` |
|         - | 1802 | `	ph7_value *pObj;` |
|         - | 1803 | `	sxu32 n;` |
|      4473 | 1804 | `	if( pLeft == pRight ){` |
|         - | 1805 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1806 | `		 * Unlike the zend engine.` |
|         - | 1807 | `		 */` |
|       ! 0 | 1808 | `		return SXRET_OK;` |
|         - | 1809 | `	}` |
|         - | 1810 | `	/* Perform the union */` |
|      4473 | 1811 | `	pEntry = pRight->pFirst;` |
|      4519 | 1812 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1813 | `		/* Make sure the given key does not exists in the left array */` |
|        50 | 1814 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1815 | `			/* BLOB key */` |
|        24 | 1816 | `			if( SXRET_OK !=` |
|        20 | 1817 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1818 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1819 | `					if( pObj ){` |
|        20 | 1820 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1821 | `						/* Perform the insertion */` |
|        20 | 1822 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1823 | `							&sSafeVal,0,FALSE);` |
|        20 | 1824 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1825 | `							return rc;` |
|         - | 1826 | `						}` |
|         8 | 1827 | `					}` |
|         8 | 1828 | `			}` |
|        14 | 1829 | `		}else{` |
|         - | 1830 | `			/* INT key */` |
|        28 | 1831 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        15 | 1832 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        15 | 1833 | `				if( pObj ){` |
|        15 | 1834 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1835 | `					/* Perform the insertion */` |
|        15 | 1836 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        15 | 1837 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1838 | `						return rc;` |
|         - | 1839 | `					}` |
|         7 | 1840 | `				}` |
|         7 | 1841 | `			}` |
|         - | 1842 | `		}` |
|         - | 1843 | `		/* Point to the next entry */` |
|        50 | 1844 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        27 | 1845 | `	}` |
|      4473 | 1846 | `	return SXRET_OK;` |
|      2239 | 1847 | `}` |
|         - | 1848 | `/*` |
|         - | 1849 | ` * Allocate a new hashmap.` |
|         - | 1850 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1851 | ` */` |
|   2234122 | 1852 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1853 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1854 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1855 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1856 | `	)` |
|         5 | 1857 | `{` |
|         - | 1858 | `	ph7_hashmap *pMap;` |
|         - | 1859 | `	/* Allocate a new instance */` |
|   2234127 | 1860 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   2234127 | 1861 | `	if( pMap == 0 ){` |
|       ! 0 | 1862 | `		return 0;` |
|         - | 1863 | `	}` |
|         - | 1864 | `	/* Zero the structure */` |
|   2234127 | 1865 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1866 | `	/* Fill in the structure */` |
|   2234127 | 1867 | `	pMap->pVm = &(*pVm);` |
|   2234127 | 1868 | `	pMap->iRef = 1;` |
|         - | 1869 | `	/* Default hash functions */` |
|   2234127 | 1870 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   2234127 | 1871 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   2234127 | 1872 | `	return pMap;` |
|   1117066 | 1873 | `}` |
|         - | 1874 | `/*` |
|         - | 1875 | ` * Install superglobals in the given virtual machine.` |
|         - | 1876 | ` * Note on superglobals.` |
|         - | 1877 | ` *  According to the PHP language reference manual.` |
|         - | 1878 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1879 | `*   Description` |
|         - | 1880 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1881 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1882 | `*   global $variable; to access them within functions or methods.` |
|         - | 1883 | `*   These superglobal variables are:` |
|         - | 1884 | `*    $GLOBALS` |
|         - | 1885 | `*    $_SERVER` |
|         - | 1886 | `*    $_GET` |
|         - | 1887 | `*    $_POST` |
|         - | 1888 | `*    $_FILES` |
|         - | 1889 | `*    $_COOKIE` |
|         - | 1890 | `*    $_SESSION` |
|         - | 1891 | `*    $_REQUEST` |
|         - | 1892 | `*    $_ENV` |
|         - | 1893 | `*/` |
|      3964 | 1894 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1895 | `{` |
|         - | 1896 | `	static const char * azSuper[] = {` |
|         - | 1897 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1898 | `		"_GET",      /* $_GET */` |
|         - | 1899 | `		"_POST",     /* $_POST */` |
|         - | 1900 | `		"_FILES",    /* $_FILES */` |
|         - | 1901 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1902 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1903 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1904 | `		"_ENV",      /* $_ENV */` |
|         - | 1905 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1906 | `		"argv"       /* $argv */` |
|         - | 1907 | `	};` |
|         - | 1908 | `	ph7_hashmap *pMap;` |
|         - | 1909 | `	ph7_value *pObj;` |
|         - | 1910 | `	SyString *pFile;` |
|         - | 1911 | `	sxi32 rc;` |
|         - | 1912 | `	sxu32 n;` |
|         - | 1913 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3969 | 1914 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3969 | 1915 | `	if( pMap == 0 ){` |
|       ! 0 | 1916 | `		return SXERR_MEM;` |
|         - | 1917 | `	}` |
|      3969 | 1918 | `	pVm->pGlobal = pMap;` |
|         - | 1919 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3969 | 1920 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3969 | 1921 | `	if( pObj == 0 ){` |
|       ! 0 | 1922 | `		return SXERR_MEM;` |
|         - | 1923 | `	}` |
|      3969 | 1924 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1925 | `	/* Record object index */` |
|      3969 | 1926 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1927 | `	/* Install the special $GLOBALS array */` |
|      3969 | 1928 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3969 | 1929 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1930 | `		return rc;` |
|         - | 1931 | `	}` |
|         - | 1932 | `	/* Install superglobals now */` |
|     43609 | 1933 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1934 | `		ph7_value *pSuper;` |
|         - | 1935 | `		/* Request an empty array */` |
|     39645 | 1936 | `		pSuper = ph7_new_array(&(*pVm));` |
|     39645 | 1937 | `		if( pSuper == 0 ){` |
|       ! 0 | 1938 | `			return SXERR_MEM;` |
|         - | 1939 | `		}` |
|         - | 1940 | `		/* Install */` |
|     39645 | 1941 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     39645 | 1942 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1943 | `			return rc;` |
|         - | 1944 | `		}` |
|         - | 1945 | `		/* Release the value now it have been installed */` |
|     39645 | 1946 | `		ph7_release_value(&(*pVm),pSuper);` |
|     19825 | 1947 | `	}` |
|         - | 1948 | `	/* Set some $_SERVER entries */` |
|      3969 | 1949 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1950 | `	/*` |
|         - | 1951 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1952 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1953 | `	 */` |
|      7933 | 1954 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1955 | `		"SCRIPT_FILENAME",` |
|      1982 | 1956 | `		pFile ? pFile->zString : ":Memory:",` |
|      3964 | 1957 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1958 | `		);` |
|         - | 1959 | `	/* All done,all super-global are installed now */` |
|      3969 | 1960 | `	return SXRET_OK;` |
|      1987 | 1961 | `}` |
|         - | 1962 | `/*` |
|         - | 1963 | ` * Release a hashmap.` |
|         - | 1964 | ` */` |
|   2077530 | 1965 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1966 | `{` |
|         - | 1967 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   2077535 | 1968 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1969 | `	sxu32 n;` |
|   2077535 | 1970 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1971 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1972 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1973 | `		return SXRET_OK;` |
|         - | 1974 | `	}` |
|   2077535 | 1975 | `	if( pMap->pActiveSteps ){` |
|         - | 1976 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1977 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1978 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1979 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1980 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1981 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1982 | `		ph7_foreach_step *pStep;` |
|        17 | 1983 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1984 | `			pStep->pCursor = 0;` |
|         5 | 1985 | `		}` |
|         4 | 1986 | `	}` |
|         - | 1987 | `	/* Start the release process */` |
|   2077535 | 1988 | `	n = 0;` |
|   2077535 | 1989 | `	pEntry = pMap->pFirst;` |
|   4725994 | 1990 | `	for(;;){` |
|   9451993 | 1991 | `		if( n >= pMap->nEntry ){` |
|   2077535 | 1992 | `			break;` |
|         - | 1993 | `		}` |
|   7374463 | 1994 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1995 | `		/* Remove the reference from the foreign table */` |
|   7374463 | 1996 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   7374463 | 1997 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1998 | `			/* Restore the ph7_value to the free list */` |
|   7374403 | 1999 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   3687199 | 2000 | `		}` |
|         - | 2001 | `		/* Release the node */` |
|   7374463 | 2002 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   2997783 | 2003 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   1498889 | 2004 | `		}` |
|   7374463 | 2005 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 2006 | `		/* Point to the next entry */` |
|   7374463 | 2007 | `		pEntry = pNext;` |
|   7374463 | 2008 | `		n++;` |
|         5 | 2009 | `	}` |
|   2077535 | 2010 | `	if( pMap->nEntry > 0 ){` |
|         - | 2011 | `		/* Release the hash bucket */` |
|   1322979 | 2012 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|    661487 | 2013 | `	}` |
|   2077535 | 2014 | `	if( FreeDS ){` |
|         - | 2015 | `		/* Free the whole instance */` |
|   2077507 | 2016 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   1038756 | 2017 | `	}else{` |
|         - | 2018 | `		/* Keep the instance but reset it's fields */` |
|        30 | 2019 | `		pMap->apBucket = 0;` |
|        30 | 2020 | `		pMap->iNextIdx = 0;` |
|        30 | 2021 | `	pMap->bIntKeySeen = 0;` |
|        30 | 2022 | `		pMap->nEntry = pMap->nSize = 0;` |
|        30 | 2023 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 2024 | `	}` |
|   2077535 | 2025 | `	return SXRET_OK;` |
|   1038770 | 2026 | `}` |
|         - | 2027 | `/*` |
|         - | 2028 | ` * Decrement the reference count of a given hashmap.` |
|         - | 2029 | ` * If the count reaches zero which mean no more variables` |
|         - | 2030 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 2031 | ` */` |
|   5155632 | 2032 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 2033 | `{` |
|   5155637 | 2034 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 2035 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|   5155637 | 2036 | `	pMap->iRef--;` |
|   5155637 | 2037 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   2077487 | 2038 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   1038741 | 2039 | `	}` |
|   5155637 | 2040 | `}` |
|         - | 2041 | `/*` |
|         - | 2042 | ` * Check if a given key exists in the given hashmap.` |
|         - | 2043 | ` * Write a pointer to the target node on success.` |
|         - | 2044 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 2045 | ` */` |
|    600760 | 2046 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 2047 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 2048 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 2049 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 2050 | `	)` |
|         5 | 2051 | `{` |
|         - | 2052 | `	sxi32 rc;` |
|    600765 | 2053 | `	if( pMap->nEntry < 1 ){` |
|         - | 2054 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 2055 | `		 */` |
|       387 | 2056 | `		return SXERR_NOTFOUND;` |
|         - | 2057 | `	}` |
|    600383 | 2058 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    600383 | 2059 | `	return rc;` |
|    300385 | 2060 | `}` |
|         - | 2061 | `/*` |
|         - | 2062 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 2063 | ` * hashmap.` |
|         - | 2064 | ` * If a node with the given key already exists in the database` |
|         - | 2065 | ` * then this function overwrite the old value.` |
|         - | 2066 | ` */` |
|   6686146 | 2067 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 2068 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2069 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2070 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 2071 | `	)` |
|         5 | 2072 | `{` |
|         - | 2073 | `	sxi32 rc;` |
|         - | 2074 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 2075 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 2076 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 2077 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   6686151 | 2078 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   6686151 | 2079 | `	return rc;` |
|         5 | 2080 | `}` |
|         - | 2081 | `/*` |
|         - | 2082 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 2083 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 2084 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 2085 | ` * This is the same routine that backs array_merge().` |
|         - | 2086 | ` */` |
|       658 | 2087 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 2088 | `{` |
|       659 | 2089 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 2090 | `}` |
|         - | 2091 | `/*` |
|         - | 2092 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 2093 | ` * hashmap.` |
|         - | 2094 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 2095 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 2096 | ` * The insertion by reference is triggered when the following` |
|         - | 2097 | ` * expression is encountered.` |
|         - | 2098 | ` * $var = 10;` |
|         - | 2099 | ` *  $a = array(&var);` |
|         - | 2100 | ` * OR` |
|         - | 2101 | ` *  $a[] =& $var;` |
|         - | 2102 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 2103 | ` * over it's contents.` |
|         - | 2104 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 2105 | ` * removed when the foreign ph7_value is unset.` |
|         - | 2106 | ` * Example:` |
|         - | 2107 | ` *  $var = 10;` |
|         - | 2108 | ` *  $a[] =& $var;` |
|         - | 2109 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 2110 | ` *  //Unset the foreign ph7_value now` |
|         - | 2111 | ` *  unset($var);` |
|         - | 2112 | ` *  echo count($a); //0` |
|         - | 2113 | ` * Note that this is a PH7 eXtension.` |
|         - | 2114 | ` * Refer to the official documentation for more information.` |
|         - | 2115 | ` * If a node with the given key already exists in the database` |
|         - | 2116 | ` * then this function overwrite the old value.` |
|         - | 2117 | ` */` |
|     53798 | 2118 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 2119 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2120 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 2121 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 2122 | `	)` |
|         5 | 2123 | `{` |
|         - | 2124 | `	sxi32 rc;` |
|     53803 | 2125 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 2126 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 2127 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 2128 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 2129 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 2130 | `		return PH7_ABORT;` |
|         - | 2131 | `	}` |
|     53803 | 2132 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     53803 | 2133 | `	return rc;` |
|     26904 | 2134 | `}` |
|         - | 2135 | `/*` |
|         - | 2136 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 2137 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 2138 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 2139 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 2140 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 2141 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 2142 | ` */` |
|     27224 | 2143 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2144 | `{` |
|     27229 | 2145 | `	pStep->pCursor = pMap->pFirst;` |
|     27229 | 2146 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     27229 | 2147 | `	pMap->pActiveSteps = pStep;` |
|     27229 | 2148 | `}` |
|         - | 2149 | `/*` |
|         - | 2150 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 2151 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 2152 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 2153 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 2154 | ` */` |
|     26978 | 2155 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 2156 | `{` |
|     26983 | 2157 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     26983 | 2158 | `	while( *ppLink ){` |
|     26983 | 2159 | `		if( *ppLink == pStep ){` |
|     26983 | 2160 | `			*ppLink = pStep->pNextActive;` |
|     26983 | 2161 | `			pStep->pNextActive = 0;` |
|     26983 | 2162 | `			return;` |
|         - | 2163 | `		}` |
|       ! 0 | 2164 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 2165 | `	}` |
|     13494 | 2166 | `}` |
|         - | 2167 | `/*` |
|         - | 2168 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 2169 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 2170 | ` * return NULL.` |
|         - | 2171 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 2172 | ` */` |
|        64 | 2173 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 2174 | `{` |
|        65 | 2175 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 2176 | `	if( pCur == 0 ){` |
|         - | 2177 | `		/* End of the list,return null */` |
|        27 | 2178 | `		return 0;` |
|         - | 2179 | `	}` |
|         - | 2180 | `	/* Advance the node cursor */` |
|        39 | 2181 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 2182 | `	return pCur;` |
|        33 | 2183 | `}` |
|         - | 2184 | `/*` |
|         - | 2185 | ` * Extract a node value.` |
|         - | 2186 | ` */` |
|    715954 | 2187 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2188 | `{` |
|    715959 | 2189 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    715959 | 2190 | `	if( pEntry ){` |
|    715959 | 2191 | `		if( bStore ){` |
|    271281 | 2192 | `			PH7_MemObjStore(pEntry,pValue);` |
|    135643 | 2193 | `		}else{` |
|    444683 | 2194 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2195 | `		}` |
|    357685 | 2196 | `	}else{` |
|       ! 0 | 2197 | `		PH7_MemObjRelease(pValue);` |
|         - | 2198 | `	}` |
|    715959 | 2199 | `}` |
|         - | 2200 | `/*` |
|         - | 2201 | ` * Extract a node key.` |
|         - | 2202 | ` */` |
|    212252 | 2203 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2204 | `{` |
|         - | 2205 | `	/* Fill with the current key */` |
|    212257 | 2206 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    204599 | 2207 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        39 | 2208 | `			SyBlobRelease(&pKey->sBlob);` |
|        19 | 2209 | `		}` |
|    204599 | 2210 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    204599 | 2211 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|    102302 | 2212 | `	}else{` |
|      7663 | 2213 | `		SyBlobReset(&pKey->sBlob);` |
|      7663 | 2214 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      7663 | 2215 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2216 | `	}` |
|    212257 | 2217 | `}` |
|         - | 2218 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2219 | `/*` |
|         - | 2220 | ` * Store the address of nodes value in the given container.` |
|         - | 2221 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2222 | ` * defined in 'builtin.c' for more information.` |
|         - | 2223 | ` */` |
|        20 | 2224 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         2 | 2225 | `{` |
|        22 | 2226 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2227 | `	ph7_value *pValue;` |
|         - | 2228 | `	sxu32 n;` |
|         - | 2229 | `	/* Initialize the container */` |
|        22 | 2230 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        54 | 2231 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2232 | `		/* Extract node value */` |
|        34 | 2233 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        34 | 2234 | `		if( pValue ){` |
|        34 | 2235 | `			SySetPut(pOut,(const void *)&pValue);` |
|        16 | 2236 | `		}` |
|         - | 2237 | `		/* Point to the next entry */` |
|        34 | 2238 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        18 | 2239 | `	}` |
|         - | 2240 | `	/* Total inserted entries */` |
|        22 | 2241 | `	return (int)SySetUsed(pOut);` |
|         2 | 2242 | `}` |
|         - | 2243 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2244 | `/*` |
|         - | 2245 | ` * Table of hashmap functions.` |
|         - | 2246 | ` */` |
|         - | 2247 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 2248 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 2249 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 2250 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 2251 | `	{"count",             ph7_hashmap_count },` |
|         - | 2252 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 2253 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 2254 | `	{"key_exists",        ph7_hashmap_key_exists },` |
|         - | 2255 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 2256 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 2257 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 2258 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 2259 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 2260 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 2261 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 2262 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 2263 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 2264 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 2265 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 2266 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 2267 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 2268 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 2269 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 2270 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 2271 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 2272 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 2273 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 2274 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 2275 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 2276 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 2277 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 2278 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 2279 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 2280 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 2281 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 2282 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 2283 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 2284 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 2285 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 2286 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 2287 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 2288 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 2289 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 2290 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 2291 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 2292 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 2293 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 2294 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 2295 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 2296 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 2297 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 2298 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 2299 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 2300 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 2301 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 2302 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 2303 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 2304 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 2305 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 2306 | `	{"natsort",           ph7_hashmap_natsort },` |
|         - | 2307 | `	{"natcasesort",       ph7_hashmap_natsort },` |
|         - | 2308 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 2309 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 2310 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 2311 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 2312 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 2313 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 2314 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 2315 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 2316 | `	{"range",             ph7_hashmap_range   },` |
|         - | 2317 | `	{"current",           ph7_hashmap_current },` |
|         - | 2318 | `	{"each",              ph7_hashmap_each    },` |
|         - | 2319 | `	{"pos",               ph7_hashmap_current },` |
|         - | 2320 | `	{"next",              ph7_hashmap_next    },` |
|         - | 2321 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 2322 | `	{"end",               ph7_hashmap_end     },` |
|         - | 2323 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 2324 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 2325 | `};` |
|         - | 2326 | `/*` |
|         - | 2327 | ` * Register the built-in hashmap functions defined above.` |
|         - | 2328 | ` */` |
|      3956 | 2329 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 2330 | `{` |
|         - | 2331 | `	sxu32 n;` |
|    308573 | 2332 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    304617 | 2333 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    152311 | 2334 | `	}` |
|      3961 | 2335 | `}` |
|         - | 2336 | `/*` |
|         - | 2337 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 2338 | ` * the BLOB given as the first argument.` |
|         - | 2339 | ` * This function is typically invoked when the user issue a call to` |
|         - | 2340 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 2341 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 2342 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 2343 | ` */` |
|         - | 2344 | `/*` |
|         - | 2345 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 2346 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 2347 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 2348 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 2349 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 2350 | ` */` |
|       346 | 2351 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         5 | 2352 | `{` |
|       351 | 2353 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2354 | `	ph7_value *pObj;` |
|       351 | 2355 | `	sxu32 n = 0;` |
|         - | 2356 | `	int isRef;` |
|       351 | 2357 | `	sxi32 rc = SXRET_OK;` |
|         - | 2358 | `	int i;` |
|       507 | 2359 | `	for(;;){` |
|      1019 | 2360 | `		if( n >= pMap->nEntry ){` |
|       351 | 2361 | `			break;` |
|         - | 2362 | `		}` |
|       673 | 2363 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 2364 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 2365 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|      1333 | 2366 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       668 | 2367 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       673 | 2368 | `		if( ShowType ){` |
|         - | 2369 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 2370 | `			 * on the next line at the same indent (php). */` |
|      1217 | 2371 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       817 | 2372 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       411 | 2373 | `			}` |
|       405 | 2374 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       273 | 2375 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|       139 | 2376 | `			}else{` |
|       203 | 2377 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|        66 | 2378 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2379 | `			}` |
|       405 | 2380 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       405 | 2381 | `			if( pObj ){` |
|       405 | 2382 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|       405 | 2383 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2384 | `					break;` |
|         - | 2385 | `				}` |
|       200 | 2386 | `			}` |
|       205 | 2387 | `		}else{` |
|         - | 2388 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 2389 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 2390 | `			 * php's extra blank line. References carry no marker. */` |
|      1456 | 2391 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1188 | 2392 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       596 | 2393 | `			}` |
|       272 | 2394 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       155 | 2395 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        79 | 2396 | `			}else{` |
|       176 | 2397 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        58 | 2398 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2399 | `			}` |
|       268 | 2400 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       151 | 2401 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        28 | 2402 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        28 | 2403 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        28 | 2404 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2405 | `					break;` |
|         - | 2406 | `				}` |
|        15 | 2407 | `			}else{` |
|       245 | 2408 | `				if( pObj ){` |
|       245 | 2409 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       121 | 2410 | `				}` |
|       245 | 2411 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 2412 | `			}` |
|         - | 2413 | `		}` |
|         - | 2414 | `		/* Point to the next entry */` |
|       673 | 2415 | `		n++;` |
|       673 | 2416 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         5 | 2417 | `	}` |
|       351 | 2418 | `	return rc;` |
|         5 | 2419 | `}` |
|       340 | 2420 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         5 | 2421 | `{` |
|         - | 2422 | `	sxi32 rc;` |
|         - | 2423 | `	int i;` |
|       345 | 2424 | `	if( nDepth > 31 ){` |
|         - | 2425 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 2426 | `		/* Nesting limit reached */` |
|       ! 0 | 2427 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 2428 | `		return SXERR_LIMIT;` |
|         - | 2429 | `	}` |
|       345 | 2430 | `	if( ShowType ){` |
|         - | 2431 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 2432 | `		 * newline (a nested array is itself an entry value line). */` |
|       225 | 2433 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|       225 | 2434 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       225 | 2435 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|       237 | 2436 | `		for( i = 0 ; i < nTab ; i++ ){` |
|        15 | 2437 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|         9 | 2438 | `		}` |
|       225 | 2439 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       225 | 2440 | `		return rc;` |
|         - | 2441 | `	}` |
|         - | 2442 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       123 | 2443 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       315 | 2444 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 2445 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 2446 | `	}` |
|       123 | 2447 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       123 | 2448 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       315 | 2449 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 2450 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 2451 | `	}` |
|       123 | 2452 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       123 | 2453 | `	return rc;` |
|       175 | 2454 | `}` |
|         - | 2455 | `/*` |
|         - | 2456 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 2457 | ` * retrieved entry.` |
|         - | 2458 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 2459 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 2460 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 2461 | ` * a value different from PH7_OK.` |
|         - | 2462 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 2463 | ` */` |
|     38068 | 2464 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 2465 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2466 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 2467 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 2468 | `	)` |
|         5 | 2469 | `{` |
|         - | 2470 | `	ph7_hashmap_node *pEntry;` |
|         - | 2471 | `	ph7_value sKey,sValue;` |
|         - | 2472 | `	sxi32 rc;` |
|         - | 2473 | `	sxu32 n;` |
|         - | 2474 | `	/* Initialize walker parameter */` |
|     38073 | 2475 | `	rc = SXRET_OK;` |
|     38073 | 2476 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     38073 | 2477 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     38073 | 2478 | `	n = pMap->nEntry;` |
|     38073 | 2479 | `	pEntry = pMap->pFirst;` |
|         - | 2480 | `	/* Start the iteration process */` |
|    116287 | 2481 | `	for(;;){` |
|    232579 | 2482 | `		if( n < 1 ){` |
|     38051 | 2483 | `			break;` |
|         - | 2484 | `		}` |
|         - | 2485 | `		/* Extract a copy of the key and a copy the current value */` |
|    194533 | 2486 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    194533 | 2487 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 2488 | `		/* Invoke the user callback */` |
|    194533 | 2489 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 2490 | `		/* Release the copy of the key and the value */` |
|    194533 | 2491 | `		PH7_MemObjRelease(&sKey);` |
|    194533 | 2492 | `		PH7_MemObjRelease(&sValue);` |
|    194533 | 2493 | `		if( rc != PH7_OK ){` |
|         - | 2494 | `			/* Callback request an operation abort */` |
|        25 | 2495 | `			return SXERR_ABORT;` |
|         - | 2496 | `		}` |
|         - | 2497 | `		/* Point to the next entry */` |
|    194511 | 2498 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    194511 | 2499 | `		n--;` |
|         5 | 2500 | `	}` |
|         - | 2501 | `	/* All done */` |
|     38051 | 2502 | `	return SXRET_OK;` |
|     19039 | 2503 | `}` |
|         - | 2504 |  |
