# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3933/4436 lines (88.66%)

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
|   7464968 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7464973 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7464973 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    640552 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    640557 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    640557 |   35 | `	sxu32 nH = 5381;` |
|    640557 |   36 | `	zEnd = &zIn[nLen];` |
|    725238 |   37 | `	for(;;){` |
|   1450481 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1234891 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1105953 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    965285 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    640557 |   43 | `	return nH;` |
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
|   3163796 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3163801 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3163801 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3163801 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3163801 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3163801 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3163801 |  112 | `	pNode->nHash = nHash;` |
|   3163801 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3163801 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3163801 |  115 | `	return pNode;` |
|   1581903 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    270296 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    270301 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    270301 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    270301 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    270301 |  133 | `	pNode->pMap  = &(*pMap);` |
|    270301 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    270301 |  135 | `	pNode->nHash = nHash;` |
|    270301 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    270301 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    270301 |  138 | `	pNode->nValIdx = nValIdx;` |
|    270301 |  139 | `	return pNode;` |
|    135153 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3434092 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3434097 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2951533 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2951533 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1475764 |  150 | `	}` |
|   3434097 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3434097 |  153 | `	if( pMap->pFirst == 0 ){` |
|     91863 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     91863 |  156 | `		pMap->pCur = pNode;` |
|     45934 |  157 | `	}else{` |
|   3342239 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3434097 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3434097 |  174 | `	++pMap->nEntry;` |
|   3434097 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7948 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7953 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7953 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7953 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7481 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3743 |  187 | `	}else{` |
|       474 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7953 |  190 | `	if( pNode->pNextCollide ){` |
|      5167 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2582 |  192 | `	}` |
|      7953 |  193 | `	if( pMap->pFirst == pNode ){` |
|       199 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        97 |  195 | `	}` |
|      7953 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       231 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       113 |  199 | `	}` |
|      7953 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7953 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7953 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       209 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       209 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       209 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       102 |  218 | `		}` |
|       102 |  219 | `	}` |
|      7953 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7691 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3843 |  222 | `	}` |
|      7953 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7953 |  224 | `	pMap->nEntry--;` |
|      7953 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       123 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       123 |  228 | `		pMap->apBucket = 0;` |
|       123 |  229 | `		pMap->nSize = 0;` |
|       123 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        59 |  231 | `	}` |
|      7953 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3434092 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3434097 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     96987 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     96987 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     96987 |  245 | `		if( nNew < 1 ){` |
|     91863 |  246 | `			nNew = 16;` |
|     45929 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     96987 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     96987 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     96987 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     96987 |  260 | `		pMap->apBucket = apNew;` |
|     96987 |  261 | `		pMap->nSize = nNew;` |
|     96987 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     91863 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5129 |  267 | `		pEntry = pMap->pFirst;` |
|      5129 |  268 | `		n = 0;` |
|   2112594 |  269 | `		for( ;; ){` |
|   4225193 |  270 | `			if( n >= pMap->nEntry ){` |
|      5129 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   4220069 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   4220069 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4220069 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3597873 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3597873 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1798934 |  280 | `			}` |
|   4220069 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   4220069 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4220069 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5129 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2562 |  288 | `	}` |
|   3342239 |  289 | `	return SXRET_OK;` |
|   1717051 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3163796 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3163801 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3163763 |  310 | `		if( pValue ){` |
|   3163757 |  311 | `			sSafeVal = *pValue;` |
|   3163757 |  312 | `			pValue = &sSafeVal;` |
|   1581876 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3163763 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3163763 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3163763 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3163757 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1581876 |  322 | `		}` |
|   3163763 |  323 | `		nIdx = pObj->nIdx;` |
|   1581884 |  324 | `	}else{` |
|        39 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3163801 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3163801 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3163801 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3163801 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        39 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3163801 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3163801 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3163801 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3163801 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3163801 |  349 | `	return SXRET_OK;` |
|   1581903 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    270296 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    270301 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    224515 |  370 | `		if( pValue ){` |
|    224205 |  371 | `			sSafeVal = *pValue;` |
|    224205 |  372 | `			pValue = &sSafeVal;` |
|    112100 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    224515 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    224515 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    224515 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    224205 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|    112100 |  382 | `		}` |
|    224515 |  383 | `		nIdx = pObj->nIdx;` |
|    112260 |  384 | `	}else{` |
|     45791 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    270301 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    270301 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    270301 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    270301 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     45791 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     22893 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    270301 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    270301 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    270301 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    270301 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    270301 |  409 | `	return SXRET_OK;` |
|    135153 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4287902 |  416 | `static sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4287907 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       727 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4287185 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4287185 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110564126 |  433 | `	for(;;){` |
| 221128257 |  434 | `		if( pNode == 0 ){` |
|   4282207 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216846050 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216843038 |  438 | `			&& pNode->nHash == nHash` |
| 108422507 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      4983 |  441 | `				if( ppNode ){` |
|      4965 |  442 | `					*ppNode = pNode;` |
|      2480 |  443 | `				}` |
|      4983 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216841073 |  447 | `		pNode = pNode->pNextCollide;` |
|         1 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4282207 |  450 | `	return SXERR_NOTFOUND;` |
|   2143956 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    406136 |  457 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    406141 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     35885 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    370261 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    370261 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    306268 |  475 | `	for(;;){` |
|    612541 |  476 | `		if( pNode == 0 ){` |
|    312133 |  477 | `			break;` |
|         - |  478 | `		}` |
|    300408 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    298897 |  480 | `			&& pNode->nHash == nHash` |
|    177807 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     58233 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     58133 |  484 | `				if( ppNode ){` |
|     58105 |  485 | `					*ppNode = pNode;` |
|     29050 |  486 | `				}` |
|     58133 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    242285 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    312133 |  493 | `	return SXERR_NOTFOUND;` |
|    203073 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    406268 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    406273 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    406273 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    406273 |  504 | `	int isNeg = FALSE, nDigit;` |
|    406273 |  505 | `	if( zIn >= zEnd ){` |
|       ! 0 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    406273 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    406269 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    406269 |  516 | `	zDigit = zIn;` |
|    203566 |  517 | `	for(;;){` |
|    407137 |  518 | `		if( zIn >= zEnd ){` |
|       251 |  519 | `			break;` |
|         - |  520 | `		}` |
|    406887 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    406019 |  523 | `			return FALSE;` |
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
|    203139 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    140836 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    140841 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    140841 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    135979 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast */` |
|       ! 0 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  559 | `		}` |
|    135979 |  560 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Perform a blob lookup */` |
|    135959 |  562 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    135959 |  563 | `			goto result;` |
|         - |  564 | `		}` |
|        10 |  565 | `	}` |
|         - |  566 | `	/* Perform an int lookup */` |
|      4887 |  567 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  568 | `		/* Force an integer cast */` |
|        35 |  569 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  570 | `	}` |
|         - |  571 | `	/* Perform an int lookup */` |
|      4887 |  572 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     70418 |  573 | `result:` |
|    140841 |  574 | `	if( rc == SXRET_OK ){` |
|         - |  575 | `		/* Node found */` |
|     62257 |  576 | `		if( ppNode ){` |
|     62207 |  577 | `			*ppNode = pNode;` |
|     31101 |  578 | `		}` |
|     62257 |  579 | `		return SXRET_OK;` |
|         - |  580 | `	}` |
|         - |  581 | `	/* No such entry */` |
|     78589 |  582 | `	return SXERR_NOTFOUND;` |
|     70423 |  583 | `}` |
|         - |  584 | `/*` |
|         - |  585 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  586 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  587 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  588 | ` * via HashmapAppendIndexBusy.` |
|         - |  589 | ` */` |
|   2141468 |  590 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  591 | `{` |
|   2141473 |  592 | `	if( !pMap->bIntKeySeen ){` |
|         - |  593 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       767 |  594 | `		pMap->bIntKeySeen = 1;` |
|       767 |  595 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       767 |  596 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  597 | `			pMap->iNextIdx++;` |
|       ! 0 |  598 | `		}` |
|       767 |  599 | `		return;` |
|         - |  600 | `	}` |
|   2140711 |  601 | `	if( iKey >= pMap->iNextIdx ){` |
|   2140465 |  602 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  603 | `		/* Make sure the automatic index is not reserved */` |
|   2140465 |  604 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  605 | `			pMap->iNextIdx++;` |
|       ! 0 |  606 | `		}` |
|   1070230 |  607 | `	}` |
|   1070739 |  608 | `}` |
|         - |  609 | `/*` |
|         - |  610 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  611 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  612 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  613 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  614 | ` */` |
|   1021960 |  615 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  616 | `{` |
|   1021965 |  617 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  618 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  619 | `		return TRUE;` |
|         - |  620 | `	}` |
|   1021959 |  621 | `	return FALSE;` |
|    510985 |  622 | `}` |
|         - |  623 | `/*` |
|         - |  624 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  625 | ` * hashmap.` |
|         - |  626 | ` * If a node with the given key already exists in the database` |
|         - |  627 | ` * then this function overwrite the old value.` |
|         - |  628 | ` */` |
|   3387848 |  629 | `static sxi32 HashmapInsert(` |
|         - |  630 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  631 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  632 | `	ph7_value *pVal    /* Node value */` |
|         - |  633 | `	)` |
|         5 |  634 | `{` |
|   3387853 |  635 | `	ph7_hashmap_node *pNode = 0;` |
|   3387853 |  636 | `	sxi32 rc = SXRET_OK;` |
|   3387853 |  637 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    227859 |  638 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  639 | `			/* Force a string cast */` |
|         3 |  640 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  641 | `		}` |
|    227859 |  642 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3585 |  643 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  644 | `				/* Automatic index assign */` |
|      3357 |  645 | `				pKey = 0;` |
|      1676 |  646 | `			}` |
|      3585 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|    336416 |  649 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    112137 |  650 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  651 | `				/* Overwrite the old value */` |
|         - |  652 | `				ph7_value *pElem;` |
|       481 |  653 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       481 |  654 | `				if( pElem ){` |
|       481 |  655 | `					if( pVal ){` |
|       481 |  656 | `						PH7_MemObjStore(pVal,pElem);` |
|       243 |  657 | `					}else{` |
|         - |  658 | `						/* Nullify the entry */` |
|       ! 0 |  659 | `						PH7_MemObjToNull(pElem);` |
|         - |  660 | `					}` |
|       238 |  661 | `				}` |
|       481 |  662 | `				return SXRET_OK;` |
|         - |  663 | `		}` |
|    223803 |  664 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  665 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  666 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       131 |  667 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  668 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  669 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  670 | `				return SXRET_OK;` |
|         - |  671 | `			}` |
|       196 |  672 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       130 |  673 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        65 |  674 | `				pVal,SXU32_HIGH);` |
|         - |  675 | `		}` |
|         - |  676 | `		/* Perform a blob-key insertion */` |
|    223673 |  677 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    223673 |  678 | `		return rc;` |
|         - |  679 | `	}` |
|   1579997 |  680 | `IntKey:` |
|   3163579 |  681 | `	if( pKey ){` |
|   2141649 |  682 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  683 | `			/* Force an integer cast */` |
|       261 |  684 | `			PH7_MemObjToInteger(pKey);` |
|       130 |  685 | `		}` |
|   2141649 |  686 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  687 | `			/* Overwrite the old value */` |
|         - |  688 | `			ph7_value *pElem;` |
|       181 |  689 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       181 |  690 | `			if( pElem ){` |
|       181 |  691 | `				if( pVal ){` |
|       181 |  692 | `					PH7_MemObjStore(pVal,pElem);` |
|        91 |  693 | `				}else{` |
|         - |  694 | `					/* Nullify the entry */` |
|       ! 0 |  695 | `					PH7_MemObjToNull(pElem);` |
|         - |  696 | `				}` |
|        90 |  697 | `			}` |
|       181 |  698 | `			return SXRET_OK;` |
|         - |  699 | `		}` |
|   2141469 |  700 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  701 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  702 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  703 | `			char zKey[24];` |
|         3 |  704 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  705 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  706 | `		}` |
|         - |  707 | `		/* Perform a 64-bit-int-key insertion */` |
|   2141467 |  708 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2141467 |  709 | `		if( rc == SXRET_OK ){` |
|   2141467 |  710 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070731 |  711 | `		}` |
|   1070736 |  712 | `	}else{` |
|   1021935 |  713 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  714 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  715 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  716 | `		}` |
|   1021933 |  717 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  718 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  719 | `		}` |
|         - |  720 | `		/* Assign an automatic index */` |
|   1021927 |  721 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1021927 |  722 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1021925 |  723 | `			++pMap->iNextIdx;` |
|    510960 |  724 | `		}` |
|         - |  725 | `	}` |
|         - |  726 | `	/* Insertion result */` |
|   3163389 |  727 | `	return rc;` |
|   1693929 |  728 | `}` |
|         - |  729 | `/*` |
|         - |  730 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  731 | ` * hashmap.` |
|         - |  732 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  733 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  734 | ` * The insertion by reference is triggered when the following` |
|         - |  735 | ` * expression is encountered.` |
|         - |  736 | ` * $var = 10;` |
|         - |  737 | ` *  $a = array(&var);` |
|         - |  738 | ` * OR` |
|         - |  739 | ` *  $a[] =& $var;` |
|         - |  740 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  741 | ` * over it's contents.` |
|         - |  742 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  743 | ` * removed when the foreign ph7_value is unset.` |
|         - |  744 | ` * Example:` |
|         - |  745 | ` *  $var = 10;` |
|         - |  746 | ` *  $a[] =& $var;` |
|         - |  747 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  748 | ` *  //Unset the foreign ph7_value now` |
|         - |  749 | ` *  unset($var);` |
|         - |  750 | ` *  echo count($a); //0` |
|         - |  751 | ` * Note that this is a PH7 eXtension.` |
|         - |  752 | ` * Refer to the official documentation for more information.` |
|         - |  753 | ` * If a node with the given key already exists in the database` |
|         - |  754 | ` * then this function overwrite the old value.` |
|         - |  755 | ` */` |
|     45834 |  756 | `static sxi32 HashmapInsertByRef(` |
|         - |  757 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  758 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  759 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  760 | `	)` |
|         5 |  761 | `{` |
|     45839 |  762 | `	ph7_hashmap_node *pNode = 0;` |
|     45839 |  763 | `	sxi32 rc = SXRET_OK;` |
|     45839 |  764 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     45803 |  765 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  766 | `			/* Force a string cast */` |
|       ! 0 |  767 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  768 | `		}` |
|     45803 |  769 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  770 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  771 | `				/* Automatic index assign */` |
|       ! 0 |  772 | `				pKey = 0;` |
|       ! 0 |  773 | `			}` |
|         3 |  774 | `			goto IntKey;` |
|         - |  775 | `		}` |
|     68699 |  776 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     22898 |  777 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  778 | `				/* Overwrite */` |
|        11 |  779 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  780 | `				pNode->nValIdx = nRefIdx;` |
|         - |  781 | `				/* Install in the reference table */` |
|        11 |  782 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  783 | `				return SXRET_OK;` |
|         - |  784 | `		}` |
|         - |  785 | `		/* Perform a blob-key insertion */` |
|     45791 |  786 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     45791 |  787 | `		return rc;` |
|         - |  788 | `	}` |
|        18 |  789 | `IntKey:` |
|        39 |  790 | `	if( pKey ){` |
|         7 |  791 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  792 | `			/* Force an integer cast */` |
|         3 |  793 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  794 | `		}` |
|         7 |  795 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  796 | `			/* Overwrite */` |
|       ! 0 |  797 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  798 | `			pNode->nValIdx = nRefIdx;` |
|         - |  799 | `			/* Install in the reference table */` |
|       ! 0 |  800 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  801 | `			return SXRET_OK;` |
|         - |  802 | `		}` |
|         - |  803 | `		/* Perform a 64-bit-int-key insertion */` |
|         7 |  804 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|         7 |  805 | `		if( rc == SXRET_OK ){` |
|         7 |  806 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         3 |  807 | `		}` |
|         4 |  808 | `	}else{` |
|        33 |  809 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  810 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  811 | `		}` |
|         - |  812 | `		/* Assign an automatic index */` |
|        33 |  813 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        33 |  814 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        33 |  815 | `			++pMap->iNextIdx;` |
|        16 |  816 | `		}` |
|         - |  817 | `	}` |
|         - |  818 | `	/* Insertion result */` |
|        39 |  819 | `	return rc;` |
|     22922 |  820 | `}` |
|         - |  821 | `/*` |
|         - |  822 | ` * Extract node value.` |
|         - |  823 | ` */` |
|   1445697 |  824 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  825 | `{` |
|         - |  826 | `	/* Point to the desired object */` |
|         - |  827 | `	ph7_value *pObj;` |
|   1445702 |  828 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1445702 |  829 | `	return pObj;` |
|         5 |  830 | `}` |
|         - |  831 | `/*` |
|         - |  832 | ` * Insert a node in the given hashmap.` |
|         - |  833 | ` * If a node with the given key already exists in the database` |
|         - |  834 | ` * then this function overwrite the old value.` |
|         - |  835 | ` */` |
|       460 |  836 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  837 | `{` |
|         - |  838 | `	ph7_value *pObj;` |
|         - |  839 | `	sxi32 rc;` |
|         - |  840 | `	/* Extract the node value */` |
|       465 |  841 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       465 |  842 | `	if( pObj == 0 ){` |
|       ! 0 |  843 | `		return SXERR_EMPTY;` |
|         - |  844 | `	}` |
|         - |  845 | `	/* Preserve key */` |
|       465 |  846 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  847 | `		/* Int64 key */` |
|       333 |  848 | `		if( !bPreserve ){` |
|         - |  849 | `			/* Assign an automatic index */` |
|       185 |  850 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        95 |  851 | `		}else{` |
|       149 |  852 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  853 | `		}` |
|       169 |  854 | `	}else{` |
|         - |  855 | `		/* Blob key */` |
|       133 |  856 | `		if( !bPreserve ){` |
|         - |  857 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  858 | `			 * original string key entirely */` |
|        35 |  859 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  860 | `		}else{` |
|       148 |  861 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        49 |  862 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  863 | `		}` |
|         - |  864 | `	}` |
|       465 |  865 | `	return rc;` |
|       235 |  866 | `}` |
|         - |  867 | `/*` |
|         - |  868 | ` * Compare two node values.` |
|         - |  869 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  870 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  871 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  872 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  873 | ` * documenation.` |
|         - |  874 | ` */` |
|     71770 |  875 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  876 | `{` |
|         - |  877 | `	ph7_value sObj1,sObj2;` |
|         - |  878 | `	sxi32 rc;` |
|     71775 |  879 | `	if( pLeft == pRight ){` |
|         - |  880 | `		/*` |
|         - |  881 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  882 | `		 * below for more information on this sceanario.` |
|         - |  883 | `		 */` |
|       ! 0 |  884 | `		return 0;` |
|         - |  885 | `	}` |
|         - |  886 | `	/* Do the comparison */` |
|     71775 |  887 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     71775 |  888 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     71775 |  889 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     71775 |  890 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     71775 |  891 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     71775 |  892 | `	PH7_MemObjRelease(&sObj1);` |
|     71775 |  893 | `	PH7_MemObjRelease(&sObj2);` |
|     71775 |  894 | `	return rc;` |
|     35863 |  895 | `}` |
|         - |  896 | `/*` |
|         - |  897 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  898 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  899 | ` */` |
|     13992 |  900 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  901 | `{` |
|     13997 |  902 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  903 | `	sxu32 nBucket;` |
|         - |  904 | `	/* Remove old collision links */` |
|     13997 |  905 | `	if( pEntry->pPrevCollide ){` |
|     11392 |  906 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5705 |  907 | `	}else{` |
|      2610 |  908 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  909 | `	}` |
|     13997 |  910 | `	if( pEntry->pNextCollide ){` |
|      1137 |  911 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       581 |  912 | `	}` |
|     13997 |  913 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  914 | `	/* Compute the new hash */` |
|     13997 |  915 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13997 |  916 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13997 |  917 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  918 | `	/* Link to the new bucket */` |
|     13997 |  919 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13997 |  920 | `	if( pMap->apBucket[nBucket] ){` |
|     11717 |  921 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5869 |  922 | `	}` |
|     13997 |  923 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13997 |  924 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  925 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  926 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  927 | `	 * the no-overflow invariant uniform). */` |
|     13997 |  928 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13997 |  929 | `		pMap->iNextIdx++;` |
|      6996 |  930 | `	}` |
|     13997 |  931 | `}` |
|         - |  932 | `/*` |
|         - |  933 | ` * Perform a linear search on a given hashmap.` |
|         - |  934 | ` * Write a pointer to the target node on success.` |
|         - |  935 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  936 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  937 | ` * for more information.` |
|         - |  938 | ` */` |
|     33050 |  939 | `static int HashmapFindValue(` |
|         - |  940 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  941 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  942 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  943 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  944 | `	)` |
|         5 |  945 | `{` |
|         - |  946 | `	ph7_hashmap_node *pEntry;` |
|         - |  947 | `	ph7_value sVal,*pVal;` |
|         - |  948 | `	ph7_value sNeedle;` |
|         - |  949 | `	sxi32 rc;` |
|         - |  950 | `	sxu32 n;` |
|         - |  951 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     33055 |  952 | `	pEntry = pMap->pFirst;` |
|     33055 |  953 | `	n = pMap->nEntry;` |
|     33055 |  954 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     33055 |  955 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     78741 |  956 | `	for(;;){` |
|    157486 |  957 | `		if( n < 1 ){` |
|       115 |  958 | `			break;` |
|         - |  959 | `		}` |
|         - |  960 | `		/* Extract node value */` |
|    157372 |  961 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    157372 |  962 | `		if( pVal ){` |
|         - |  963 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  964 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  965 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  966 | `			 * so null needles/values take the same path as everything else` |
|         - |  967 | `			 * (the historical null-to-null shortcut here made` |
|         - |  968 | `			 * in_array(null, [""]) false where php says true). */` |
|    157372 |  969 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    157372 |  970 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    157372 |  971 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    157372 |  972 | `			PH7_MemObjRelease(&sVal);` |
|    157372 |  973 | `			PH7_MemObjRelease(&sNeedle);` |
|    157372 |  974 | `			if( rc == 0 ){` |
|     32941 |  975 | `				if( ppNode ){` |
|        23 |  976 | `					*ppNode = pEntry;` |
|        11 |  977 | `				}` |
|         - |  978 | `				/* Match found*/` |
|     32941 |  979 | `				return SXRET_OK;` |
|         - |  980 | `			}` |
|     62216 |  981 | `		}` |
|         - |  982 | `		/* Point to the next entry */` |
|    124436 |  983 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    124436 |  984 | `		n--;` |
|         5 |  985 | `	}` |
|         - |  986 | `	/* No such entry */` |
|       115 |  987 | `	return SXERR_NOTFOUND;` |
|     16530 |  988 | `}` |
|         - |  989 | `/*` |
|         - |  990 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - |  991 | ` * for values comparison.` |
|         - |  992 | ` * Write a pointer to the target node on success.` |
|         - |  993 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  994 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - |  995 | ` * for more information.` |
|         - |  996 | ` */` |
|        22 |  997 | `static int HashmapFindValueByCallback(` |
|         - |  998 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - |  999 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - | 1000 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - | 1001 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - | 1002 | `	)` |
|         1 | 1003 | `{` |
|         - | 1004 | `	ph7_hashmap_node *pEntry;` |
|         - | 1005 | `	ph7_value sResult,*pVal;` |
|         - | 1006 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - | 1007 | `	sxi32 rc;` |
|         - | 1008 | `	sxu32 n;` |
|        23 | 1009 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - | 1010 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - | 1011 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 | 1012 | `		return SXERR_NOTFOUND;` |
|         - | 1013 | `	}` |
|         - | 1014 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        23 | 1015 | `	pEntry = pMap->pFirst;` |
|        23 | 1016 | `	n = pMap->nEntry;` |
|         - | 1017 | `	/* Store callback result here */` |
|        23 | 1018 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - | 1019 | `	/* First argument to the callback */` |
|        23 | 1020 | `	apArg[0] = pNeedle;` |
|        25 | 1021 | `	for(;;){` |
|        51 | 1022 | `		if( n < 1 ){` |
|         9 | 1023 | `			break;` |
|         - | 1024 | `		}` |
|         - | 1025 | `		/* Extract node value */` |
|        43 | 1026 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        43 | 1027 | `		if( pVal ){` |
|         - | 1028 | `			/* Invoke the user callback */` |
|        43 | 1029 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        43 | 1030 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        43 | 1031 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 1032 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1033 | `				 * and report no match for the rest of the run. */` |
|         5 | 1034 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1035 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1036 | `				return SXERR_NOTFOUND;` |
|         - | 1037 | `			}` |
|        39 | 1038 | `			if( rc == SXRET_OK ){` |
|         - | 1039 | `				/* Extract callback result */` |
|        39 | 1040 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1041 | `					/* Perform an int cast */` |
|       ! 0 | 1042 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1043 | `				}` |
|        39 | 1044 | `				rc = (sxi32)sResult.x.iVal;` |
|        39 | 1045 | `				PH7_MemObjRelease(&sResult);` |
|        39 | 1046 | `				if( rc == 0 ){` |
|         - | 1047 | `					/* Match found*/` |
|        11 | 1048 | `					if( ppNode ){` |
|       ! 0 | 1049 | `						*ppNode = pEntry;` |
|       ! 0 | 1050 | `					}` |
|        11 | 1051 | `					return SXRET_OK;` |
|         - | 1052 | `				}` |
|        14 | 1053 | `			}` |
|        14 | 1054 | `		}` |
|         - | 1055 | `		/* Point to the next entry */` |
|        29 | 1056 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 1057 | `		n--;` |
|         1 | 1058 | `	}` |
|         - | 1059 | `	/* No such entry */` |
|         9 | 1060 | `	return SXERR_NOTFOUND;` |
|        12 | 1061 | `}` |
|         - | 1062 | `/*` |
|         - | 1063 | ` * Compare two hashmaps.` |
|         - | 1064 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1065 | ` * Note on array comparison operators.` |
|         - | 1066 | ` *  According to the PHP language reference manual.` |
|         - | 1067 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1068 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1069 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1070 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1071 | ` *                          order and of the same types.` |
|         - | 1072 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1073 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1074 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1075 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1076 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1077 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1078 | ` * <?php` |
|         - | 1079 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1080 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1081 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1082 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1083 | ` * var_dump($c);` |
|         - | 1084 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1085 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1086 | ` * var_dump($c);` |
|         - | 1087 | ` * ?>` |
|         - | 1088 | ` * When executed, this script will print the following:` |
|         - | 1089 | ` * Union of $a and $b:` |
|         - | 1090 | ` * array(3) {` |
|         - | 1091 | ` *  ["a"]=>` |
|         - | 1092 | ` *  string(5) "apple"` |
|         - | 1093 | ` *  ["b"]=>` |
|         - | 1094 | ` * string(6) "banana"` |
|         - | 1095 | ` *  ["c"]=>` |
|         - | 1096 | ` * string(6) "cherry"` |
|         - | 1097 | ` * }` |
|         - | 1098 | ` * Union of $b and $a:` |
|         - | 1099 | ` * array(3) {` |
|         - | 1100 | ` * ["a"]=>` |
|         - | 1101 | ` * string(4) "pear"` |
|         - | 1102 | ` * ["b"]=>` |
|         - | 1103 | ` * string(10) "strawberry"` |
|         - | 1104 | ` * ["c"]=>` |
|         - | 1105 | ` * string(6) "cherry"` |
|         - | 1106 | ` * }` |
|         - | 1107 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1108 | ` */` |
|        30 | 1109 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1110 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1111 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1112 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1113 | `	)` |
|         1 | 1114 | `{` |
|         - | 1115 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1116 | `	sxi32 rc;` |
|         - | 1117 | `	sxu32 n;` |
|        31 | 1118 | `	if( pLeft == pRight ){` |
|         - | 1119 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1120 | `		 * Unlike the zend engine.` |
|         - | 1121 | `		 */` |
|         3 | 1122 | `		return 0;` |
|         - | 1123 | `	}` |
|        29 | 1124 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1125 | `		/* Must have the same number of entries */` |
|         5 | 1126 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1127 | `	}` |
|         - | 1128 | `	/* Point to the first inserted entry of the left hashmap */` |
|        25 | 1129 | `	pLe = pLeft->pFirst;` |
|        25 | 1130 | `	pRe = 0; /* cc warning */` |
|         - | 1131 | `	/* Perform the comparison */` |
|        25 | 1132 | `	n = pLeft->nEntry;` |
|        59 | 1133 | `	for(;;){` |
|       119 | 1134 | `		if( n < 1 ){` |
|        23 | 1135 | `			break;` |
|         - | 1136 | `		}` |
|        97 | 1137 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1138 | `			/* Int key */` |
|        89 | 1139 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        45 | 1140 | `		}else{` |
|         9 | 1141 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1142 | `			/* Blob key */` |
|         9 | 1143 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1144 | `		}` |
|        97 | 1145 | `		if( rc != SXRET_OK ){` |
|         - | 1146 | `			/* No such entry in the right side */` |
|       ! 0 | 1147 | `			return 1;` |
|         - | 1148 | `		}` |
|        97 | 1149 | `		rc = 0;` |
|        97 | 1150 | `		if( bStrict ){` |
|         - | 1151 | `			/* Make sure,the keys are of the same type */` |
|        81 | 1152 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1153 | `				rc = 1;` |
|       ! 0 | 1154 | `			}` |
|        40 | 1155 | `		}` |
|        97 | 1156 | `		if( !rc ){` |
|         - | 1157 | `			/* Compare nodes */` |
|        97 | 1158 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        48 | 1159 | `		}` |
|        97 | 1160 | `		if( rc != 0 ){` |
|         - | 1161 | `			/* Nodes key/value differ */` |
|         3 | 1162 | `			return rc;` |
|         - | 1163 | `		}` |
|         - | 1164 | `		/* Point to the next entry */` |
|        95 | 1165 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        95 | 1166 | `		n--;` |
|         1 | 1167 | `	}` |
|        23 | 1168 | `	return 0; /* Hashmaps are equals */` |
|        16 | 1169 | `}` |
|         - | 1170 | `/*` |
|         - | 1171 | ` * Duplicate a hashmap node.` |
|         - | 1172 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1173 | ` */` |
|    664144 | 1174 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1175 | `	ph7_hashmap *pDest,` |
|         - | 1176 | `	ph7_hashmap_node *pEntry,` |
|         - | 1177 | `	ph7_value *pVal,` |
|         - | 1178 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1179 | `	)` |
|         5 | 1180 | `{` |
|         - | 1181 | `	ph7_value sSafeVal;` |
|         - | 1182 | `	ph7_value sKey;` |
|         - | 1183 | `	sxi32 rc;` |
|         - | 1184 |  |
|    664149 | 1185 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 1186 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|         - | 1187 | `		 * Re-insert it by reference so the reference survives the duplication` |
|         - | 1188 | `		 * instead of being flattened to a value copy. This keeps spread` |
|         - | 1189 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|         - | 1190 | `		 * with PHP semantics. */` |
|         7 | 1191 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         7 | 1192 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1193 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1194 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1195 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1196 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1197 | `		}else{` |
|         5 | 1198 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         5 | 1199 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         2 | 1200 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1201 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1202 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1203 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1204 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1205 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1206 | `			}` |
|         - | 1207 | `		}` |
|         7 | 1208 | `		return rc;` |
|         - | 1209 | `	}` |
|    664143 | 1210 | `	sSafeVal = *pVal;` |
|         - | 1211 |  |
|    664143 | 1212 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1213 | `		/* Blob key insertion */` |
|      3967 | 1214 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      3967 | 1215 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      3967 | 1216 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      3967 | 1217 | `		PH7_MemObjRelease(&sKey);` |
|      1986 | 1218 | `	}else{` |
|         - | 1219 | `		/* Int key */` |
|    660181 | 1220 | `		if( iAction == 0 ){ /* Merge */` |
|    659939 | 1221 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    330212 | 1222 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1223 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1224 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1225 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1226 | `		}else{ /* Dup */` |
|       215 | 1227 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1228 | `		}` |
|         - | 1229 | `	}` |
|    664143 | 1230 | `	return rc;` |
|    332077 | 1231 | `}` |
|         - | 1232 | `/*` |
|         - | 1233 | ` * Merge two hashmaps.` |
|         - | 1234 | ` * Note on the merge process` |
|         - | 1235 | ` * According to the PHP language reference manual.` |
|         - | 1236 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1237 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1238 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1239 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1240 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1241 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1242 | ` *  keys starting from zero in the result array.` |
|         - | 1243 | ` */` |
|      2780 | 1244 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1245 | `{` |
|         - | 1246 | `	ph7_hashmap_node *pEntry;` |
|         - | 1247 | `	ph7_value *pVal;` |
|         - | 1248 | `	sxi32 rc;` |
|         - | 1249 | `	sxu32 n;` |
|      2785 | 1250 | `	if( pSrc == pDest ){` |
|         - | 1251 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1252 | `		 * Unlike the zend engine.` |
|         - | 1253 | `		 */` |
|       ! 0 | 1254 | `		return SXRET_OK;` |
|         - | 1255 | `	}` |
|         - | 1256 | `	/* Point to the first inserted entry in the source */` |
|      2785 | 1257 | `	pEntry = pSrc->pFirst;` |
|         - | 1258 | `	/* Perform the merge */` |
|    662777 | 1259 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1260 | `		/* Extract the node value */` |
|    659997 | 1261 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    659997 | 1262 | `		if( pVal ){` |
|         - | 1263 | `			/* Make a local copy of the value.` |
|         - | 1264 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1265 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1266 | `			 * to the old pool.` |
|         - | 1267 | `			 */` |
|    659997 | 1268 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    330001 | 1269 | `		}else{` |
|       ! 0 | 1270 | `			rc = SXRET_OK;` |
|         - | 1271 | `		}` |
|    659997 | 1272 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1273 | `			return rc;` |
|         - | 1274 | `		}` |
|         - | 1275 | `		/* Point to the next entry */` |
|    659997 | 1276 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    330001 | 1277 | `	}` |
|      2785 | 1278 | `	return SXRET_OK;` |
|      1395 | 1279 | `}` |
|         - | 1280 | `/*` |
|         - | 1281 | ` * Overwrite entries with the same key.` |
|         - | 1282 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1283 | ` *  According to the PHP language reference manual.` |
|         - | 1284 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1285 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1286 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1287 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1288 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1289 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1290 | ` *  overwriting the previous values.` |
|         - | 1291 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1292 | ` *  by whatever type is in the second array.` |
|         - | 1293 | ` */` |
|        34 | 1294 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1295 | `{` |
|         - | 1296 | `	ph7_hashmap_node *pEntry;` |
|         - | 1297 | `	ph7_value *pVal;` |
|         - | 1298 | `	sxi32 rc;` |
|         - | 1299 | `	sxu32 n;` |
|        36 | 1300 | `	if( pSrc == pDest ){` |
|         - | 1301 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1302 | `		 * Unlike the zend engine.` |
|         - | 1303 | `		 */` |
|       ! 0 | 1304 | `		return SXRET_OK;` |
|         - | 1305 | `	}` |
|         - | 1306 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1307 | `	pEntry = pSrc->pFirst;` |
|         - | 1308 | `	/* Perform the merge */` |
|        80 | 1309 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1310 | `		/* Extract the node value */` |
|        46 | 1311 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1312 | `		if( pVal ){` |
|        46 | 1313 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1314 | `		}else{` |
|       ! 0 | 1315 | `			rc = SXRET_OK;` |
|         - | 1316 | `		}` |
|        46 | 1317 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1318 | `			return rc;` |
|         - | 1319 | `		}` |
|         - | 1320 | `		/* Point to the next entry */` |
|        46 | 1321 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1322 | `	}` |
|        36 | 1323 | `	return SXRET_OK;` |
|        19 | 1324 | `}` |
|         - | 1325 | `/*` |
|         - | 1326 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1327 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1328 | ` */` |
|      3868 | 1329 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1330 | `{` |
|         - | 1331 | `	ph7_hashmap_node *pEntry;` |
|         - | 1332 | `	ph7_value *pVal;` |
|         - | 1333 | `	sxi32 rc;` |
|         - | 1334 | `	sxu32 n;` |
|      3873 | 1335 | `	if( pSrc == pDest ){` |
|         - | 1336 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1337 | `		 * Unlike the zend engine.` |
|         - | 1338 | `		 */` |
|       ! 0 | 1339 | `		return SXRET_OK;` |
|         - | 1340 | `	}` |
|         - | 1341 | `	/* Point to the first inserted entry in the source */` |
|      3873 | 1342 | `	pEntry = pSrc->pFirst;` |
|         - | 1343 | `	/* Perform the duplication */` |
|      7981 | 1344 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1345 | `		/* Extract the node value */` |
|      4113 | 1346 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4113 | 1347 | `		if( pVal ){` |
|      4113 | 1348 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2059 | 1349 | `		}else{` |
|       ! 0 | 1350 | `			rc = SXRET_OK;` |
|         - | 1351 | `		}` |
|      4113 | 1352 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1353 | `			return rc;` |
|         - | 1354 | `		}` |
|         - | 1355 | `		/* Point to the next entry */` |
|      4113 | 1356 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2059 | 1357 | `	}` |
|      3873 | 1358 | `	return SXRET_OK;` |
|      1939 | 1359 | `}` |
|         - | 1360 | `/*` |
|         - | 1361 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1362 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1363 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1364 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1365 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1366 | ` * this instead of PH7_HashmapDup.` |
|         - | 1367 | ` */` |
|        12 | 1368 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1369 | `{` |
|         - | 1370 | `	ph7_hashmap_node *pEntry;` |
|         - | 1371 | `	ph7_value *pVal;` |
|         - | 1372 | `	sxi32 rc;` |
|         - | 1373 | `	sxu32 n;` |
|        13 | 1374 | `	if( pSrc == pDest ){` |
|       ! 0 | 1375 | `		return SXRET_OK;` |
|         - | 1376 | `	}` |
|        13 | 1377 | `	pEntry = pSrc->pFirst;` |
|       749 | 1378 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1379 | `		/* Extract the node value (resolves foreign references) */` |
|       737 | 1380 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       736 | 1381 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       496 | 1382 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1383 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1384 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1385 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1386 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1387 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1388 | `			pVal = 0;` |
|         2 | 1389 | `		}` |
|       737 | 1390 | `		if( pVal ){` |
|       733 | 1391 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1093 | 1392 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       364 | 1393 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       365 | 1394 | `			}else{` |
|         5 | 1395 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1396 | `			}` |
|       733 | 1397 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1398 | `				return rc;` |
|         - | 1399 | `			}` |
|       366 | 1400 | `		}` |
|         - | 1401 | `		/* Point to the next entry */` |
|       737 | 1402 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       369 | 1403 | `	}` |
|        13 | 1404 | `	return SXRET_OK;` |
|         7 | 1405 | `}` |
|         - | 1406 | `/*` |
|         - | 1407 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1408 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1409 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1410 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1411 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1412 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1413 | ` * iterate-a-snapshot semantic.` |
|         - | 1414 | ` */` |
|        50 | 1415 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1416 | `{` |
|         - | 1417 | `	ph7_foreach_step *pStep;` |
|        53 | 1418 | `	sxi32 nRef = 0;` |
|       103 | 1419 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        53 | 1420 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1421 | `			nRef++;` |
|        21 | 1422 | `		}` |
|        28 | 1423 | `	}` |
|        53 | 1424 | `	return nRef;` |
|         3 | 1425 | `}` |
|         - | 1426 | `/*` |
|         - | 1427 | ` * Copy-on-write separation for arrays.` |
|         - | 1428 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1429 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1430 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1431 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1432 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1433 | ` * on the live map the loop is walking, like php.` |
|         - | 1434 | ` */` |
|    233408 | 1435 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1436 | `{` |
|    233413 | 1437 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1438 | `	ph7_hashmap *pNew;` |
|         - | 1439 | `	ph7_value *pBacking;` |
|         - | 1440 | `	sxu32 nValIdx;` |
|         - | 1441 | `	int bValueInPool;` |
|    233413 | 1442 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    233413 | 1443 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1444 | `		/* Sole owner, no separation needed */` |
|    230731 | 1445 | `		return pMap;` |
|         - | 1446 | `	}` |
|      2687 | 1447 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1448 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1449 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1450 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1451 | `		return pMap;` |
|         - | 1452 | `	}` |
|         - | 1453 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1454 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1455 | `	 * frame is popped. */` |
|      2561 | 1456 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2561 | 1457 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2556 | 1458 | `		if( pBacking && pBacking != pValue` |
|      2532 | 1459 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2513 | 1460 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1461 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2513 | 1462 | `			pMap->iRef--;` |
|      2513 | 1463 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1464 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2467 | 1465 | `				pMap->iRef++;` |
|      2467 | 1466 | `				return pMap;` |
|         - | 1467 | `			}` |
|        48 | 1468 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        48 | 1469 | `			if( pNew == 0 ){` |
|       ! 0 | 1470 | `				pMap->iRef++;` |
|       ! 0 | 1471 | `				return pMap;` |
|         - | 1472 | `			}` |
|        48 | 1473 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1474 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1475 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1476 | `				pMap->iRef++;` |
|       ! 0 | 1477 | `				return pMap;` |
|         - | 1478 | `			}` |
|        48 | 1479 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        48 | 1480 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1481 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1482 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1483 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1484 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1485 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1486 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1487 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        48 | 1488 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        48 | 1489 | `			if( pBacking ){` |
|        48 | 1490 | `				pBacking->x.pOther = pNew;` |
|        23 | 1491 | `			}` |
|         - | 1492 | `			/* Update the stack value to match */` |
|        48 | 1493 | `			pValue->x.pOther = pNew;` |
|        48 | 1494 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        48 | 1495 | `			return pNew;` |
|         - | 1496 | `		}` |
|        24 | 1497 | `	}` |
|         - | 1498 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1499 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1500 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1501 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1502 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1503 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1504 | `	 * pBacking in the backing-variable branch above). */` |
|        50 | 1505 | `	nValIdx = pValue->nIdx;` |
|        74 | 1506 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        48 | 1507 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        50 | 1508 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        50 | 1509 | `	if( pNew == 0 ){` |
|         - | 1510 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1511 | `		return pMap;` |
|         - | 1512 | `	}` |
|        50 | 1513 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1514 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1515 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1516 | `		return pMap;` |
|         - | 1517 | `	}` |
|        50 | 1518 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        50 | 1519 | `	pMap->iRef--;` |
|        50 | 1520 | `	if( bValueInPool ){` |
|         - | 1521 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        50 | 1522 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        50 | 1523 | `		if( pValue == 0 ){` |
|       ! 0 | 1524 | `			return pNew;` |
|         - | 1525 | `		}` |
|        24 | 1526 | `	}` |
|        50 | 1527 | `	pValue->x.pOther = pNew;` |
|        50 | 1528 | `	return pNew;` |
|    116709 | 1529 | `}` |
|         - | 1530 | `/*` |
|         - | 1531 | ` * Perform the union of two hashmaps.` |
|         - | 1532 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1533 | ` * with a variable holding an array as follows:` |
|         - | 1534 | ` * <?php` |
|         - | 1535 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1536 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1537 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1538 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1539 | ` * var_dump($c);` |
|         - | 1540 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1541 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1542 | ` * var_dump($c);` |
|         - | 1543 | ` * ?>` |
|         - | 1544 | ` * When executed, this script will print the following:` |
|         - | 1545 | ` * Union of $a and $b:` |
|         - | 1546 | ` * array(3) {` |
|         - | 1547 | ` *  ["a"]=>` |
|         - | 1548 | ` *  string(5) "apple"` |
|         - | 1549 | ` *  ["b"]=>` |
|         - | 1550 | ` * string(6) "banana"` |
|         - | 1551 | ` *  ["c"]=>` |
|         - | 1552 | ` * string(6) "cherry"` |
|         - | 1553 | ` * }` |
|         - | 1554 | ` * Union of $b and $a:` |
|         - | 1555 | ` * array(3) {` |
|         - | 1556 | ` * ["a"]=>` |
|         - | 1557 | ` * string(4) "pear"` |
|         - | 1558 | ` * ["b"]=>` |
|         - | 1559 | ` * string(10) "strawberry"` |
|         - | 1560 | ` * ["c"]=>` |
|         - | 1561 | ` * string(6) "cherry"` |
|         - | 1562 | ` * }` |
|         - | 1563 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1564 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1565 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1566 | ` */` |
|      3746 | 1567 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1568 | `{` |
|         - | 1569 | `	ph7_hashmap_node *pEntry;` |
|      3751 | 1570 | `	sxi32 rc = SXRET_OK;` |
|         - | 1571 | `	ph7_value *pObj;` |
|         - | 1572 | `	sxu32 n;` |
|      3751 | 1573 | `	if( pLeft == pRight ){` |
|         - | 1574 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1575 | `		 * Unlike the zend engine.` |
|         - | 1576 | `		 */` |
|       ! 0 | 1577 | `		return SXRET_OK;` |
|         - | 1578 | `	}` |
|         - | 1579 | `	/* Perform the union */` |
|      3751 | 1580 | `	pEntry = pRight->pFirst;` |
|      3785 | 1581 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1582 | `		/* Make sure the given key does not exists in the left array */` |
|        38 | 1583 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1584 | `			/* BLOB key */` |
|        24 | 1585 | `			if( SXRET_OK !=` |
|        20 | 1586 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1587 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1588 | `					if( pObj ){` |
|        20 | 1589 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1590 | `						/* Perform the insertion */` |
|        20 | 1591 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1592 | `							&sSafeVal,0,FALSE);` |
|        20 | 1593 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1594 | `							return rc;` |
|         - | 1595 | `						}` |
|         8 | 1596 | `					}` |
|         8 | 1597 | `			}` |
|        14 | 1598 | `		}else{` |
|         - | 1599 | `			/* INT key */` |
|        16 | 1600 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        11 | 1601 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        11 | 1602 | `				if( pObj ){` |
|        11 | 1603 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1604 | `					/* Perform the insertion */` |
|        11 | 1605 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        11 | 1606 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1607 | `						return rc;` |
|         - | 1608 | `					}` |
|         5 | 1609 | `				}` |
|         5 | 1610 | `			}` |
|         - | 1611 | `		}` |
|         - | 1612 | `		/* Point to the next entry */` |
|        38 | 1613 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 1614 | `	}` |
|      3751 | 1615 | `	return SXRET_OK;` |
|      1878 | 1616 | `}` |
|         - | 1617 | `/*` |
|         - | 1618 | ` * Allocate a new hashmap.` |
|         - | 1619 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1620 | ` */` |
|    143566 | 1621 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1622 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1623 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1624 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1625 | `	)` |
|         5 | 1626 | `{` |
|         - | 1627 | `	ph7_hashmap *pMap;` |
|         - | 1628 | `	/* Allocate a new instance */` |
|    143571 | 1629 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    143571 | 1630 | `	if( pMap == 0 ){` |
|       ! 0 | 1631 | `		return 0;` |
|         - | 1632 | `	}` |
|         - | 1633 | `	/* Zero the structure */` |
|    143571 | 1634 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1635 | `	/* Fill in the structure */` |
|    143571 | 1636 | `	pMap->pVm = &(*pVm);` |
|    143571 | 1637 | `	pMap->iRef = 1;` |
|         - | 1638 | `	/* Default hash functions */` |
|    143571 | 1639 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    143571 | 1640 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    143571 | 1641 | `	return pMap;` |
|     71788 | 1642 | `}` |
|         - | 1643 | `/*` |
|         - | 1644 | ` * Install superglobals in the given virtual machine.` |
|         - | 1645 | ` * Note on superglobals.` |
|         - | 1646 | ` *  According to the PHP language reference manual.` |
|         - | 1647 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1648 | `*   Description` |
|         - | 1649 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1650 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1651 | `*   global $variable; to access them within functions or methods.` |
|         - | 1652 | `*   These superglobal variables are:` |
|         - | 1653 | `*    $GLOBALS` |
|         - | 1654 | `*    $_SERVER` |
|         - | 1655 | `*    $_GET` |
|         - | 1656 | `*    $_POST` |
|         - | 1657 | `*    $_FILES` |
|         - | 1658 | `*    $_COOKIE` |
|         - | 1659 | `*    $_SESSION` |
|         - | 1660 | `*    $_REQUEST` |
|         - | 1661 | `*    $_ENV` |
|         - | 1662 | `*/` |
|      3350 | 1663 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1664 | `{` |
|         - | 1665 | `	static const char * azSuper[] = {` |
|         - | 1666 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1667 | `		"_GET",      /* $_GET */` |
|         - | 1668 | `		"_POST",     /* $_POST */` |
|         - | 1669 | `		"_FILES",    /* $_FILES */` |
|         - | 1670 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1671 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1672 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1673 | `		"_ENV",      /* $_ENV */` |
|         - | 1674 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1675 | `		"argv"       /* $argv */` |
|         - | 1676 | `	};` |
|         - | 1677 | `	ph7_hashmap *pMap;` |
|         - | 1678 | `	ph7_value *pObj;` |
|         - | 1679 | `	SyString *pFile;` |
|         - | 1680 | `	sxi32 rc;` |
|         - | 1681 | `	sxu32 n;` |
|         - | 1682 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3355 | 1683 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3355 | 1684 | `	if( pMap == 0 ){` |
|       ! 0 | 1685 | `		return SXERR_MEM;` |
|         - | 1686 | `	}` |
|      3355 | 1687 | `	pVm->pGlobal = pMap;` |
|         - | 1688 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3355 | 1689 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3355 | 1690 | `	if( pObj == 0 ){` |
|       ! 0 | 1691 | `		return SXERR_MEM;` |
|         - | 1692 | `	}` |
|      3355 | 1693 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1694 | `	/* Record object index */` |
|      3355 | 1695 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1696 | `	/* Install the special $GLOBALS array */` |
|      3355 | 1697 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3355 | 1698 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1699 | `		return rc;` |
|         - | 1700 | `	}` |
|         - | 1701 | `	/* Install superglobals now */` |
|     36855 | 1702 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1703 | `		ph7_value *pSuper;` |
|         - | 1704 | `		/* Request an empty array */` |
|     33505 | 1705 | `		pSuper = ph7_new_array(&(*pVm));` |
|     33505 | 1706 | `		if( pSuper == 0 ){` |
|       ! 0 | 1707 | `			return SXERR_MEM;` |
|         - | 1708 | `		}` |
|         - | 1709 | `		/* Install */` |
|     33505 | 1710 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     33505 | 1711 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1712 | `			return rc;` |
|         - | 1713 | `		}` |
|         - | 1714 | `		/* Release the value now it have been installed */` |
|     33505 | 1715 | `		ph7_release_value(&(*pVm),pSuper);` |
|     16755 | 1716 | `	}` |
|         - | 1717 | `	/* Set some $_SERVER entries */` |
|      3355 | 1718 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1719 | `	/*` |
|         - | 1720 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1721 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1722 | `	 */` |
|      6705 | 1723 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1724 | `		"SCRIPT_FILENAME",` |
|      1675 | 1725 | `		pFile ? pFile->zString : ":Memory:",` |
|      3350 | 1726 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1727 | `		);` |
|         - | 1728 | `	/* All done,all super-global are installed now */` |
|      3355 | 1729 | `	return SXRET_OK;` |
|      1680 | 1730 | `}` |
|         - | 1731 | `/*` |
|         - | 1732 | ` * Release a hashmap.` |
|         - | 1733 | ` */` |
|    101790 | 1734 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1735 | `{` |
|         - | 1736 | `	ph7_hashmap_node *pEntry,*pNext;` |
|    101795 | 1737 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1738 | `	sxu32 n;` |
|    101795 | 1739 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1740 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1741 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1742 | `		return SXRET_OK;` |
|         - | 1743 | `	}` |
|    101795 | 1744 | `	if( pMap->pActiveSteps ){` |
|         - | 1745 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1746 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1747 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1748 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1749 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1750 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1751 | `		ph7_foreach_step *pStep;` |
|        17 | 1752 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1753 | `			pStep->pCursor = 0;` |
|         5 | 1754 | `		}` |
|         4 | 1755 | `	}` |
|         - | 1756 | `	/* Start the release process */` |
|    101795 | 1757 | `	n = 0;` |
|    101795 | 1758 | `	pEntry = pMap->pFirst;` |
|   1725092 | 1759 | `	for(;;){` |
|   3450189 | 1760 | `		if( n >= pMap->nEntry ){` |
|    101795 | 1761 | `			break;` |
|         - | 1762 | `		}` |
|   3348399 | 1763 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1764 | `		/* Remove the reference from the foreign table */` |
|   3348399 | 1765 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3348399 | 1766 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1767 | `			/* Restore the ph7_value to the free list */` |
|   3348369 | 1768 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1674182 | 1769 | `		}` |
|         - | 1770 | `		/* Release the node */` |
|   3348399 | 1771 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    196379 | 1772 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     98187 | 1773 | `		}` |
|   3348399 | 1774 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1775 | `		/* Point to the next entry */` |
|   3348399 | 1776 | `		pEntry = pNext;` |
|   3348399 | 1777 | `		n++;` |
|         5 | 1778 | `	}` |
|    101795 | 1779 | `	if( pMap->nEntry > 0 ){` |
|         - | 1780 | `		/* Release the hash bucket */` |
|     76727 | 1781 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     38361 | 1782 | `	}` |
|    101795 | 1783 | `	if( FreeDS ){` |
|         - | 1784 | `		/* Free the whole instance */` |
|    101769 | 1785 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     50887 | 1786 | `	}else{` |
|         - | 1787 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1788 | `		pMap->apBucket = 0;` |
|        28 | 1789 | `		pMap->iNextIdx = 0;` |
|        28 | 1790 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1791 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1792 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1793 | `	}` |
|    101795 | 1794 | `	return SXRET_OK;` |
|     50900 | 1795 | `}` |
|         - | 1796 | `/*` |
|         - | 1797 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1798 | ` * If the count reaches zero which mean no more variables` |
|         - | 1799 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1800 | ` */` |
|    841122 | 1801 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1802 | `{` |
|    841127 | 1803 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1804 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    841127 | 1805 | `	pMap->iRef--;` |
|    841127 | 1806 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|    101749 | 1807 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     50872 | 1808 | `	}` |
|    841127 | 1809 | `}` |
|         - | 1810 | `/*` |
|         - | 1811 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1812 | ` * Write a pointer to the target node on success.` |
|         - | 1813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1814 | ` */` |
|    141008 | 1815 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1816 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1817 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1818 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1819 | `	)` |
|         5 | 1820 | `{` |
|         - | 1821 | `	sxi32 rc;` |
|    141013 | 1822 | `	if( pMap->nEntry < 1 ){` |
|         - | 1823 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1824 | `		 */` |
|       177 | 1825 | `		return SXERR_NOTFOUND;` |
|         - | 1826 | `	}` |
|    140841 | 1827 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    140841 | 1828 | `	return rc;` |
|     70509 | 1829 | `}` |
|         - | 1830 | `/*` |
|         - | 1831 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1832 | ` * hashmap.` |
|         - | 1833 | ` * If a node with the given key already exists in the database` |
|         - | 1834 | ` * then this function overwrite the old value.` |
|         - | 1835 | ` */` |
|   2727662 | 1836 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1837 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1838 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1839 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1840 | `	)` |
|         5 | 1841 | `{` |
|         - | 1842 | `	sxi32 rc;` |
|         - | 1843 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1844 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1845 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1846 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   2727667 | 1847 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2727667 | 1848 | `	return rc;` |
|         5 | 1849 | `}` |
|         - | 1850 | `/*` |
|         - | 1851 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1852 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1853 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1854 | ` * This is the same routine that backs array_merge().` |
|         - | 1855 | ` */` |
|       654 | 1856 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1857 | `{` |
|       655 | 1858 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1859 | `}` |
|         - | 1860 | `/*` |
|         - | 1861 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1862 | ` * hashmap.` |
|         - | 1863 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1864 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1865 | ` * The insertion by reference is triggered when the following` |
|         - | 1866 | ` * expression is encountered.` |
|         - | 1867 | ` * $var = 10;` |
|         - | 1868 | ` *  $a = array(&var);` |
|         - | 1869 | ` * OR` |
|         - | 1870 | ` *  $a[] =& $var;` |
|         - | 1871 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1872 | ` * over it's contents.` |
|         - | 1873 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1874 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1875 | ` * Example:` |
|         - | 1876 | ` *  $var = 10;` |
|         - | 1877 | ` *  $a[] =& $var;` |
|         - | 1878 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1879 | ` *  //Unset the foreign ph7_value now` |
|         - | 1880 | ` *  unset($var);` |
|         - | 1881 | ` *  echo count($a); //0` |
|         - | 1882 | ` * Note that this is a PH7 eXtension.` |
|         - | 1883 | ` * Refer to the official documentation for more information.` |
|         - | 1884 | ` * If a node with the given key already exists in the database` |
|         - | 1885 | ` * then this function overwrite the old value.` |
|         - | 1886 | ` */` |
|     45828 | 1887 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1888 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1889 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1890 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1891 | `	)` |
|         5 | 1892 | `{` |
|         - | 1893 | `	sxi32 rc;` |
|     45833 | 1894 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1895 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1896 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1897 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1898 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1899 | `		return PH7_ABORT;` |
|         - | 1900 | `	}` |
|     45833 | 1901 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     45833 | 1902 | `	return rc;` |
|     22919 | 1903 | `}` |
|         - | 1904 | `/*` |
|         - | 1905 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1906 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1907 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1908 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1909 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1910 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1911 | ` */` |
|     18862 | 1912 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1913 | `{` |
|     18867 | 1914 | `	pStep->pCursor = pMap->pFirst;` |
|     18867 | 1915 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     18867 | 1916 | `	pMap->pActiveSteps = pStep;` |
|     18867 | 1917 | `}` |
|         - | 1918 | `/*` |
|         - | 1919 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1920 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1921 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1922 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1923 | ` */` |
|     18762 | 1924 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1925 | `{` |
|     18767 | 1926 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     18767 | 1927 | `	while( *ppLink ){` |
|     18767 | 1928 | `		if( *ppLink == pStep ){` |
|     18767 | 1929 | `			*ppLink = pStep->pNextActive;` |
|     18767 | 1930 | `			pStep->pNextActive = 0;` |
|     18767 | 1931 | `			return;` |
|         - | 1932 | `		}` |
|       ! 0 | 1933 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1934 | `	}` |
|      9386 | 1935 | `}` |
|         - | 1936 | `/*` |
|         - | 1937 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1938 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1939 | ` * return NULL.` |
|         - | 1940 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1941 | ` */` |
|        64 | 1942 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 1943 | `{` |
|        65 | 1944 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 1945 | `	if( pCur == 0 ){` |
|         - | 1946 | `		/* End of the list,return null */` |
|        27 | 1947 | `		return 0;` |
|         - | 1948 | `	}` |
|         - | 1949 | `	/* Advance the node cursor */` |
|        39 | 1950 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 1951 | `	return pCur;` |
|        33 | 1952 | `}` |
|         - | 1953 | `/*` |
|         - | 1954 | ` * Extract a node value.` |
|         - | 1955 | ` */` |
|    589918 | 1956 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1957 | `{` |
|    589923 | 1958 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    589923 | 1959 | `	if( pEntry ){` |
|    589923 | 1960 | `		if( bStore ){` |
|    234145 | 1961 | `			PH7_MemObjStore(pEntry,pValue);` |
|    117075 | 1962 | `		}else{` |
|    355783 | 1963 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1964 | `		}` |
|    294910 | 1965 | `	}else{` |
|       ! 0 | 1966 | `		PH7_MemObjRelease(pValue);` |
|         - | 1967 | `	}` |
|    589923 | 1968 | `}` |
|         - | 1969 | `/*` |
|         - | 1970 | ` * Extract a node key.` |
|         - | 1971 | ` */` |
|    155582 | 1972 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1973 | `{` |
|         - | 1974 | `	/* Fill with the current key */` |
|    155587 | 1975 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    150429 | 1976 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 1977 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 1978 | `		}` |
|    150429 | 1979 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    150429 | 1980 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     75217 | 1981 | `	}else{` |
|      5163 | 1982 | `		SyBlobReset(&pKey->sBlob);` |
|      5163 | 1983 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      5163 | 1984 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1985 | `	}` |
|    155587 | 1986 | `}` |
|         - | 1987 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 1988 | `/*` |
|         - | 1989 | ` * Store the address of nodes value in the given container.` |
|         - | 1990 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 1991 | ` * defined in 'builtin.c' for more information.` |
|         - | 1992 | ` */` |
|        12 | 1993 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 1994 | `{` |
|        13 | 1995 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 1996 | `	ph7_value *pValue;` |
|         - | 1997 | `	sxu32 n;` |
|         - | 1998 | `	/* Initialize the container */` |
|        13 | 1999 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 2000 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2001 | `		/* Extract node value */` |
|        21 | 2002 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        21 | 2003 | `		if( pValue ){` |
|        21 | 2004 | `			SySetPut(pOut,(const void *)&pValue);` |
|        10 | 2005 | `		}` |
|         - | 2006 | `		/* Point to the next entry */` |
|        21 | 2007 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        11 | 2008 | `	}` |
|         - | 2009 | `	/* Total inserted entries */` |
|        13 | 2010 | `	return (int)SySetUsed(pOut);` |
|         1 | 2011 | `}` |
|         - | 2012 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2013 | `/* SPDX-SnippetBegin */` |
|         - | 2014 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 2015 | `/* SPDX-License-Identifier: blessing */` |
|         - | 2016 | `/*` |
|         - | 2017 | ` * Merge sort.` |
|         - | 2018 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 2019 | ` * Status: Public domain` |
|         - | 2020 | ` */` |
|         - | 2021 | `/* Node comparison callback signature */` |
|         - | 2022 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 2023 | `/*` |
|         - | 2024 | `** Inputs:` |
|         - | 2025 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2026 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 2027 | `**   cmp:     A pointer to the comparison function.` |
|         - | 2028 | `**` |
|         - | 2029 | `** Return Value:` |
|         - | 2030 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 2031 | `**   of both a and b.` |
|         - | 2032 | `**` |
|         - | 2033 | `** Side effects:` |
|         - | 2034 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 2035 | `**   changed.` |
|         - | 2036 | `*/` |
|     35930 | 2037 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2038 | `{` |
|         - | 2039 | `	ph7_hashmap_node result,*pTail;` |
|         - | 2040 | `    /* Prevent compiler warning */` |
|     35935 | 2041 | `	result.pNext = result.pPrev = 0;` |
|     35935 | 2042 | `	pTail = &result;` |
|    107848 | 2043 | `	while( pA && pB ){` |
|     71918 | 2044 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     47607 | 2045 | `			pTail->pPrev = pA;` |
|     47607 | 2046 | `			pA->pNext = pTail;` |
|     47607 | 2047 | `			pTail = pA;` |
|     47607 | 2048 | `			pA = pA->pPrev;` |
|     23778 | 2049 | `		}else{` |
|     24316 | 2050 | `			pTail->pPrev = pB;` |
|     24316 | 2051 | `			pB->pNext = pTail;` |
|     24316 | 2052 | `			pTail = pB;` |
|     24316 | 2053 | `			pB = pB->pPrev;` |
|         - | 2054 | `		}` |
|         5 | 2055 | `	}` |
|     35935 | 2056 | `	if( pA ){` |
|     25383 | 2057 | `		pTail->pPrev = pA;` |
|     25383 | 2058 | `		pA->pNext = pTail;` |
|     23264 | 2059 | `	}else if( pB ){` |
|     10331 | 2060 | `		pTail->pPrev = pB;` |
|     10331 | 2061 | `		pB->pNext = pTail;` |
|      5150 | 2062 | `	}else{` |
|       231 | 2063 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 2064 | `	}` |
|     35935 | 2065 | `	return result.pPrev;` |
|         5 | 2066 | `}` |
|         - | 2067 | `/*` |
|         - | 2068 | `** Inputs:` |
|         - | 2069 | `**   Map:       Input hashmap` |
|         - | 2070 | `**   cmp:       A comparison function.` |
|         - | 2071 | `**` |
|         - | 2072 | `** Return Value:` |
|         - | 2073 | `**   Sorted hashmap.` |
|         - | 2074 | `**` |
|         - | 2075 | `** Side effects:` |
|         - | 2076 | `**   The "next" pointers for elements in list are changed.` |
|         - | 2077 | `*/` |
|         - | 2078 | `#define N_SORT_BUCKET  32` |
|       750 | 2079 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 2080 | `{` |
|         - | 2081 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 2082 | `	sxu32 i;` |
|       755 | 2083 | `	SyZero(a,sizeof(a));` |
|         - | 2084 | `	/* Point to the first inserted entry */` |
|       755 | 2085 | `	pIn = pMap->pFirst;` |
|     14759 | 2086 | `	while( pIn ){` |
|     14009 | 2087 | `		p = pIn;` |
|     14009 | 2088 | `		pIn = p->pPrev;` |
|     14009 | 2089 | `		p->pPrev = 0;` |
|     26689 | 2090 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26689 | 2091 | `			if( a[i]==0 ){` |
|     14009 | 2092 | `				a[i] = p;` |
|     14009 | 2093 | `				break;` |
|       ! 0 | 2094 | `			}else{` |
|     12685 | 2095 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12685 | 2096 | `				a[i] = 0;` |
|         - | 2097 | `			}` |
|      6345 | 2098 | `		}` |
|     14009 | 2099 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2100 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2101 | `			 * But that is impossible.` |
|         - | 2102 | `			 */` |
|       ! 0 | 2103 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2104 | `		}` |
|         5 | 2105 | `	}` |
|       755 | 2106 | `	p = a[0];` |
|     24005 | 2107 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     23255 | 2108 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11630 | 2109 | `	}` |
|       755 | 2110 | `	p->pNext = 0;` |
|         - | 2111 | `	/* Reflect the change */` |
|       755 | 2112 | `	pMap->pFirst = p;` |
|         - | 2113 | `	/* Reset the loop cursor */` |
|       755 | 2114 | `	pMap->pCur = pMap->pFirst;` |
|       755 | 2115 | `	return SXRET_OK;` |
|         5 | 2116 | `}` |
|         - | 2117 | `/* SPDX-SnippetEnd */` |
|         - | 2118 | `/*` |
|         - | 2119 | ` * Node comparison callback.` |
|         - | 2120 | ` * used-by: [sort(),asort(),...]` |
|         - | 2121 | ` */` |
|     71640 | 2122 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2123 | `{` |
|         - | 2124 | `	ph7_value sA,sB;` |
|         - | 2125 | `	sxi32 iFlags;` |
|         - | 2126 | `	int rc;` |
|     71645 | 2127 | `	if( pCmpData == 0 ){` |
|         - | 2128 | `		/* Perform a standard comparison */` |
|     71621 | 2129 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     71621 | 2130 | `		return rc;` |
|         - | 2131 | `	}` |
|        25 | 2132 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2133 | `	/* Duplicate node values */` |
|        25 | 2134 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        25 | 2135 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        25 | 2136 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        25 | 2137 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        25 | 2138 | `	if( iFlags == 5 ){` |
|         - | 2139 | `		/* String cast */` |
|         - | 2140 | `		const char *zA,*zB;` |
|         - | 2141 | `		sxu32 nA,nB,nMin;` |
|        15 | 2142 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2143 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2144 | `		}` |
|        15 | 2145 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2146 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2147 | `		}` |
|         - | 2148 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        15 | 2149 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        15 | 2150 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        15 | 2151 | `		nA = SyBlobLength(&sA.sBlob);` |
|        15 | 2152 | `		nB = SyBlobLength(&sB.sBlob);` |
|        15 | 2153 | `		nMin = nA < nB ? nA : nB;` |
|        15 | 2154 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        15 | 2155 | `		if( rc == 0 ){` |
|         5 | 2156 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2157 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2158 | `		}` |
|         8 | 2159 | `	}else{` |
|         - | 2160 | `		/* Numeric cast */` |
|        11 | 2161 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2162 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2163 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2164 | `	}` |
|        25 | 2165 | `	PH7_MemObjRelease(&sA);` |
|        25 | 2166 | `	PH7_MemObjRelease(&sB);` |
|        25 | 2167 | `	return rc;` |
|     35798 | 2168 | `}` |
|         - | 2169 | `/*` |
|         - | 2170 | ` * Shared key comparison for ksort()/krsort(): php 8 semantics. Two string` |
|         - | 2171 | ` * keys compare bytewise. Mixed int/string keys: a NUMERIC string compares` |
|         - | 2172 | ` * numerically with the int key; a non-numeric one makes the int key compare` |
|         - | 2173 | ` * AS A STRING ("5" < "b", so int keys land before alphabetic ones — pre-fix` |
|         - | 2174 | ` * PHL cast "b" to 0 and sorted string keys first).` |
|         - | 2175 | ` */` |
|         - | 2176 | `/* True lexicographic compare (memcmp on the common prefix, length breaks` |
|         - | 2177 | ` * ties) — SyBlobCmp compares LENGTH first, which is fine for equality but` |
|         - | 2178 | ` * wrong for ordering ("c" would sort before "a.y"). */` |
|        36 | 2179 | `static sxi32 HashmapLexCmp(const char *zA,sxu32 nA,const char *zB,sxu32 nB)` |
|         2 | 2180 | `{` |
|        38 | 2181 | `	sxu32 nMin = nA < nB ? nA : nB;` |
|        38 | 2182 | `	sxi32 rc = nMin ? SyMemcmp(zA,zB,nMin) : 0;` |
|        38 | 2183 | `	if( rc == 0 ){` |
|       ! 0 | 2184 | `		rc = (sxi32)nA - (sxi32)nB;` |
|       ! 0 | 2185 | `	}` |
|        38 | 2186 | `	return rc;` |
|         2 | 2187 | `}` |
|        58 | 2188 | `static sxi32 HashmapKeyNodeCmp(ph7_hashmap_node *pA,ph7_hashmap_node *pB)` |
|         2 | 2189 | `{` |
|         - | 2190 | `	sxi32 rc;` |
|        60 | 2191 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2192 | `		/* Perform a string comparison */` |
|        32 | 2193 | `		rc = HashmapLexCmp((const char *)SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey),` |
|        20 | 2194 | `			(const char *)SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|        12 | 2195 | `	}else{` |
|         - | 2196 | `		SyString sStr;` |
|        39 | 2197 | `		sxi64 iA = 0,iB = 0;` |
|        39 | 2198 | `		int bNum = 1;` |
|        39 | 2199 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|        11 | 2200 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|        11 | 2201 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|        11 | 2202 | `				bNum = 0;` |
|         6 | 2203 | `			}else{` |
|       ! 0 | 2204 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2205 | `			}` |
|         6 | 2206 | `		}else{` |
|        29 | 2207 | `			iA = pA->xKey.iKey;` |
|         - | 2208 | `		}` |
|        39 | 2209 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         7 | 2210 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         7 | 2211 | `			if( sStr.nByte < 1 \|\| SyStrIsNumeric(sStr.zString,sStr.nByte,0,0) != SXRET_OK ){` |
|         7 | 2212 | `				bNum = 0;` |
|         4 | 2213 | `			}else{` |
|       ! 0 | 2214 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2215 | `			}` |
|         4 | 2216 | `		}else{` |
|        33 | 2217 | `			iB = pB->xKey.iKey;` |
|         - | 2218 | `		}` |
|        39 | 2219 | `		if( bNum ){` |
|        23 | 2220 | `			rc = iA < iB ? -1 : (iA > iB ? 1 : 0);` |
|        12 | 2221 | `		}else{` |
|         - | 2222 | `			/* Render the int key and compare bytewise like php */` |
|         - | 2223 | `			char zNumA[24],zNumB[24];` |
|         - | 2224 | `			SyString sA,sB;` |
|        17 | 2225 | `			if( pA->iType != HASHMAP_BLOB_NODE ){` |
|         7 | 2226 | `				sxu32 n = SyBufferFormat(zNumA,sizeof(zNumA),"%qd",pA->xKey.iKey);` |
|         7 | 2227 | `				SyStringInitFromBuf(&sA,zNumA,n);` |
|         4 | 2228 | `			}else{` |
|        11 | 2229 | `				SyStringInitFromBuf(&sA,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|         - | 2230 | `			}` |
|        17 | 2231 | `			if( pB->iType != HASHMAP_BLOB_NODE ){` |
|        11 | 2232 | `				sxu32 n = SyBufferFormat(zNumB,sizeof(zNumB),"%qd",pB->xKey.iKey);` |
|        11 | 2233 | `				SyStringInitFromBuf(&sB,zNumB,n);` |
|         6 | 2234 | `			}else{` |
|         7 | 2235 | `				SyStringInitFromBuf(&sB,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|         - | 2236 | `			}` |
|        17 | 2237 | `			rc = HashmapLexCmp(sA.zString,sA.nByte,sB.zString,sB.nByte);` |
|         - | 2238 | `		}` |
|         - | 2239 | `	}` |
|        60 | 2240 | `	return rc;` |
|         2 | 2241 | `}` |
|         - | 2242 | `/*` |
|         - | 2243 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2244 | ` * used-by: [ksort()]` |
|         - | 2245 | ` */` |
|        44 | 2246 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2247 | `{` |
|        22 | 2248 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        46 | 2249 | `	return HashmapKeyNodeCmp(pA,pB);` |
|         2 | 2250 | `}` |
|         - | 2251 | `/*` |
|         - | 2252 | ` * Node comparison callback.` |
|         - | 2253 | ` * Used by: [rsort(),arsort()];` |
|         - | 2254 | ` */` |
|        78 | 2255 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2256 | `{` |
|         - | 2257 | `	ph7_value sA,sB;` |
|         - | 2258 | `	sxi32 iFlags;` |
|         - | 2259 | `	int rc;` |
|        79 | 2260 | `	if( pCmpData == 0 ){` |
|         - | 2261 | `		/* Perform a standard comparison */` |
|        59 | 2262 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2263 | `		return -rc;` |
|         - | 2264 | `	}` |
|        21 | 2265 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2266 | `	/* Duplicate node values */` |
|        21 | 2267 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2268 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2269 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2270 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2271 | `	if( iFlags == 5 ){` |
|         - | 2272 | `		/* String cast */` |
|         - | 2273 | `		const char *zA,*zB;` |
|         - | 2274 | `		sxu32 nA,nB,nMin;` |
|        11 | 2275 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2276 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2277 | `		}` |
|        11 | 2278 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2279 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2280 | `		}` |
|         - | 2281 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2282 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2283 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2284 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2285 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2286 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2287 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2288 | `		if( rc == 0 ){` |
|         3 | 2289 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2290 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2291 | `		}` |
|         6 | 2292 | `	}else{` |
|         - | 2293 | `		/* Numeric cast */` |
|        11 | 2294 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2295 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2296 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2297 | `	}` |
|        21 | 2298 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2299 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2300 | `	return -rc;` |
|        40 | 2301 | `}` |
|         - | 2302 | `/*` |
|         - | 2303 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2304 | ` * used-by: [usort(),uasort()]` |
|         - | 2305 | ` */` |
|       110 | 2306 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         2 | 2307 | `{` |
|         - | 2308 | `	ph7_value sResult,*pCallback;` |
|         - | 2309 | `	ph7_value *pV1,*pV2;` |
|         - | 2310 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2311 | `	sxi32 rc;` |
|         - | 2312 | `	/* Point to the desired callback */` |
|       112 | 2313 | `	pCallback = (ph7_value *)pCmpData;` |
|       112 | 2314 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2315 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2316 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2317 | `		return 0;` |
|         - | 2318 | `	}` |
|         - | 2319 | `	/* initialize the result value */` |
|       106 | 2320 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2321 | `	/* Extract nodes values */` |
|       106 | 2322 | `	pV1 = HashmapExtractNodeValue(pA);` |
|       106 | 2323 | `	pV2 = HashmapExtractNodeValue(pB);` |
|       106 | 2324 | `	apArg[0] = pV1;` |
|       106 | 2325 | `	apArg[1] = pV2;` |
|         - | 2326 | `	/* Invoke the callback */` |
|       106 | 2327 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       106 | 2328 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2329 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2330 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2331 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2332 | `		rc = 0;` |
|       102 | 2333 | `	}else if( rc != SXRET_OK ){` |
|         - | 2334 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2335 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2336 | `	}else{` |
|         - | 2337 | `		/* Extract callback result */` |
|        98 | 2338 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2339 | `			/* Perform an int cast */` |
|       ! 0 | 2340 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2341 | `		}` |
|        98 | 2342 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2343 | `	}` |
|       106 | 2344 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2345 | `	/* Callback result */` |
|       106 | 2346 | `	return rc;` |
|        57 | 2347 | `}` |
|         - | 2348 | `/*` |
|         - | 2349 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2350 | ` * used-by: [krsort()]` |
|         - | 2351 | ` */` |
|        14 | 2352 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2353 | `{` |
|         7 | 2354 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        15 | 2355 | `	return -HashmapKeyNodeCmp(pA,pB); /* Reverse result */` |
|         1 | 2356 | `}` |
|         - | 2357 | `/*` |
|         - | 2358 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2359 | ` * used-by: [uksort()]` |
|         - | 2360 | ` */` |
|         6 | 2361 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2362 | `{` |
|         - | 2363 | `	ph7_value sResult,*pCallback;` |
|         - | 2364 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2365 | `	ph7_value sK1,sK2;` |
|         - | 2366 | `	sxi32 rc;` |
|         - | 2367 | `	/* Point to the desired callback */` |
|         7 | 2368 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2369 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2370 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2371 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2372 | `		return 0;` |
|         - | 2373 | `	}` |
|         - | 2374 | `	/* initialize the result value */` |
|         7 | 2375 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2376 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2377 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2378 | `	/* Extract nodes keys */` |
|         7 | 2379 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2380 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2381 | `	apArg[0] = &sK1;` |
|         7 | 2382 | `	apArg[1] = &sK2;` |
|         - | 2383 | `	/* Mark keys as constants */` |
|         7 | 2384 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2385 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2386 | `	/* Invoke the callback */` |
|         7 | 2387 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2388 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2389 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2390 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2391 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2392 | `		rc = 0;` |
|         7 | 2393 | `	}else if( rc != SXRET_OK ){` |
|         - | 2394 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2395 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2396 | `	}else{` |
|         - | 2397 | `		/* Extract callback result */` |
|         7 | 2398 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2399 | `			/* Perform an int cast */` |
|       ! 0 | 2400 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2401 | `		}` |
|         7 | 2402 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2403 | `	}` |
|         7 | 2404 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2405 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2406 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2407 | `	/* Callback result */` |
|         7 | 2408 | `	return rc;` |
|         4 | 2409 | `}` |
|         - | 2410 | `/*` |
|         - | 2411 | ` * Node comparison callback: Random node comparison.` |
|         - | 2412 | ` * used-by: [shuffle()]` |
|         - | 2413 | ` */` |
|        21 | 2414 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2415 | `{` |
|         - | 2416 | `	sxu32 n;` |
|        12 | 2417 | `	SXUNUSED(pB); /* cc warning */` |
|        12 | 2418 | `	SXUNUSED(pCmpData);` |
|         - | 2419 | `	/* Grab a random number */` |
|        22 | 2420 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2421 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2422 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2423 | `	 */` |
|        22 | 2424 | `	return n&1 ? 1 : -1;` |
|         1 | 2425 | `}` |
|         - | 2426 | `/*` |
|         - | 2427 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2428 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2429 | ` */` |
|       680 | 2430 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2431 | `{` |
|         - | 2432 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2433 | `	sxu32 i;` |
|         - | 2434 | `	/* Rehash all entries */` |
|       685 | 2435 | `	pLast = p = pMap->pFirst;` |
|       685 | 2436 | `	pMap->iNextIdx = 0;` |
|       685 | 2437 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|       685 | 2438 | `	i = 0;` |
|      7224 | 2439 | `	for( ;; ){` |
|     14453 | 2440 | `		if( i >= pMap->nEntry ){` |
|       685 | 2441 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       685 | 2442 | `			break;` |
|         - | 2443 | `		}` |
|     13773 | 2444 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2445 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2446 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2447 | `			/* Change key type */` |
|         5 | 2448 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2449 | `		}` |
|     13773 | 2450 | `		HashmapRehashIntNode(p);` |
|         - | 2451 | `		/* Point to the next entry */` |
|     13773 | 2452 | `		i++;` |
|     13773 | 2453 | `		pLast = p;` |
|     13773 | 2454 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2455 | `	}` |
|       685 | 2456 | `}` |
|         - | 2457 | `/*` |
|         - | 2458 | ` * Array functions implementation.` |
|         - | 2459 | ` * Status:` |
|         - | 2460 | ` *  Stable.` |
|         - | 2461 | ` */` |
|         - | 2462 | `/*` |
|         - | 2463 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2464 | ` * Sort an array.` |
|         - | 2465 | ` * Parameters` |
|         - | 2466 | ` *  $array` |
|         - | 2467 | ` *   The input array.` |
|         - | 2468 | ` * $sort_flags` |
|         - | 2469 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2470 | ` *  Sorting type flags:` |
|         - | 2471 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2472 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2473 | ` *   SORT_STRING - compare items as strings` |
|         - | 2474 | ` * Return` |
|         - | 2475 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2476 | ` *` |
|         - | 2477 | ` */` |
|      1016 | 2478 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2479 | `{` |
|         - | 2480 | `	ph7_hashmap *pMap;` |
|         - | 2481 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1021 | 2482 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2483 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2484 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2485 | `		return PH7_OK;` |
|         - | 2486 | `	}` |
|         - | 2487 | `	/* Point to the internal representation of the input hashmap */` |
|      1021 | 2488 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1021 | 2489 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1021 | 2490 | `	if( pMap->nEntry > 1 ){` |
|       661 | 2491 | `		sxi32 iCmpFlags = 0;` |
|       661 | 2492 | `		if( nArg > 1 ){` |
|         - | 2493 | `			/* Extract comparison flags */` |
|         3 | 2494 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2495 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2496 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2497 | `			}` |
|         1 | 2498 | `		}` |
|         - | 2499 | `		/* Do the merge sort */` |
|       661 | 2500 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2501 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       661 | 2502 | `		HashmapSortRehash(pMap);` |
|       328 | 2503 | `	}` |
|         - | 2504 | `	/* All done,return TRUE */` |
|      1021 | 2505 | `	ph7_result_bool(pCtx,1);` |
|      1021 | 2506 | `	return PH7_OK;` |
|       513 | 2507 | `}` |
|         - | 2508 | `/*` |
|         - | 2509 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2510 | ` *  Sort an array and maintain index association.` |
|         - | 2511 | ` * Parameters` |
|         - | 2512 | ` *  $array` |
|         - | 2513 | ` *   The input array.` |
|         - | 2514 | ` * $sort_flags` |
|         - | 2515 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2516 | ` *  Sorting type flags:` |
|         - | 2517 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2518 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2519 | ` *   SORT_STRING - compare items as strings` |
|         - | 2520 | ` * Return` |
|         - | 2521 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2522 | ` */` |
|        32 | 2523 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2524 | `{` |
|         - | 2525 | `	ph7_hashmap *pMap;` |
|         - | 2526 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2527 | `	if( nArg < 1 ){` |
|       ! 0 | 2528 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2529 | `			"ArgumentCountError",` |
|         - | 2530 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2531 | `			);` |
|         - | 2532 | `	}` |
|         - | 2533 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        37 | 2534 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2535 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2536 | `			"TypeError",` |
|         - | 2537 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2538 | `			ph7_type_name(apArg[0])` |
|         - | 2539 | `			);` |
|         - | 2540 | `	}` |
|         - | 2541 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 2542 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        25 | 2543 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        25 | 2544 | `	if( pMap->nEntry > 1 ){` |
|        21 | 2545 | `		sxi32 iCmpFlags = 0;` |
|        21 | 2546 | `		if( nArg > 1 ){` |
|         - | 2547 | `			/* Extract comparison flags */` |
|         5 | 2548 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2549 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2550 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2551 | `			}` |
|         2 | 2552 | `		}` |
|         - | 2553 | `		/* Do the merge sort */` |
|        21 | 2554 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2555 | `		/* Fix the last link broken by the merge */` |
|        49 | 2556 | `		while(pMap->pLast->pPrev){` |
|        29 | 2557 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2558 | `		}` |
|        10 | 2559 | `	}` |
|         - | 2560 | `	/* All done,return TRUE */` |
|        25 | 2561 | `	ph7_result_bool(pCtx,1);` |
|        25 | 2562 | `	return PH7_OK;` |
|        21 | 2563 | `}` |
|         - | 2564 | `/*` |
|         - | 2565 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2566 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2567 | ` * Parameters` |
|         - | 2568 | ` *  $array` |
|         - | 2569 | ` *   The input array.` |
|         - | 2570 | ` * $sort_flags` |
|         - | 2571 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2572 | ` *  Sorting type flags:` |
|         - | 2573 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2574 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2575 | ` *   SORT_STRING - compare items as strings` |
|         - | 2576 | ` * Return` |
|         - | 2577 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2578 | ` */` |
|        30 | 2579 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2580 | `{` |
|         - | 2581 | `	ph7_hashmap *pMap;` |
|         - | 2582 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        35 | 2583 | `	if( nArg < 1 ){` |
|       ! 0 | 2584 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2585 | `			"ArgumentCountError",` |
|         - | 2586 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2587 | `			);` |
|         - | 2588 | `	}` |
|         - | 2589 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2590 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2591 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2592 | `			"TypeError",` |
|         - | 2593 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2594 | `			ph7_type_name(apArg[0])` |
|         - | 2595 | `			);` |
|         - | 2596 | `	}` |
|         - | 2597 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2598 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2599 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2600 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2601 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2602 | `		if( nArg > 1 ){` |
|         - | 2603 | `			/* Extract comparison flags */` |
|         5 | 2604 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2605 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2606 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2607 | `			}` |
|         2 | 2608 | `		}` |
|         - | 2609 | `		/* Do the merge sort */` |
|        19 | 2610 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2611 | `		/* Fix the last link broken by the merge */` |
|        35 | 2612 | `		while(pMap->pLast->pPrev){` |
|        17 | 2613 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2614 | `		}` |
|         9 | 2615 | `	}` |
|         - | 2616 | `	/* All done,return TRUE */` |
|        23 | 2617 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2618 | `	return PH7_OK;` |
|        20 | 2619 | `}` |
|         - | 2620 | `/*` |
|         - | 2621 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2622 | ` *  Sort an array by key.` |
|         - | 2623 | ` * Parameters` |
|         - | 2624 | ` *  $array` |
|         - | 2625 | ` *   The input array.` |
|         - | 2626 | ` * $sort_flags` |
|         - | 2627 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2628 | ` *  Sorting type flags:` |
|         - | 2629 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2630 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2631 | ` *   SORT_STRING - compare items as strings` |
|         - | 2632 | ` * Return` |
|         - | 2633 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2634 | ` */` |
|        14 | 2635 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2636 | `{` |
|         - | 2637 | `	ph7_hashmap *pMap;` |
|         - | 2638 | `	/* Make sure we are dealing with a valid hashmap */` |
|        16 | 2639 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2640 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2641 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2642 | `		return PH7_OK;` |
|         - | 2643 | `	}` |
|         - | 2644 | `	/* Point to the internal representation of the input hashmap */` |
|        16 | 2645 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        16 | 2646 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        16 | 2647 | `	if( pMap->nEntry > 1 ){` |
|        16 | 2648 | `		sxi32 iCmpFlags = 0;` |
|        16 | 2649 | `		if( nArg > 1 ){` |
|         - | 2650 | `			/* Extract comparison flags */` |
|       ! 0 | 2651 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2652 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2653 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2654 | `			}` |
|       ! 0 | 2655 | `		}` |
|         - | 2656 | `		/* Do the merge sort */` |
|        16 | 2657 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2658 | `		/* Fix the last link broken by the merge */` |
|        38 | 2659 | `		while(pMap->pLast->pPrev){` |
|        23 | 2660 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2661 | `		}` |
|         7 | 2662 | `	}` |
|         - | 2663 | `	/* All done,return TRUE */` |
|        16 | 2664 | `	ph7_result_bool(pCtx,1);` |
|        16 | 2665 | `	return PH7_OK;` |
|         9 | 2666 | `}` |
|         - | 2667 | `/*` |
|         - | 2668 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2669 | ` *  Sort an array by key in reverse order.` |
|         - | 2670 | ` * Parameters` |
|         - | 2671 | ` *  $array` |
|         - | 2672 | ` *   The input array.` |
|         - | 2673 | ` * $sort_flags` |
|         - | 2674 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2675 | ` *  Sorting type flags:` |
|         - | 2676 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2677 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2678 | ` *   SORT_STRING - compare items as strings` |
|         - | 2679 | ` * Return` |
|         - | 2680 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2681 | ` */` |
|         4 | 2682 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2683 | `{` |
|         - | 2684 | `	ph7_hashmap *pMap;` |
|         - | 2685 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2686 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2687 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2688 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2689 | `		return PH7_OK;` |
|         - | 2690 | `	}` |
|         - | 2691 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2692 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2693 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2694 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2695 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2696 | `		if( nArg > 1 ){` |
|         - | 2697 | `			/* Extract comparison flags */` |
|       ! 0 | 2698 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2699 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2700 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2701 | `			}` |
|       ! 0 | 2702 | `		}` |
|         - | 2703 | `		/* Do the merge sort */` |
|         5 | 2704 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2705 | `		/* Fix the last link broken by the merge */` |
|        17 | 2706 | `		while(pMap->pLast->pPrev){` |
|        13 | 2707 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2708 | `		}` |
|         2 | 2709 | `	}` |
|         - | 2710 | `	/* All done,return TRUE */` |
|         5 | 2711 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2712 | `	return PH7_OK;` |
|         3 | 2713 | `}` |
|         - | 2714 | `/*` |
|         - | 2715 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2716 | ` * Sort an array in reverse order.` |
|         - | 2717 | ` * Parameters` |
|         - | 2718 | ` *  $array` |
|         - | 2719 | ` *   The input array.` |
|         - | 2720 | ` * $sort_flags` |
|         - | 2721 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2722 | ` *  Sorting type flags:` |
|         - | 2723 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2724 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2725 | ` *   SORT_STRING - compare items as strings` |
|         - | 2726 | ` * Return` |
|         - | 2727 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2728 | ` */` |
|         2 | 2729 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2730 | `{` |
|         - | 2731 | `	ph7_hashmap *pMap;` |
|         - | 2732 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2733 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2734 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2735 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2736 | `		return PH7_OK;` |
|         - | 2737 | `	}` |
|         - | 2738 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2739 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2740 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2741 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2742 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2743 | `		if( nArg > 1 ){` |
|         - | 2744 | `			/* Extract comparison flags */` |
|       ! 0 | 2745 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2746 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2747 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2748 | `			}` |
|       ! 0 | 2749 | `		}` |
|         - | 2750 | `		/* Do the merge sort */` |
|         3 | 2751 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2752 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2753 | `		HashmapSortRehash(pMap);` |
|         1 | 2754 | `	}` |
|         - | 2755 | `	/* All done,return TRUE */` |
|         3 | 2756 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2757 | `	return PH7_OK;` |
|         2 | 2758 | `}` |
|         - | 2759 | `/*` |
|         - | 2760 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2761 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2762 | ` * Parameters` |
|         - | 2763 | ` *  $array` |
|         - | 2764 | ` *   The input array.` |
|         - | 2765 | ` * $cmp_function` |
|         - | 2766 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2767 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2768 | ` *  to, or greater than the second.` |
|         - | 2769 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2770 | ` * Return` |
|         - | 2771 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2772 | ` */` |
|        18 | 2773 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 2774 | `{` |
|         - | 2775 | `	ph7_hashmap *pMap;` |
|         - | 2776 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 2777 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2778 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2779 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2780 | `		return PH7_OK;` |
|         - | 2781 | `	}` |
|         - | 2782 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 2783 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        20 | 2784 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 2785 | `	if( pMap->nEntry > 1 ){` |
|        20 | 2786 | `		ph7_value *pCallback = 0;` |
|         - | 2787 | `		ProcNodeCmp xCmp;` |
|        20 | 2788 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        20 | 2789 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2790 | `			/* Point to the desired callback */` |
|        20 | 2791 | `			pCallback = apArg[1];` |
|        11 | 2792 | `		}else{` |
|         - | 2793 | `			/* Use the default comparison function */` |
|       ! 0 | 2794 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2795 | `		}` |
|         - | 2796 | `		/* Do the merge sort */` |
|        20 | 2797 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        20 | 2798 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2799 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        20 | 2800 | `		HashmapSortRehash(pMap);` |
|        20 | 2801 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2802 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2803 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2804 | `			return PH7_EXCEPTION;` |
|         - | 2805 | `		}` |
|         5 | 2806 | `	}` |
|         - | 2807 | `	/* All done,return TRUE */` |
|        12 | 2808 | `	ph7_result_bool(pCtx,1);` |
|        12 | 2809 | `	return PH7_OK;` |
|        11 | 2810 | `}` |
|         - | 2811 | `/*` |
|         - | 2812 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2813 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2814 | ` *  and maintain index association.` |
|         - | 2815 | ` * Parameters` |
|         - | 2816 | ` *  $array` |
|         - | 2817 | ` *   The input array.` |
|         - | 2818 | ` * $cmp_function` |
|         - | 2819 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2820 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2821 | ` *  to, or greater than the second.` |
|         - | 2822 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2823 | ` * Return` |
|         - | 2824 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2825 | ` */` |
|        10 | 2826 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2827 | `{` |
|         - | 2828 | `	ph7_hashmap *pMap;` |
|         - | 2829 | `	/* Make sure we are dealing with a valid hashmap */` |
|        11 | 2830 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2831 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2832 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2833 | `		return PH7_OK;` |
|         - | 2834 | `	}` |
|         - | 2835 | `	/* Point to the internal representation of the input hashmap */` |
|        11 | 2836 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        11 | 2837 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        11 | 2838 | `	if( pMap->nEntry > 1 ){` |
|        11 | 2839 | `		ph7_value *pCallback = 0;` |
|         - | 2840 | `		ProcNodeCmp xCmp;` |
|        11 | 2841 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        11 | 2842 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2843 | `			/* Point to the desired callback */` |
|        11 | 2844 | `			pCallback = apArg[1];` |
|         6 | 2845 | `		}else{` |
|         - | 2846 | `			/* Use the default comparison function */` |
|       ! 0 | 2847 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2848 | `		}` |
|         - | 2849 | `		/* Do the merge sort */` |
|        11 | 2850 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        11 | 2851 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2852 | `		/* Fix the last link broken by the merge */` |
|        23 | 2853 | `		while(pMap->pLast->pPrev){` |
|        13 | 2854 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2855 | `		}` |
|        11 | 2856 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2857 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2858 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2859 | `			return PH7_EXCEPTION;` |
|         - | 2860 | `		}` |
|         5 | 2861 | `	}` |
|         - | 2862 | `	/* All done,return TRUE */` |
|        11 | 2863 | `	ph7_result_bool(pCtx,1);` |
|        11 | 2864 | `	return PH7_OK;` |
|         6 | 2865 | `}` |
|         - | 2866 | `/*` |
|         - | 2867 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2868 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2869 | ` *  function and maintain index association.` |
|         - | 2870 | ` * Parameters` |
|         - | 2871 | ` *  $array` |
|         - | 2872 | ` *   The input array.` |
|         - | 2873 | ` * $cmp_function` |
|         - | 2874 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2875 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2876 | ` *  to, or greater than the second.` |
|         - | 2877 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2878 | ` * Return` |
|         - | 2879 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2880 | ` */` |
|         2 | 2881 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2882 | `{` |
|         - | 2883 | `	ph7_hashmap *pMap;` |
|         - | 2884 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2885 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2886 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2887 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2888 | `		return PH7_OK;` |
|         - | 2889 | `	}` |
|         - | 2890 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2891 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2892 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2893 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2894 | `		ph7_value *pCallback = 0;` |
|         - | 2895 | `		ProcNodeCmp xCmp;` |
|         3 | 2896 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2897 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2898 | `			/* Point to the desired callback */` |
|         3 | 2899 | `			pCallback = apArg[1];` |
|         2 | 2900 | `		}else{` |
|         - | 2901 | `			/* Use the default comparison function */` |
|       ! 0 | 2902 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2903 | `		}` |
|         - | 2904 | `		/* Do the merge sort */` |
|         3 | 2905 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2906 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2907 | `		/* Fix the last link broken by the merge */` |
|         3 | 2908 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2909 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2910 | `		}` |
|         3 | 2911 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2912 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2913 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2914 | `			return PH7_EXCEPTION;` |
|         - | 2915 | `		}` |
|         1 | 2916 | `	}` |
|         - | 2917 | `	/* All done,return TRUE */` |
|         3 | 2918 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2919 | `	return PH7_OK;` |
|         2 | 2920 | `}` |
|         - | 2921 | `/*` |
|         - | 2922 | ` * bool shuffle(array &$array)` |
|         - | 2923 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2924 | ` * Parameters` |
|         - | 2925 | ` *  $array` |
|         - | 2926 | ` *   The input array.` |
|         - | 2927 | ` * Return` |
|         - | 2928 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2929 | ` *` |
|         - | 2930 | ` */` |
|         2 | 2931 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2932 | `{` |
|         - | 2933 | `	ph7_hashmap *pMap;` |
|         - | 2934 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2935 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2936 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2937 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2938 | `		return PH7_OK;` |
|         - | 2939 | `	}` |
|         - | 2940 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2941 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2942 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2943 | `	if( pMap->nEntry > 1 ){` |
|         - | 2944 | `		/* Do the merge sort */` |
|         3 | 2945 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2946 | `		/* Fix the last link broken by the merge */` |
|         8 | 2947 | `		while(pMap->pLast->pPrev){` |
|         5 | 2948 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2949 | `		}` |
|         1 | 2950 | `	}` |
|         - | 2951 | `	/* All done,return TRUE */` |
|         3 | 2952 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2953 | `	return PH7_OK;` |
|         2 | 2954 | `}` |
|         - | 2955 | `/*` |
|         - | 2956 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2957 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2958 | ` * Parameters` |
|         - | 2959 | ` *  $var` |
|         - | 2960 | ` *   The array or the object.` |
|         - | 2961 | ` * $mode` |
|         - | 2962 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2963 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2964 | ` *  all the elements of a multidimensional array.` |
|         - | 2965 | ` * Return` |
|         - | 2966 | ` *  Returns the number of elements in the array.` |
|         - | 2967 | ` */` |
|      1870 | 2968 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2969 | `{` |
|      1875 | 2970 | `	int bRecursive = FALSE;` |
|      1875 | 2971 | `	int bCycleDetected = FALSE;` |
|         - | 2972 | `	sxi64 iCount;` |
|      1875 | 2973 | `	if( nArg < 1 ){` |
|       ! 0 | 2974 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2975 | `			"ArgumentCountError",` |
|         - | 2976 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2977 | `			);` |
|         - | 2978 | `	}` |
|      1875 | 2979 | `	if( nArg > 2 ){` |
|         4 | 2980 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2981 | `			"ArgumentCountError",` |
|         - | 2982 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2983 | `			nArg` |
|         - | 2984 | `			);` |
|         - | 2985 | `	}` |
|         - | 2986 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2987 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2988 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1873 | 2989 | `	if( nArg > 1 ){` |
|        45 | 2990 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2991 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        11 | 2992 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2993 | `				"ValueError",` |
|         - | 2994 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2995 | `				);` |
|         - | 2996 | `		}` |
|        34 | 2997 | `		bRecursive = iMode == 1;` |
|        16 | 2998 | `	}` |
|      1865 | 2999 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3000 | `		/* Countable object: dispatch to ->count() */` |
|        73 | 3001 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        62 | 3002 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        62 | 3003 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        62 | 3004 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        59 | 3005 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 3006 | `					"count",sizeof("count")-1);` |
|        59 | 3007 | `				if( pMeth ){` |
|         - | 3008 | `					ph7_value sResult;` |
|        59 | 3009 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        59 | 3010 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        59 | 3011 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        59 | 3012 | `					PH7_MemObjRelease(&sResult);` |
|        59 | 3013 | `					return PH7_OK;` |
|         - | 3014 | `				}` |
|       ! 0 | 3015 | `			}` |
|         1 | 3016 | `		}` |
|        22 | 3017 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3018 | `			"TypeError",` |
|         - | 3019 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 3020 | `			ph7_type_name(apArg[0])` |
|         - | 3021 | `			);` |
|         - | 3022 | `	}` |
|         - | 3023 | `	/* Count */` |
|      1797 | 3024 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1797 | 3025 | `	if( bCycleDetected ){` |
|         3 | 3026 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 3027 | `	}` |
|      1797 | 3028 | `	ph7_result_int64(pCtx,iCount);` |
|      1797 | 3029 | `	return PH7_OK;` |
|       940 | 3030 | `}` |
|         - | 3031 | `/*` |
|         - | 3032 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 3033 | ` *  Checks if the given key or index exists in the array.` |
|         - | 3034 | ` * Parameters` |
|         - | 3035 | ` * $key` |
|         - | 3036 | ` *   Value to check.` |
|         - | 3037 | ` * $search` |
|         - | 3038 | ` *  An array with keys to check.` |
|         - | 3039 | ` * Return` |
|         - | 3040 | ` *  TRUE on success or FALSE on failure.` |
|         - | 3041 | ` */` |
|        90 | 3042 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3043 | `{` |
|         - | 3044 | `	sxi32 rc;` |
|        94 | 3045 | `	if( nArg != 2 ){` |
|         - | 3046 | `		/* PHP requires exactly two arguments */` |
|         4 | 3047 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3048 | `			"ArgumentCountError",` |
|         - | 3049 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         1 | 3050 | `			nArg` |
|         - | 3051 | `			);` |
|         - | 3052 | `	}` |
|         - | 3053 | `	/* Make sure we are dealing with a valid hashmap */` |
|        92 | 3054 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 3055 | `		/* Type mismatch -> TypeError */` |
|         8 | 3056 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3057 | `			"TypeError",` |
|         - | 3058 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 3059 | `			ph7_type_name(apArg[1])` |
|         - | 3060 | `			);` |
|         - | 3061 | `	}` |
|         - | 3062 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        88 | 3063 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         3 | 3064 | `		ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3065 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 3066 | `			"use an empty string instead"` |
|         - | 3067 | `			);` |
|        87 | 3068 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 3069 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 3070 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 3071 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 3072 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 3073 | `				,rVal` |
|         - | 3074 | `				);` |
|         1 | 3075 | `		}` |
|         1 | 3076 | `	}` |
|         - | 3077 | `	/* Perform the lookup */` |
|        88 | 3078 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 3079 | `	/* lookup result */` |
|        88 | 3080 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        88 | 3081 | `	return PH7_OK;` |
|        49 | 3082 | `}` |
|         - | 3083 | `/*` |
|         - | 3084 | ` * value array_pop(array $array)` |
|         - | 3085 | ` *   POP the last inserted element from the array.` |
|         - | 3086 | ` * Parameter` |
|         - | 3087 | ` *  The array to get the value from.` |
|         - | 3088 | ` * Return` |
|         - | 3089 | ` *  Poped value or NULL on failure.` |
|         - | 3090 | ` */` |
|       102 | 3091 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 3092 | `{` |
|         - | 3093 | `	ph7_hashmap *pMap;` |
|         - | 3094 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|       106 | 3095 | `	if( nArg != 1 ){` |
|         4 | 3096 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3097 | `			"ArgumentCountError",` |
|         - | 3098 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         1 | 3099 | `			nArg` |
|         - | 3100 | `			);` |
|         - | 3101 | `	}` |
|         - | 3102 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3103 | `	 * error message as official PHP. Check the index to detect constants. */` |
|       104 | 3104 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3105 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3106 | `			"Error",` |
|         - | 3107 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3108 | `			);` |
|         - | 3109 | `	}` |
|         - | 3110 | `	/* Make sure we are dealing with a valid hashmap */` |
|        98 | 3111 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3112 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3113 | `			"TypeError",` |
|         - | 3114 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3115 | `			ph7_type_name(apArg[0])` |
|         - | 3116 | `			);` |
|         - | 3117 | `	}` |
|        95 | 3118 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        95 | 3119 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        95 | 3120 | `	if( pMap->nEntry < 1 ){` |
|         - | 3121 | `		/* Nothing to pop,return NULL */` |
|         3 | 3122 | `		ph7_result_null(pCtx);` |
|         2 | 3123 | `	}else{` |
|        93 | 3124 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3125 | `		ph7_value *pObj;` |
|        93 | 3126 | `		pObj = HashmapExtractNodeValue(pLast);` |
|        93 | 3127 | `		if( pObj ){` |
|         - | 3128 | `			/* Node value */` |
|        93 | 3129 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3130 | `			/* Unlink the node */` |
|        93 | 3131 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|        47 | 3132 | `		}else{` |
|       ! 0 | 3133 | `			ph7_result_null(pCtx);` |
|         - | 3134 | `		}` |
|         - | 3135 | `		/* Reset the cursor */` |
|        93 | 3136 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3137 | `	}` |
|        95 | 3138 | `	return PH7_OK;` |
|        55 | 3139 | `}` |
|         - | 3140 | `/*` |
|         - | 3141 | ` * int array_push($array,$var,...)` |
|         - | 3142 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3143 | ` * Parameters` |
|         - | 3144 | ` *  array` |
|         - | 3145 | ` *    The input array.` |
|         - | 3146 | ` *  var` |
|         - | 3147 | ` *   On or more value to push.` |
|         - | 3148 | ` * Return` |
|         - | 3149 | ` *  New array count (including old items).` |
|         - | 3150 | ` */` |
|        22 | 3151 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3152 | `{` |
|         - | 3153 | `	ph7_hashmap *pMap;` |
|         - | 3154 | `	sxi32 rc;` |
|         - | 3155 | `	int i;` |
|        27 | 3156 | `	if( nArg < 1 ){` |
|       ! 0 | 3157 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3158 | `			"ArgumentCountError",` |
|         - | 3159 | `			"array_push() expects at least 1 argument, %d given",` |
|       ! 0 | 3160 | `			nArg` |
|         - | 3161 | `			);` |
|         - | 3162 | `	}` |
|         - | 3163 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3164 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        27 | 3165 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3166 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3167 | `			"Error",` |
|         - | 3168 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3169 | `			);` |
|         - | 3170 | `	}` |
|         - | 3171 | `	/* Make sure we are dealing with a valid hashmap */` |
|        21 | 3172 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3173 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3174 | `			"TypeError",` |
|         - | 3175 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3176 | `			ph7_type_name(apArg[0])` |
|         - | 3177 | `			);` |
|         - | 3178 | `	}` |
|         - | 3179 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3180 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3181 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3182 | `	/* Start pushing given values */` |
|        34 | 3183 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3184 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3185 | `		if( rc != SXRET_OK ){` |
|         3 | 3186 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3187 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3188 | `				return rc;` |
|         - | 3189 | `			}` |
|       ! 0 | 3190 | `			break;` |
|         - | 3191 | `		}` |
|         9 | 3192 | `	}` |
|         - | 3193 | `	/* Return the new count */` |
|        15 | 3194 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3195 | `	return PH7_OK;` |
|        16 | 3196 | `}` |
|         - | 3197 | `/*` |
|         - | 3198 | ` * value array_shift(array $array)` |
|         - | 3199 | ` *   Shift an element off the beginning of array.` |
|         - | 3200 | ` * Parameter` |
|         - | 3201 | ` *  The array to get the value from.` |
|         - | 3202 | ` * Return` |
|         - | 3203 | ` *  Shifted value or NULL on failure.` |
|         - | 3204 | ` */` |
|        44 | 3205 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3206 | `{` |
|         - | 3207 | `	ph7_hashmap *pMap;` |
|         - | 3208 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        49 | 3209 | `	if( nArg != 1 ){` |
|         4 | 3210 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3211 | `			"ArgumentCountError",` |
|         - | 3212 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         1 | 3213 | `			nArg` |
|         - | 3214 | `			);` |
|         - | 3215 | `	}` |
|         - | 3216 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        47 | 3217 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3218 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3219 | `			"Error",` |
|         - | 3220 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3221 | `			);` |
|         - | 3222 | `	}` |
|         - | 3223 | `	/* Make sure we are dealing with a valid hashmap */` |
|        43 | 3224 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3225 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3226 | `			"TypeError",` |
|         - | 3227 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3228 | `			ph7_type_name(apArg[0])` |
|         - | 3229 | `			);` |
|         - | 3230 | `	}` |
|         - | 3231 | `	/* Point to the internal representation of the hashmap */` |
|        41 | 3232 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        41 | 3233 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 3234 | `	if( pMap->nEntry < 1 ){` |
|         - | 3235 | `		/* Empty hashmap,return NULL */` |
|         3 | 3236 | `		ph7_result_null(pCtx);` |
|         2 | 3237 | `	}else{` |
|        39 | 3238 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3239 | `		ph7_value *pObj;` |
|         - | 3240 | `		sxu32 n;` |
|        39 | 3241 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        39 | 3242 | `		if( pObj ){` |
|         - | 3243 | `			/* Node value */` |
|        39 | 3244 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3245 | `			/* Unlink the first node */` |
|        39 | 3246 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        22 | 3247 | `		}else{` |
|       ! 0 | 3248 | `			ph7_result_null(pCtx);` |
|         - | 3249 | `		}` |
|         - | 3250 | `		/* Rehash all int keys */` |
|        39 | 3251 | `		n = pMap->nEntry;` |
|        39 | 3252 | `		pEntry = pMap->pFirst;` |
|        39 | 3253 | `		pMap->iNextIdx = 0;` |
|        39 | 3254 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|        47 | 3255 | `		for(;;){` |
|        99 | 3256 | `			if( n < 1 ){` |
|        39 | 3257 | `				break;` |
|         - | 3258 | `			}` |
|        65 | 3259 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        65 | 3260 | `				HashmapRehashIntNode(pEntry);` |
|        30 | 3261 | `			}` |
|         - | 3262 | `			/* Point to the next entry */` |
|        65 | 3263 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        65 | 3264 | `			n--;` |
|         5 | 3265 | `		}` |
|         - | 3266 | `		/* Reset the cursor */` |
|        39 | 3267 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3268 | `	}` |
|        41 | 3269 | `	return PH7_OK;` |
|        27 | 3270 | `}` |
|         - | 3271 | `/*` |
|         - | 3272 | ` * Extract the node cursor value.` |
|         - | 3273 | ` */` |
|      1094 | 3274 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3275 | `{` |
|      1095 | 3276 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3277 | `	ph7_value *pVal;` |
|      1095 | 3278 | `	if( pCur == 0 ){` |
|         - | 3279 | `		/* Cursor does not point to anything,return FALSE */` |
|        39 | 3280 | `		ph7_result_bool(pCtx,0);` |
|        39 | 3281 | `		return PH7_OK;` |
|         - | 3282 | `	}` |
|      1057 | 3283 | `	if( iDirection != 0 ){` |
|       201 | 3284 | `		if( iDirection > 0 ){` |
|         - | 3285 | `			/* Point to the next entry */` |
|       199 | 3286 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       199 | 3287 | `			pCur = pMap->pCur;` |
|       100 | 3288 | `		}else{` |
|         - | 3289 | `			/* Point to the previous entry */` |
|         3 | 3290 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3291 | `			pCur = pMap->pCur;` |
|         - | 3292 | `		}` |
|       201 | 3293 | `		if( pCur == 0 ){` |
|         - | 3294 | `			/* End of input reached,return FALSE */` |
|        83 | 3295 | `			ph7_result_bool(pCtx,0);` |
|        83 | 3296 | `			return PH7_OK;` |
|         - | 3297 | `		}` |
|        59 | 3298 | `	}` |
|         - | 3299 | `	/* Point to the desired element */` |
|       975 | 3300 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       975 | 3301 | `	if( pVal ){` |
|       975 | 3302 | `		ph7_result_value(pCtx,pVal);` |
|       488 | 3303 | `	}else{` |
|       ! 0 | 3304 | `		ph7_result_bool(pCtx,0);` |
|         - | 3305 | `	}` |
|       975 | 3306 | `	return PH7_OK;` |
|       548 | 3307 | `}` |
|         - | 3308 | `/*` |
|         - | 3309 | ` * value current(array $array)` |
|         - | 3310 | ` *  Return the current element in an array.` |
|         - | 3311 | ` * Parameter` |
|         - | 3312 | ` *  $input: The input array.` |
|         - | 3313 | ` * Return` |
|         - | 3314 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3315 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3316 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3317 | ` *  is empty, current() returns FALSE.` |
|         - | 3318 | ` */` |
|       302 | 3319 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3320 | `{` |
|       303 | 3321 | `	if( nArg < 1 ){` |
|         - | 3322 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3323 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3324 | `		return PH7_OK;` |
|         - | 3325 | `	}` |
|         - | 3326 | `	/* Make sure we are dealing with a valid hashmap */` |
|       303 | 3327 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3328 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3329 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3330 | `		return PH7_OK;` |
|         - | 3331 | `	}` |
|       303 | 3332 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|       303 | 3333 | `	return PH7_OK;` |
|       152 | 3334 | `}` |
|         - | 3335 | `/*` |
|         - | 3336 | ` * value next(array $input)` |
|         - | 3337 | ` *  Advance the internal array pointer of an array.` |
|         - | 3338 | ` * Parameter` |
|         - | 3339 | ` *  $input: The input array.` |
|         - | 3340 | ` * Return` |
|         - | 3341 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3342 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3343 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3344 | ` */` |
|       198 | 3345 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3346 | `{` |
|       199 | 3347 | `	if( nArg < 1 ){` |
|         - | 3348 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3349 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3350 | `		return PH7_OK;` |
|         - | 3351 | `	}` |
|         - | 3352 | `	/* Make sure we are dealing with a valid hashmap */` |
|       199 | 3353 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3354 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3355 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3356 | `		return PH7_OK;` |
|         - | 3357 | `	}` |
|       199 | 3358 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       199 | 3359 | `	return PH7_OK;` |
|       100 | 3360 | `}` |
|         - | 3361 | `/*` |
|         - | 3362 | ` * value prev(array $input)` |
|         - | 3363 | ` *  Rewind the internal array pointer.` |
|         - | 3364 | ` * Parameter` |
|         - | 3365 | ` *  $input: The input array.` |
|         - | 3366 | ` * Return` |
|         - | 3367 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3368 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3369 | ` *  elements.` |
|         - | 3370 | ` */` |
|         2 | 3371 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3372 | `{` |
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
|         3 | 3384 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3385 | `	return PH7_OK;` |
|         2 | 3386 | `}` |
|         - | 3387 | `/*` |
|         - | 3388 | ` * value end(array $input)` |
|         - | 3389 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3390 | ` * Parameter` |
|         - | 3391 | ` *  $input: The input array.` |
|         - | 3392 | ` * Return` |
|         - | 3393 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3394 | ` */` |
|       348 | 3395 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3396 | `{` |
|         - | 3397 | `	ph7_hashmap *pMap;` |
|       349 | 3398 | `	if( nArg < 1 ){` |
|         - | 3399 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3400 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3401 | `		return PH7_OK;` |
|         - | 3402 | `	}` |
|         - | 3403 | `	/* Make sure we are dealing with a valid hashmap */` |
|       349 | 3404 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3405 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3406 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3407 | `		return PH7_OK;` |
|         - | 3408 | `	}` |
|         - | 3409 | `	/* Point to the internal representation of the input hashmap */` |
|       349 | 3410 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3411 | `	/* Point to the last node */` |
|       349 | 3412 | `	pMap->pCur = pMap->pLast;` |
|         - | 3413 | `	/* Return the last node value */` |
|       349 | 3414 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       349 | 3415 | `	return PH7_OK;` |
|       175 | 3416 | `}` |
|         - | 3417 | `/*` |
|         - | 3418 | ` * value reset(array $array )` |
|         - | 3419 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3420 | ` * Parameter` |
|         - | 3421 | ` *  $input: The input array.` |
|         - | 3422 | ` * Return` |
|         - | 3423 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3424 | ` */` |
|       244 | 3425 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3426 | `{` |
|         - | 3427 | `	ph7_hashmap *pMap;` |
|       245 | 3428 | `	if( nArg < 1 ){` |
|         - | 3429 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3430 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3431 | `		return PH7_OK;` |
|         - | 3432 | `	}` |
|         - | 3433 | `	/* Make sure we are dealing with a valid hashmap */` |
|       245 | 3434 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3435 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3436 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3437 | `		return PH7_OK;` |
|         - | 3438 | `	}` |
|         - | 3439 | `	/* Point to the internal representation of the input hashmap */` |
|       245 | 3440 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3441 | `	/* Point to the first node */` |
|       245 | 3442 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3443 | `	/* Return the last node value if available */` |
|       245 | 3444 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       245 | 3445 | `	return PH7_OK;` |
|       123 | 3446 | `}` |
|         - | 3447 | `/*` |
|         - | 3448 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3449 | ` * array_key_first() and array_key_last().` |
|         - | 3450 | ` */` |
|       672 | 3451 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3452 | `{` |
|       673 | 3453 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3454 | `		/* Key is integer */` |
|       283 | 3455 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       142 | 3456 | `	}else{` |
|         - | 3457 | `		/* Key is blob */` |
|       586 | 3458 | `		ph7_result_string(pCtx,` |
|       390 | 3459 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3460 | `	}` |
|       673 | 3461 | `}` |
|         - | 3462 | `/*` |
|         - | 3463 | ` * value key(array $array)` |
|         - | 3464 | ` *   Fetch a key from an array` |
|         - | 3465 | ` * Parameter` |
|         - | 3466 | ` *  $input` |
|         - | 3467 | ` *   The input array.` |
|         - | 3468 | ` * Return` |
|         - | 3469 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3470 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3471 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3472 | ` *  is empty, key() returns NULL.` |
|         - | 3473 | ` */` |
|       776 | 3474 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3475 | `{` |
|         - | 3476 | `	ph7_hashmap_node *pCur;` |
|         - | 3477 | `	ph7_hashmap *pMap;` |
|       777 | 3478 | `	if( nArg < 1 ){` |
|         - | 3479 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3480 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3481 | `		return PH7_OK;` |
|         - | 3482 | `	}` |
|         - | 3483 | `	/* Make sure we are dealing with a valid hashmap */` |
|       777 | 3484 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3485 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3486 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3487 | `		return PH7_OK;` |
|         - | 3488 | `	}` |
|       777 | 3489 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       777 | 3490 | `	pCur = pMap->pCur;` |
|       777 | 3491 | `	if( pCur == 0 ){` |
|         - | 3492 | `		/* Cursor does not point to anything,return NULL */` |
|       121 | 3493 | `		ph7_result_null(pCtx);` |
|       121 | 3494 | `		return PH7_OK;` |
|         - | 3495 | `	}` |
|       657 | 3496 | `	HashmapResultNodeKey(pCtx,pCur);` |
|       657 | 3497 | `	return PH7_OK;` |
|       389 | 3498 | `}` |
|         - | 3499 | `/*` |
|         - | 3500 | ` * array each(array $input)` |
|         - | 3501 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3502 | ` * Parameter` |
|         - | 3503 | ` *  $input` |
|         - | 3504 | ` *    The input array.` |
|         - | 3505 | ` * Return` |
|         - | 3506 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3507 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3508 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3509 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3510 | ` *  each() returns FALSE.` |
|         - | 3511 | ` */` |
|        22 | 3512 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3513 | `{` |
|         - | 3514 | `	ph7_hashmap_node *pCur;` |
|         - | 3515 | `	ph7_hashmap *pMap;` |
|         - | 3516 | `	ph7_value *pArray;` |
|         - | 3517 | `	ph7_value *pVal;` |
|         - | 3518 | `	ph7_value sKey;` |
|        23 | 3519 | `	if( nArg < 1 ){` |
|         - | 3520 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3521 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3522 | `		return PH7_OK;` |
|         - | 3523 | `	}` |
|         - | 3524 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3525 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3526 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3527 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3528 | `		return PH7_OK;` |
|         - | 3529 | `	}` |
|         - | 3530 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3531 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3532 | `	if( pMap->pCur == 0 ){` |
|         - | 3533 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3534 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3535 | `		return PH7_OK;` |
|         - | 3536 | `	}` |
|        15 | 3537 | `	pCur = pMap->pCur;` |
|         - | 3538 | `	/* Create a new array */` |
|        15 | 3539 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3540 | `	if( pArray == 0 ){` |
|       ! 0 | 3541 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3542 | `		return PH7_OK;` |
|         - | 3543 | `	}` |
|        15 | 3544 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3545 | `	/* Insert the current value */` |
|        15 | 3546 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3547 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3548 | `	/* Make the key */` |
|        15 | 3549 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3550 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3551 | `	}else{` |
|         9 | 3552 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3553 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3554 | `	}` |
|         - | 3555 | `	/* Insert the current key */` |
|        15 | 3556 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3557 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3558 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3559 | `	/* Advance the cursor */` |
|        15 | 3560 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3561 | `	/* Return the current entry */` |
|        15 | 3562 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3563 | `	return PH7_OK;` |
|        12 | 3564 | `}` |
|         - | 3565 | `/*` |
|         - | 3566 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3567 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3568 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3569 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3570 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3571 | ` */` |
|         - | 3572 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3573 | `/*` |
|         - | 3574 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3575 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3576 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3577 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3578 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3579 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3580 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3581 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3582 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3583 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3584 | ` */` |
|         - | 3585 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3586 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3587 | `/*` |
|         - | 3588 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3589 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3590 | ` */` |
|       ! 0 | 3591 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       ! 0 | 3592 | `{` |
|       ! 0 | 3593 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 3594 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       ! 0 | 3595 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       ! 0 | 3596 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       ! 0 | 3597 | `		zBuf[n] = 0;` |
|       ! 0 | 3598 | `		return zBuf;` |
|         - | 3599 | `	}` |
|       ! 0 | 3600 | `	return ph7_type_name(pVal);` |
|       ! 0 | 3601 | `}` |
|         - | 3602 | `/*` |
|         - | 3603 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3604 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3605 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3606 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3607 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3608 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3609 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3610 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3611 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3612 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3613 | ` */` |
|       156 | 3614 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3615 | `{` |
|       157 | 3616 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3617 | `	sxu64 uVal = 0;` |
|       157 | 3618 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3619 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3620 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3621 | `		bNeg = (z[0] == '-');` |
|         3 | 3622 | `		z++;` |
|         1 | 3623 | `	}` |
|       237 | 3624 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3625 | `		int d = z[0] - '0';` |
|         - | 3626 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3627 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3628 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3629 | `			bOverflow = 1;` |
|       ! 0 | 3630 | `		}else{` |
|        81 | 3631 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3632 | `		}` |
|        81 | 3633 | `		bDigit = 1;` |
|        81 | 3634 | `		z++;` |
|         1 | 3635 | `	}` |
|       157 | 3636 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3637 | `		bReal = 1;` |
|         3 | 3638 | `		z++;` |
|         5 | 3639 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3640 | `			bDigit = 1;` |
|         3 | 3641 | `			z++;` |
|         1 | 3642 | `		}` |
|         1 | 3643 | `	}` |
|         - | 3644 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3645 | `	if( !bDigit ){` |
|        61 | 3646 | `		return RANGE_IN_ERROR;` |
|         - | 3647 | `	}` |
|         - | 3648 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3649 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3650 | `		z++;` |
|         9 | 3651 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3652 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3653 | `			return RANGE_IN_ERROR;` |
|         - | 3654 | `		}` |
|         9 | 3655 | `		bReal = 1;` |
|        17 | 3656 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3657 | `	}` |
|         - | 3658 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3659 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3660 | `	if( z != zEnd ){` |
|        13 | 3661 | `		return RANGE_IN_ERROR;` |
|         - | 3662 | `	}` |
|        84 | 3663 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3664 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3665 | `		bReal = 1;` |
|        84 | 3666 | `	}` |
|        43 | 3667 | `	if( bReal ){` |
|        11 | 3668 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3669 | `		return RANGE_IN_DOUBLE;` |
|         - | 3670 | `	}` |
|         - | 3671 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3672 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3673 | `	return RANGE_IN_LONG;` |
|        58 | 3674 | `}` |
|         - | 3675 | `/*` |
|         - | 3676 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3677 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3678 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3679 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3680 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3681 | ` */` |
|       328 | 3682 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3683 | `{` |
|         - | 3684 | `	char zMsg[160];` |
|       329 | 3685 | `	*pRc = PH7_OK;` |
|       329 | 3686 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3687 | `		char zType[80];` |
|       ! 0 | 3688 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3689 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       ! 0 | 3690 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3691 | `		return FALSE;` |
|         - | 3692 | `	}` |
|       329 | 3693 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3694 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3695 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3696 | `			iArg,zName);` |
|         5 | 3697 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3698 | `		*pbNullCoerced = TRUE;` |
|         2 | 3699 | `	}` |
|       329 | 3700 | `	return TRUE;` |
|       165 | 3701 | `}` |
|         - | 3702 | `/*` |
|         - | 3703 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3704 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3705 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3706 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3707 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3708 | ` */` |
|        60 | 3709 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3710 | `{` |
|        61 | 3711 | `	*pRc = PH7_OK;` |
|        61 | 3712 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3713 | `		char zType[80];` |
|       ! 0 | 3714 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3715 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       ! 0 | 3716 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       ! 0 | 3717 | `		return RANGE_IN_ERROR;` |
|         - | 3718 | `	}` |
|        61 | 3719 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3720 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3721 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3722 | `		*pLong = 0;` |
|         3 | 3723 | `		return RANGE_IN_LONG;` |
|         - | 3724 | `	}` |
|        59 | 3725 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3726 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3727 | `		return RANGE_IN_DOUBLE;` |
|         - | 3728 | `	}` |
|        35 | 3729 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3730 | `		const char *zStr;` |
|         - | 3731 | `		int nLen;` |
|         - | 3732 | `		sxu8 iKind;` |
|         3 | 3733 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3734 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3735 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3736 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3737 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3738 | `		}` |
|         3 | 3739 | `		return iKind;` |
|         - | 3740 | `	}` |
|         - | 3741 | `	/* int / bool */` |
|        33 | 3742 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3743 | `	return RANGE_IN_LONG;` |
|        31 | 3744 | `}` |
|         - | 3745 | `/*` |
|         - | 3746 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3747 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3748 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3749 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3750 | ` */` |
|       296 | 3751 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3752 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3753 | `{` |
|         - | 3754 | `	char zMsg[160];` |
|         - | 3755 | `	double r;` |
|       297 | 3756 | `	*pRc = PH7_OK;` |
|       297 | 3757 | `	if( bNullCoerced ){` |
|         - | 3758 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3759 | `		*pLong = 0;` |
|         5 | 3760 | `		*pDouble = 0.0;` |
|         5 | 3761 | `		return RANGE_IN_LONG;` |
|         - | 3762 | `	}` |
|       293 | 3763 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3764 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3765 | `check_dval:` |
|        25 | 3766 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3767 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3768 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3769 | `			return RANGE_IN_ERROR;` |
|         - | 3770 | `		}` |
|        21 | 3771 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3772 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3773 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3774 | `			return RANGE_IN_ERROR;` |
|         - | 3775 | `		}` |
|        17 | 3776 | `		*pDouble = r;` |
|        17 | 3777 | `		return RANGE_IN_DOUBLE;` |
|         - | 3778 | `	}` |
|       273 | 3779 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3780 | `		const char *zStr;` |
|         - | 3781 | `		int nLen;` |
|         - | 3782 | `		sxu8 iKind;` |
|        81 | 3783 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3784 | `		if( nLen == 0 ){` |
|         7 | 3785 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3786 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3787 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3788 | `			*pLong = 0;` |
|         5 | 3789 | `			*pDouble = 0.0;` |
|        41 | 3790 | `			return RANGE_IN_LONG;` |
|         - | 3791 | `		}` |
|        77 | 3792 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3793 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3794 | `			r = *pDouble;` |
|         5 | 3795 | `			goto check_dval;` |
|         - | 3796 | `		}` |
|        73 | 3797 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3798 | `			*pDouble = (double)*pLong;` |
|        23 | 3799 | `			if( nLen == 1 ){` |
|         - | 3800 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3801 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3802 | `				return RANGE_IN_DIGIT;` |
|         - | 3803 | `			}` |
|        15 | 3804 | `			return RANGE_IN_LONG;` |
|         - | 3805 | `		}` |
|        51 | 3806 | `		if( nLen != 1 ){` |
|        10 | 3807 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3808 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3809 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3810 | `		}` |
|        51 | 3811 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3812 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3813 | `		*pLong = 0;` |
|        51 | 3814 | `		*pDouble = 0.0;` |
|        51 | 3815 | `		return RANGE_IN_STRING;` |
|         - | 3816 | `	}` |
|         - | 3817 | `	/* int / bool */` |
|       193 | 3818 | `	*pLong = ph7_value_to_int64(pIn);` |
|       193 | 3819 | `	*pDouble = (double)*pLong;` |
|       193 | 3820 | `	return RANGE_IN_LONG;` |
|       149 | 3821 | `}` |
|         - | 3822 | `/*` |
|         - | 3823 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3824 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3825 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3826 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3827 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3828 | ` * exactly like php's two macros.` |
|         - | 3829 | ` */` |
|         6 | 3830 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3831 | `{` |
|        10 | 3832 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3833 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3834 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3835 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3836 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3837 | `}` |
|         6 | 3838 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3839 | `{` |
|         - | 3840 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3841 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3842 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3843 | `	const unsigned int nBuf = 1500;` |
|         7 | 3844 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3845 | `	if( zMsg == 0 ){` |
|       ! 0 | 3846 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3847 | `	}` |
|         7 | 3848 | `	snprintf(zMsg,nBuf,` |
|         - | 3849 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3850 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3851 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3852 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3853 | `}` |
|         - | 3854 | `/*` |
|         - | 3855 | ` * Set the element container to the next range element and append it to the` |
|         - | 3856 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3857 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3858 | ` * below stay one line per iteration.` |
|         - | 3859 | ` */` |
|      1680 | 3860 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3861 | `{` |
|      1681 | 3862 | `	ph7_value_int64(pValue,iVal);` |
|      1681 | 3863 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3864 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3865 | `	}` |
|      1681 | 3866 | `	return PH7_OK;` |
|       841 | 3867 | `}` |
|        70 | 3868 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3869 | `{` |
|        71 | 3870 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3871 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3872 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3873 | `	}` |
|        71 | 3874 | `	return PH7_OK;` |
|        36 | 3875 | `}` |
|       168 | 3876 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3877 | `{` |
|       169 | 3878 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3879 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3880 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3881 | `	}` |
|       169 | 3882 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3883 | `	return PH7_OK;` |
|        85 | 3884 | `}` |
|         - | 3885 | `/*` |
|         - | 3886 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3887 | ` *  Create an array containing a range of elements.` |
|         - | 3888 | ` * Return` |
|         - | 3889 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3890 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3891 | ` */` |
|       166 | 3892 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3893 | `{` |
|         - | 3894 | `	ph7_value *pValue,*pArray;` |
|       167 | 3895 | `	sxi32 rc = PH7_OK;` |
|       167 | 3896 | `	int is_step_double = 0,is_step_negative = 0;` |
|       167 | 3897 | `	double step_double = 1.0;` |
|       167 | 3898 | `	sxi64 step = 1;` |
|         - | 3899 | `	sxu8 start_type,end_type;` |
|       167 | 3900 | `	sxi64 start_long = 0,end_long = 0;` |
|       167 | 3901 | `	double start_double = 0.0,end_double = 0.0;` |
|       167 | 3902 | `	unsigned char cStart = 0,cEnd = 0;` |
|       167 | 3903 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3904 | `	sxu32 i,size;` |
|         - | 3905 |  |
|         - | 3906 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       167 | 3907 | `	if( nArg > 3 ){` |
|         4 | 3908 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3909 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3910 | `	}` |
|       165 | 3911 | `	if( nArg < 2 ){` |
|         - | 3912 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3913 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3914 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3915 | `	}` |
|         - | 3916 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3917 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       165 | 3918 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       ! 0 | 3919 | `		return rc;` |
|         - | 3920 | `	}` |
|       165 | 3921 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3922 | `		return rc;` |
|         - | 3923 | `	}` |
|       165 | 3924 | `	if( nArg > 2 ){` |
|        61 | 3925 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        61 | 3926 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         3 | 3927 | `			return rc;` |
|         - | 3928 | `		}` |
|        59 | 3929 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3930 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3931 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3932 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3933 | `			}` |
|        23 | 3934 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3935 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3936 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3937 | `			}` |
|         - | 3938 | `			/* We only want positive step values. */` |
|        21 | 3939 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3940 | `				is_step_negative = 1;` |
|       ! 0 | 3941 | `				step_double *= -1;` |
|       ! 0 | 3942 | `			}` |
|         - | 3943 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3944 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3945 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3946 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3947 | `				step = (sxi64)step_double;` |
|        19 | 3948 | `				if( (double)step != step_double ){` |
|        17 | 3949 | `					is_step_double = 1;` |
|         8 | 3950 | `				}` |
|        10 | 3951 | `			}else{` |
|         - | 3952 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3953 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3954 | `				is_step_double = 1;` |
|         - | 3955 | `			}` |
|        11 | 3956 | `		}else{` |
|         - | 3957 | `			/* We only want positive step values. */` |
|        35 | 3958 | `			if( step < 0 ){` |
|        11 | 3959 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3960 | `					/* -step would overflow */` |
|         4 | 3961 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3962 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3963 | `				}` |
|         9 | 3964 | `				is_step_negative = 1;` |
|         9 | 3965 | `				step = -step;` |
|         4 | 3966 | `			}` |
|        33 | 3967 | `			step_double = (double)step;` |
|         - | 3968 | `		}` |
|        53 | 3969 | `		if( step_double == 0.0 ){` |
|         7 | 3970 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3971 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3972 | `		}` |
|        23 | 3973 | `	}` |
|       151 | 3974 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       151 | 3975 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3976 | `		return rc;` |
|         - | 3977 | `	}` |
|       147 | 3978 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       147 | 3979 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3980 | `		return rc;` |
|         - | 3981 | `	}` |
|         - | 3982 | `	/* Element container + result array */` |
|       143 | 3983 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       143 | 3984 | `	pArray = ph7_context_new_array(pCtx);` |
|       143 | 3985 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3986 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3987 | `	}` |
|         - | 3988 | `	/* If the range is given as strings, generate an array of characters. */` |
|       143 | 3989 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3990 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3991 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3992 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3993 | `			 * and the range is numeric. */` |
|        15 | 3994 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3995 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3996 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3997 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3998 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3999 | `				}` |
|         7 | 4000 | `				end_type = RANGE_IN_LONG;` |
|         4 | 4001 | `			}else{` |
|         9 | 4002 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 4003 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4004 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 4005 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 4006 | `				}` |
|         9 | 4007 | `				start_type = RANGE_IN_LONG;` |
|         - | 4008 | `			}` |
|        15 | 4009 | `			goto handle_numeric_inputs;` |
|         - | 4010 | `		}` |
|        23 | 4011 | `		if( is_step_double ){` |
|         - | 4012 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 4013 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 4014 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 4015 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 4016 | `					" of characters, inputs converted to 0");` |
|         1 | 4017 | `			}` |
|         5 | 4018 | `			start_type = RANGE_IN_LONG;` |
|         5 | 4019 | `			end_type = RANGE_IN_LONG;` |
|         5 | 4020 | `			goto handle_numeric_inputs;` |
|         - | 4021 | `		}` |
|         - | 4022 | `		/* Generate an array of characters */` |
|        19 | 4023 | `		if( cStart > cEnd ){` |
|         - | 4024 | `			/* Decreasing char range */` |
|         - | 4025 | `			int iCur;` |
|         3 | 4026 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 4027 | `				goto boundary_error;` |
|         - | 4028 | `			}` |
|        17 | 4029 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 4030 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4031 | `					return rc;` |
|         - | 4032 | `				}` |
|         8 | 4033 | `			}` |
|        18 | 4034 | `		}else if( cEnd > cStart ){` |
|         - | 4035 | `			/* Increasing char range */` |
|         - | 4036 | `			int iCur;` |
|        15 | 4037 | `			if( is_step_negative ){` |
|         3 | 4038 | `				goto negative_step_error;` |
|         - | 4039 | `			}` |
|        13 | 4040 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 4041 | `				goto boundary_error;` |
|         - | 4042 | `			}` |
|       163 | 4043 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 4044 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 4045 | `					return rc;` |
|         - | 4046 | `				}` |
|        77 | 4047 | `			}` |
|         6 | 4048 | `		}else{` |
|         3 | 4049 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 4050 | `				return rc;` |
|         - | 4051 | `			}` |
|         - | 4052 | `		}` |
|        15 | 4053 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 4054 | `		return PH7_OK;` |
|         - | 4055 | `	}` |
|        53 | 4056 | `handle_numeric_inputs:` |
|       133 | 4057 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 4058 | `		/* Float range */` |
|         - | 4059 | `		double elem,calc;` |
|        25 | 4060 | `		if( start_double > end_double ){` |
|         - | 4061 | `			/* Decreasing float range */` |
|         7 | 4062 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 4063 | `				goto boundary_error;` |
|         - | 4064 | `			}` |
|         7 | 4065 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 4066 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 4067 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 4068 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 4069 | `			}` |
|         5 | 4070 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 4071 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 4072 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4073 | `					return rc;` |
|         - | 4074 | `				}` |
|         8 | 4075 | `			}` |
|        21 | 4076 | `		}else if( end_double > start_double ){` |
|         - | 4077 | `			/* Increasing float range */` |
|        17 | 4078 | `			if( is_step_negative ){` |
|       ! 0 | 4079 | `				goto negative_step_error;` |
|         - | 4080 | `			}` |
|        17 | 4081 | `			if( end_double - start_double < step_double ){` |
|         3 | 4082 | `				goto boundary_error;` |
|         - | 4083 | `			}` |
|        15 | 4084 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 4085 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 4086 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 4087 | `			}` |
|        11 | 4088 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 4089 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 4090 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 4091 | `					return rc;` |
|         - | 4092 | `				}` |
|        28 | 4093 | `			}` |
|         6 | 4094 | `		}else{` |
|         3 | 4095 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 4096 | `				return rc;` |
|         - | 4097 | `			}` |
|         - | 4098 | `		}` |
|         9 | 4099 | `	}else{` |
|         - | 4100 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4101 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4102 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|       101 | 4103 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4104 | `		sxu64 calc;` |
|       101 | 4105 | `		if( start_long > end_long ){` |
|         - | 4106 | `			/* Decreasing int range */` |
|        19 | 4107 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4108 | `				goto boundary_error;` |
|         - | 4109 | `			}` |
|        17 | 4110 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4111 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4112 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4113 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4114 | `			}` |
|        15 | 4115 | `			size = (sxu32)(calc + 1);` |
|       101 | 4116 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4117 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4118 | `					return rc;` |
|         - | 4119 | `				}` |
|        44 | 4120 | `			}` |
|        90 | 4121 | `		}else if( end_long > start_long ){` |
|         - | 4122 | `			/* Increasing int range */` |
|        77 | 4123 | `			if( is_step_negative ){` |
|         3 | 4124 | `				goto negative_step_error;` |
|         - | 4125 | `			}` |
|        75 | 4126 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4127 | `				goto boundary_error;` |
|         - | 4128 | `			}` |
|        73 | 4129 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        73 | 4130 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4131 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4132 | `			}` |
|        69 | 4133 | `			size = (sxu32)(calc + 1);` |
|      1657 | 4134 | `			for( i = 0 ; i < size ; ++i ){` |
|      1589 | 4135 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4136 | `					return rc;` |
|         - | 4137 | `				}` |
|       795 | 4138 | `			}` |
|        35 | 4139 | `		}else{` |
|         7 | 4140 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4141 | `				return rc;` |
|         - | 4142 | `			}` |
|         - | 4143 | `		}` |
|         - | 4144 | `	}` |
|         - | 4145 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4146 | `	 * virtual machine as soon as we return from this foreign function. */` |
|       105 | 4147 | `	ph7_result_value(pCtx,pArray);` |
|       105 | 4148 | `	return PH7_OK;` |
|         2 | 4149 | `negative_step_error:` |
|         5 | 4150 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4151 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4152 | `boundary_error:` |
|         9 | 4153 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4154 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        84 | 4155 | `}` |
|         - | 4156 | `/*` |
|         - | 4157 | ` * array array_values(array $array)` |
|         - | 4158 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4159 | ` * Parameters` |
|         - | 4160 | ` *  $array` |
|         - | 4161 | ` *   The input array.` |
|         - | 4162 | ` * Return` |
|         - | 4163 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4164 | ` */` |
|        48 | 4165 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4166 | `{` |
|         - | 4167 | `	ph7_hashmap_node *pNode;` |
|         - | 4168 | `	ph7_hashmap *pMap;` |
|         - | 4169 | `	ph7_value *pArray;` |
|         - | 4170 | `	ph7_value *pObj;` |
|         - | 4171 | `	sxu32 n;` |
|        51 | 4172 | `	if( nArg != 1 ){` |
|         - | 4173 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         4 | 4174 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4175 | `			"ArgumentCountError",` |
|         - | 4176 | `			"array_values() expects exactly 1 argument, %d given",` |
|         1 | 4177 | `			nArg` |
|         - | 4178 | `			);` |
|         - | 4179 | `	}` |
|         - | 4180 | `	/* Make sure we are dealing with a valid hashmap */` |
|        48 | 4181 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4182 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4183 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4184 | `			"TypeError",` |
|         - | 4185 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4186 | `			ph7_type_name(apArg[0])` |
|         - | 4187 | `			);` |
|         - | 4188 | `	}` |
|         - | 4189 | `	/* Point to the internal representation that describe the input hashmap */` |
|        46 | 4190 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4191 | `	/* Create a new array */` |
|        46 | 4192 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 4193 | `	if( pArray == 0 ){` |
|       ! 0 | 4194 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4195 | `		return PH7_OK;` |
|         - | 4196 | `	}` |
|         - | 4197 | `	/* Perform the requested operation */` |
|        46 | 4198 | `	pNode = pMap->pFirst;` |
|       144 | 4199 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       100 | 4200 | `		pObj = HashmapExtractNodeValue(pNode);` |
|       100 | 4201 | `		if( pObj ){` |
|         - | 4202 | `			/* perform the insertion */` |
|       100 | 4203 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        49 | 4204 | `		}` |
|         - | 4205 | `		/* Point to the next entry */` |
|       100 | 4206 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        51 | 4207 | `	}` |
|         - | 4208 | `	/* return the new array */` |
|        46 | 4209 | `	ph7_result_value(pCtx,pArray);` |
|        46 | 4210 | `	return PH7_OK;` |
|        27 | 4211 | `}` |
|         - | 4212 | `/*` |
|         - | 4213 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4214 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4215 | ` * Parameters` |
|         - | 4216 | ` *  $input` |
|         - | 4217 | ` *   An array containing keys to return.` |
|         - | 4218 | ` * $search_value` |
|         - | 4219 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4220 | ` * $strict` |
|         - | 4221 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4222 | ` * Return` |
|         - | 4223 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4224 | ` */` |
|       160 | 4225 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4226 | `{` |
|         - | 4227 | `	ph7_hashmap_node *pNode;` |
|         - | 4228 | `	ph7_hashmap *pMap;` |
|         - | 4229 | `	ph7_value *pArray;` |
|         - | 4230 | `	ph7_value sObj;` |
|         - | 4231 | `	ph7_value sVal;` |
|         - | 4232 | `	SyString sKey;` |
|         - | 4233 | `	int bStrict;` |
|         - | 4234 | `	sxi32 rc;` |
|         - | 4235 | `	sxu32 n;` |
|       164 | 4236 | `	if( nArg < 1 ){` |
|         - | 4237 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4238 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4239 | `			"ArgumentCountError",` |
|         - | 4240 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4241 | `			);` |
|         - | 4242 | `	}` |
|         - | 4243 | `	/* Make sure we are dealing with a valid hashmap */` |
|       164 | 4244 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4245 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4246 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4247 | `			"TypeError",` |
|         - | 4248 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4249 | `			ph7_type_name(apArg[0])` |
|         - | 4250 | `			);` |
|         - | 4251 | `	}` |
|         - | 4252 | `	/* Point to the internal representation of the input hashmap */` |
|       161 | 4253 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4254 | `	/* Create a new array */` |
|       161 | 4255 | `	pArray = ph7_context_new_array(pCtx);` |
|       161 | 4256 | `	if( pArray == 0 ){` |
|       ! 0 | 4257 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4258 | `		return PH7_OK;` |
|         - | 4259 | `	}` |
|       161 | 4260 | `	bStrict = FALSE;` |
|       161 | 4261 | `	if( nArg > 2 ){` |
|         - | 4262 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         9 | 4263 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4264 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4265 | `				"TypeError",` |
|         - | 4266 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4267 | `				ph7_type_name(apArg[2])` |
|         - | 4268 | `				);` |
|         - | 4269 | `		}` |
|         9 | 4270 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4271 | `	}` |
|         - | 4272 | `	/* Perform the requested operation */` |
|       161 | 4273 | `	pNode = pMap->pFirst;` |
|       161 | 4274 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1463 | 4275 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1305 | 4276 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       185 | 4277 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        94 | 4278 | `		}else{` |
|      1122 | 4279 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1122 | 4280 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4281 | `		}` |
|      1305 | 4282 | `		rc = 0;` |
|      1305 | 4283 | `		if( nArg > 1 ){` |
|        65 | 4284 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        65 | 4285 | `			if( pValue ){` |
|         - | 4286 | `				ph7_value sNeedle;` |
|        65 | 4287 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        65 | 4288 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4289 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|         - | 4290 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|         - | 4291 | `				 * mutated on the first element (e.g. null coerced) would` |
|         - | 4292 | `				 * corrupt every later comparison. */` |
|        65 | 4293 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|        65 | 4294 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|        65 | 4295 | `				PH7_MemObjRelease(&sNeedle);` |
|        65 | 4296 | `				PH7_MemObjRelease(&sVal);` |
|        32 | 4297 | `			}` |
|        32 | 4298 | `		}` |
|      1305 | 4299 | `		if( rc == 0 ){` |
|         - | 4300 | `			/* Perform the insertion */` |
|      1273 | 4301 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       635 | 4302 | `		}` |
|      1305 | 4303 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4304 | `		/* Point to the next entry */` |
|      1305 | 4305 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       654 | 4306 | `	}` |
|         - | 4307 | `	/* return the new array */` |
|       161 | 4308 | `	ph7_result_value(pCtx,pArray);` |
|       161 | 4309 | `	return PH7_OK;` |
|        84 | 4310 | `}` |
|         - | 4311 | `/*` |
|         - | 4312 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4313 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4314 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4315 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4316 | ` * Parameters` |
|         - | 4317 | ` *  $arr1` |
|         - | 4318 | ` *   First array` |
|         - | 4319 | ` *  $arr2` |
|         - | 4320 | ` *   Second array` |
|         - | 4321 | ` * Return` |
|         - | 4322 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4323 | ` * Note` |
|         - | 4324 | ` *  This function is a symisc eXtension.` |
|         - | 4325 | ` */` |
|         4 | 4326 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4327 | `{` |
|         - | 4328 | `	ph7_hashmap *p1,*p2;` |
|         - | 4329 | `	int rc;` |
|         5 | 4330 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4331 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4332 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4333 | `		return PH7_OK;` |
|         - | 4334 | `	}` |
|         - | 4335 | `	/* Point to the hashmaps */` |
|         5 | 4336 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4337 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4338 | `	rc = (p1 == p2);` |
|         - | 4339 | `	/* Same instance? */` |
|         5 | 4340 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4341 | `	return PH7_OK;` |
|         3 | 4342 | `}` |
|         - | 4343 | `/*` |
|         - | 4344 | ` * array array_merge(array ...$arrays)` |
|         - | 4345 | ` *  Merge one or more arrays.` |
|         - | 4346 | ` * Parameters` |
|         - | 4347 | ` *  ...$arrays` |
|         - | 4348 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4349 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4350 | ` * Return` |
|         - | 4351 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4352 | ` *  with no arguments.` |
|         - | 4353 | ` */` |
|      1056 | 4354 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4355 | `{` |
|         - | 4356 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4357 | `	ph7_value *pArray;` |
|         - | 4358 | `	int i;` |
|         - | 4359 | `	/* Create a new array */` |
|      1061 | 4360 | `	pArray = ph7_context_new_array(pCtx);` |
|      1061 | 4361 | `	if( pArray == 0 ){` |
|       ! 0 | 4362 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4363 | `		return PH7_OK;` |
|         - | 4364 | `	}` |
|         - | 4365 | `	/* Point to the internal representation of the hashmap */` |
|      1061 | 4366 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4367 | `	/* Start merging */` |
|      3163 | 4368 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4369 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2111 | 4370 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4371 | `			/* Type mismatch -> TypeError */` |
|         8 | 4372 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4373 | `				"TypeError",` |
|         - | 4374 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4375 | `				i + 1,` |
|         4 | 4376 | `				ph7_type_name(apArg[i])` |
|         - | 4377 | `				);` |
|       ! 0 | 4378 | `		}else{` |
|      2107 | 4379 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4380 | `			/* Merge the two hashmaps */` |
|      2107 | 4381 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4382 | `		}` |
|      1056 | 4383 | `	}` |
|         - | 4384 | `	/* Return the freshly created array */` |
|      1057 | 4385 | `	ph7_result_value(pCtx,pArray);` |
|      1057 | 4386 | `	return PH7_OK;` |
|       533 | 4387 | `}` |
|         - | 4388 | `/*` |
|         - | 4389 | ` * array array_copy(array $source)` |
|         - | 4390 | ` *  Make a blind copy of the target array.` |
|         - | 4391 | ` * Parameters` |
|         - | 4392 | ` *  $source` |
|         - | 4393 | ` *   Target array` |
|         - | 4394 | ` * Return` |
|         - | 4395 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4396 | ` * Note` |
|         - | 4397 | ` *  This function is a symisc eXtension.` |
|         - | 4398 | ` */` |
|        18 | 4399 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4400 | `{` |
|         - | 4401 | `	ph7_hashmap *pMap;` |
|         - | 4402 | `	ph7_value *pArray;` |
|        19 | 4403 | `	if( nArg < 1 ){` |
|         - | 4404 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4405 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4406 | `		return PH7_OK;` |
|         - | 4407 | `	}` |
|         - | 4408 | `	/* Create a new array */` |
|        19 | 4409 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 4410 | `	if( pArray == 0 ){` |
|       ! 0 | 4411 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4412 | `		return PH7_OK;` |
|         - | 4413 | `	}` |
|         - | 4414 | `	/* Point to the internal representation of the hashmap */` |
|        19 | 4415 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        19 | 4416 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4417 | `		/* Point to the internal representation of the source */` |
|        19 | 4418 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4419 | `		/* Perform the copy */` |
|        19 | 4420 | `		PH7_HashmapDup(pSrc,pMap);` |
|        10 | 4421 | `	}else{` |
|         - | 4422 | `		/* Simple insertion */` |
|       ! 0 | 4423 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4424 | `	}` |
|         - | 4425 | `	/* Return the duplicated array */` |
|        19 | 4426 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 4427 | `	return PH7_OK;` |
|        10 | 4428 | `}` |
|         - | 4429 | `/*` |
|         - | 4430 | ` * bool array_erase(array $source)` |
|         - | 4431 | ` *  Remove all elements from a given array.` |
|         - | 4432 | ` * Parameters` |
|         - | 4433 | ` *  $source` |
|         - | 4434 | ` *   Target array` |
|         - | 4435 | ` * Return` |
|         - | 4436 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4437 | ` * Note` |
|         - | 4438 | ` *  This function is a symisc eXtension.` |
|         - | 4439 | ` */` |
|        26 | 4440 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4441 | `{` |
|         - | 4442 | `	ph7_hashmap *pMap;` |
|        28 | 4443 | `	if( nArg < 1 ){` |
|         - | 4444 | `		/* Missing arguments */` |
|       ! 0 | 4445 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4446 | `		return PH7_OK;` |
|         - | 4447 | `	}` |
|         - | 4448 | `	/* Point to the target hashmap */` |
|        28 | 4449 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        28 | 4450 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4451 | `	/* Erase */` |
|        28 | 4452 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        28 | 4453 | `	return PH7_OK;` |
|        15 | 4454 | `}` |
|         - | 4455 | `/*` |
|         - | 4456 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4457 | ` *  Extract a slice of the array.` |
|         - | 4458 | ` * Parameters` |
|         - | 4459 | ` *  $array` |
|         - | 4460 | ` *    The input array.` |
|         - | 4461 | ` * $offset` |
|         - | 4462 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4463 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4464 | ` * $length (optional, nullable)` |
|         - | 4465 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4466 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4467 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4468 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4469 | ` * $preserve_keys (optional)` |
|         - | 4470 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4471 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4472 | ` * Return` |
|         - | 4473 | ` *   The new slice.` |
|         - | 4474 | ` */` |
|        46 | 4475 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4476 | `{` |
|         - | 4477 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4478 | `	ph7_hashmap_node *pCur;` |
|         - | 4479 | `	ph7_value *pArray;` |
|         - | 4480 | `	int iLength,iOfft;` |
|         - | 4481 | `	int bPreserve;` |
|         - | 4482 | `	sxi32 rc;` |
|        51 | 4483 | `	if( nArg < 2 ){` |
|       ! 0 | 4484 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4485 | `			"ArgumentCountError",` |
|         - | 4486 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4487 | `			nArg` |
|         - | 4488 | `			);` |
|         - | 4489 | `	}` |
|        51 | 4490 | `	if( nArg > 4 ){` |
|         4 | 4491 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4492 | `			"ArgumentCountError",` |
|         - | 4493 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4494 | `			nArg` |
|         - | 4495 | `			);` |
|         - | 4496 | `	}` |
|        49 | 4497 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4498 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4499 | `			"TypeError",` |
|         - | 4500 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4501 | `			ph7_type_name(apArg[0])` |
|         - | 4502 | `			);` |
|         - | 4503 | `	}` |
|         - | 4504 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4505 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4506 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4507 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4508 | `			"TypeError",` |
|         - | 4509 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4510 | `			ph7_type_name(apArg[1])` |
|         - | 4511 | `			);` |
|         - | 4512 | `	}` |
|         - | 4513 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4514 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4515 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4516 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4517 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4518 | `				"TypeError",` |
|         - | 4519 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4520 | `				ph7_type_name(apArg[2])` |
|         - | 4521 | `				);` |
|         - | 4522 | `		}` |
|         8 | 4523 | `	}` |
|         - | 4524 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4525 | `	if( nArg > 3 ){` |
|         7 | 4526 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4527 | `			ph7_value_is_resource(apArg[3]) ){` |
|       ! 0 | 4528 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4529 | `				"TypeError",` |
|         - | 4530 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 4531 | `				ph7_type_name(apArg[3])` |
|         - | 4532 | `				);` |
|         - | 4533 | `		}` |
|         2 | 4534 | `	}` |
|         - | 4535 | `	/* Point the internal representation of the target array */` |
|        43 | 4536 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        43 | 4537 | `	bPreserve = FALSE;` |
|         - | 4538 | `	/* Get the offset */` |
|         - | 4539 | `	{` |
|        43 | 4540 | `		sxi64 iTmp = 0;` |
|        43 | 4541 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|        43 | 4542 | `		if( rcArg != PH7_OK ){` |
|       ! 0 | 4543 | `			return rcArg;` |
|         - | 4544 | `		}` |
|        43 | 4545 | `		iOfft = (int)iTmp;` |
|         - | 4546 | `	}` |
|        43 | 4547 | `	if( iOfft < 0 ){` |
|         5 | 4548 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4549 | `		if( iOfft < 0 ){` |
|         3 | 4550 | `			iOfft = 0;` |
|         1 | 4551 | `		}` |
|         2 | 4552 | `	}` |
|        43 | 4553 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4554 | `		/* Offset past end of array, return empty array */` |
|         5 | 4555 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4556 | `		if( pArray == 0 ){` |
|       ! 0 | 4557 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4558 | `			return PH7_OK;` |
|         - | 4559 | `		}` |
|         5 | 4560 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4561 | `		return PH7_OK;` |
|         - | 4562 | `	}` |
|         - | 4563 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        39 | 4564 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        39 | 4565 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        17 | 4566 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        17 | 4567 | `		if( iLength < 0 ){` |
|         5 | 4568 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4569 | `		}` |
|        17 | 4570 | `		if( iLength < 0 ){` |
|         3 | 4571 | `			iLength = 0;` |
|         1 | 4572 | `		}` |
|        17 | 4573 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4574 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4575 | `		}` |
|         8 | 4576 | `	}` |
|        39 | 4577 | `	if( nArg > 3 ){` |
|         5 | 4578 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4579 | `	}` |
|         - | 4580 | `	/* Create a new array */` |
|        39 | 4581 | `	pArray = ph7_context_new_array(pCtx);` |
|        39 | 4582 | `	if( pArray == 0 ){` |
|       ! 0 | 4583 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4584 | `		return PH7_OK;` |
|         - | 4585 | `	}` |
|        39 | 4586 | `	if( iLength < 1 ){` |
|         - | 4587 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4588 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4589 | `		return PH7_OK;` |
|         - | 4590 | `	}` |
|         - | 4591 | `	/* Point to the desired entry */` |
|        35 | 4592 | `	pCur = pSrc->pFirst;` |
|        29 | 4593 | `	for(;;){` |
|        63 | 4594 | `		if( iOfft < 1 ){` |
|        35 | 4595 | `			break;` |
|         - | 4596 | `		}` |
|         - | 4597 | `		/* Point to the next entry */` |
|        33 | 4598 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4599 | `		iOfft--;` |
|         5 | 4600 | `	}` |
|         - | 4601 | `	/* Point to the internal representation of the hashmap */` |
|        35 | 4602 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        54 | 4603 | `	for(;;){` |
|       113 | 4604 | `		if( iLength < 1 ){` |
|        35 | 4605 | `			break;` |
|         - | 4606 | `		}` |
|         - | 4607 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4608 | `		{` |
|        83 | 4609 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        83 | 4610 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4611 | `		}` |
|        83 | 4612 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4613 | `			break;` |
|         - | 4614 | `		}` |
|         - | 4615 | `		/* Point to the next entry */` |
|        83 | 4616 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        83 | 4617 | `		iLength--;` |
|         5 | 4618 | `	}` |
|         - | 4619 | `	/* Return the freshly created array */` |
|        35 | 4620 | `	ph7_result_value(pCtx,pArray);` |
|        35 | 4621 | `	return PH7_OK;` |
|        28 | 4622 | `}` |
|         - | 4623 | `/*` |
|         - | 4624 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4625 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4626 | ` * beginning (becomes the new pFirst).` |
|         - | 4627 | ` */` |
|        38 | 4628 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4629 | `{` |
|         - | 4630 | `	ph7_hashmap_node *pNode;` |
|         - | 4631 | `	ph7_hashmap_node *pOldNext;` |
|        39 | 4632 | `	pNode = pMap->pLast;` |
|        39 | 4633 | `	if( pNode == 0 ){` |
|       ! 0 | 4634 | `		return;` |
|         - | 4635 | `	}` |
|        39 | 4636 | `	if( pNode->pNext == 0 ){` |
|         - | 4637 | `		/* Only node in the list, nothing to move */` |
|         5 | 4638 | `		return;` |
|         - | 4639 | `	}` |
|        35 | 4640 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4641 | `		/* Already in the correct position */` |
|         9 | 4642 | `		return;` |
|         - | 4643 | `	}` |
|         - | 4644 | `	/* Unlink pNode from the end of the list */` |
|        27 | 4645 | `	pMap->pLast = pNode->pNext;` |
|        27 | 4646 | `	pMap->pLast->pPrev = 0;` |
|         - | 4647 | `	/* Insert pNode after pAfter in iteration order */` |
|        27 | 4648 | `	if( pAfter == 0 ){` |
|         - | 4649 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4650 | `		pNode->pNext = 0;` |
|         3 | 4651 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4652 | `		if( pMap->pFirst ){` |
|         3 | 4653 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4654 | `		}` |
|         3 | 4655 | `		pMap->pFirst = pNode;` |
|         2 | 4656 | `	}else{` |
|        25 | 4657 | `		pOldNext = pAfter->pPrev;` |
|        25 | 4658 | `		pNode->pPrev = pOldNext;` |
|        25 | 4659 | `		pNode->pNext = pAfter;` |
|        25 | 4660 | `		pAfter->pPrev = pNode;` |
|        25 | 4661 | `		if( pOldNext ){` |
|        25 | 4662 | `			pOldNext->pNext = pNode;` |
|        13 | 4663 | `		}else{` |
|       ! 0 | 4664 | `			pMap->pLast = pNode;` |
|         - | 4665 | `		}` |
|         - | 4666 | `	}` |
|        20 | 4667 | `}` |
|         - | 4668 | `/*` |
|         - | 4669 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4670 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4671 | ` * Parameters` |
|         - | 4672 | ` *  $array` |
|         - | 4673 | ` *    The input array.` |
|         - | 4674 | ` *  $offset` |
|         - | 4675 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4676 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4677 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4678 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4679 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4680 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4681 | ` *  $length (optional)` |
|         - | 4682 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4683 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4684 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4685 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4686 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4687 | ` *  $replacement (optional)` |
|         - | 4688 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4689 | ` *    with elements from this array.` |
|         - | 4690 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4691 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4692 | ` *    offset.` |
|         - | 4693 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4694 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4695 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4696 | ` * Return` |
|         - | 4697 | ` *   A new array consisting of the extracted elements.` |
|         - | 4698 | ` */` |
|        64 | 4699 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4700 | `{` |
|         - | 4701 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4702 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4703 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4704 | `	int iLength,iOfft,i;` |
|         - | 4705 | `	sxi32 rc;` |
|        66 | 4706 | `	if( nArg < 2 ){` |
|       ! 0 | 4707 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4708 | `			"ArgumentCountError",` |
|         - | 4709 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       ! 0 | 4710 | `			nArg` |
|         - | 4711 | `			);` |
|         - | 4712 | `	}` |
|        66 | 4713 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4714 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4715 | `			"TypeError",` |
|         - | 4716 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4717 | `			ph7_type_name(apArg[0])` |
|         - | 4718 | `			);` |
|         - | 4719 | `	}` |
|         - | 4720 | `	/* Point to the internal representation of the target array */` |
|        63 | 4721 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        63 | 4722 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4723 | `	/* Get the offset and clamp to valid range */` |
|        63 | 4724 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        63 | 4725 | `	if( iOfft < 0 ){` |
|         9 | 4726 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         9 | 4727 | `		if( iOfft < 0 ){` |
|         3 | 4728 | `			iOfft = 0;` |
|         2 | 4729 | `		}` |
|        59 | 4730 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4731 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4732 | `	}` |
|         - | 4733 | `	/* Get the length and clamp to valid range.` |
|         - | 4734 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        63 | 4735 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        63 | 4736 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        45 | 4737 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        45 | 4738 | `		if( iLength < 0 ){` |
|         7 | 4739 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4740 | `			if( iLength < 0 ){` |
|         3 | 4741 | `				iLength = 0;` |
|         1 | 4742 | `			}` |
|         3 | 4743 | `		}` |
|        45 | 4744 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4745 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4746 | `		}` |
|        22 | 4747 | `	}` |
|         - | 4748 | `	/* Create the result array for removed elements */` |
|        63 | 4749 | `	pArray = ph7_context_new_array(pCtx);` |
|        63 | 4750 | `	if( pArray == 0 ){` |
|       ! 0 | 4751 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4752 | `		return PH7_OK;` |
|         - | 4753 | `	}` |
|         - | 4754 | `	/* Get replacement array if provided */` |
|        63 | 4755 | `	pRep = 0;` |
|        63 | 4756 | `	if( nArg > 3 ){` |
|        27 | 4757 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4758 | `			/* Perform an array cast */` |
|         3 | 4759 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4760 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4761 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4762 | `			}` |
|         2 | 4763 | `		}else{` |
|        25 | 4764 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4765 | `		}` |
|        27 | 4766 | `		if( pRep ){` |
|         - | 4767 | `			/* Reset the loop cursor */` |
|        27 | 4768 | `			pRep->pCur = pRep->pFirst;` |
|        13 | 4769 | `		}` |
|        13 | 4770 | `	}` |
|         - | 4771 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|         - | 4772 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|         - | 4773 | `	/* Navigate to the offset position */` |
|        63 | 4774 | `	pCur = pSrc->pFirst;` |
|       131 | 4775 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        69 | 4776 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        35 | 4777 | `	}` |
|         - | 4778 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4779 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4780 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        63 | 4781 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4782 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        63 | 4783 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       141 | 4784 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        79 | 4785 | `		pPrev = pCur->pPrev;` |
|        79 | 4786 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        79 | 4787 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        79 | 4788 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4789 | `			break;` |
|         - | 4790 | `		}` |
|        79 | 4791 | `		pCur = pPrev; /* Reverse link */` |
|        40 | 4792 | `	}` |
|         - | 4793 | `	/* Insert replacement elements at the correct position */` |
|        63 | 4794 | `	if( pRep ){` |
|         - | 4795 | `		ph7_value sSafeVal;` |
|        78 | 4796 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        39 | 4797 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        39 | 4798 | `			if( pRvalue ){` |
|         - | 4799 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4800 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4801 | `				 * since it points into that same pool. */` |
|        39 | 4802 | `				sSafeVal = *pRvalue;` |
|        39 | 4803 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        39 | 4804 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        39 | 4805 | `					pNewNode = pSrc->pLast;` |
|        39 | 4806 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        39 | 4807 | `					pInsertAfter = pNewNode;` |
|        19 | 4808 | `				}` |
|        19 | 4809 | `			}` |
|         1 | 4810 | `		}` |
|        13 | 4811 | `	}` |
|         - | 4812 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|         - | 4813 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|         - | 4814 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|         - | 4815 | `	 * and removals left gaps. */` |
|         - | 4816 | `	{` |
|        63 | 4817 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|        63 | 4818 | `		sxu32 n = pSrc->nEntry;` |
|        63 | 4819 | `		pSrc->iNextIdx = 0;` |
|       233 | 4820 | `		while( n > 0 ){` |
|       171 | 4821 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       165 | 4822 | `				HashmapRehashIntNode(pEntry);` |
|        82 | 4823 | `			}` |
|       171 | 4824 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|       171 | 4825 | `			n--;` |
|         1 | 4826 | `		}` |
|        63 | 4827 | `		pSrc->pCur = pSrc->pFirst;` |
|         - | 4828 | `	}` |
|         - | 4829 | `	/* Return the freshly created array */` |
|        63 | 4830 | `	ph7_result_value(pCtx,pArray);` |
|        63 | 4831 | `	return PH7_OK;` |
|        34 | 4832 | `}` |
|         - | 4833 | `/*` |
|         - | 4834 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4835 | ` *  Checks if a value exists in an array.` |
|         - | 4836 | ` * Parameters` |
|         - | 4837 | ` *  $needle` |
|         - | 4838 | ` *   The searched value.` |
|         - | 4839 | ` *   Note:` |
|         - | 4840 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4841 | ` * $haystack` |
|         - | 4842 | ` *  The target array.` |
|         - | 4843 | ` * $strict` |
|         - | 4844 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4845 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4846 | ` */` |
|     32858 | 4847 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4848 | `{` |
|         - | 4849 | `	ph7_value *pNeedle;` |
|         - | 4850 | `	int bStrict;` |
|         - | 4851 | `	int rc;` |
|     32863 | 4852 | `	if( nArg < 2 ){` |
|         - | 4853 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4854 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4855 | `		return PH7_OK;` |
|         - | 4856 | `	}` |
|     32863 | 4857 | `	pNeedle = apArg[0];` |
|     32863 | 4858 | `	bStrict = 0;` |
|     32863 | 4859 | `	if( nArg > 2 ){` |
|        53 | 4860 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        26 | 4861 | `	}` |
|     32863 | 4862 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4863 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4864 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4865 | `		/* Set the comparison result */` |
|       ! 0 | 4866 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4867 | `		return PH7_OK;` |
|         - | 4868 | `	}` |
|         - | 4869 | `	/* Perform the lookup */` |
|     32863 | 4870 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4871 | `	/* Lookup result */` |
|     32863 | 4872 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32863 | 4873 | `	return PH7_OK;` |
|     16434 | 4874 | `}` |
|         - | 4875 | `/*` |
|         - | 4876 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4877 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4878 | ` * Parameters` |
|         - | 4879 | ` * $needle` |
|         - | 4880 | ` *   The searched value.` |
|         - | 4881 | ` * $haystack` |
|         - | 4882 | ` *   The array.` |
|         - | 4883 | ` * $strict` |
|         - | 4884 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4885 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4886 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4887 | ` * Return` |
|         - | 4888 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4889 | ` */` |
|        26 | 4890 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 4891 | `{` |
|         - | 4892 | `	ph7_hashmap_node *pEntry;` |
|         - | 4893 | `	ph7_value *pVal,sNeedle;` |
|         - | 4894 | `	ph7_hashmap *pMap;` |
|         - | 4895 | `	ph7_value sVal;` |
|         - | 4896 | `	int bStrict;` |
|         - | 4897 | `	sxu32 n;` |
|         - | 4898 | `	int rc;` |
|        28 | 4899 | `	if( nArg < 2 ){` |
|         - | 4900 | `		/* Missing argument,throw ArgumentCountError */` |
|       ! 0 | 4901 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4902 | `			"ArgumentCountError",` |
|         - | 4903 | `			"array_search() expects at least 2 arguments, %d given",` |
|       ! 0 | 4904 | `			nArg` |
|         - | 4905 | `			);` |
|         - | 4906 | `	}` |
|        28 | 4907 | `	bStrict = FALSE;` |
|        28 | 4908 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4909 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4910 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4911 | `			"TypeError",` |
|         - | 4912 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4913 | `			ph7_type_name(apArg[1])` |
|         - | 4914 | `			);` |
|         - | 4915 | `	}` |
|        25 | 4916 | `	if( nArg > 2 ){` |
|         - | 4917 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        11 | 4918 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 4919 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4920 | `				"TypeError",` |
|         - | 4921 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       ! 0 | 4922 | `				ph7_type_name(apArg[2])` |
|         - | 4923 | `				);` |
|         - | 4924 | `		}` |
|        11 | 4925 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         5 | 4926 | `	}` |
|         - | 4927 | `	/* Point to the internal representation of the internal hashmap */` |
|        25 | 4928 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4929 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        25 | 4930 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        25 | 4931 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        25 | 4932 | `	pEntry = pMap->pFirst;` |
|        25 | 4933 | `	n = pMap->nEntry;` |
|        28 | 4934 | `	for(;;){` |
|        57 | 4935 | `		if( !n ){` |
|         9 | 4936 | `			break;` |
|         - | 4937 | `		}` |
|         - | 4938 | `		/* Extract node value */` |
|        49 | 4939 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 4940 | `		if( pVal ){` |
|         - | 4941 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4942 | `			 * can change their type.` |
|         - | 4943 | `			 */` |
|        49 | 4944 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        49 | 4945 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        49 | 4946 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        49 | 4947 | `			PH7_MemObjRelease(&sVal);` |
|        49 | 4948 | `			PH7_MemObjRelease(&sNeedle);` |
|        49 | 4949 | `			if( rc == 0 ){` |
|         - | 4950 | `				/* Match found,return key */` |
|        17 | 4951 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4952 | `					/* INT key */` |
|        11 | 4953 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         6 | 4954 | `				}else{` |
|         7 | 4955 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4956 | `					/* Blob key */` |
|         7 | 4957 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4958 | `				}` |
|        17 | 4959 | `				return PH7_OK;` |
|         - | 4960 | `			}` |
|        16 | 4961 | `		}` |
|         - | 4962 | `		/* Point to the next entry */` |
|        33 | 4963 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 4964 | `		n--;` |
|         1 | 4965 | `	}` |
|         - | 4966 | `	/* No such value,return FALSE */` |
|         9 | 4967 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4968 | `	return PH7_OK;` |
|        15 | 4969 | `}` |
|         - | 4970 | `/*` |
|         - | 4971 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4972 | ` *  Computes the difference of arrays.` |
|         - | 4973 | ` * Parameters` |
|         - | 4974 | ` *  $array1` |
|         - | 4975 | ` *    The array to compare from` |
|         - | 4976 | ` *  $array2` |
|         - | 4977 | ` *    An array to compare against` |
|         - | 4978 | ` *  $...` |
|         - | 4979 | ` *   More arrays to compare against` |
|         - | 4980 | ` * Return` |
|         - | 4981 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4982 | ` *  are not present in any of the other arrays.` |
|         - | 4983 | ` */` |
|        20 | 4984 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 4985 | `{` |
|         - | 4986 | `	ph7_hashmap_node *pEntry;` |
|         - | 4987 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4988 | `	ph7_value *pArray;` |
|         - | 4989 | `	ph7_value *pVal;` |
|         - | 4990 | `	sxi32 rc;` |
|         - | 4991 | `	sxu32 n;` |
|         - | 4992 | `	int i;` |
|         - | 4993 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4994 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4995 | `	 * debugging difficult. */` |
|        23 | 4996 | `	if( nArg < 1 ){` |
|       ! 0 | 4997 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4998 | `			"ArgumentCountError",` |
|         - | 4999 | `			"array_diff() expects at least 1 argument, %d given",` |
|       ! 0 | 5000 | `			nArg` |
|         - | 5001 | `			);` |
|         - | 5002 | `	}` |
|        23 | 5003 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5004 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5005 | `			"TypeError",` |
|         - | 5006 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5007 | `			ph7_type_name(apArg[0])` |
|         - | 5008 | `			);` |
|         - | 5009 | `	}` |
|        36 | 5010 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 5011 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5012 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5013 | `				"TypeError",` |
|         - | 5014 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 5015 | `				i + 1,` |
|         2 | 5016 | `				ph7_type_name(apArg[i])` |
|         - | 5017 | `				);` |
|         - | 5018 | `		}` |
|         9 | 5019 | `	}` |
|        17 | 5020 | `	if( nArg == 1 ){` |
|         - | 5021 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5022 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5023 | `		return PH7_OK;` |
|         - | 5024 | `	}` |
|         - | 5025 | `	/* Create a new array */` |
|        15 | 5026 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5027 | `	if( pArray == 0 ){` |
|       ! 0 | 5028 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5029 | `		return PH7_OK;` |
|         - | 5030 | `	}` |
|         - | 5031 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5032 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5033 | `	/* Perform the diff */` |
|        15 | 5034 | `	pEntry = pSrc->pFirst;` |
|        15 | 5035 | `	n = pSrc->nEntry;` |
|        27 | 5036 | `	for(;;){` |
|        55 | 5037 | `		if( n < 1 ){` |
|        15 | 5038 | `			break;` |
|         - | 5039 | `		}` |
|         - | 5040 | `		/* Extract the node value */` |
|        41 | 5041 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 5042 | `		if( pVal ){` |
|        69 | 5043 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5044 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 5045 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5046 | `				/* Perform the lookup */` |
|        45 | 5047 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 5048 | `				if( rc == SXRET_OK ){` |
|         - | 5049 | `					/* Value exist */` |
|        17 | 5050 | `					break;` |
|         - | 5051 | `				}` |
|        15 | 5052 | `			}` |
|        41 | 5053 | `			if( i >= nArg ){` |
|         - | 5054 | `				/* Perform the insertion */` |
|        25 | 5055 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5056 | `			}` |
|        20 | 5057 | `		}` |
|         - | 5058 | `		/* Point to the next entry */` |
|        41 | 5059 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 5060 | `		n--;` |
|         1 | 5061 | `	}` |
|         - | 5062 | `	/* Return the freshly created array */` |
|        15 | 5063 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5064 | `	return PH7_OK;` |
|        13 | 5065 | `}` |
|         - | 5066 | `/*` |
|         - | 5067 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 5068 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 5069 | ` * Parameters` |
|         - | 5070 | ` *  $array1` |
|         - | 5071 | ` *    The array to compare from` |
|         - | 5072 | ` *  $array2` |
|         - | 5073 | ` *    An array to compare against` |
|         - | 5074 | ` *  $...` |
|         - | 5075 | ` *   More arrays to compare against.` |
|         - | 5076 | ` * $callback` |
|         - | 5077 | ` *  The callback comparison function.` |
|         - | 5078 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5079 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5080 | ` *  than the second.` |
|         - | 5081 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5082 | ` * Return` |
|         - | 5083 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5084 | ` *  are not present in any of the other arrays.` |
|         - | 5085 | ` */` |
|        20 | 5086 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5087 | `{` |
|         - | 5088 | `	ph7_hashmap_node *pEntry;` |
|         - | 5089 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5090 | `	ph7_value *pCallback;` |
|         - | 5091 | `	ph7_value *pArray;` |
|         - | 5092 | `	ph7_value *pVal;` |
|         - | 5093 | `	sxi32 rc;` |
|         - | 5094 | `	sxu32 n;` |
|         - | 5095 | `	int i;` |
|         - | 5096 |  |
|         - | 5097 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        25 | 5098 | `	if( nArg < 2 ){` |
|       ! 0 | 5099 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5100 | `			"ArgumentCountError",` |
|         - | 5101 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       ! 0 | 5102 | `			nArg` |
|         - | 5103 | `			);` |
|         - | 5104 | `	}` |
|        25 | 5105 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5106 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5107 | `			"TypeError",` |
|         - | 5108 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5109 | `			ph7_type_name(apArg[0])` |
|         - | 5110 | `			);` |
|         - | 5111 | `	}` |
|         - | 5112 |  |
|        23 | 5113 | `	if( nArg == 2 ){` |
|         - | 5114 | `		/* Only the original array and the callback were provided. */` |
|         - | 5115 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 5116 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 5117 | `		 * validation order.` |
|         - | 5118 | `		 */` |
|         4 | 5119 | `	} else {` |
|         - | 5120 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 5121 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 5122 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 5123 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5124 | `					"TypeError",` |
|         - | 5125 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5126 | `					i + 1,` |
|         6 | 5127 | `					ph7_type_name(apArg[i])` |
|         - | 5128 | `					);` |
|         - | 5129 | `			}` |
|         7 | 5130 | `		}` |
|         - | 5131 | `	}` |
|         - | 5132 |  |
|         - | 5133 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5134 | `	pCallback = apArg[nArg - 1];` |
|         - | 5135 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5136 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5137 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5138 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5139 | `				"TypeError",` |
|         - | 5140 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5141 | `				nArg` |
|         - | 5142 | `				);` |
|         - | 5143 | `		}` |
|         6 | 5144 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5145 | `			int len;` |
|         3 | 5146 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5147 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5148 | `				"TypeError",` |
|         - | 5149 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5150 | `				nArg,` |
|         1 | 5151 | `				zName` |
|         - | 5152 | `				);` |
|         - | 5153 | `		}` |
|         4 | 5154 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5155 | `			"TypeError",` |
|         - | 5156 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5157 | `			nArg` |
|         - | 5158 | `			);` |
|         - | 5159 | `	}` |
|         - | 5160 |  |
|         7 | 5161 | `	if( nArg == 2 ){` |
|         - | 5162 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5163 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5164 | `		return PH7_OK;` |
|         - | 5165 | `	}` |
|         - | 5166 |  |
|         - | 5167 | `	/* Create a new array */` |
|         5 | 5168 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5169 | `	if( pArray == 0 ){` |
|       ! 0 | 5170 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5171 | `		return PH7_OK;` |
|         - | 5172 | `	}` |
|         - | 5173 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5174 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5175 | `	/* Perform the diff */` |
|         5 | 5176 | `	pEntry = pSrc->pFirst;` |
|         5 | 5177 | `	n = pSrc->nEntry;` |
|         5 | 5178 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5179 | `	for(;;){` |
|        11 | 5180 | `		if( n < 1 ){` |
|         3 | 5181 | `			break;` |
|         - | 5182 | `		}` |
|         - | 5183 | `		/* Extract the node value */` |
|         9 | 5184 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5185 | `		if( pVal ){` |
|        15 | 5186 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5187 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5188 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5189 | `				/* Perform the lookup */` |
|         9 | 5190 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5191 | `				if( rc == SXRET_OK ){` |
|         - | 5192 | `					/* Value exist */` |
|         3 | 5193 | `					break;` |
|         - | 5194 | `				}` |
|         4 | 5195 | `			}` |
|         9 | 5196 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5197 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5198 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5199 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5200 | `				return PH7_EXCEPTION;` |
|         - | 5201 | `			}` |
|         7 | 5202 | `			if( i >= (nArg - 1)){` |
|         - | 5203 | `				/* Perform the insertion */` |
|         5 | 5204 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5205 | `			}` |
|         3 | 5206 | `		}` |
|         - | 5207 | `		/* Point to the next entry */` |
|         7 | 5208 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5209 | `		n--;` |
|         1 | 5210 | `	}` |
|         - | 5211 | `	/* Return the freshly created array */` |
|         3 | 5212 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5213 | `	return PH7_OK;` |
|        15 | 5214 | `}` |
|         - | 5215 | `/*` |
|         - | 5216 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5217 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5218 | ` * Parameters` |
|         - | 5219 | ` *  $array1` |
|         - | 5220 | ` *    The array to compare from` |
|         - | 5221 | ` *  $array2` |
|         - | 5222 | ` *    An array to compare against` |
|         - | 5223 | ` *  $...` |
|         - | 5224 | ` *   More arrays to compare against` |
|         - | 5225 | ` * Return` |
|         - | 5226 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5227 | ` *  are not present in any of the other arrays.` |
|         - | 5228 | ` */` |
|        20 | 5229 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5230 | `{` |
|         - | 5231 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5232 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5233 | `	ph7_value *pArray;` |
|         - | 5234 | `	ph7_value *pVal;` |
|         - | 5235 | `	sxi32 rc;` |
|         - | 5236 | `	sxu32 n;` |
|         - | 5237 | `	int i;` |
|         - | 5238 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5239 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5240 | `	 * accompanying integration tests to pass. */` |
|        24 | 5241 | `	if( nArg < 1 ){` |
|       ! 0 | 5242 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5243 | `			"ArgumentCountError",` |
|         - | 5244 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5245 | `			nArg` |
|         - | 5246 | `			);` |
|         - | 5247 | `	}` |
|        24 | 5248 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5249 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5250 | `			"TypeError",` |
|         - | 5251 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5252 | `			ph7_type_name(apArg[0])` |
|         - | 5253 | `			);` |
|         - | 5254 | `	}` |
|        37 | 5255 | `	for(i = 1 ; i < nArg ; i++){` |
|        23 | 5256 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5257 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5258 | `				"TypeError",` |
|         - | 5259 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5260 | `				i + 1,` |
|         4 | 5261 | `				ph7_type_name(apArg[i])` |
|         - | 5262 | `				);` |
|         - | 5263 | `		}` |
|        10 | 5264 | `	}` |
|        15 | 5265 | `	if( nArg == 1 ){` |
|         - | 5266 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5267 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5268 | `		return PH7_OK;` |
|         - | 5269 | `	}` |
|         - | 5270 | `	/* Create a new array */` |
|        13 | 5271 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 5272 | `	if( pArray == 0 ){` |
|       ! 0 | 5273 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5274 | `		return PH7_OK;` |
|         - | 5275 | `	}` |
|         - | 5276 | `	/* Point to the internal representation of the source hashmap */` |
|        13 | 5277 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5278 | `	/* Perform the diff */` |
|        13 | 5279 | `	pEntry = pSrc->pFirst;` |
|        13 | 5280 | `	n = pSrc->nEntry;` |
|        13 | 5281 | `	pN1 = pN2 = 0;` |
|        34 | 5282 | `	for(;;){` |
|         - | 5283 | `		int keep;` |
|        41 | 5284 | `		if( n < 1 ){` |
|        13 | 5285 | `			break;` |
|         - | 5286 | `		}` |
|         - | 5287 | `		/* assume the element should be kept until we find a match */` |
|        29 | 5288 | `		keep = 1;` |
|        47 | 5289 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5290 | `			/* all arguments have been validated already, so cast directly */` |
|        33 | 5291 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5292 | `			/* Perform a key lookup first */` |
|        33 | 5293 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5294 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5295 | `			}else{` |
|        21 | 5296 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5297 | `			}` |
|        33 | 5298 | `			if( rc != SXRET_OK ){` |
|         - | 5299 | `				/* this array does not contain the key, continue checking others */` |
|        17 | 5300 | `				continue;` |
|         - | 5301 | `			}` |
|         - | 5302 | `			/* key exists; check that value stored in the matching node is equal */` |
|        17 | 5303 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        17 | 5304 | `			if( pVal ){` |
|         - | 5305 | `				/* directly compare with value at pN1 rather than searching again */` |
|        17 | 5306 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        17 | 5307 | `				if( pVal2 ){` |
|         - | 5308 | `					ph7_value sV1,sV2;` |
|         - | 5309 | `					sxi32 cmp;` |
|         - | 5310 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|         - | 5311 | `					 * operands in place and these are LIVE array elements (a` |
|         - | 5312 | `					 * null element used to come back bool(false) in the` |
|         - | 5313 | `					 * caller's array). */` |
|        17 | 5314 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        17 | 5315 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        17 | 5316 | `					PH7_MemObjLoad(pVal,&sV1);` |
|        17 | 5317 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|        17 | 5318 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        17 | 5319 | `					PH7_MemObjRelease(&sV1);` |
|        17 | 5320 | `					PH7_MemObjRelease(&sV2);` |
|        17 | 5321 | `					if( cmp == 0 ){` |
|         - | 5322 | `						/* identical key+value found in one of the arrays => drop it */` |
|        15 | 5323 | `						keep = 0;` |
|        15 | 5324 | `						break;` |
|         - | 5325 | `					}` |
|         1 | 5326 | `				}` |
|         1 | 5327 | `			}` |
|         2 | 5328 | `		}` |
|        29 | 5329 | `		if( keep ){` |
|         - | 5330 | `			/* Perform the insertion */` |
|        15 | 5331 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         7 | 5332 | `		}` |
|         - | 5333 | `		/* Point to the next entry */` |
|        29 | 5334 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 5335 | `		n--;` |
|         1 | 5336 | `	}` |
|         - | 5337 | `	/* Return the freshly created array */` |
|        13 | 5338 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 5339 | `	return PH7_OK;` |
|        14 | 5340 | `}` |
|         - | 5341 | `/*` |
|         - | 5342 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5343 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5344 | ` *  by a user supplied callback function.` |
|         - | 5345 | ` * Parameters` |
|         - | 5346 | ` *  $array1` |
|         - | 5347 | ` *    The array to compare from` |
|         - | 5348 | ` *  $array2` |
|         - | 5349 | ` *    An array to compare against` |
|         - | 5350 | ` *  $...` |
|         - | 5351 | ` *   More arrays to compare against.` |
|         - | 5352 | ` *  $key_compare_func` |
|         - | 5353 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5354 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5355 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5356 | ` * Return` |
|         - | 5357 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5358 | ` *  are not present in any of the other arrays.` |
|         - | 5359 | ` */` |
|        22 | 5360 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5361 | `{` |
|         - | 5362 | `	ph7_hashmap_node *pEntry;` |
|         - | 5363 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5364 | `	ph7_value *pCallback;` |
|         - | 5365 | `	ph7_value *pArray;` |
|         - | 5366 | `	sxi32 rc;` |
|         - | 5367 | `	sxu32 n;` |
|         - | 5368 | `	int i;` |
|         - | 5369 |  |
|         - | 5370 | `	/* Argument validation mimicking PHP errors. */` |
|        26 | 5371 | `	if( nArg < 2 ){` |
|       ! 0 | 5372 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5373 | `			"ArgumentCountError",` |
|         - | 5374 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       ! 0 | 5375 | `			nArg` |
|         - | 5376 | `			);` |
|         - | 5377 | `	}` |
|        26 | 5378 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5379 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5380 | `			"TypeError",` |
|         - | 5381 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5382 | `			ph7_type_name(apArg[0])` |
|         - | 5383 | `			);` |
|         - | 5384 | `	}` |
|         - | 5385 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5386 | `	 * expected to be a callback. */` |
|        38 | 5387 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5388 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5389 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5390 | `				"TypeError",` |
|         - | 5391 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5392 | `				i + 1,` |
|         2 | 5393 | `				ph7_type_name(apArg[i])` |
|         - | 5394 | `				);` |
|         - | 5395 | `		}` |
|         9 | 5396 | `	}` |
|         - | 5397 | `	/* Point to the callback value */` |
|        22 | 5398 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5399 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5400 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5401 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5402 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5403 | `		 * string given" which we also reproduce. */` |
|         9 | 5404 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5405 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5406 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5407 | `				"TypeError",` |
|         - | 5408 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5409 | `				nArg` |
|         - | 5410 | `				);` |
|         - | 5411 | `		}` |
|         6 | 5412 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5413 | `			/* neither array nor string */` |
|         8 | 5414 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5415 | `				"TypeError",` |
|         - | 5416 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5417 | `				nArg` |
|         - | 5418 | `				);` |
|         - | 5419 | `		}` |
|         - | 5420 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5421 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5422 | `			"TypeError",` |
|         - | 5423 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5424 | `			nArg,` |
|       ! 0 | 5425 | `			ph7_type_name(pCallback)` |
|         - | 5426 | `			);` |
|         - | 5427 | `	}` |
|        13 | 5428 | `	if( nArg == 2 ){` |
|         - | 5429 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5430 | `		 * input array. */` |
|         3 | 5431 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5432 | `		return PH7_OK;` |
|         - | 5433 | `	}` |
|         - | 5434 | `	/* Create a new array */` |
|        11 | 5435 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5436 | `	if( pArray == 0 ){` |
|       ! 0 | 5437 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5438 | `		return PH7_OK;` |
|         - | 5439 | `	}` |
|         - | 5440 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5441 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5442 | `	/* Perform the diff */` |
|        11 | 5443 | `	pEntry = pSrc->pFirst;` |
|        11 | 5444 | `	n = pSrc->nEntry;` |
|        21 | 5445 | `	for(;;){` |
|         - | 5446 | `		int keep;` |
|        27 | 5447 | `		if( n < 1 ){` |
|         9 | 5448 | `			break;` |
|         - | 5449 | `		}` |
|        19 | 5450 | `		keep = 1;` |
|        31 | 5451 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5452 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5453 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5454 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5455 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5456 | `			while( pIt ){` |
|         - | 5457 | `				/* build temporary key values for callback */` |
|         - | 5458 | `				ph7_value key1, key2, result;` |
|         - | 5459 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5460 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5461 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5462 | `				}else{` |
|         - | 5463 | `					SyString sStr;` |
|        33 | 5464 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5465 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5466 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5467 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5468 | `				}` |
|        33 | 5469 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5470 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5471 | `				}else{` |
|         - | 5472 | `					SyString sStr;` |
|        33 | 5473 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5474 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5475 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5476 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5477 | `				}` |
|        33 | 5478 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5479 | `				/* call user callback with (key1, key2) */` |
|         - | 5480 | `				{` |
|         - | 5481 | `					ph7_value *apK[2];` |
|        33 | 5482 | `					apK[0] = &key1;` |
|        33 | 5483 | `					apK[1] = &key2;` |
|        33 | 5484 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5485 | `				}` |
|        33 | 5486 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5487 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5488 | `					 * array_uintersect (which signal back from` |
|         - | 5489 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5490 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5491 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5492 | `					PH7_MemObjRelease(&result);` |
|         3 | 5493 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5494 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5495 | `					return PH7_EXCEPTION;` |
|         - | 5496 | `				}` |
|        31 | 5497 | `				if( rc == SXRET_OK ){` |
|        31 | 5498 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5499 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5500 | `					}` |
|        31 | 5501 | `					if( result.x.iVal == 0 ){` |
|         - | 5502 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5503 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5504 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5505 | `						if( pVal1 && pVal2 ){` |
|         - | 5506 | `							ph7_value sV1,sV2;` |
|         - | 5507 | `							sxi32 cmp;` |
|         - | 5508 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|         - | 5509 | `							 * place and these are LIVE array elements. */` |
|        13 | 5510 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|        13 | 5511 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|        13 | 5512 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|        13 | 5513 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|        13 | 5514 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|        13 | 5515 | `							PH7_MemObjRelease(&sV1);` |
|        13 | 5516 | `							PH7_MemObjRelease(&sV2);` |
|        13 | 5517 | `							if( cmp == 0 ){` |
|         9 | 5518 | `								keep = 0;` |
|         9 | 5519 | `								PH7_MemObjRelease(&result);` |
|         - | 5520 | `								/* release keys too before breaking */` |
|         9 | 5521 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5522 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5523 | `								break;` |
|         - | 5524 | `							}` |
|         2 | 5525 | `						}` |
|         2 | 5526 | `					}` |
|        11 | 5527 | `				}` |
|        23 | 5528 | `				PH7_MemObjRelease(&result);` |
|        23 | 5529 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5530 | `				PH7_MemObjRelease(&key2);` |
|         - | 5531 | `				/* move to next node */` |
|        23 | 5532 | `				pIt = pIt->pPrev;` |
|        23 | 5533 | `				if( keep == 0 ) break;` |
|         1 | 5534 | `			}` |
|        21 | 5535 | `			if( keep == 0 ) break;` |
|         7 | 5536 | `		}` |
|        17 | 5537 | `		if( keep ){` |
|         - | 5538 | `			/* Perform the insertion */` |
|         9 | 5539 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5540 | `		}` |
|         - | 5541 | `		/* Point to the next entry */` |
|        17 | 5542 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5543 | `		n--;` |
|         1 | 5544 | `	}` |
|         - | 5545 | `	/* Return the freshly created array */` |
|         9 | 5546 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5547 | `	return PH7_OK;` |
|        15 | 5548 | `}` |
|         - | 5549 | `/*` |
|         - | 5550 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5551 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5552 | ` * Parameters` |
|         - | 5553 | ` *  $array1` |
|         - | 5554 | ` *    The array to compare from` |
|         - | 5555 | ` *  $array2` |
|         - | 5556 | ` *    An array to compare against` |
|         - | 5557 | ` *  $...` |
|         - | 5558 | ` *   More arrays to compare against` |
|         - | 5559 | ` * Return` |
|         - | 5560 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5561 | ` *  in any of the other arrays.` |
|         - | 5562 | ` * Note that NULL is returned on failure.` |
|         - | 5563 | ` */` |
|        12 | 5564 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5565 | `{` |
|         - | 5566 | `	ph7_hashmap_node *pEntry;` |
|         - | 5567 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5568 | `	ph7_value *pArray;` |
|         - | 5569 | `	sxi32 rc;` |
|         - | 5570 | `	sxu32 n;` |
|         - | 5571 | `	int i;` |
|         - | 5572 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5573 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5574 | `	 * helpers. */` |
|        15 | 5575 | `	if( nArg < 1 ){` |
|       ! 0 | 5576 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5577 | `			"ArgumentCountError",` |
|         - | 5578 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5579 | `			nArg` |
|         - | 5580 | `			);` |
|         - | 5581 | `	}` |
|        15 | 5582 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5583 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5584 | `			"TypeError",` |
|         - | 5585 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5586 | `			ph7_type_name(apArg[0])` |
|         - | 5587 | `			);` |
|         - | 5588 | `	}` |
|        20 | 5589 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5590 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5591 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5592 | `				"TypeError",` |
|         - | 5593 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5594 | `				i + 1,` |
|         2 | 5595 | `				ph7_type_name(apArg[i])` |
|         - | 5596 | `				);` |
|         - | 5597 | `		}` |
|         5 | 5598 | `	}` |
|         9 | 5599 | `	if( nArg == 1 ){` |
|         - | 5600 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5601 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5602 | `		return PH7_OK;` |
|         - | 5603 | `	}` |
|         - | 5604 | `	/* Create a new array */` |
|         7 | 5605 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5606 | `	if( pArray == 0 ){` |
|       ! 0 | 5607 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5608 | `		return PH7_OK;` |
|         - | 5609 | `	}` |
|         - | 5610 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5611 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5612 | `	/* Perfrom the diff */` |
|         7 | 5613 | `	pEntry = pSrc->pFirst;` |
|         7 | 5614 | `	n = pSrc->nEntry;` |
|        12 | 5615 | `	for(;;){` |
|        25 | 5616 | `		if( n < 1 ){` |
|         7 | 5617 | `			break;` |
|         - | 5618 | `		}` |
|        31 | 5619 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5620 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5621 | `				/* ignore */` |
|       ! 0 | 5622 | `				continue;` |
|         - | 5623 | `			}` |
|        23 | 5624 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5625 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5626 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5627 | `				/* Blob lookup */` |
|        17 | 5628 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5629 | `			}else{` |
|         - | 5630 | `				/* Int lookup */` |
|         7 | 5631 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5632 | `			}` |
|        23 | 5633 | `			if( rc == SXRET_OK ){` |
|         - | 5634 | `				/* Key exists,break immediately */` |
|        11 | 5635 | `				break;` |
|         - | 5636 | `			}` |
|         7 | 5637 | `		}` |
|        19 | 5638 | `		if( i >= nArg ){` |
|         - | 5639 | `			/* Perform the insertion */` |
|         9 | 5640 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5641 | `		}` |
|         - | 5642 | `		/* Point to the next entry */` |
|        19 | 5643 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5644 | `		n--;` |
|         1 | 5645 | `	}` |
|         - | 5646 | `	/* Return the freshly created array */` |
|         7 | 5647 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5648 | `	return PH7_OK;` |
|         9 | 5649 | `}` |
|         - | 5650 | `/*` |
|         - | 5651 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5652 | ` *  Computes the intersection of arrays.` |
|         - | 5653 | ` * Parameters` |
|         - | 5654 | ` *  $array1` |
|         - | 5655 | ` *    The array to compare from` |
|         - | 5656 | ` *  $array2` |
|         - | 5657 | ` *    An array to compare against` |
|         - | 5658 | ` *  $...` |
|         - | 5659 | ` *   More arrays to compare against` |
|         - | 5660 | ` * Return` |
|         - | 5661 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5662 | ` *  in all of the parameters.` |
|         - | 5663 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5664 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5665 | ` */` |
|        20 | 5666 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5667 | `{` |
|         - | 5668 | `	ph7_hashmap_node *pEntry;` |
|         - | 5669 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5670 | `	ph7_value *pArray;` |
|         - | 5671 | `	ph7_value *pVal;` |
|         - | 5672 | `	sxi32 rc;` |
|         - | 5673 | `	sxu32 n;` |
|         - | 5674 | `	int i;` |
|        23 | 5675 | `	if( nArg < 1 ){` |
|       ! 0 | 5676 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5677 | `			"ArgumentCountError",` |
|         - | 5678 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       ! 0 | 5679 | `			nArg` |
|         - | 5680 | `			);` |
|         - | 5681 | `	}` |
|        23 | 5682 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5683 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5684 | `			"TypeError",` |
|         - | 5685 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5686 | `			ph7_type_name(apArg[0])` |
|         - | 5687 | `			);` |
|         - | 5688 | `	}` |
|        36 | 5689 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5690 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5691 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5692 | `				"TypeError",` |
|         - | 5693 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5694 | `				i + 1,` |
|         2 | 5695 | `				ph7_type_name(apArg[i])` |
|         - | 5696 | `				);` |
|         - | 5697 | `		}` |
|         9 | 5698 | `	}` |
|        17 | 5699 | `	if( nArg == 1 ){` |
|         - | 5700 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5701 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5702 | `		return PH7_OK;` |
|         - | 5703 | `	}` |
|         - | 5704 | `	/* Create a new array */` |
|        15 | 5705 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5706 | `	if( pArray == 0 ){` |
|       ! 0 | 5707 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5708 | `		return PH7_OK;` |
|         - | 5709 | `	}` |
|         - | 5710 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5711 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5712 | `	/* Perform the intersection */` |
|        15 | 5713 | `	pEntry = pSrc->pFirst;` |
|        15 | 5714 | `	n = pSrc->nEntry;` |
|        31 | 5715 | `	for(;;){` |
|        63 | 5716 | `		if( n < 1 ){` |
|        15 | 5717 | `			break;` |
|         - | 5718 | `		}` |
|         - | 5719 | `		/* Extract the node value */` |
|        49 | 5720 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5721 | `		if( pVal ){` |
|        79 | 5722 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5723 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5724 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5725 | `				/* Perform the lookup */` |
|        55 | 5726 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5727 | `				if( rc != SXRET_OK ){` |
|         - | 5728 | `					/* Value does not exist */` |
|        25 | 5729 | `					break;` |
|         - | 5730 | `				}` |
|        16 | 5731 | `			}` |
|        49 | 5732 | `			if( i >= nArg ){` |
|         - | 5733 | `				/* Perform the insertion */` |
|        25 | 5734 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5735 | `			}` |
|        24 | 5736 | `		}` |
|         - | 5737 | `		/* Point to the next entry */` |
|        49 | 5738 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5739 | `		n--;` |
|         1 | 5740 | `	}` |
|         - | 5741 | `	/* Return the freshly created array */` |
|        15 | 5742 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5743 | `	return PH7_OK;` |
|        13 | 5744 | `}` |
|         - | 5745 | `/*` |
|         - | 5746 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5747 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5748 | ` * Parameters` |
|         - | 5749 | ` *  $array1` |
|         - | 5750 | ` *    The array to compare from` |
|         - | 5751 | ` *  $array2` |
|         - | 5752 | ` *    An array to compare against` |
|         - | 5753 | ` *  $...` |
|         - | 5754 | ` *   More arrays to compare against` |
|         - | 5755 | ` * Return` |
|         - | 5756 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5757 | ` *  in all the arguments, with matching keys.` |
|         - | 5758 | ` */` |
|        20 | 5759 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5760 | `{` |
|         - | 5761 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5762 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5763 | `	ph7_value *pArray;` |
|         - | 5764 | `	ph7_value *pVal;` |
|         - | 5765 | `	sxi32 rc;` |
|         - | 5766 | `	sxu32 n;` |
|         - | 5767 | `	int i;` |
|        23 | 5768 | `	if( nArg < 1 ){` |
|       ! 0 | 5769 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5770 | `			"ArgumentCountError",` |
|         - | 5771 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       ! 0 | 5772 | `			nArg` |
|         - | 5773 | `			);` |
|         - | 5774 | `	}` |
|        23 | 5775 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5776 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5777 | `			"TypeError",` |
|         - | 5778 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5779 | `			ph7_type_name(apArg[0])` |
|         - | 5780 | `			);` |
|         - | 5781 | `	}` |
|        36 | 5782 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5783 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5784 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5785 | `				"TypeError",` |
|         - | 5786 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5787 | `				i + 1,` |
|         2 | 5788 | `				ph7_type_name(apArg[i])` |
|         - | 5789 | `				);` |
|         - | 5790 | `		}` |
|         9 | 5791 | `	}` |
|        17 | 5792 | `	if( nArg == 1 ){` |
|         - | 5793 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5794 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5795 | `		return PH7_OK;` |
|         - | 5796 | `	}` |
|         - | 5797 | `	/* Create a new array */` |
|        15 | 5798 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5799 | `	if( pArray == 0 ){` |
|       ! 0 | 5800 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5801 | `		return PH7_OK;` |
|         - | 5802 | `	}` |
|         - | 5803 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5804 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5805 | `	/* Perform the intersection */` |
|        15 | 5806 | `	pEntry = pSrc->pFirst;` |
|        15 | 5807 | `	n = pSrc->nEntry;` |
|        15 | 5808 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5809 | `	for(;;){` |
|        47 | 5810 | `		if( n < 1 ){` |
|        15 | 5811 | `			break;` |
|         - | 5812 | `		}` |
|         - | 5813 | `		/* Extract the node value */` |
|        33 | 5814 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5815 | `		if( pVal ){` |
|        53 | 5816 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5817 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5818 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5819 | `				/* Perform a key lookup first */` |
|        37 | 5820 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5821 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5822 | `				}else{` |
|        23 | 5823 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5824 | `				}` |
|        37 | 5825 | `				if( rc != SXRET_OK ){` |
|         - | 5826 | `					/* No such key,break immediately */` |
|         7 | 5827 | `					break;` |
|         - | 5828 | `				}` |
|         - | 5829 | `				/* Perform the lookup */` |
|        31 | 5830 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5831 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5832 | `					/* Value does not exist */` |
|         6 | 5833 | `					break;` |
|         - | 5834 | `				}` |
|        11 | 5835 | `			}` |
|        33 | 5836 | `			if( i >= nArg ){` |
|         - | 5837 | `				/* Perform the insertion */` |
|        17 | 5838 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5839 | `			}` |
|        16 | 5840 | `		}` |
|         - | 5841 | `		/* Point to the next entry */` |
|        33 | 5842 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5843 | `		n--;` |
|         1 | 5844 | `	}` |
|         - | 5845 | `	/* Return the freshly created array */` |
|        15 | 5846 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5847 | `	return PH7_OK;` |
|        13 | 5848 | `}` |
|         - | 5849 | `/*` |
|         - | 5850 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5851 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5852 | ` * Parameters` |
|         - | 5853 | ` *  $array1` |
|         - | 5854 | ` *    The array to compare from` |
|         - | 5855 | ` *  $...` |
|         - | 5856 | ` *   More arrays to compare against` |
|         - | 5857 | ` * Return` |
|         - | 5858 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5859 | ` *  have keys that are present in all arguments.` |
|         - | 5860 | ` * Note that NULL is returned on failure.` |
|         - | 5861 | ` */` |
|        20 | 5862 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 5863 | `{` |
|         - | 5864 | `	ph7_hashmap_node *pEntry;` |
|         - | 5865 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5866 | `	ph7_value *pArray;` |
|         - | 5867 | `	sxi32 rc;` |
|         - | 5868 | `	sxu32 n;` |
|         - | 5869 | `	int i;` |
|        23 | 5870 | `	if( nArg < 1 ){` |
|       ! 0 | 5871 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5872 | `			"ArgumentCountError",` |
|         - | 5873 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       ! 0 | 5874 | `			nArg` |
|         - | 5875 | `			);` |
|         - | 5876 | `	}` |
|        23 | 5877 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5878 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5879 | `			"TypeError",` |
|         - | 5880 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5881 | `			ph7_type_name(apArg[0])` |
|         - | 5882 | `			);` |
|         - | 5883 | `	}` |
|        36 | 5884 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5885 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5886 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5887 | `				"TypeError",` |
|         - | 5888 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5889 | `				i + 1,` |
|         2 | 5890 | `				ph7_type_name(apArg[i])` |
|         - | 5891 | `				);` |
|         - | 5892 | `		}` |
|         9 | 5893 | `	}` |
|        17 | 5894 | `	if( nArg == 1 ){` |
|         - | 5895 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5896 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5897 | `		return PH7_OK;` |
|         - | 5898 | `	}` |
|         - | 5899 | `	/* Create a new array */` |
|        15 | 5900 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5901 | `	if( pArray == 0 ){` |
|       ! 0 | 5902 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5903 | `		return PH7_OK;` |
|         - | 5904 | `	}` |
|         - | 5905 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5906 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5907 | `	/* Perform the intersection */` |
|        15 | 5908 | `	pEntry = pSrc->pFirst;` |
|        15 | 5909 | `	n = pSrc->nEntry;` |
|        24 | 5910 | `	for(;;){` |
|        49 | 5911 | `		if( n < 1 ){` |
|        15 | 5912 | `			break;` |
|         - | 5913 | `		}` |
|        57 | 5914 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5915 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5916 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5917 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5918 | `				/* Blob lookup */` |
|        27 | 5919 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5920 | `			}else{` |
|         - | 5921 | `				/* Int key */` |
|        13 | 5922 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5923 | `			}` |
|        39 | 5924 | `			if( rc != SXRET_OK ){` |
|         - | 5925 | `				/* Key does not exist, break immediately */` |
|        17 | 5926 | `				break;` |
|         - | 5927 | `			}` |
|        12 | 5928 | `		}` |
|        35 | 5929 | `		if( i >= nArg ){` |
|         - | 5930 | `			/* Perform the insertion */` |
|        19 | 5931 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5932 | `		}` |
|         - | 5933 | `		/* Point to the next entry */` |
|        35 | 5934 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5935 | `		n--;` |
|         1 | 5936 | `	}` |
|         - | 5937 | `	/* Return the freshly created array */` |
|        15 | 5938 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5939 | `	return PH7_OK;` |
|        13 | 5940 | `}` |
|         - | 5941 | `/*` |
|         - | 5942 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5943 | ` *  Computes the intersection of arrays.` |
|         - | 5944 | ` * Parameters` |
|         - | 5945 | ` *  $array1` |
|         - | 5946 | ` *    The array to compare from` |
|         - | 5947 | ` *  $array2` |
|         - | 5948 | ` *    An array to compare against` |
|         - | 5949 | ` *  $...` |
|         - | 5950 | ` *   More arrays to compare against` |
|         - | 5951 | ` * $callback` |
|         - | 5952 | ` *  The callback comparison function.` |
|         - | 5953 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5954 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5955 | ` *  than the second.` |
|         - | 5956 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5957 | ` * Return` |
|         - | 5958 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5959 | ` *  in all of the parameters. .` |
|         - | 5960 | ` * Note that NULL is returned on failure.` |
|         - | 5961 | ` */` |
|        24 | 5962 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5963 | `{` |
|         - | 5964 | `	ph7_hashmap_node *pEntry;` |
|         - | 5965 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5966 | `	ph7_value *pCallback;` |
|         - | 5967 | `	ph7_value *pArray;` |
|         - | 5968 | `	ph7_value *pVal;` |
|         - | 5969 | `	sxi32 rc;` |
|         - | 5970 | `	sxu32 n;` |
|         - | 5971 | `	int i;` |
|         - | 5972 |  |
|         - | 5973 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        29 | 5974 | `	if( nArg < 2 ){` |
|       ! 0 | 5975 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5976 | `			"ArgumentCountError",` |
|         - | 5977 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       ! 0 | 5978 | `			nArg` |
|         - | 5979 | `			);` |
|         - | 5980 | `	}` |
|        29 | 5981 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5982 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5983 | `			"TypeError",` |
|         - | 5984 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5985 | `			ph7_type_name(apArg[0])` |
|         - | 5986 | `			);` |
|         - | 5987 | `	}` |
|         - | 5988 |  |
|        27 | 5989 | `	if( nArg == 2 ){` |
|         - | 5990 | `		/* Only the original array and the callback were provided. */` |
|         - | 5991 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5992 | `		 * validation ordering. */` |
|         3 | 5993 | `	} else {` |
|         - | 5994 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5995 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5996 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5997 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5998 | `					"TypeError",` |
|         - | 5999 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 6000 | `					i + 1,` |
|         2 | 6001 | `					ph7_type_name(apArg[i])` |
|         - | 6002 | `					);` |
|         - | 6003 | `			}` |
|        13 | 6004 | `		}` |
|         - | 6005 | `	}` |
|         - | 6006 |  |
|         - | 6007 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 6008 | `	pCallback = apArg[nArg - 1];` |
|         - | 6009 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 6010 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 6011 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 6012 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 6013 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 6014 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 6015 | `			 */` |
|         9 | 6016 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 6017 | `			if( pCb->nEntry != 2 ){` |
|         4 | 6018 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6019 | `					"TypeError",` |
|         - | 6020 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 6021 | `					nArg` |
|         - | 6022 | `					);` |
|         - | 6023 | `			}` |
|         - | 6024 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 6025 | `			{` |
|         6 | 6026 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 6027 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 6028 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 6029 | `					int nMethodLen;` |
|         6 | 6030 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 6031 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 6032 | `					if( pClass ){` |
|         - | 6033 | `						/* Class exists but method is missing. */` |
|         4 | 6034 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6035 | `							"TypeError",` |
|         - | 6036 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 6037 | `							nArg,` |
|         1 | 6038 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 6039 | `							zMethod` |
|         - | 6040 | `							);` |
|         - | 6041 | `					}` |
|         - | 6042 | `					/* Class not found */` |
|         - | 6043 | `					{` |
|         - | 6044 | `						int nName;` |
|         3 | 6045 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 6046 | `						return PH7_VmThrowException(pCtx,` |
|         - | 6047 | `							"TypeError",` |
|         - | 6048 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 6049 | `							nArg,` |
|         1 | 6050 | `							zName` |
|         - | 6051 | `							);` |
|         - | 6052 | `					}` |
|         - | 6053 | `				}` |
|         - | 6054 | `			}` |
|         - | 6055 | `			/* Fallback message */` |
|       ! 0 | 6056 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6057 | `				"TypeError",` |
|         - | 6058 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 6059 | `				nArg` |
|         - | 6060 | `				);` |
|         - | 6061 | `		}` |
|         6 | 6062 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 6063 | `			int len;` |
|         3 | 6064 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 6065 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6066 | `				"TypeError",` |
|         - | 6067 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 6068 | `				nArg,` |
|         1 | 6069 | `				zName` |
|         - | 6070 | `				);` |
|         - | 6071 | `		}` |
|         4 | 6072 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6073 | `			"TypeError",` |
|         - | 6074 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 6075 | `			nArg` |
|         - | 6076 | `			);` |
|         - | 6077 | `	}` |
|         - | 6078 |  |
|        11 | 6079 | `	if( nArg == 2 ){` |
|         - | 6080 | `		/* Only the original array and the callback were provided. */` |
|         5 | 6081 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 6082 | `		return PH7_OK;` |
|         - | 6083 | `	}` |
|         - | 6084 |  |
|         - | 6085 | `	/* Create a new array */` |
|         7 | 6086 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 6087 | `	if( pArray == 0 ){` |
|       ! 0 | 6088 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6089 | `		return PH7_OK;` |
|         - | 6090 | `	}` |
|         - | 6091 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 6092 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6093 | `	/* Perform the intersection */` |
|         7 | 6094 | `	pEntry = pSrc->pFirst;` |
|         7 | 6095 | `	n = pSrc->nEntry;` |
|         7 | 6096 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 6097 | `	for(;;){` |
|        19 | 6098 | `		if( n < 1 ){` |
|         5 | 6099 | `			break;` |
|         - | 6100 | `		}` |
|         - | 6101 | `		/* Extract the node value */` |
|        15 | 6102 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 6103 | `		if( pVal ){` |
|        23 | 6104 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 6105 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 6106 | `					/* ignore */` |
|       ! 0 | 6107 | `					continue;` |
|         - | 6108 | `				}` |
|         - | 6109 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 6110 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 6111 | `				/* Perform the lookup */` |
|        15 | 6112 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 6113 | `				if( rc != SXRET_OK ){` |
|         - | 6114 | `					/* Value does not exist */` |
|         7 | 6115 | `					break;` |
|         - | 6116 | `				}` |
|         5 | 6117 | `			}` |
|        15 | 6118 | `			if( i >= (nArg-1) ){` |
|         - | 6119 | `				/* Perform the insertion */` |
|         9 | 6120 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 6121 | `			}` |
|         7 | 6122 | `		}` |
|        15 | 6123 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 6124 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 6125 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 6126 | `			return PH7_EXCEPTION;` |
|         - | 6127 | `		}` |
|         - | 6128 | `		/* Point to the next entry */` |
|        13 | 6129 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 6130 | `		n--;` |
|         1 | 6131 | `	}` |
|         - | 6132 | `	/* Return the freshly created array */` |
|         5 | 6133 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 6134 | `	return PH7_OK;` |
|        17 | 6135 | `}` |
|         - | 6136 | `/*` |
|         - | 6137 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 6138 | ` *  Fill an array with values.` |
|         - | 6139 | ` * Parameters` |
|         - | 6140 | ` *  $start_index` |
|         - | 6141 | ` *    The first index of the returned array.` |
|         - | 6142 | ` *  $num` |
|         - | 6143 | ` *   Number of elements to insert.` |
|         - | 6144 | ` *  $value` |
|         - | 6145 | ` *    Value to use for filling.` |
|         - | 6146 | ` * Return` |
|         - | 6147 | ` *  The filled array or null on failure.` |
|         - | 6148 | ` */` |
|       240 | 6149 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6150 | `{` |
|         - | 6151 | `	ph7_value *pArray;` |
|         - | 6152 | `	int i,nEntry;` |
|         - | 6153 |  |
|         - | 6154 | `	/* PHP enforces argument count and type checks. */` |
|       244 | 6155 | `	if( nArg != 3 ){` |
|         - | 6156 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6157 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6158 | `			"ArgumentCountError",` |
|         - | 6159 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         1 | 6160 | `			nArg` |
|         - | 6161 | `			);` |
|         - | 6162 | `	}` |
|         - | 6163 |  |
|         - | 6164 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6165 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6166 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6167 | `	 * and NULLs are rejected outright. */` |
|       357 | 6168 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       361 | 6169 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       ! 0 | 6170 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6171 | `			"TypeError",` |
|         - | 6172 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       ! 0 | 6173 | `			ph7_type_name(apArg[0])` |
|         - | 6174 | `			);` |
|         - | 6175 | `	}` |
|       242 | 6176 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6177 | `		int len;` |
|         8 | 6178 | `		sxu8 bReal = FALSE;` |
|         8 | 6179 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6180 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6181 | `			/* Non‑numeric string is an error. */` |
|         3 | 6182 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6183 | `				"TypeError",` |
|         - | 6184 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6185 | `				);` |
|         - | 6186 | `		}` |
|         5 | 6187 | `		if( bReal ){` |
|         - | 6188 | `			/* float-string -> deprecation warning */` |
|         4 | 6189 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6190 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6191 | `				zStr` |
|         - | 6192 | `				);` |
|         1 | 6193 | `		}` |
|         2 | 6194 | `	}` |
|         - | 6195 |  |
|         - | 6196 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6197 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6198 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6199 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6200 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6201 | `			"TypeError",` |
|         - | 6202 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6203 | `			ph7_type_name(apArg[1])` |
|         - | 6204 | `			);` |
|         - | 6205 | `	}` |
|       239 | 6206 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6207 | `		int len;` |
|         3 | 6208 | `		sxu8 bReal = FALSE;` |
|         3 | 6209 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6210 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6211 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6212 | `				"TypeError",` |
|         - | 6213 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6214 | `				);` |
|         - | 6215 | `		}` |
|       ! 0 | 6216 | `	}` |
|         - | 6217 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6218 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6219 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6220 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6221 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6222 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6223 | `		if( d != (double)i64 ){` |
|         7 | 6224 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6225 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6226 | `				d` |
|         - | 6227 | `				);` |
|         2 | 6228 | `		}` |
|         2 | 6229 | `	}` |
|         - | 6230 |  |
|         - | 6231 | `	/* Total number of entries to insert */` |
|       236 | 6232 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6233 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6234 | `	if( nEntry < 0 ){` |
|         3 | 6235 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6236 | `			"ValueError",` |
|         - | 6237 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6238 | `			);` |
|         - | 6239 | `	}` |
|         - | 6240 |  |
|         - | 6241 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6242 | `	if( nEntry == 0 ){` |
|         7 | 6243 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6244 | `		return PH7_OK;` |
|         - | 6245 | `	}` |
|         - | 6246 |  |
|         - | 6247 | `	/* Create a new array */` |
|       227 | 6248 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6249 | `	if( pArray == 0 ){` |
|       ! 0 | 6250 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6251 | `	}` |
|         - | 6252 |  |
|         - | 6253 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6254 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6255 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6256 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6257 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6258 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6259 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6260 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6261 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6262 | `		}` |
|   1058803 | 6263 | `	}` |
|         - | 6264 | `	/* Return the filled array */` |
|       227 | 6265 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6266 | `	return PH7_OK;` |
|       124 | 6267 | `}` |
|         - | 6268 | `/*` |
|         - | 6269 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6270 | ` *  Fill an array with values, specifying keys.` |
|         - | 6271 | ` * Parameters` |
|         - | 6272 | ` *  $input` |
|         - | 6273 | ` *   Array of values that will be used as key.` |
|         - | 6274 | ` *  $value` |
|         - | 6275 | ` *    Value to use for filling.` |
|         - | 6276 | ` * Return` |
|         - | 6277 | ` *  The filled array.` |
|         - | 6278 | ` * Throws` |
|         - | 6279 | ` *  ValueError if $input is not an array.` |
|         - | 6280 | ` */` |
|        22 | 6281 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6282 | `{` |
|         - | 6283 | `	ph7_hashmap_node *pEntry;` |
|         - | 6284 | `	ph7_hashmap *pSrc;` |
|         - | 6285 | `	ph7_value *pArray;` |
|         - | 6286 | `	sxu32 n;` |
|         - | 6287 | `	/* PHP enforces exactly 2 arguments. */` |
|        25 | 6288 | `	if( nArg != 2 ){` |
|         4 | 6289 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6290 | `			"ArgumentCountError",` |
|         - | 6291 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         1 | 6292 | `			nArg` |
|         - | 6293 | `			);` |
|         - | 6294 | `	}` |
|         - | 6295 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6296 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6297 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6298 | `			"TypeError",` |
|         - | 6299 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6300 | `			ph7_type_name(apArg[0])` |
|         - | 6301 | `			);` |
|         - | 6302 | `	}` |
|         - | 6303 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6304 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6305 | `	/* Create a new array */` |
|        17 | 6306 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6307 | `	if( pArray == 0 ){` |
|       ! 0 | 6308 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6309 | `		return PH7_OK;` |
|         - | 6310 | `	}` |
|         - | 6311 | `	/* Perform the requested operation */` |
|        17 | 6312 | `	pEntry = pSrc->pFirst;` |
|        45 | 6313 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6314 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6315 | `		/* Point to the next entry */` |
|        29 | 6316 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6317 | `	}` |
|         - | 6318 | `	/* Return the filled array */` |
|        17 | 6319 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6320 | `	return PH7_OK;` |
|        14 | 6321 | `}` |
|         - | 6322 | `/*` |
|         - | 6323 | ` * array array_combine(array $keys,array $values)` |
|         - | 6324 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6325 | ` * Parameters` |
|         - | 6326 | ` *  $keys` |
|         - | 6327 | ` *    Array of keys to be used.` |
|         - | 6328 | ` * $values` |
|         - | 6329 | ` *   Array of values to be used.` |
|         - | 6330 | ` * Return` |
|         - | 6331 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6332 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6333 | ` *  not an array.` |
|         - | 6334 | ` */` |
|        16 | 6335 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6336 | `{` |
|         - | 6337 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6338 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6339 | `	ph7_value *pArray;` |
|         - | 6340 | `	sxu32 n;` |
|         - | 6341 | `	/* PHP enforces argument count and type checks. */` |
|        20 | 6342 | `	if( nArg != 2 ){` |
|         - | 6343 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       ! 0 | 6344 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6345 | `			"ArgumentCountError",` |
|         - | 6346 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       ! 0 | 6347 | `			nArg` |
|         - | 6348 | `			);` |
|         - | 6349 | `	}` |
|         - | 6350 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6351 | `	 * argument index in the error message. */` |
|        20 | 6352 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6353 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6354 | `			"TypeError",` |
|         - | 6355 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6356 | `			ph7_type_name(apArg[0])` |
|         - | 6357 | `			);` |
|         - | 6358 | `	}` |
|        17 | 6359 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6360 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6361 | `			"TypeError",` |
|         - | 6362 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6363 | `			ph7_type_name(apArg[1])` |
|         - | 6364 | `			);` |
|         - | 6365 | `	}` |
|         - | 6366 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6367 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6368 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6369 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6370 | `		/* Length mismatch -> ValueError */` |
|         3 | 6371 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6372 | `			"ValueError",` |
|         - | 6373 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6374 | `			);` |
|         - | 6375 | `	}` |
|         - | 6376 | `	/* Create a new array */` |
|        11 | 6377 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6378 | `	if( pArray == 0 ){` |
|       ! 0 | 6379 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6380 | `		return PH7_OK;` |
|         - | 6381 | `	}` |
|         - | 6382 | `	/* Perform the requested operation */` |
|        11 | 6383 | `	pKe = pKey->pFirst;` |
|        11 | 6384 | `	pVe = pValue->pFirst;` |
|        33 | 6385 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6386 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6387 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6388 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6389 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6390 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6391 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6392 | `		 * original array must not be mutated. */` |
|        23 | 6393 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6394 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6395 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6396 | `			if( pTmpKey ){` |
|         5 | 6397 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6398 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6399 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6400 | `				pKeyCopy = pTmpKey;` |
|         2 | 6401 | `			}` |
|         2 | 6402 | `		}` |
|        23 | 6403 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6404 | `		/* Point to the next entry */` |
|        23 | 6405 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6406 | `		pVe = pVe->pPrev;` |
|        12 | 6407 | `	}` |
|         - | 6408 | `	/* Return the filled array */` |
|        11 | 6409 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6410 | `	return PH7_OK;` |
|        12 | 6411 | `}` |
|         - | 6412 | `/*` |
|         - | 6413 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6414 | ` *  Return an array with elements in reverse order.` |
|         - | 6415 | ` * Parameters` |
|         - | 6416 | ` *  $array` |
|         - | 6417 | ` *   The input array.` |
|         - | 6418 | ` *  $preserve_keys (optional)` |
|         - | 6419 | ` *   If set to TRUE keys are preserved.` |
|         - | 6420 | ` * Return` |
|         - | 6421 | ` *  The reversed array.` |
|         - | 6422 | ` */` |
|        18 | 6423 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 6424 | `{` |
|         - | 6425 | `	ph7_hashmap_node *pEntry;` |
|         - | 6426 | `	ph7_hashmap *pSrc;` |
|         - | 6427 | `	ph7_value *pArray;` |
|         - | 6428 | `	int bPreserve;` |
|         - | 6429 | `	sxu32 n;` |
|        20 | 6430 | `	if( nArg < 1 ){` |
|       ! 0 | 6431 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6432 | `			"ArgumentCountError",` |
|         - | 6433 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       ! 0 | 6434 | `			nArg` |
|         - | 6435 | `			);` |
|         - | 6436 | `	}` |
|         - | 6437 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6438 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6439 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6440 | `			"TypeError",` |
|         - | 6441 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6442 | `			ph7_type_name(apArg[0])` |
|         - | 6443 | `			);` |
|         - | 6444 | `	}` |
|        17 | 6445 | `	bPreserve = FALSE;` |
|        17 | 6446 | `	if( nArg > 1 ){` |
|         7 | 6447 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6448 | `	}` |
|         - | 6449 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6450 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6451 | `	/* Create a new array */` |
|        17 | 6452 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6453 | `	if( pArray == 0 ){` |
|       ! 0 | 6454 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6455 | `		return PH7_OK;` |
|         - | 6456 | `	}` |
|         - | 6457 | `	/* Perform the requested operation */` |
|        17 | 6458 | `	pEntry = pSrc->pLast;` |
|        55 | 6459 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6460 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6461 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6462 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6463 | `		/* Point to the previous entry */` |
|        39 | 6464 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6465 | `	}` |
|        17 | 6466 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6467 | `	return PH7_OK;` |
|        11 | 6468 | `}` |
|         - | 6469 | `/*` |
|         - | 6470 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6471 | ` *  Removes duplicate values from an array.` |
|         - | 6472 | ` * Parameters` |
|         - | 6473 | ` *  $array` |
|         - | 6474 | ` *   The input array.` |
|         - | 6475 | ` *  $flags` |
|         - | 6476 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6477 | ` *   behavior using these values:` |
|         - | 6478 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6479 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6480 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6481 | ` * Return` |
|         - | 6482 | ` *  The filtered array.` |
|         - | 6483 | ` */` |
|        22 | 6484 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6485 | `{` |
|         - | 6486 | `	ph7_hashmap_node *pEntry;` |
|         - | 6487 | `	ph7_value *pNeedle;` |
|         - | 6488 | `	ph7_hashmap *pSrc;` |
|         - | 6489 | `	ph7_value *pArray;` |
|         - | 6490 | `	int bStrict;` |
|         - | 6491 | `	sxi32 rc;` |
|         - | 6492 | `	sxu32 n;` |
|        25 | 6493 | `	if( nArg < 1 ){` |
|         - | 6494 | `		/* Missing arguments, throw ArgumentCountError */` |
|       ! 0 | 6495 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6496 | `			"ArgumentCountError",` |
|         - | 6497 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6498 | `			);` |
|         - | 6499 | `	}` |
|        25 | 6500 | `	if( nArg > 2 ){` |
|         - | 6501 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6502 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6503 | `			"ArgumentCountError",` |
|         - | 6504 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6505 | `			nArg` |
|         - | 6506 | `			);` |
|         - | 6507 | `	}` |
|         - | 6508 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6509 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6510 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6511 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6512 | `			"TypeError",` |
|         - | 6513 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6514 | `			ph7_type_name(apArg[0])` |
|         - | 6515 | `			);` |
|         - | 6516 | `	}` |
|        19 | 6517 | `	bStrict = FALSE;` |
|         - | 6518 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6519 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6520 | `	/* Create a new array */` |
|        19 | 6521 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6522 | `	if( pArray == 0 ){` |
|       ! 0 | 6523 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6524 | `		return PH7_OK;` |
|         - | 6525 | `	}` |
|         - | 6526 | `	/* Perform the requested operation */` |
|        19 | 6527 | `	pEntry = pSrc->pFirst;` |
|        83 | 6528 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6529 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6530 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6531 | `		if( pNeedle ){` |
|        65 | 6532 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6533 | `		}` |
|        65 | 6534 | `		if( rc != SXRET_OK ){` |
|         - | 6535 | `			/* Perform the insertion */` |
|        37 | 6536 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6537 | `		}` |
|         - | 6538 | `		/* Point to the next entry */` |
|        65 | 6539 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6540 | `	}` |
|         - | 6541 | `	/* Return the freshly created array */` |
|        19 | 6542 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6543 | `	return PH7_OK;` |
|        14 | 6544 | `}` |
|         - | 6545 | `/*` |
|         - | 6546 | ` * array array_flip(array $input)` |
|         - | 6547 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6548 | ` * Parameter` |
|         - | 6549 | ` *  $input` |
|         - | 6550 | ` *   Input array.` |
|         - | 6551 | ` * Return` |
|         - | 6552 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6553 | ` */` |
|        30 | 6554 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6555 | `{` |
|         - | 6556 | `	ph7_hashmap_node *pEntry;` |
|         - | 6557 | `	ph7_hashmap *pSrc;` |
|         - | 6558 | `	ph7_value *pArray;` |
|         - | 6559 | `	ph7_value *pKey;` |
|         - | 6560 | `	ph7_value sVal;` |
|         - | 6561 | `	sxu32 n;` |
|         - | 6562 |  |
|         - | 6563 | `	/* PHP requires exactly one argument */` |
|        33 | 6564 | `	if( nArg != 1 ){` |
|         - | 6565 | `		/* Use ArgumentCountError like other array helpers */` |
|         4 | 6566 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6567 | `			"ArgumentCountError",` |
|         - | 6568 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         1 | 6569 | `			nArg` |
|         - | 6570 | `			);` |
|         - | 6571 | `	}` |
|         - | 6572 | `	/* Make sure we are dealing with a valid hashmap */` |
|        30 | 6573 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6574 | `		/* Type mismatch -> TypeError */` |
|         4 | 6575 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6576 | `			"TypeError",` |
|         - | 6577 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6578 | `			ph7_type_name(apArg[0])` |
|         - | 6579 | `			);` |
|         - | 6580 | `	}` |
|         - | 6581 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6582 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6583 | `	/* Create a new array */` |
|        27 | 6584 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6585 | `	if( pArray == 0 ){` |
|       ! 0 | 6586 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6587 | `		return PH7_OK;` |
|         - | 6588 | `	}` |
|         - | 6589 | `	/* Start processing */` |
|        27 | 6590 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6591 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6592 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6593 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6594 | `		if( pKey ){` |
|         - | 6595 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6596 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6597 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6598 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6599 | `					);` |
|     22236 | 6600 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6601 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6602 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6603 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6604 | `				}else{` |
|         - | 6605 | `					SyString sStr;` |
|      2227 | 6606 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6607 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6608 | `				}` |
|         - | 6609 | `				/* Perform the insertion */` |
|     22227 | 6610 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6611 | `				/* Safely release the value because each inserted entry` |
|         - | 6612 | `				 * has its own private copy of the value.` |
|         - | 6613 | `				 */` |
|     22227 | 6614 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6615 | `			}else{` |
|         - | 6616 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6617 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6618 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6619 | `					);` |
|         - | 6620 | `			}` |
|     11118 | 6621 | `		}` |
|         - | 6622 | `		/* Point to the next entry */` |
|     22237 | 6623 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6624 | `	}` |
|         - | 6625 | `	/* Return the freshly created array */` |
|        27 | 6626 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6627 | `	return PH7_OK;` |
|        18 | 6628 | `}` |
|         - | 6629 | `/*` |
|         - | 6630 | ` * number array_sum(array $array )` |
|         - | 6631 | ` *  Calculate the sum of values in an array.` |
|         - | 6632 | ` * Parameters` |
|         - | 6633 | ` *  $array: The input array.` |
|         - | 6634 | ` * Return` |
|         - | 6635 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6636 | ` */` |
|        24 | 6637 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6638 | `{` |
|         - | 6639 | `	ph7_hashmap_node *pEntry;` |
|         - | 6640 | `	ph7_value *pObj;` |
|        26 | 6641 | `	double dSum = 0;` |
|         - | 6642 | `	sxu32 n;` |
|        26 | 6643 | `	pEntry = pMap->pFirst;` |
|        92 | 6644 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        68 | 6645 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        68 | 6646 | `		if( pObj ){` |
|        68 | 6647 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        30 | 6648 | `				dSum += pObj->rVal;` |
|        54 | 6649 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6650 | `				dSum += (double)pObj->x.iVal;` |
|        30 | 6651 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        16 | 6652 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6653 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|         - | 6654 | `					 * resource cases below already did; only this one was silent) */` |
|         3 | 6655 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6656 | `						"Addition is not supported on type string");` |
|        14 | 6657 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6658 | `					double dv = 0;` |
|        13 | 6659 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6660 | `					dSum += dv;` |
|         8 | 6661 | `				}` |
|        12 | 6662 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6663 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6664 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6665 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6666 | `				/* php names the CLASS here, not the literal word "object" */` |
|       ! 0 | 6667 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       ! 0 | 6668 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6669 | `					"Addition is not supported on type %s",` |
|       ! 0 | 6670 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         3 | 6671 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6672 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6673 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6674 | `			}` |
|         - | 6675 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6676 | `		}` |
|         - | 6677 | `		/* Point to the next entry */` |
|        68 | 6678 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 6679 | `	}` |
|         - | 6680 | `	/* Return sum */` |
|        26 | 6681 | `	ph7_result_double(pCtx,dSum);` |
|        26 | 6682 | `}` |
|       688 | 6683 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6684 | `{` |
|         - | 6685 | `	ph7_hashmap_node *pEntry;` |
|         - | 6686 | `	ph7_value *pObj;` |
|       690 | 6687 | `	sxi64 nSum = 0;` |
|         - | 6688 | `	sxu32 n;` |
|       690 | 6689 | `	pEntry = pMap->pFirst;` |
|      4702 | 6690 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4014 | 6691 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4014 | 6692 | `		if( pObj ){` |
|      4014 | 6693 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3994 | 6694 | `				nSum += pObj->x.iVal;` |
|      2018 | 6695 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        12 | 6696 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|         - | 6697 | `					/* php warns and SKIPS a non-numeric string */` |
|         5 | 6698 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6699 | `						"Addition is not supported on type string");` |
|        10 | 6700 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         8 | 6701 | `					sxi64 nv = 0;` |
|         8 | 6702 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         8 | 6703 | `					nSum += nv;` |
|         5 | 6704 | `				}` |
|        17 | 6705 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 6706 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6707 | `					"array_sum(): Addition is not supported on type array");` |
|        10 | 6708 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - | 6709 | `				/* php names the CLASS here, not the literal word "object" */` |
|         3 | 6710 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         5 | 6711 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|         - | 6712 | `					"Addition is not supported on type %s",` |
|         2 | 6713 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|         7 | 6714 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6715 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6716 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6717 | `			}` |
|         - | 6718 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      2006 | 6719 | `		}` |
|         - | 6720 | `		/* Point to the next entry */` |
|      4014 | 6721 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2008 | 6722 | `	}` |
|         - | 6723 | `	/* Return sum */` |
|       690 | 6724 | `	ph7_result_int64(pCtx,nSum);` |
|       690 | 6725 | `}` |
|         - | 6726 | `/* number array_sum(array $array )` |
|         - | 6727 | ` * (See block-coment above)` |
|         - | 6728 | ` */` |
|       724 | 6729 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6730 | `{` |
|         - | 6731 | `	ph7_hashmap_node *pEntry;` |
|         - | 6732 | `	ph7_hashmap *pMap;` |
|         - | 6733 | `	ph7_value *pObj;` |
|       728 | 6734 | `	int useDouble = 0;` |
|         - | 6735 | `	sxu32 n;` |
|         - | 6736 | `	/* PHP requires exactly one argument */` |
|       728 | 6737 | `	if( nArg != 1 ){` |
|         4 | 6738 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6739 | `			"ArgumentCountError",` |
|         - | 6740 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         1 | 6741 | `			nArg` |
|         - | 6742 | `			);` |
|         - | 6743 | `	}` |
|         - | 6744 | `	/* Make sure we are dealing with a valid hashmap */` |
|       725 | 6745 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6746 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6747 | `		char zBuf[64];` |
|         8 | 6748 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6749 | `			"TypeError",` |
|         - | 6750 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6751 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6752 | `			);` |
|         - | 6753 | `	}` |
|       720 | 6754 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       720 | 6755 | `	if( pMap->nEntry < 1 ){` |
|         - | 6756 | `		/* Nothing to compute,return 0 */` |
|         7 | 6757 | `		ph7_result_int(pCtx,0);` |
|         7 | 6758 | `		return PH7_OK;` |
|         - | 6759 | `	}` |
|         - | 6760 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6761 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6762 | `	 */` |
|       714 | 6763 | `	pEntry = pMap->pFirst;` |
|      4734 | 6764 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      4046 | 6765 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      4046 | 6766 | `		if( pObj ){` |
|      4046 | 6767 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        20 | 6768 | `				useDouble = 1;` |
|        20 | 6769 | `				break;` |
|         - | 6770 | `			}` |
|      4028 | 6771 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        18 | 6772 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        18 | 6773 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6774 | `				sxu32 i;` |
|        32 | 6775 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        22 | 6776 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6777 | `						useDouble = 1;` |
|         7 | 6778 | `						break;` |
|         - | 6779 | `					}` |
|         9 | 6780 | `				}` |
|        18 | 6781 | `				if( useDouble ){` |
|         7 | 6782 | `					break;` |
|         - | 6783 | `				}` |
|         5 | 6784 | `			}` |
|      2010 | 6785 | `		}` |
|      4022 | 6786 | `		pEntry = pEntry->pPrev;` |
|      2012 | 6787 | `	}` |
|       714 | 6788 | `	if( useDouble ){` |
|        26 | 6789 | `		DoubleSum(pCtx,pMap);` |
|        14 | 6790 | `	}else{` |
|       690 | 6791 | `		Int64Sum(pCtx,pMap);` |
|         - | 6792 | `	}` |
|       714 | 6793 | `	return PH7_OK;` |
|       366 | 6794 | `}` |
|         - | 6795 | `/*` |
|         - | 6796 | ` * number array_product(array $array )` |
|         - | 6797 | ` *  Calculate the product of values in an array.` |
|         - | 6798 | ` * Parameters` |
|         - | 6799 | ` *  $array: The input array.` |
|         - | 6800 | ` * Return` |
|         - | 6801 | ` *  Returns the product of values as an integer or float.` |
|         - | 6802 | ` */` |
|         2 | 6803 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6804 | `{` |
|         - | 6805 | `	ph7_hashmap_node *pEntry;` |
|         - | 6806 | `	ph7_value *pObj;` |
|         - | 6807 | `	double dProd;` |
|         - | 6808 | `	sxu32 n;` |
|         3 | 6809 | `	pEntry = pMap->pFirst;` |
|         3 | 6810 | `	dProd = 1;` |
|         7 | 6811 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6812 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6813 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6814 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6815 | `				dProd *= pObj->rVal;` |
|         4 | 6816 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6817 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6818 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6819 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6820 | `					double dv = 0;` |
|       ! 0 | 6821 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6822 | `					dProd *= dv;` |
|       ! 0 | 6823 | `				}` |
|       ! 0 | 6824 | `			}` |
|         2 | 6825 | `		}` |
|         - | 6826 | `		/* Point to the next entry */` |
|         5 | 6827 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6828 | `	}` |
|         - | 6829 | `	/* Return product */` |
|         3 | 6830 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6831 | `}` |
|         2 | 6832 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6833 | `{` |
|         - | 6834 | `	ph7_hashmap_node *pEntry;` |
|         - | 6835 | `	ph7_value *pObj;` |
|         - | 6836 | `	sxi64 nProd;` |
|         - | 6837 | `	sxu32 n;` |
|         3 | 6838 | `	pEntry = pMap->pFirst;` |
|         3 | 6839 | `	nProd = 1;` |
|         9 | 6840 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6841 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6842 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6843 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6844 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6845 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6846 | `				nProd *= pObj->x.iVal;` |
|         3 | 6847 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6848 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6849 | `					sxi64 nv = 0;` |
|       ! 0 | 6850 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6851 | `					nProd *= nv;` |
|       ! 0 | 6852 | `				}` |
|       ! 0 | 6853 | `			}` |
|         3 | 6854 | `		}` |
|         - | 6855 | `		/* Point to the next entry */` |
|         7 | 6856 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6857 | `	}` |
|         - | 6858 | `	/* Return product */` |
|         3 | 6859 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6860 | `}` |
|         - | 6861 | `/* number array_product(array $array )` |
|         - | 6862 | ` * (See block-block comment above)` |
|         - | 6863 | ` */` |
|        16 | 6864 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6865 | `{` |
|         - | 6866 | `	ph7_hashmap *pMap;` |
|         - | 6867 | `	ph7_value *pObj;` |
|        17 | 6868 | `	if( nArg < 1 ){` |
|         - | 6869 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6870 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6871 | `		return PH7_OK;` |
|         - | 6872 | `	}` |
|         - | 6873 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        17 | 6874 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6875 | `		char zBuf[64];` |
|        16 | 6876 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6877 | `			"TypeError",` |
|         - | 6878 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         5 | 6879 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6880 | `			);` |
|         - | 6881 | `	}` |
|         7 | 6882 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6883 | `	if( pMap->nEntry < 1 ){` |
|         - | 6884 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6885 | `		ph7_result_int(pCtx,1);` |
|         3 | 6886 | `		return PH7_OK;` |
|         - | 6887 | `	}` |
|         - | 6888 | `	/* If the first element is of type float,then perform floating` |
|         - | 6889 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6890 | `	 */` |
|         5 | 6891 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6892 | `	if( pObj == 0 ){` |
|       ! 0 | 6893 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6894 | `		return PH7_OK;` |
|         - | 6895 | `	}` |
|         5 | 6896 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6897 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6898 | `	}else{` |
|         3 | 6899 | `		Int64Prod(pCtx,pMap);` |
|         - | 6900 | `	}` |
|         5 | 6901 | `	return PH7_OK;` |
|         9 | 6902 | `}` |
|         - | 6903 | `/*` |
|         - | 6904 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6905 | ` *  Pick one or more random entries out of an array.` |
|         - | 6906 | ` * Parameters` |
|         - | 6907 | ` * $input` |
|         - | 6908 | ` *  The input array.` |
|         - | 6909 | ` * $num_req` |
|         - | 6910 | ` *  Specifies how many entries you want to pick.` |
|         - | 6911 | ` * Return` |
|         - | 6912 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6913 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6914 | ` *  NULL is returned on failure.` |
|         - | 6915 | ` */` |
|        36 | 6916 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6917 | `{` |
|         - | 6918 | `	ph7_hashmap_node *pNode;` |
|         - | 6919 | `	ph7_hashmap *pMap;` |
|        37 | 6920 | `	int nItem = 1;` |
|        37 | 6921 | `	if( nArg < 1 ){` |
|         - | 6922 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6923 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6924 | `		return PH7_OK;` |
|         - | 6925 | `	}` |
|         - | 6926 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        37 | 6927 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6928 | `		char zBuf[64];` |
|        10 | 6929 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6930 | `			"TypeError",` |
|         - | 6931 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6932 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6933 | `			);` |
|         - | 6934 | `	}` |
|         - | 6935 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6936 | `	 * check, matching its ZPP-before-body ordering. */` |
|        31 | 6937 | `	if( nArg > 1 ){` |
|        23 | 6938 | `		ph7_value *pNum = apArg[1];` |
|        22 | 6939 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        23 | 6940 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6941 | `			char zBuf[64];` |
|       ! 0 | 6942 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6943 | `				"TypeError",` |
|         - | 6944 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|       ! 0 | 6945 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6946 | `				);` |
|         - | 6947 | `		}` |
|        23 | 6948 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6949 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6950 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6951 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6952 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6953 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6954 | `			int len;` |
|         9 | 6955 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6956 | `			sxi64 iLong; double dReal;` |
|         9 | 6957 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6958 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6959 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6960 | `					"TypeError",` |
|         - | 6961 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6962 | `					);` |
|         - | 6963 | `			}` |
|         - | 6964 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6965 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6966 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6967 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6968 | `			}` |
|         3 | 6969 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6970 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6971 | `			nItem = (int)iLong;` |
|         2 | 6972 | `		}else{` |
|        15 | 6973 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6974 | `		}` |
|         8 | 6975 | `	}` |
|         - | 6976 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6977 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6978 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6979 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6980 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6981 | `			"ValueError",` |
|         - | 6982 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6983 | `			);` |
|         - | 6984 | `	}` |
|         - | 6985 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6986 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6987 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6988 | `			"ValueError",` |
|         - | 6989 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6990 | `			);` |
|         - | 6991 | `	}` |
|        13 | 6992 | `	if( nItem < 2 ){` |
|         - | 6993 | `		sxu32 nEntry;` |
|         - | 6994 | `		/* Select a random number */` |
|         9 | 6995 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6996 | `		/* Extract the desired entry.` |
|         - | 6997 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6998 | `		 */` |
|         9 | 6999 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         4 | 7000 | `			pNode = pMap->pLast;` |
|         4 | 7001 | `			nEntry = pMap->nEntry - nEntry;` |
|         4 | 7002 | `			if( nEntry > 1 ){` |
|       ! 0 | 7003 | `				for(;;){` |
|       ! 0 | 7004 | `					if( nEntry == 0 ){` |
|       ! 0 | 7005 | `						break;` |
|         - | 7006 | `					}` |
|         - | 7007 | `					/* Point to the previous entry */` |
|       ! 0 | 7008 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 7009 | `					nEntry--;` |
|       ! 0 | 7010 | `				}` |
|       ! 0 | 7011 | `			}` |
|         3 | 7012 | `		}else{` |
|         6 | 7013 | `			pNode = pMap->pFirst;` |
|         2 | 7014 | `			for(;;){` |
|         7 | 7015 | `				if( nEntry == 0 ){` |
|         6 | 7016 | `					break;` |
|         - | 7017 | `				}` |
|         - | 7018 | `				/* Point to the next entry */` |
|         2 | 7019 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         2 | 7020 | `				nEntry--;` |
|         1 | 7021 | `			}` |
|         - | 7022 | `		}` |
|         9 | 7023 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 7024 | `			/* Int key */` |
|         7 | 7025 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 7026 | `		}else{` |
|         - | 7027 | `			/* Blob key */` |
|         3 | 7028 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 7029 | `		}` |
|         5 | 7030 | `	}else{` |
|         - | 7031 | `		ph7_value sKey,*pArray;` |
|         - | 7032 | `		ph7_hashmap *pDest;` |
|         - | 7033 | `		/* Create a new array */` |
|         5 | 7034 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 7035 | `		if( pArray == 0 ){` |
|       ! 0 | 7036 | `			ph7_result_null(pCtx);` |
|       ! 0 | 7037 | `			return PH7_OK;` |
|         - | 7038 | `		}` |
|         - | 7039 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 7040 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 7041 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 7042 | `		/* Copy the first n items */` |
|         5 | 7043 | `		pNode = pMap->pFirst;` |
|         5 | 7044 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 7045 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 7046 | `		}` |
|        15 | 7047 | `		while( nItem > 0){` |
|        11 | 7048 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 7049 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 7050 | `			PH7_MemObjRelease(&sKey);` |
|         - | 7051 | `			/* Point to the next entry */` |
|        11 | 7052 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 7053 | `			nItem--;` |
|         1 | 7054 | `		}` |
|         - | 7055 | `		/* Shuffle the array */` |
|         5 | 7056 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 7057 | `		/* Rehash node */` |
|         5 | 7058 | `		HashmapSortRehash(pDest);` |
|         - | 7059 | `		/* Return the random array */` |
|         5 | 7060 | `		ph7_result_value(pCtx,pArray);` |
|         - | 7061 | `	}` |
|        13 | 7062 | `	return PH7_OK;` |
|        19 | 7063 | `}` |
|         - | 7064 | `/*` |
|         - | 7065 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 7066 | ` *  Split an array into chunks.` |
|         - | 7067 | ` * Parameters` |
|         - | 7068 | ` * $input` |
|         - | 7069 | ` *   The array to work on` |
|         - | 7070 | ` * $size` |
|         - | 7071 | ` *   The size of each chunk` |
|         - | 7072 | ` * $preserve_keys` |
|         - | 7073 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 7074 | ` *   the chunk numerically.` |
|         - | 7075 | ` * Return` |
|         - | 7076 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 7077 | ` *  zero, with each dimension containing size elements.` |
|         - | 7078 | ` */` |
|        36 | 7079 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7080 | `{` |
|         - | 7081 | `	ph7_value *pArray,*pChunk;` |
|         - | 7082 | `	ph7_hashmap_node *pEntry;` |
|         - | 7083 | `	ph7_hashmap *pMap;` |
|         - | 7084 | `	int bPreserve;` |
|         - | 7085 | `	sxu32 nChunk;` |
|         - | 7086 | `	sxu32 nSize;` |
|         - | 7087 | `	sxu32 n;` |
|         - | 7088 | `	/* Argument count and types follow PHP semantics. */` |
|        41 | 7089 | `	if( nArg < 2 ){` |
|         - | 7090 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       ! 0 | 7091 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7092 | `			"ArgumentCountError",` |
|         - | 7093 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7094 | `			nArg` |
|         - | 7095 | `			);` |
|         - | 7096 | `	}` |
|        41 | 7097 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7098 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7099 | `			"TypeError",` |
|         - | 7100 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7101 | `			ph7_type_name(apArg[0])` |
|         - | 7102 | `			);` |
|         - | 7103 | `	}` |
|         - | 7104 | `	/* Create a new array */` |
|        38 | 7105 | `	pArray = ph7_context_new_array(pCtx);` |
|        38 | 7106 | `	if( pArray == 0 ){` |
|       ! 0 | 7107 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7108 | `		return PH7_OK;` |
|         - | 7109 | `	}` |
|         - | 7110 | `	/* Point to the internal representation of the input hashmap */` |
|        38 | 7111 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7112 | `	/* Extract and validate the chunk size argument. */` |
|         - | 7113 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        51 | 7114 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        72 | 7115 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        34 | 7116 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 7117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7118 | `			"TypeError",` |
|         - | 7119 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7120 | `			ph7_type_name(apArg[1])` |
|         - | 7121 | `			);` |
|         - | 7122 | `	}` |
|         - | 7123 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 7124 | `	 * strings are permitted; however those representing floats lose` |
|         - | 7125 | `	 * precision and PHP emits a deprecation warning. */` |
|        38 | 7126 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7127 | `		int len;` |
|         3 | 7128 | `		sxu8 bReal = FALSE;` |
|         3 | 7129 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 7130 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 7131 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7132 | `				"TypeError",` |
|         - | 7133 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7134 | `				);` |
|         - | 7135 | `		}` |
|       ! 0 | 7136 | `		if( bReal ){` |
|         - | 7137 | `			/* float-string -> warn but allow */` |
|       ! 0 | 7138 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7139 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7140 | `				zStr` |
|         - | 7141 | `				);` |
|       ! 0 | 7142 | `		}` |
|       ! 0 | 7143 | `	}` |
|         - | 7144 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 7145 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 7146 | `	 * later via ph7_value_to_int. */` |
|        35 | 7147 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 7148 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 7149 | `		sxi64 i = (sxi64)d;` |
|         3 | 7150 | `		if( d != (double)i ){` |
|         4 | 7151 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 7152 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 7153 | `				d` |
|         - | 7154 | `				);` |
|         1 | 7155 | `		}` |
|         1 | 7156 | `	}` |
|         - | 7157 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 7158 | `	 * eliminated, this will not produce a warning. */` |
|         - | 7159 | `	{` |
|        35 | 7160 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        35 | 7161 | `		if( nSizeSigned < 1 ){` |
|         - | 7162 | `			/* size <= 0 -> ValueError */` |
|         6 | 7163 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7164 | `				"ValueError",` |
|         - | 7165 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7166 | `				);` |
|         - | 7167 | `		}` |
|        29 | 7168 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7169 | `	}` |
|        29 | 7170 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7171 | `		/* Return the whole array */` |
|         3 | 7172 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7173 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7174 | `		return PH7_OK;` |
|         - | 7175 | `	}` |
|        27 | 7176 | `	bPreserve = 0;` |
|        27 | 7177 | `	if( nArg > 2 ){` |
|         - | 7178 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7179 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7180 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7181 | `		 * normally, matching PHP behaviour. */` |
|        30 | 7182 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        31 | 7183 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7184 | `			ph7_value_is_resource(apArg[2]) ){` |
|       ! 0 | 7185 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7186 | `				"TypeError",` |
|         - | 7187 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       ! 0 | 7188 | `				ph7_type_name(apArg[2])` |
|         - | 7189 | `				);` |
|         - | 7190 | `		}` |
|        21 | 7191 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7192 | `	}` |
|         - | 7193 | `	/* Start processing */` |
|        27 | 7194 | `	pEntry = pMap->pFirst;` |
|        27 | 7195 | `	nChunk = 0;` |
|        27 | 7196 | `	pChunk = 0;` |
|        27 | 7197 | `	n = pMap->nEntry;` |
|        56 | 7198 | `	for( ;; ){` |
|       113 | 7199 | `		if( n < 1 ){` |
|         - | 7200 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7201 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7202 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7203 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7204 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7205 | `			 * exists. */` |
|        27 | 7206 | `			if( pChunk ){` |
|        27 | 7207 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7208 | `			}` |
|        27 | 7209 | `			break;` |
|         - | 7210 | `		}` |
|        87 | 7211 | `		if( nChunk < 1 ){` |
|        71 | 7212 | `			if( pChunk ){` |
|         - | 7213 | `				/* Put the first chunk */` |
|        45 | 7214 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7215 | `			}` |
|         - | 7216 | `			/* Create a new dimension */` |
|        71 | 7217 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7218 | `												   * will be automatically released as soon we return` |
|         - | 7219 | `												   * from this function */` |
|        71 | 7220 | `			if( pChunk == 0 ){` |
|       ! 0 | 7221 | `				break;` |
|         - | 7222 | `			}` |
|        71 | 7223 | `			nChunk = nSize;` |
|        35 | 7224 | `		}` |
|         - | 7225 | `		/* Insert the entry */` |
|        87 | 7226 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7227 | `		/* Point to the next entry */` |
|        87 | 7228 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7229 | `		nChunk--;` |
|        87 | 7230 | `		n--;` |
|         1 | 7231 | `	}` |
|         - | 7232 | `	/* Return the multidimensional array */` |
|        27 | 7233 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7234 | `	return PH7_OK;` |
|        23 | 7235 | `}` |
|         - | 7236 | `/*` |
|         - | 7237 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7238 | ` *  Pad array to the specified length with a value.` |
|         - | 7239 | ` * $input` |
|         - | 7240 | ` *   Initial array of values to pad.` |
|         - | 7241 | ` * $pad_size` |
|         - | 7242 | ` *   New size of the array.` |
|         - | 7243 | ` * $pad_value` |
|         - | 7244 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7245 | ` */` |
|         - | 7246 | `/*` |
|         - | 7247 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|         - | 7248 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|         - | 7249 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|         - | 7250 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|         - | 7251 | ` * independent of the input array's size and symmetric for negative lengths).` |
|         - | 7252 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|         - | 7253 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|         - | 7254 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|         - | 7255 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|         - | 7256 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|         - | 7257 | ` * propagate. The cap constant is shared with range()'s guards` |
|         - | 7258 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|         - | 7259 | ` */` |
|        50 | 7260 | `static sxi32 HashmapGuardArraySize(` |
|         - | 7261 | `	ph7_context *pCtx,` |
|         - | 7262 | `	const char *zFunc,     /* Function name for the message */` |
|         - | 7263 | `	int iArg,              /* 1-based argument position */` |
|         - | 7264 | `	const char *zParam     /* "$length"-style parameter name */,` |
|         - | 7265 | `	sxi64 nRequested       /* Absolute requested element count */` |
|         - | 7266 | `	)` |
|         1 | 7267 | `{` |
|        51 | 7268 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|        22 | 7269 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7270 | `			"ValueError",` |
|         - | 7271 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|         7 | 7272 | `			zFunc,iArg,zParam` |
|         - | 7273 | `			);` |
|         - | 7274 | `	}` |
|        37 | 7275 | `	return SXRET_OK;` |
|        26 | 7276 | `}` |
|        62 | 7277 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7278 | `{` |
|         - | 7279 | `	ph7_hashmap *pMap;` |
|         - | 7280 | `	ph7_value *pArray;` |
|         - | 7281 | `	sxi64 iLen,iAbs;` |
|         - | 7282 | `	int nEntry;` |
|         - | 7283 | `	sxi32 rc;` |
|        65 | 7284 | `	if( nArg != 3 ){` |
|         4 | 7285 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7286 | `			"ArgumentCountError",` |
|         - | 7287 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         1 | 7288 | `			nArg` |
|         - | 7289 | `			);` |
|         - | 7290 | `	}` |
|        62 | 7291 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7292 | `		char zBuf[64];` |
|        11 | 7293 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7294 | `			"TypeError",` |
|         - | 7295 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 7296 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7297 | `			);` |
|         - | 7298 | `	}` |
|         - | 7299 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7300 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7301 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7302 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        54 | 7303 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        55 | 7304 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7305 | `		char zBuf[64];` |
|       ! 0 | 7306 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7307 | `			"TypeError",` |
|         - | 7308 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 7309 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7310 | `			);` |
|         - | 7311 | `	}` |
|        55 | 7312 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7313 | `		int nStr;` |
|        11 | 7314 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7315 | `		sxi64 iLong; double dReal;` |
|        11 | 7316 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7317 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7318 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7319 | `				"TypeError",` |
|         - | 7320 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7321 | `				);` |
|         - | 7322 | `		}` |
|         7 | 7323 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         - | 7324 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|         - | 7325 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|         3 | 7326 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|       ! 0 | 7327 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7328 | `					"TypeError",` |
|         - | 7329 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7330 | `					);` |
|         - | 7331 | `			}` |
|         3 | 7332 | `			iLen = (sxi64)dReal;` |
|         3 | 7333 | `			if( (double)iLen != dReal ){` |
|       ! 0 | 7334 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|         - | 7335 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 7336 | `					zStr` |
|         - | 7337 | `					);` |
|       ! 0 | 7338 | `			}` |
|         2 | 7339 | `		}else{` |
|         5 | 7340 | `			iLen = iLong;` |
|         - | 7341 | `		}` |
|         4 | 7342 | `	}else{` |
|        45 | 7343 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|         - | 7344 | `	}` |
|         - | 7345 | `	/* Point to the internal representation of the input hashmap */` |
|        51 | 7346 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7347 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|         - | 7348 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|         - | 7349 | `	 * overflow). */` |
|        51 | 7350 | `	iAbs = iLen;` |
|        51 | 7351 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|        15 | 7352 | `		iAbs = -iAbs;` |
|         7 | 7353 | `	}` |
|        51 | 7354 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|        51 | 7355 | `	if( rc != SXRET_OK ){` |
|        15 | 7356 | `		return rc;` |
|         - | 7357 | `	}` |
|        37 | 7358 | `	nEntry = (int)iLen;` |
|         - | 7359 | `	/* Create a new array */` |
|        37 | 7360 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 7361 | `	if( pArray == 0 ){` |
|       ! 0 | 7362 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7363 | `	}` |
|        37 | 7364 | `	if( nEntry < 0 ){` |
|        11 | 7365 | `		nEntry = -nEntry;` |
|        11 | 7366 | `		if( nEntry > (int)pMap->nEntry ){` |
|         7 | 7367 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7368 | `			/* Insert given items first */` |
|        25 | 7369 | `			while( nEntry > 0 ){` |
|        19 | 7370 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7371 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7372 | `				}` |
|        19 | 7373 | `				nEntry--;` |
|         1 | 7374 | `			}` |
|         - | 7375 | `			/* Merge the two arrays */` |
|         7 | 7376 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         4 | 7377 | `		}else{` |
|         5 | 7378 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7379 | `		}` |
|        32 | 7380 | `	}else if( nEntry > 0 ){` |
|        25 | 7381 | `		if( nEntry > (int)pMap->nEntry ){` |
|        19 | 7382 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7383 | `			/* Merge the two arrays first */` |
|        19 | 7384 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7385 | `			/* Insert given items */` |
|       275 | 7386 | `			while( nEntry > 0 ){` |
|       257 | 7387 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7388 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7389 | `				}` |
|       257 | 7390 | `				nEntry--;` |
|         1 | 7391 | `			}` |
|        10 | 7392 | `		}else{` |
|         7 | 7393 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7394 | `		}` |
|        13 | 7395 | `	}else{` |
|         - | 7396 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7397 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7398 | `	}` |
|         - | 7399 | `	/* Return the new array */` |
|        37 | 7400 | `	ph7_result_value(pCtx,pArray);` |
|        37 | 7401 | `	return PH7_OK;` |
|        34 | 7402 | `}` |
|         - | 7403 | `/*` |
|         - | 7404 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7405 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7406 | ` * Parameters` |
|         - | 7407 | ` * $array` |
|         - | 7408 | ` *   The array in which elements are replaced.` |
|         - | 7409 | ` * $array1` |
|         - | 7410 | ` *   The array from which elements will be extracted.` |
|         - | 7411 | ` * ....` |
|         - | 7412 | ` *  More arrays from which elements will be extracted.` |
|         - | 7413 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7414 | ` * Return` |
|         - | 7415 | ` *  Returns an array.` |
|         - | 7416 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7417 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7418 | ` */` |
|        20 | 7419 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7420 | `{` |
|         - | 7421 | `	ph7_hashmap *pMap;` |
|         - | 7422 | `	ph7_value *pArray;` |
|         - | 7423 | `	int i;` |
|        23 | 7424 | `	if( nArg < 1 ){` |
|       ! 0 | 7425 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7426 | `			"ArgumentCountError",` |
|         - | 7427 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7428 | `			);` |
|         - | 7429 | `	}` |
|        23 | 7430 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7431 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7432 | `			"TypeError",` |
|         - | 7433 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7434 | `			ph7_type_name(apArg[0])` |
|         - | 7435 | `			);` |
|         - | 7436 | `	}` |
|         - | 7437 | `	/* Create a new array */` |
|        20 | 7438 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7439 | `	if( pArray == 0 ){` |
|       ! 0 | 7440 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7441 | `		return PH7_OK;` |
|         - | 7442 | `	}` |
|         - | 7443 | `	/* Overwrite from the first array */` |
|        20 | 7444 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7445 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7446 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7447 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7448 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7449 | `			/* Type mismatch -> TypeError */` |
|         4 | 7450 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7451 | `				"TypeError",` |
|         - | 7452 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7453 | `				i + 1,` |
|         2 | 7454 | `				ph7_type_name(apArg[i])` |
|         - | 7455 | `				);` |
|         - | 7456 | `		}` |
|         - | 7457 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7458 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7459 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7460 | `	}` |
|         - | 7461 | `	/* Return the new array */` |
|        17 | 7462 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7463 | `	return PH7_OK;` |
|        13 | 7464 | `}` |
|         - | 7465 | `/*` |
|         - | 7466 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7467 | ` *  Filters elements of an array using a callback function.` |
|         - | 7468 | ` * Parameters` |
|         - | 7469 | ` *  $input` |
|         - | 7470 | ` *    The array to iterate over` |
|         - | 7471 | ` * $callback` |
|         - | 7472 | ` *    The callback function to use` |
|         - | 7473 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7474 | ` *    will be removed.` |
|         - | 7475 | ` * Return` |
|         - | 7476 | ` *  The filtered array.` |
|         - | 7477 | ` */` |
|        30 | 7478 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7479 | `{` |
|         - | 7480 | `	ph7_hashmap_node *pEntry;` |
|         - | 7481 | `	ph7_hashmap *pMap;` |
|         - | 7482 | `	ph7_value *pArray;` |
|         - | 7483 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7484 | `	ph7_value *pValue;` |
|         - | 7485 | `	sxi32 rc;` |
|         - | 7486 | `	int keep;` |
|         - | 7487 | `	sxu32 n;` |
|        32 | 7488 | `	if( nArg < 1 ){` |
|         - | 7489 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7490 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7491 | `		return PH7_OK;` |
|         - | 7492 | `	}` |
|         - | 7493 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        32 | 7494 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7495 | `		char zBuf[64];` |
|        19 | 7496 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7497 | `			"TypeError",` |
|         - | 7498 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 7499 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7500 | `			);` |
|         - | 7501 | `	}` |
|         - | 7502 | `	/* Create a new array */` |
|        20 | 7503 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7504 | `	if( pArray == 0 ){` |
|       ! 0 | 7505 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7506 | `		return PH7_OK;` |
|         - | 7507 | `	}` |
|         - | 7508 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7509 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7510 | `	pEntry = pMap->pFirst;` |
|        20 | 7511 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7512 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7513 | `	/* Perform the requested operation */` |
|        78 | 7514 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7515 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7516 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7517 | `		if( pValue == 0 ){` |
|         - | 7518 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7519 | `			keep = FALSE;` |
|        64 | 7520 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7521 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7522 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7523 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7524 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7525 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7526 | `					int len;` |
|         3 | 7527 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7528 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7529 | `						"TypeError",` |
|         - | 7530 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7531 | `						zName` |
|         - | 7532 | `						);` |
|       ! 0 | 7533 | `				}else{` |
|       ! 0 | 7534 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7535 | `						"TypeError",` |
|         - | 7536 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7537 | `						ph7_type_name(apArg[1])` |
|         - | 7538 | `						);` |
|         - | 7539 | `				}` |
|         - | 7540 | `			}` |
|        33 | 7541 | `			keep = FALSE;` |
|        33 | 7542 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7543 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7544 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7545 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7546 | `				return PH7_EXCEPTION;` |
|         - | 7547 | `			}` |
|        31 | 7548 | `			if( rc == SXRET_OK ){` |
|         - | 7549 | `				/* Perform a boolean cast */` |
|        31 | 7550 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7551 | `			}` |
|        31 | 7552 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7553 | `		}else{` |
|         - | 7554 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7555 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7556 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7557 | `			 */` |
|        29 | 7558 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7559 | `		}` |
|        59 | 7560 | `		if( keep ){` |
|         - | 7561 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7562 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7563 | `		}` |
|         - | 7564 | `		/* Point to the next entry */` |
|        59 | 7565 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7566 | `	}` |
|        15 | 7567 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7568 | `	return PH7_OK;` |
|        17 | 7569 | `}` |
|         - | 7570 | `/*` |
|         - | 7571 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7572 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7573 | ` * Parameters` |
|         - | 7574 | ` *  $callback` |
|         - | 7575 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7576 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7577 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7578 | ` *   are zipped together.` |
|         - | 7579 | ` *  $array` |
|         - | 7580 | ` *   The first array to run through the callback function.` |
|         - | 7581 | ` *  $arrays` |
|         - | 7582 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7583 | ` * Return` |
|         - | 7584 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7585 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7586 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7587 | ` *  padding shorter arrays with NULL.` |
|         - | 7588 | ` */` |
|        58 | 7589 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 7590 | `{` |
|         - | 7591 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7592 | `	ph7_hashmap_node *pEntry;` |
|         - | 7593 | `	ph7_hashmap *pMap;` |
|         - | 7594 | `	ph7_vm *pVm;` |
|         - | 7595 | `	int bNullCallback;` |
|         - | 7596 | `	sxi32 rc;` |
|         - | 7597 | `	int i;` |
|         - | 7598 | `	sxu32 n;` |
|        61 | 7599 | `	if( nArg < 2 ){` |
|       ! 0 | 7600 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7601 | `			"ArgumentCountError",` |
|         - | 7602 | `			"array_map() expects at least 2 arguments, %d given",` |
|       ! 0 | 7603 | `			nArg` |
|         - | 7604 | `			);` |
|         - | 7605 | `	}` |
|        61 | 7606 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        61 | 7607 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7608 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7609 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7610 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7611 | `				"TypeError",` |
|         - | 7612 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7613 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7614 | `				zFunc` |
|         - | 7615 | `				);` |
|         - | 7616 | `		}` |
|         3 | 7617 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7618 | `			"TypeError",` |
|         - | 7619 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7620 | `			"no array or string given"` |
|         - | 7621 | `			);` |
|         - | 7622 | `	}` |
|         - | 7623 | `	/* Every remaining argument must be an array */` |
|       121 | 7624 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        69 | 7625 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7626 | `			if( i == 1 ){` |
|         4 | 7627 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7628 | `					"TypeError",` |
|         - | 7629 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7630 | `					ph7_type_name(apArg[1])` |
|         - | 7631 | `					);` |
|         - | 7632 | `			}` |
|       ! 0 | 7633 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7634 | `				"TypeError",` |
|         - | 7635 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7636 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7637 | `				);` |
|         - | 7638 | `		}` |
|        34 | 7639 | `	}` |
|        54 | 7640 | `	pVm = pCtx->pVm;` |
|         - | 7641 | `	/* Create a new array */` |
|        54 | 7642 | `	pArray = ph7_context_new_array(pCtx);` |
|        54 | 7643 | `	if( pArray == 0 ){` |
|       ! 0 | 7644 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7645 | `		return PH7_OK;` |
|         - | 7646 | `	}` |
|        54 | 7647 | `	PH7_MemObjInit(pVm,&sResult);` |
|        54 | 7648 | `	PH7_MemObjInit(pVm,&sKey);` |
|        54 | 7649 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7650 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        54 | 7651 | `	if( nArg == 2 ){` |
|         - | 7652 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        44 | 7653 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        44 | 7654 | `		pEntry = pMap->pFirst;` |
|       134 | 7655 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7656 | `			/* Extract the node value */` |
|        96 | 7657 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        96 | 7658 | `			if( pValue ){` |
|         - | 7659 | `				/* Extract the node key */` |
|        96 | 7660 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        96 | 7661 | `				if( bNullCallback ){` |
|         - | 7662 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7663 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7664 | `				}else{` |
|         - | 7665 | `					/* Invoke the supplied callback */` |
|        86 | 7666 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        86 | 7667 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7668 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7669 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7670 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7671 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7672 | `						return PH7_EXCEPTION;` |
|         - | 7673 | `					}` |
|         - | 7674 | `					/* Insert the callback return value */` |
|        82 | 7675 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7676 | `				}` |
|        92 | 7677 | `				PH7_MemObjRelease(&sKey);` |
|        92 | 7678 | `				PH7_MemObjRelease(&sResult);` |
|        45 | 7679 | `			}` |
|         - | 7680 | `			/* Point to the next entry */` |
|        92 | 7681 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        47 | 7682 | `		}` |
|        21 | 7683 | `	}else{` |
|         - | 7684 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7685 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7686 | `		int nArrays = nArg - 1;` |
|         - | 7687 | `		ph7_hashmap_node **apCur;` |
|         - | 7688 | `		ph7_value **apCallArg;` |
|         - | 7689 | `		ph7_value sNull;` |
|        11 | 7690 | `		sxu32 nMax = 0;` |
|        11 | 7691 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7692 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7693 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7694 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7695 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7696 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7697 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7698 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7699 | `			return PH7_OK;` |
|         - | 7700 | `		}` |
|        11 | 7701 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7702 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7703 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7704 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7705 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7706 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7707 | `				nMax = pMap->nEntry;` |
|         6 | 7708 | `			}` |
|        12 | 7709 | `		}` |
|        35 | 7710 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7711 | `			ph7_value *pZip = 0;` |
|        25 | 7712 | `			if( bNullCallback ){` |
|         - | 7713 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7714 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7715 | `			}` |
|        79 | 7716 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7717 | `				ph7_value *pv = &sNull;` |
|        55 | 7718 | `				if( apCur[i] ){` |
|        53 | 7719 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7720 | `					if( pNodeVal ){` |
|        53 | 7721 | `						pv = pNodeVal;` |
|        26 | 7722 | `					}` |
|        53 | 7723 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7724 | `				}` |
|        55 | 7725 | `				if( bNullCallback ){` |
|         9 | 7726 | `					if( pZip ){` |
|         9 | 7727 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7728 | `					}` |
|         5 | 7729 | `				}else{` |
|        47 | 7730 | `					apCallArg[i] = pv;` |
|         - | 7731 | `				}` |
|        28 | 7732 | `			}` |
|        25 | 7733 | `			if( bNullCallback ){` |
|         5 | 7734 | `				if( pZip ){` |
|         5 | 7735 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7736 | `				}` |
|         3 | 7737 | `			}else{` |
|        21 | 7738 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7739 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7740 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7741 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7742 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7743 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7744 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7745 | `					return PH7_EXCEPTION;` |
|         - | 7746 | `				}` |
|        21 | 7747 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7748 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7749 | `			}` |
|        13 | 7750 | `		}` |
|        11 | 7751 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7752 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7753 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7754 | `	}` |
|        50 | 7755 | `	PH7_MemObjRelease(&sKey);` |
|        50 | 7756 | `	PH7_MemObjRelease(&sResult);` |
|        50 | 7757 | `	ph7_result_value(pCtx,pArray);` |
|        50 | 7758 | `	return PH7_OK;` |
|        32 | 7759 | `}` |
|         - | 7760 | `/*` |
|         - | 7761 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7762 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7763 | ` * Parameters` |
|         - | 7764 | ` *  $array` |
|         - | 7765 | ` *   The input array.` |
|         - | 7766 | ` *  $callback` |
|         - | 7767 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7768 | ` *  $initial` |
|         - | 7769 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7770 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7771 | ` * Return` |
|         - | 7772 | ` *  Returns the resulting value.` |
|         - | 7773 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7774 | ` */` |
|        30 | 7775 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7776 | `{` |
|         - | 7777 | `	ph7_hashmap_node *pEntry;` |
|         - | 7778 | `	ph7_hashmap *pMap;` |
|         - | 7779 | `	ph7_value *pValue;` |
|         - | 7780 | `	ph7_value sResult;` |
|         - | 7781 | `	sxi32 rc;` |
|         - | 7782 | `	sxu32 n;` |
|        35 | 7783 | `	if( nArg < 2 ){` |
|       ! 0 | 7784 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7785 | `			"ArgumentCountError",` |
|         - | 7786 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       ! 0 | 7787 | `			nArg` |
|         - | 7788 | `			);` |
|         - | 7789 | `	}` |
|        35 | 7790 | `	if( nArg > 3 ){` |
|         4 | 7791 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7792 | `			"ArgumentCountError",` |
|         - | 7793 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7794 | `			nArg` |
|         - | 7795 | `			);` |
|         - | 7796 | `	}` |
|        33 | 7797 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7798 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7799 | `			"TypeError",` |
|         - | 7800 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7801 | `			ph7_type_name(apArg[0])` |
|         - | 7802 | `			);` |
|         - | 7803 | `	}` |
|        31 | 7804 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7805 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7806 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7807 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7808 | `				"TypeError",` |
|         - | 7809 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7810 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7811 | `				zFunc` |
|         - | 7812 | `				);` |
|         - | 7813 | `		}` |
|         9 | 7814 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7815 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7816 | `				"TypeError",` |
|         - | 7817 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7818 | `				"array callback must have exactly two members"` |
|         - | 7819 | `				);` |
|         - | 7820 | `		}` |
|         6 | 7821 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7822 | `			"TypeError",` |
|         - | 7823 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7824 | `			"no array or string given"` |
|         - | 7825 | `			);` |
|         - | 7826 | `	}` |
|         - | 7827 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7828 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7829 | `	/* Assume a NULL initial value */` |
|        19 | 7830 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7831 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7832 | `	if( nArg > 2 ){` |
|         - | 7833 | `		/* Set the initial value */` |
|        13 | 7834 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7835 | `	}` |
|         - | 7836 | `	/* Perform the requested operation */` |
|        19 | 7837 | `	pEntry = pMap->pFirst;` |
|        55 | 7838 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7839 | `		/* Extract the node value */` |
|        39 | 7840 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7841 | `		/* Invoke the supplied callback */` |
|        39 | 7842 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7843 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7844 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7845 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7846 | `			return PH7_EXCEPTION;` |
|         - | 7847 | `		}` |
|         - | 7848 | `		/* Point to the next entry */` |
|        37 | 7849 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7850 | `	}` |
|        17 | 7851 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7852 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7853 | `	return PH7_OK;` |
|        20 | 7854 | `}` |
|         - | 7855 | `/*` |
|         - | 7856 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7857 | ` *  Apply a user function to every member of an array.` |
|         - | 7858 | ` * Parameters` |
|         - | 7859 | ` *  $array` |
|         - | 7860 | ` *   The input array.` |
|         - | 7861 | ` *  $funcname` |
|         - | 7862 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7863 | ` *   the first, and the key/index second.` |
|         - | 7864 | ` * Note:` |
|         - | 7865 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7866 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7867 | ` *  be made in the original array itself.` |
|         - | 7868 | ` *  $userdata` |
|         - | 7869 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7870 | ` *   to the callback funcname.` |
|         - | 7871 | ` * Return` |
|         - | 7872 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7873 | ` */` |
|        34 | 7874 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7875 | `{` |
|         - | 7876 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7877 | `	ph7_hashmap_node *pEntry;` |
|         - | 7878 | `	ph7_hashmap *pMap;` |
|         - | 7879 | `	sxu32 n;` |
|        39 | 7880 | `	if( nArg < 2 ){` |
|       ! 0 | 7881 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7882 | `			"ArgumentCountError",` |
|         - | 7883 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       ! 0 | 7884 | `			nArg` |
|         - | 7885 | `			);` |
|         - | 7886 | `	}` |
|        39 | 7887 | `	if( nArg > 3 ){` |
|         4 | 7888 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7889 | `			"ArgumentCountError",` |
|         - | 7890 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7891 | `			nArg` |
|         - | 7892 | `			);` |
|         - | 7893 | `	}` |
|        37 | 7894 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7895 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7896 | `			"TypeError",` |
|         - | 7897 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7898 | `			ph7_type_name(apArg[0])` |
|         - | 7899 | `			);` |
|         - | 7900 | `	}` |
|        35 | 7901 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7902 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7903 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7904 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7905 | `				"TypeError",` |
|         - | 7906 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7907 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7908 | `				zFunc` |
|         - | 7909 | `				);` |
|         - | 7910 | `		}` |
|        12 | 7911 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7912 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7913 | `				"TypeError",` |
|         - | 7914 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7915 | `				"array callback must have exactly two members"` |
|         - | 7916 | `				);` |
|         - | 7917 | `		}` |
|         6 | 7918 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7919 | `			"TypeError",` |
|         - | 7920 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7921 | `			"no array or string given"` |
|         - | 7922 | `			);` |
|         - | 7923 | `	}` |
|        21 | 7924 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7925 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7926 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7927 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7928 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7929 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7930 | `	/* Perform the desired operation */` |
|        21 | 7931 | `	pEntry = pMap->pFirst;` |
|        61 | 7932 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7933 | `		/* Extract the node value */` |
|        43 | 7934 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7935 | `		if( pValue ){` |
|         - | 7936 | `			sxi32 rcW;` |
|         - | 7937 | `			/* Extract the entry key */` |
|        43 | 7938 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7939 | `			/* Invoke the supplied callback */` |
|        43 | 7940 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7941 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7942 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7943 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7944 | `				return PH7_EXCEPTION;` |
|         - | 7945 | `			}` |
|        20 | 7946 | `		}` |
|         - | 7947 | `		/* Point to the next entry */` |
|        41 | 7948 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7949 | `	}` |
|         - | 7950 | `	/* All done, return TRUE */` |
|        19 | 7951 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7952 | `	return PH7_OK;` |
|        22 | 7953 | `}` |
|         - | 7954 | `/*` |
|         - | 7955 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7956 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7957 | ` */` |
|        22 | 7958 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7959 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7960 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7961 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7962 | `	int iNest             /* Nesting level */` |
|         - | 7963 | `	)` |
|         1 | 7964 | `{` |
|         - | 7965 | `	ph7_hashmap_node *pEntry;` |
|         - | 7966 | `	ph7_value *pValue,sKey;` |
|         - | 7967 | `	sxi32 rc;` |
|         - | 7968 | `	sxu32 n;` |
|         - | 7969 | `	/* Iterate through hashmap entries */` |
|        23 | 7970 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7971 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7972 | `	pEntry = pMap->pFirst;` |
|        59 | 7973 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7974 | `		/* Extract the node value */` |
|        37 | 7975 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7976 | `		if( pValue ){` |
|        37 | 7977 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7978 | `				if( iNest < 32 ){` |
|         - | 7979 | `					/* Recurse */` |
|        11 | 7980 | `					iNest++;` |
|        11 | 7981 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7982 | `					iNest--;` |
|        11 | 7983 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7984 | `						return PH7_EXCEPTION;` |
|         - | 7985 | `					}` |
|         5 | 7986 | `				}` |
|         6 | 7987 | `			}else{` |
|         - | 7988 | `				/* Extract the node key */` |
|        27 | 7989 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7990 | `				/* Invoke the supplied callback */` |
|        27 | 7991 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7992 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7993 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7994 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7995 | `					return PH7_EXCEPTION;` |
|         - | 7996 | `				}` |
|         - | 7997 | `			}` |
|        18 | 7998 | `		}` |
|         - | 7999 | `		/* Point to the next entry */` |
|        37 | 8000 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 8001 | `	}` |
|        23 | 8002 | `	return PH7_OK;` |
|        12 | 8003 | `}` |
|         - | 8004 | `/*` |
|         - | 8005 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 8006 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 8007 | ` * Parameters` |
|         - | 8008 | ` *  $array` |
|         - | 8009 | ` *   The input array.` |
|         - | 8010 | ` *  $funcname` |
|         - | 8011 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 8012 | ` *   the first, and the key/index second.` |
|         - | 8013 | ` * Note:` |
|         - | 8014 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 8015 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 8016 | ` *  be made in the original array itself.` |
|         - | 8017 | ` *  $userdata` |
|         - | 8018 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 8019 | ` *   to the callback funcname.` |
|         - | 8020 | ` * Return` |
|         - | 8021 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 8022 | ` */` |
|        26 | 8023 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 8024 | `{` |
|         - | 8025 | `	ph7_hashmap *pMap;` |
|        31 | 8026 | `	if( nArg < 2 ){` |
|       ! 0 | 8027 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8028 | `			"ArgumentCountError",` |
|         - | 8029 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       ! 0 | 8030 | `			nArg` |
|         - | 8031 | `			);` |
|         - | 8032 | `	}` |
|        31 | 8033 | `	if( nArg > 3 ){` |
|         4 | 8034 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8035 | `			"ArgumentCountError",` |
|         - | 8036 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 8037 | `			nArg` |
|         - | 8038 | `			);` |
|         - | 8039 | `	}` |
|        29 | 8040 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8041 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8042 | `			"TypeError",` |
|         - | 8043 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8044 | `			ph7_type_name(apArg[0])` |
|         - | 8045 | `			);` |
|         - | 8046 | `	}` |
|        27 | 8047 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 8048 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 8049 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 8050 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8051 | `				"TypeError",` |
|         - | 8052 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8053 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 8054 | `				zFunc` |
|         - | 8055 | `				);` |
|         - | 8056 | `		}` |
|        12 | 8057 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 8058 | `			return PH7_VmThrowException(pCtx,` |
|         - | 8059 | `				"TypeError",` |
|         - | 8060 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8061 | `				"array callback must have exactly two members"` |
|         - | 8062 | `				);` |
|         - | 8063 | `		}` |
|         6 | 8064 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8065 | `			"TypeError",` |
|         - | 8066 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 8067 | `			"no array or string given"` |
|         - | 8068 | `			);` |
|         - | 8069 | `	}` |
|         - | 8070 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 8071 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 8072 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 8073 | `	/* Perform the desired operation */` |
|        13 | 8074 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 8075 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8076 | `		return PH7_EXCEPTION;` |
|         - | 8077 | `	}` |
|         - | 8078 | `	/* All done, return TRUE */` |
|        13 | 8079 | `	ph7_result_bool(pCtx,1);` |
|        13 | 8080 | `	return PH7_OK;` |
|        18 | 8081 | `}` |
|         - | 8082 | `/*` |
|         - | 8083 | ` * bool array_is_list(array $array)` |
|         - | 8084 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 8085 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 8086 | ` * Return` |
|         - | 8087 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 8088 | ` */` |
|         - | 8089 | `/*` |
|         - | 8090 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 8091 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 8092 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 8093 | ` */` |
|       246 | 8094 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 8095 | `{` |
|       247 | 8096 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       247 | 8097 | `	sxi64 iExpect = 0;` |
|         - | 8098 | `	sxu32 n;` |
|       555 | 8099 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       409 | 8100 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 8101 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|       101 | 8102 | `			return 0;` |
|         - | 8103 | `		}` |
|       309 | 8104 | `		++iExpect;` |
|       309 | 8105 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       155 | 8106 | `	}` |
|       147 | 8107 | `	return 1;` |
|       124 | 8108 | `}` |
|        12 | 8109 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8110 | `{` |
|        13 | 8111 | `	if( nArg < 1 ){` |
|       ! 0 | 8112 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8113 | `			"ArgumentCountError",` |
|         - | 8114 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 8115 | `			);` |
|         - | 8116 | `	}` |
|        13 | 8117 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8118 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8119 | `			"TypeError",` |
|         - | 8120 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8121 | `			ph7_type_name(apArg[0])` |
|         - | 8122 | `			);` |
|         - | 8123 | `	}` |
|        13 | 8124 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 8125 | `	return PH7_OK;` |
|         7 | 8126 | `}` |
|         - | 8127 | `/*` |
|         - | 8128 | ` * mixed array_first(array $array)` |
|         - | 8129 | ` * mixed array_last(array $array)` |
|         - | 8130 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 8131 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8132 | ` *  untouched (unlike reset()/end()).` |
|         - | 8133 | ` */` |
|        18 | 8134 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8135 | `{` |
|         - | 8136 | `	ph7_hashmap *pMap;` |
|         - | 8137 | `	ph7_hashmap_node *pNode;` |
|         - | 8138 | `	ph7_value *pVal;` |
|        19 | 8139 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        19 | 8140 | `	if( nArg < 1 ){` |
|       ! 0 | 8141 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8142 | `			"ArgumentCountError",` |
|         - | 8143 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8144 | `			zName` |
|         - | 8145 | `			);` |
|         - | 8146 | `	}` |
|        19 | 8147 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8148 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8149 | `			"TypeError",` |
|         - | 8150 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8151 | `			zName,` |
|         1 | 8152 | `			ph7_type_name(apArg[0])` |
|         - | 8153 | `			);` |
|         - | 8154 | `	}` |
|        17 | 8155 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 8156 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 8157 | `	if( pNode == 0 ){` |
|         - | 8158 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8159 | `		ph7_result_null(pCtx);` |
|         5 | 8160 | `		return PH7_OK;` |
|         - | 8161 | `	}` |
|        13 | 8162 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 8163 | `	if( pVal ){` |
|        13 | 8164 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 8165 | `	}else{` |
|       ! 0 | 8166 | `		ph7_result_null(pCtx);` |
|         - | 8167 | `	}` |
|        13 | 8168 | `	return PH7_OK;` |
|        10 | 8169 | `}` |
|         8 | 8170 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8171 | `{` |
|         9 | 8172 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8173 | `}` |
|        10 | 8174 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8175 | `{` |
|        11 | 8176 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8177 | `}` |
|         - | 8178 | `/*` |
|         - | 8179 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 8180 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 8181 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 8182 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 8183 | ` *  untouched.` |
|         - | 8184 | ` */` |
|        22 | 8185 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 8186 | `{` |
|         - | 8187 | `	ph7_hashmap *pMap;` |
|         - | 8188 | `	ph7_hashmap_node *pNode;` |
|        23 | 8189 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        23 | 8190 | `	if( nArg < 1 ){` |
|       ! 0 | 8191 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8192 | `			"ArgumentCountError",` |
|         - | 8193 | `			"%s() expects exactly 1 argument, 0 given",` |
|       ! 0 | 8194 | `			zName` |
|         - | 8195 | `			);` |
|         - | 8196 | `	}` |
|        23 | 8197 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 8198 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8199 | `			"TypeError",` |
|         - | 8200 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 8201 | `			zName,` |
|         1 | 8202 | `			ph7_type_name(apArg[0])` |
|         - | 8203 | `			);` |
|         - | 8204 | `	}` |
|        21 | 8205 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 8206 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 8207 | `	if( pNode == 0 ){` |
|         - | 8208 | `		/* Empty array: PHP returns NULL */` |
|         5 | 8209 | `		ph7_result_null(pCtx);` |
|         5 | 8210 | `		return PH7_OK;` |
|         - | 8211 | `	}` |
|        17 | 8212 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 8213 | `	return PH7_OK;` |
|        12 | 8214 | `}` |
|        10 | 8215 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8216 | `{` |
|        11 | 8217 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 8218 | `}` |
|        12 | 8219 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8220 | `{` |
|        13 | 8221 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 8222 | `}` |
|         - | 8223 | `/*` |
|         - | 8224 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 8225 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8226 | ` * array_column() for both the column value and the index key.` |
|         - | 8227 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8228 | ` * container or the key is absent.` |
|         - | 8229 | ` */` |
|        32 | 8230 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8231 | `{` |
|        33 | 8232 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8233 | `		ph7_hashmap_node *pNode;` |
|        25 | 8234 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8235 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8236 | `		}` |
|        11 | 8237 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8238 | `		ph7_value sName;` |
|         - | 8239 | `		const char *zName;` |
|         - | 8240 | `		ph7_value *pAttr;` |
|         - | 8241 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8242 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8243 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8244 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8245 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8246 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8247 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8248 | `		return pAttr;` |
|         - | 8249 | `	}` |
|         5 | 8250 | `	return 0;` |
|        17 | 8251 | `}` |
|         - | 8252 | `/*` |
|         - | 8253 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8254 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8255 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8256 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8257 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8258 | ` *  Each row may be an array or an object.` |
|         - | 8259 | ` */` |
|        12 | 8260 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8261 | `{` |
|         - | 8262 | `	ph7_hashmap_node *pNode;` |
|         - | 8263 | `	ph7_hashmap *pMap;` |
|         - | 8264 | `	ph7_value *pArray;` |
|         - | 8265 | `	ph7_value *pRow;` |
|         - | 8266 | `	ph7_value *pCol;` |
|         - | 8267 | `	ph7_value *pIdx;` |
|         - | 8268 | `	int bWantCol;` |
|         - | 8269 | `	int bWantIdx;` |
|         - | 8270 | `	sxu32 n;` |
|        13 | 8271 | `	if( nArg < 2 ){` |
|       ! 0 | 8272 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8273 | `			"ArgumentCountError",` |
|         - | 8274 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8275 | `			nArg` |
|         - | 8276 | `			);` |
|         - | 8277 | `	}` |
|        13 | 8278 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8279 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8280 | `			"TypeError",` |
|         - | 8281 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8282 | `			ph7_type_name(apArg[0])` |
|         - | 8283 | `			);` |
|         - | 8284 | `	}` |
|        13 | 8285 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8286 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8287 | `	if( pArray == 0 ){` |
|       ! 0 | 8288 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8289 | `		return PH7_OK;` |
|         - | 8290 | `	}` |
|         - | 8291 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8292 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8293 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8294 | `	pNode = pMap->pFirst;` |
|        33 | 8295 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8296 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8297 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8298 | `		if( pRow == 0 ){` |
|       ! 0 | 8299 | `			continue;` |
|         - | 8300 | `		}` |
|        21 | 8301 | `		if( bWantCol ){` |
|        19 | 8302 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8303 | `			if( pCol == 0 ){` |
|         - | 8304 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8305 | `				continue;` |
|         - | 8306 | `			}` |
|         9 | 8307 | `		}else{` |
|         3 | 8308 | `			pCol = pRow;` |
|         - | 8309 | `		}` |
|        19 | 8310 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8311 | `		if( pIdx ){` |
|        13 | 8312 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8313 | `		}else{` |
|         7 | 8314 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8315 | `		}` |
|        10 | 8316 | `	}` |
|        13 | 8317 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8318 | `	return PH7_OK;` |
|         7 | 8319 | `}` |
|         - | 8320 | `/*` |
|         - | 8321 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8322 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8323 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8324 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8325 | ` */` |
|        28 | 8326 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8327 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8328 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8329 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8330 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8331 | `	)` |
|         1 | 8332 | `{` |
|         - | 8333 | `	ph7_hashmap_node *pEntry;` |
|         - | 8334 | `	ph7_hashmap *pMap;` |
|         - | 8335 | `	ph7_value *pValue;` |
|         - | 8336 | `	ph7_value *apCbArg[2];` |
|         - | 8337 | `	ph7_value sKey;` |
|         - | 8338 | `	ph7_value sResult;` |
|         - | 8339 | `	sxi32 rc;` |
|         - | 8340 | `	sxu32 n;` |
|        29 | 8341 | `	*ppMatch = 0;` |
|        29 | 8342 | `	if( nArg < 2 ){` |
|       ! 0 | 8343 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8344 | `			"ArgumentCountError",` |
|         - | 8345 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8346 | `			zName,nArg` |
|         - | 8347 | `			);` |
|         - | 8348 | `	}` |
|        29 | 8349 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8350 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8351 | `			"TypeError",` |
|         - | 8352 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8353 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8354 | `			);` |
|         - | 8355 | `	}` |
|        29 | 8356 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8357 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8358 | `			"TypeError",` |
|         - | 8359 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8360 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8361 | `			);` |
|         - | 8362 | `	}` |
|        29 | 8363 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8364 | `	pEntry = pMap->pFirst;` |
|        29 | 8365 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8366 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8367 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8368 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8369 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8370 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8371 | `		if( pValue ){` |
|         - | 8372 | `			/* The callback receives ($value, $key). */` |
|        59 | 8373 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8374 | `			apCbArg[0] = pValue;` |
|        59 | 8375 | `			apCbArg[1] = &sKey;` |
|        59 | 8376 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8377 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8378 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8379 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8380 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8381 | `				return PH7_EXCEPTION;` |
|         - | 8382 | `			}` |
|        59 | 8383 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8384 | `				*ppMatch = pEntry;` |
|        15 | 8385 | `				break;` |
|         - | 8386 | `			}` |
|        22 | 8387 | `		}` |
|        45 | 8388 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8389 | `	}` |
|        29 | 8390 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8391 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8392 | `	return PH7_OK;` |
|        15 | 8393 | `}` |
|         - | 8394 | `/*` |
|         - | 8395 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8396 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8397 | ` *  is truthy, or NULL if none match.` |
|         - | 8398 | ` */` |
|         6 | 8399 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8400 | `{` |
|         - | 8401 | `	ph7_hashmap_node *pMatch;` |
|         - | 8402 | `	ph7_value *pVal;` |
|         - | 8403 | `	sxi32 rc;` |
|         7 | 8404 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8405 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8406 | `		return rc;` |
|         - | 8407 | `	}` |
|         7 | 8408 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8409 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8410 | `	}else{` |
|         3 | 8411 | `		ph7_result_null(pCtx);` |
|         - | 8412 | `	}` |
|         7 | 8413 | `	return PH7_OK;` |
|         4 | 8414 | `}` |
|         - | 8415 | `/*` |
|         - | 8416 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8417 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8418 | ` *  is truthy, or NULL if none match.` |
|         - | 8419 | ` */` |
|         6 | 8420 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8421 | `{` |
|         - | 8422 | `	ph7_hashmap_node *pMatch;` |
|         - | 8423 | `	sxi32 rc;` |
|         7 | 8424 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8425 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8426 | `		return rc;` |
|         - | 8427 | `	}` |
|         7 | 8428 | `	if( pMatch == 0 ){` |
|         3 | 8429 | `		ph7_result_null(pCtx);` |
|         6 | 8430 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8431 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8432 | `	}else{` |
|         4 | 8433 | `		ph7_result_string(pCtx,` |
|         2 | 8434 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8435 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8436 | `	}` |
|         7 | 8437 | `	return PH7_OK;` |
|         4 | 8438 | `}` |
|         - | 8439 | `/*` |
|         - | 8440 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8441 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8442 | ` *  FALSE for an empty array.` |
|         - | 8443 | ` */` |
|         8 | 8444 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8445 | `{` |
|         - | 8446 | `	ph7_hashmap_node *pMatch;` |
|         - | 8447 | `	sxi32 rc;` |
|         9 | 8448 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8449 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8450 | `		return rc;` |
|         - | 8451 | `	}` |
|         9 | 8452 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8453 | `	return PH7_OK;` |
|         5 | 8454 | `}` |
|         - | 8455 | `/*` |
|         - | 8456 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8457 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8458 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8459 | ` */` |
|         8 | 8460 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8461 | `{` |
|         - | 8462 | `	ph7_hashmap_node *pMatch;` |
|         - | 8463 | `	sxi32 rc;` |
|         9 | 8464 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8465 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8466 | `		return rc;` |
|         - | 8467 | `	}` |
|         9 | 8468 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8469 | `	return PH7_OK;` |
|         5 | 8470 | `}` |
|         - | 8471 | `/*` |
|         - | 8472 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8473 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8474 | ` */` |
|         - | 8475 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8476 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        80 | 8477 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8478 | `{` |
|        84 | 8479 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        40 | 8480 | `	(void)pVm;` |
|        84 | 8481 | `	p->nCount++;` |
|        84 | 8482 | `	if( p->pArray ){` |
|         - | 8483 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8484 | `		 * otherwise append with an auto-assigned int index. */` |
|        70 | 8485 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        33 | 8486 | `	}` |
|        84 | 8487 | `	return SXRET_OK;` |
|         4 | 8488 | `}` |
|         - | 8489 | `/*` |
|         - | 8490 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8491 | ` */` |
|        30 | 8492 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8493 | `{` |
|         - | 8494 | `	struct IterCollect sCol;` |
|         - | 8495 | `	ph7_value *pArray;` |
|         - | 8496 | `	sxi32 rc;` |
|        34 | 8497 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8498 | `	pArray = ph7_context_new_array(pCtx);` |
|        34 | 8499 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        34 | 8500 | `	sCol.pArray = pArray;` |
|        34 | 8501 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        34 | 8502 | `	sCol.nCount = 0;` |
|        34 | 8503 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8504 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8505 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8506 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8507 | `		sxu32 n;` |
|         9 | 8508 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8509 | `			ph7_value sKey, *pVal;` |
|         7 | 8510 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8511 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8512 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8513 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8514 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8515 | `			pEntry = pEntry->pPrev;` |
|         4 | 8516 | `		}` |
|         3 | 8517 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8518 | `		return PH7_OK;` |
|         - | 8519 | `	}` |
|        32 | 8520 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        32 | 8521 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        30 | 8522 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8523 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8524 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8525 | `			ph7_type_name(apArg[0]));` |
|         - | 8526 | `	}` |
|        30 | 8527 | `	ph7_result_value(pCtx,pArray);` |
|        30 | 8528 | `	return PH7_OK;` |
|        19 | 8529 | `}` |
|         - | 8530 | `/*` |
|         - | 8531 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8532 | ` */` |
|         8 | 8533 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8534 | `{` |
|         - | 8535 | `	struct IterCollect sCol;` |
|         - | 8536 | `	sxi32 rc;` |
|         9 | 8537 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8538 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8539 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8540 | `		return PH7_OK;` |
|         - | 8541 | `	}` |
|         7 | 8542 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         7 | 8543 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         7 | 8544 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8545 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8546 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8547 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8548 | `			ph7_type_name(apArg[0]));` |
|         - | 8549 | `	}` |
|         7 | 8550 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         7 | 8551 | `	return PH7_OK;` |
|         5 | 8552 | `}` |
|         - | 8553 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8554 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8555 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8556 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        32 | 8557 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8558 | `{` |
|        33 | 8559 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8560 | `	ph7_value sResult;` |
|         - | 8561 | `	SySet aArg;` |
|         - | 8562 | `	sxi32 rc;` |
|         - | 8563 | `	int bContinue;` |
|        16 | 8564 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        33 | 8565 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 8566 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8567 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8568 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8569 | `		sxu32 n;` |
|        17 | 8570 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8571 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8572 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8573 | `			pEntry = pEntry->pPrev;` |
|         5 | 8574 | `		}` |
|         4 | 8575 | `	}` |
|        33 | 8576 | `	PH7_MemObjInit(pVm,&sResult);` |
|        49 | 8577 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        32 | 8578 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        33 | 8579 | `	SySetRelease(&aArg);` |
|        33 | 8580 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        31 | 8581 | `	p->nCount++;` |
|        31 | 8582 | `	PH7_MemObjToBool(&sResult);` |
|        31 | 8583 | `	bContinue = (sResult.x.iVal != 0);` |
|        31 | 8584 | `	PH7_MemObjRelease(&sResult);` |
|        31 | 8585 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        17 | 8586 | `}` |
|         - | 8587 | `/*` |
|         - | 8588 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8589 | ` */` |
|        12 | 8590 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8591 | `{` |
|         - | 8592 | `	struct IterApply sApp;` |
|         - | 8593 | `	sxi32 rc;` |
|        13 | 8594 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|        13 | 8595 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8596 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8597 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8598 | `	}` |
|        13 | 8599 | `	sApp.pCallback = apArg[1];` |
|        13 | 8600 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|        13 | 8601 | `	sApp.nCount = 0;` |
|        13 | 8602 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|        13 | 8603 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        11 | 8604 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8605 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8606 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8607 | `			ph7_type_name(apArg[0]));` |
|         - | 8608 | `	}` |
|        11 | 8609 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|        11 | 8610 | `	return PH7_OK;` |
|         7 | 8611 | `}` |
|         - | 8612 | `/*` |
|         - | 8613 | ` * Table of hashmap functions.` |
|         - | 8614 | ` */` |
|         - | 8615 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8616 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8617 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8618 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8619 | `	{"count",             ph7_hashmap_count },` |
|         - | 8620 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8621 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8622 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8623 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8624 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8625 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8626 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8627 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8628 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8629 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8630 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8631 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8632 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8633 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8634 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8635 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8636 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8637 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8638 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8639 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8640 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8641 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8642 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8643 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8644 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8645 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8646 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8647 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8648 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8649 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8650 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8651 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8652 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8653 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8654 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8655 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8656 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8657 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8658 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8659 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8660 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8661 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8662 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8663 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8664 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8665 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8666 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8667 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8668 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8669 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8670 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8671 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8672 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8673 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8674 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8675 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8676 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8677 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8678 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8679 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8680 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8681 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8682 | `	{"current",           ph7_hashmap_current },` |
|         - | 8683 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8684 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8685 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8686 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8687 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8688 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8689 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8690 | `};` |
|         - | 8691 | `/*` |
|         - | 8692 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8693 | ` */` |
|      3342 | 8694 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8695 | `{` |
|         - | 8696 | `	sxu32 n;` |
|    250655 | 8697 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    247313 | 8698 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    123659 | 8699 | `	}` |
|      3347 | 8700 | `}` |
|         - | 8701 | `/*` |
|         - | 8702 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8703 | ` * the BLOB given as the first argument.` |
|         - | 8704 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8705 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8706 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8707 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8708 | ` */` |
|         - | 8709 | `/*` |
|         - | 8710 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8711 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8712 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8713 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8714 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8715 | ` */` |
|       120 | 8716 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8717 | `{` |
|       123 | 8718 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8719 | `	ph7_value *pObj;` |
|       123 | 8720 | `	sxu32 n = 0;` |
|         - | 8721 | `	int isRef;` |
|       123 | 8722 | `	sxi32 rc = SXRET_OK;` |
|         - | 8723 | `	int i;` |
|       195 | 8724 | `	for(;;){` |
|       393 | 8725 | `		if( n >= pMap->nEntry ){` |
|       123 | 8726 | `			break;` |
|         - | 8727 | `		}` |
|       273 | 8728 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       273 | 8729 | `		isRef = (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0;` |
|       273 | 8730 | `		if( ShowType ){` |
|         - | 8731 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 8732 | `			 * on the next line at the same indent (php). */` |
|       105 | 8733 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|        71 | 8734 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        37 | 8735 | `			}` |
|        37 | 8736 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        23 | 8737 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        12 | 8738 | `			}else{` |
|        21 | 8739 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         6 | 8740 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8741 | `			}` |
|        37 | 8742 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        37 | 8743 | `			if( pObj ){` |
|        37 | 8744 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        37 | 8745 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8746 | `					break;` |
|         - | 8747 | `				}` |
|        17 | 8748 | `			}` |
|        20 | 8749 | `		}else{` |
|         - | 8750 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 8751 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 8752 | `			 * php's extra blank line. References carry no marker. */` |
|      1294 | 8753 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1058 | 8754 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       530 | 8755 | `			}` |
|       238 | 8756 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       125 | 8757 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        63 | 8758 | `			}else{` |
|       170 | 8759 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 8760 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8761 | `			}` |
|       236 | 8762 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       132 | 8763 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 8764 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 8765 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 8766 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8767 | `					break;` |
|         - | 8768 | `				}` |
|        13 | 8769 | `			}else{` |
|       214 | 8770 | `				if( pObj ){` |
|       214 | 8771 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       106 | 8772 | `				}` |
|       214 | 8773 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8774 | `			}` |
|         - | 8775 | `		}` |
|         - | 8776 | `		/* Point to the next entry */` |
|       273 | 8777 | `		n++;` |
|       273 | 8778 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8779 | `	}` |
|       123 | 8780 | `	return rc;` |
|         3 | 8781 | `}` |
|       116 | 8782 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8783 | `{` |
|         - | 8784 | `	sxi32 rc;` |
|         - | 8785 | `	int i;` |
|       118 | 8786 | `	if( nDepth > 31 ){` |
|         - | 8787 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8788 | `		/* Nesting limit reached */` |
|       ! 0 | 8789 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8790 | `		return SXERR_LIMIT;` |
|         - | 8791 | `	}` |
|       118 | 8792 | `	if( ShowType ){` |
|         - | 8793 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 8794 | `		 * newline (a nested array is itself an entry value line). */` |
|        14 | 8795 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        14 | 8796 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        14 | 8797 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        14 | 8798 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 8799 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 8800 | `		}` |
|        14 | 8801 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        14 | 8802 | `		return rc;` |
|         - | 8803 | `	}` |
|         - | 8804 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       105 | 8805 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       297 | 8806 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8807 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8808 | `	}` |
|       105 | 8809 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       105 | 8810 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       297 | 8811 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 8812 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 8813 | `	}` |
|       105 | 8814 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       105 | 8815 | `	return rc;` |
|        60 | 8816 | `}` |
|         - | 8817 | `/*` |
|         - | 8818 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8819 | ` * retrieved entry.` |
|         - | 8820 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8821 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8822 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8823 | ` * a value different from PH7_OK.` |
|         - | 8824 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8825 | ` */` |
|     33770 | 8826 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8827 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8828 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8829 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8830 | `	)` |
|         5 | 8831 | `{` |
|         - | 8832 | `	ph7_hashmap_node *pEntry;` |
|         - | 8833 | `	ph7_value sKey,sValue;` |
|         - | 8834 | `	sxi32 rc;` |
|         - | 8835 | `	sxu32 n;` |
|         - | 8836 | `	/* Initialize walker parameter */` |
|     33775 | 8837 | `	rc = SXRET_OK;` |
|     33775 | 8838 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     33775 | 8839 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     33775 | 8840 | `	n = pMap->nEntry;` |
|     33775 | 8841 | `	pEntry = pMap->pFirst;` |
|         - | 8842 | `	/* Start the iteration process */` |
|     91964 | 8843 | `	for(;;){` |
|    183933 | 8844 | `		if( n < 1 ){` |
|     33775 | 8845 | `			break;` |
|         - | 8846 | `		}` |
|         - | 8847 | `		/* Extract a copy of the key and a copy the current value */` |
|    150163 | 8848 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    150163 | 8849 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8850 | `		/* Invoke the user callback */` |
|    150163 | 8851 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8852 | `		/* Release the copy of the key and the value */` |
|    150163 | 8853 | `		PH7_MemObjRelease(&sKey);` |
|    150163 | 8854 | `		PH7_MemObjRelease(&sValue);` |
|    150163 | 8855 | `		if( rc != PH7_OK ){` |
|         - | 8856 | `			/* Callback request an operation abort */` |
|       ! 0 | 8857 | `			return SXERR_ABORT;` |
|         - | 8858 | `		}` |
|         - | 8859 | `		/* Point to the next entry */` |
|    150163 | 8860 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    150163 | 8861 | `		n--;` |
|         5 | 8862 | `	}` |
|         - | 8863 | `	/* All done */` |
|     33775 | 8864 | `	return SXRET_OK;` |
|     16890 | 8865 | `}` |
|         - | 8866 |  |
