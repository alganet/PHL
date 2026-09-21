# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1048/1145 lines (91.53%)

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
|   8856962 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   8856967 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   8856967 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|   5670851 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|   5670856 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|   5670856 |   35 | `	sxu32 nH = 5381;` |
|   5670856 |   36 | `	zEnd = &zIn[nLen];` |
|   6378181 |   37 | `	for(;;){` |
|  12756368 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   8463712 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   7343425 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   7210885 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|   5670856 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      2470 |   52 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      2475 |   54 | `	sxi64 iCount = 0;` |
|      2475 |   55 | `	if( !bRecursive ){` |
|      2301 |   56 | `		iCount = pMap->nEntry;` |
|      1153 |   57 | `	}else{` |
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
|      2475 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   4524000 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   4524005 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   4524005 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   4524005 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   4524005 |  110 | `	pNode->pMap  = &(*pMap);` |
|   4524005 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   4524005 |  112 | `	pNode->nHash = nHash;` |
|   4524005 |  113 | `	pNode->xKey.iKey = iKey;` |
|   4524005 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   4524005 |  115 | `	return pNode;` |
|   2262005 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|   3071453 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|   3071458 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3071458 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|   3071458 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|   3071458 |  133 | `	pNode->pMap  = &(*pMap);` |
|   3071458 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   3071458 |  135 | `	pNode->nHash = nHash;` |
|   3071458 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   3071458 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   3071458 |  138 | `	pNode->nValIdx = nValIdx;` |
|   3071458 |  139 | `	return pNode;` |
|   1535731 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   7595453 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   7595458 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   3932695 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   3932695 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1966344 |  150 | `	}` |
|   7595458 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   7595458 |  153 | `	if( pMap->pFirst == 0 ){` |
|   1434048 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|   1434048 |  156 | `		pMap->pCur = pNode;` |
|    717026 |  157 | `	}else{` |
|   6161415 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   7595458 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   7595458 |  174 | `	++pMap->nEntry;` |
|   7595458 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7562 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7567 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7567 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7567 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      7055 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3530 |  187 | `	}else{` |
|       515 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7567 |  190 | `	if( pNode->pNextCollide ){` |
|      4834 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2416 |  192 | `	}` |
|      7567 |  193 | `	if( pMap->pFirst == pNode ){` |
|       243 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|       119 |  195 | `	}` |
|      7567 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       279 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       137 |  199 | `	}` |
|      7567 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7567 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7567 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       215 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       215 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       215 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       105 |  218 | `		}` |
|       105 |  219 | `	}` |
|      7567 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7323 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3659 |  222 | `	}` |
|      7567 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7567 |  224 | `	pMap->nEntry--;` |
|      7567 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       113 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       113 |  228 | `		pMap->apBucket = 0;` |
|       113 |  229 | `		pMap->nSize = 0;` |
|       113 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        54 |  231 | `	}` |
|      7567 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   7595453 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   7595458 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   1439926 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   1439926 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|   1439926 |  245 | `		if( nNew < 1 ){` |
|   1434048 |  246 | `			nNew = 16;` |
|    717021 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|   1439926 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   1439926 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|   1439926 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|   1439926 |  260 | `		pMap->apBucket = apNew;` |
|   1439926 |  261 | `		pMap->nSize = nNew;` |
|   1439926 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   1434048 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5883 |  267 | `		pEntry = pMap->pFirst;` |
|      5883 |  268 | `		n = 0;` |
|   2564699 |  269 | `		for( ;; ){` |
|   5129403 |  270 | `			if( n >= pMap->nEntry ){` |
|      5883 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   5123525 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   5123525 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   5123525 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   4428075 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   4428075 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2214035 |  280 | `			}` |
|   5123525 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   5123525 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   5123525 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5883 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2939 |  288 | `	}` |
|   6161415 |  289 | `	return SXRET_OK;` |
|   3797731 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   4524000 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   4524005 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   4523957 |  310 | `		if( pValue ){` |
|   4523939 |  311 | `			sSafeVal = *pValue;` |
|   4523939 |  312 | `			pValue = &sSafeVal;` |
|   2261967 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   4523957 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   4523957 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   4523957 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   4523939 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   2261967 |  322 | `		}` |
|   4523957 |  323 | `		nIdx = pObj->nIdx;` |
|   2261981 |  324 | `	}else{` |
|        50 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   4524005 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   4524005 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   4524005 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   4524005 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        50 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        24 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   4524005 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   4524005 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   4524005 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   4524005 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   4524005 |  349 | `	return SXRET_OK;` |
|   2262005 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|   3071453 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|   3071458 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3021754 |  370 | `		if( pValue ){` |
|   3021428 |  371 | `			sSafeVal = *pValue;` |
|   3021428 |  372 | `			pValue = &sSafeVal;` |
|   1510711 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|   3021754 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3021754 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|   3021754 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|   3021428 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|   1510711 |  382 | `		}` |
|   3021754 |  383 | `		nIdx = pObj->nIdx;` |
|   1510879 |  384 | `	}else{` |
|     49709 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|   3071458 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|   3071458 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   3071458 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|   3071458 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     49709 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     24852 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3071458 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3071458 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|   3071458 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|   3071458 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|   3071458 |  409 | `	return SXRET_OK;` |
|   1535731 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4316416 |  416 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4316421 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|      1011 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4315415 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4315415 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110594534 |  433 | `	for(;;){` |
| 221189068 |  434 | `		if( pNode == 0 ){` |
|   4306445 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216882623 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216879598 |  438 | `			&& pNode->nHash == nHash` |
| 108442776 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      8975 |  441 | `				if( ppNode ){` |
|      8941 |  442 | `					*ppNode = pNode;` |
|      4468 |  443 | `				}` |
|      8975 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216873655 |  447 | `		pNode = pNode->pNextCollide;` |
|         2 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4306445 |  450 | `	return SXERR_NOTFOUND;` |
|   2158216 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|   3250457 |  457 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|   3250462 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|    651064 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|   2599403 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|   2599403 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|   1650783 |  475 | `	for(;;){` |
|   3301571 |  476 | `		if( pNode == 0 ){` |
|   2510517 |  477 | `			break;` |
|         - |  478 | `		}` |
|    791054 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    789533 |  480 | `			&& pNode->nHash == nHash` |
|    438501 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     88995 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     88891 |  484 | `				if( ppNode ){` |
|     88863 |  485 | `					*ppNode = pNode;` |
|     44429 |  486 | `				}` |
|     88891 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    702173 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|   2510517 |  493 | `	return SXERR_NOTFOUND;` |
|   1625233 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|   3250841 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|   3250846 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|   3250846 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|   3250846 |  504 | `	int isNeg = FALSE, nDigit;` |
|   3250846 |  505 | `	if( zIn >= zEnd ){` |
|        69 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|   3250780 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|   3250776 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         7 |  513 | `		isNeg = (zIn[0] == '-');` |
|         7 |  514 | `		zIn++;` |
|         3 |  515 | `	}` |
|   3250776 |  516 | `	zDigit = zIn;` |
|   1626033 |  517 | `	for(;;){` |
|   3252072 |  518 | `		if( zIn >= zEnd ){` |
|       503 |  519 | `			break;` |
|         - |  520 | `		}` |
|   3251570 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|   3250274 |  523 | `			return FALSE;` |
|         - |  524 | `		}` |
|      1297 |  525 | `		zIn++;` |
|         1 |  526 | `	}` |
|         - |  527 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  528 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  529 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  530 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       503 |  531 | `	nDigit = (int)(zEnd - zDigit);` |
|       503 |  532 | `	if( nDigit < 1 ){` |
|         - |  533 | `		/* A lone sign ("-"/"+") */` |
|       ! 0 |  534 | `		return FALSE;` |
|         - |  535 | `	}` |
|       507 |  536 | `	if( nDigit > 19 \|\|` |
|       254 |  537 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|         7 |  538 | `		return FALSE;` |
|         - |  539 | `	}` |
|       497 |  540 | `	return TRUE;` |
|   1625425 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    188130 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    188135 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    188135 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    179289 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|        21 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|        10 |  559 | `		}` |
|    179289 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    179217 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    179217 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|        36 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      8923 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        84 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        41 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      8923 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     94065 |  575 | `result:` |
|    188135 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     96939 |  578 | `		if( ppNode ){` |
|     96883 |  579 | `			*ppNode = pNode;` |
|     48439 |  580 | `		}` |
|     96939 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     91201 |  584 | `	return SXERR_NOTFOUND;` |
|     94070 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  588 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  589 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  590 | ` * via HashmapAppendIndexBusy.` |
|         - |  591 | ` */` |
|   2153452 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2153457 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|      1073 |  596 | `		pMap->bIntKeySeen = 1;` |
|      1073 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|      1073 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|      1073 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2152389 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2152101 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2152101 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1076051 |  609 | `	}` |
|   1076731 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   2365934 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   2365939 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   2365933 |  623 | `	return FALSE;` |
|   1182972 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   7541041 |  631 | `PH7_PRIVATE sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   7541046 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   7541046 |  638 | `	sxi32 rc = SXRET_OK;` |
|   7541046 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|   3021844 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         7 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         3 |  645 | `		}` |
|   3021844 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       423 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|   4532130 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   1510708 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  654 | `				/* Overwrite the old value */` |
|         - |  655 | `				ph7_value *pElem;` |
|       503 |  656 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       503 |  657 | `				if( pElem ){` |
|       503 |  658 | `					if( pVal ){` |
|       503 |  659 | `						PH7_MemObjStore(pVal,pElem);` |
|       254 |  660 | `					}else{` |
|         - |  661 | `						/* Nullify the entry */` |
|       ! 0 |  662 | `						PH7_MemObjToNull(pElem);` |
|         - |  663 | `					}` |
|       249 |  664 | `				}` |
|       503 |  665 | `				return SXRET_OK;` |
|         - |  666 | `		}` |
|   3020924 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  668 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  669 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       158 |  670 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  671 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  672 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  673 | `				return SXRET_OK;` |
|         - |  674 | `			}` |
|       236 |  675 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       156 |  676 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        78 |  677 | `				pVal,SXU32_HIGH);` |
|         - |  678 | `		}` |
|         - |  679 | `		/* Perform a blob-key insertion */` |
|   3020768 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   3020768 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   2259601 |  683 | `IntKey:` |
|   4519629 |  684 | `	if( pKey ){` |
|   2153731 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       442 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       220 |  688 | `		}` |
|   2153731 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  690 | `			/* Overwrite the old value */` |
|         - |  691 | `			ph7_value *pElem;` |
|       285 |  692 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       285 |  693 | `			if( pElem ){` |
|       285 |  694 | `				if( pVal ){` |
|       285 |  695 | `					PH7_MemObjStore(pVal,pElem);` |
|       144 |  696 | `				}else{` |
|         - |  697 | `					/* Nullify the entry */` |
|       ! 0 |  698 | `					PH7_MemObjToNull(pElem);` |
|         - |  699 | `				}` |
|       141 |  700 | `			}` |
|       285 |  701 | `			return SXRET_OK;` |
|         - |  702 | `		}` |
|   2153449 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2153447 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2153447 |  712 | `		if( rc == SXRET_OK ){` |
|   2153447 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1076721 |  714 | `		}` |
|   1076726 |  715 | `	}else{` |
|   2365903 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   2365901 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   2365895 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   2365895 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   2365893 |  726 | `			++pMap->iNextIdx;` |
|   1182944 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   4519337 |  730 | `	return rc;` |
|   3770525 |  731 | `}` |
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
|     49764 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     49769 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     49769 |  766 | `	sxi32 rc = SXRET_OK;` |
|     49769 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|     49723 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | ``			/* Force a string cast. NULL casts to "": `$a[null] =& $x` binds under the`` |
|         - |  770 | `			 * EMPTY STRING key, symmetric with HashmapInsert (the by-value path). */` |
|         3 |  771 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  772 | `		}` |
|     49723 |  773 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  774 | `			goto IntKey;` |
|         - |  775 | `		}` |
|         - |  776 | ``		/* An empty key is a REAL key: `$a[""] =& $x` binds (and OVERWRITES an existing`` |
|         - |  777 | `		 * "" element) under "", it does NOT auto-index. The legacy path turned "" into` |
|         - |  778 | ``		 * the next integer slot — `$a[""] =& $x` filed under 0 and a second write added`` |
|         - |  779 | `		 * a duplicate rather than rebinding. A genuine auto-index caller passes` |
|         - |  780 | `		 * pKey == 0 (a literal null pointer), handled at IntKey below, never a "" blob. */` |
|     74579 |  781 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     24858 |  782 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  783 | `				/* Overwrite */` |
|        14 |  784 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        14 |  785 | `				pNode->nValIdx = nRefIdx;` |
|         - |  786 | `				/* Install in the reference table */` |
|        14 |  787 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        14 |  788 | `				return SXRET_OK;` |
|         - |  789 | `		}` |
|         - |  790 | `		/* Perform a blob-key insertion */` |
|     49709 |  791 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     49709 |  792 | `		return rc;` |
|         - |  793 | `	}` |
|        23 |  794 | `IntKey:` |
|        50 |  795 | `	if( pKey ){` |
|        12 |  796 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  797 | `			/* Force an integer cast */` |
|         3 |  798 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  799 | `		}` |
|        12 |  800 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  801 | `			/* Overwrite */` |
|       ! 0 |  802 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  803 | `			pNode->nValIdx = nRefIdx;` |
|         - |  804 | `			/* Install in the reference table */` |
|       ! 0 |  805 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  806 | `			return SXRET_OK;` |
|         - |  807 | `		}` |
|         - |  808 | `		/* Perform a 64-bit-int-key insertion */` |
|        12 |  809 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|        12 |  810 | `		if( rc == SXRET_OK ){` |
|        12 |  811 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         5 |  812 | `		}` |
|         7 |  813 | `	}else{` |
|        40 |  814 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  815 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  816 | `		}` |
|         - |  817 | `		/* Assign an automatic index */` |
|        40 |  818 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        40 |  819 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        40 |  820 | `			++pMap->iNextIdx;` |
|        19 |  821 | `		}` |
|         - |  822 | `	}` |
|         - |  823 | `	/* Insertion result */` |
|        50 |  824 | `	return rc;` |
|     24887 |  825 | `}` |
|         - |  826 | `/*` |
|         - |  827 | ` * Extract node value.` |
|         - |  828 | ` */` |
|   1604042 |  829 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  830 | `{` |
|         - |  831 | `	/* Point to the desired object */` |
|         - |  832 | `	ph7_value *pObj;` |
|   1604047 |  833 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1604047 |  834 | `	return pObj;` |
|         5 |  835 | `}` |
|         - |  836 | `/*` |
|         - |  837 | ` * Insert a node in the given hashmap.` |
|         - |  838 | ` * If a node with the given key already exists in the database` |
|         - |  839 | ` * then this function overwrite the old value.` |
|         - |  840 | ` */` |
|      1080 |  841 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  842 | `{` |
|         - |  843 | `	ph7_value *pObj;` |
|         - |  844 | `	sxi32 rc;` |
|         - |  845 | `	/* Extract the node value */` |
|      1085 |  846 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|      1085 |  847 | `	if( pObj == 0 ){` |
|       ! 0 |  848 | `		return SXERR_EMPTY;` |
|         - |  849 | `	}` |
|      1080 |  850 | `	if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|      1084 |  851 | `	 \|\| PH7_VmSlotIsReferenced(pMap->pVm,pNode->nValIdx) ){` |
|         - |  852 | `		/* A referenced element keeps its reference through the copy (php: array_slice()` |
|         - |  853 | ``		 * of an array holding `$r = &$a[1]` still var_dumps that element as &int(2)).`` |
|         - |  854 | `		 * Same rule HashmapDuplicateNode applies for array_merge()/spread. */` |
|         3 |  855 | `		sxu32 nRefIdx = pNode->nValIdx;` |
|         - |  856 | `		ph7_value sKey;` |
|         3 |  857 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         3 |  858 | `			if( !bPreserve ){` |
|         3 |  859 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  860 | `			}` |
|       ! 0 |  861 | `			PH7_MemObjInitFromInt(pMap->pVm,&sKey,pNode->xKey.iKey);` |
|       ! 0 |  862 | `		}else{` |
|       ! 0 |  863 | `			if( !bPreserve ){` |
|       ! 0 |  864 | `				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);` |
|         - |  865 | `			}` |
|       ! 0 |  866 | `			PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       ! 0 |  867 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pNode->xKey.sKey),` |
|       ! 0 |  868 | `				SyBlobLength(&pNode->xKey.sKey));` |
|         - |  869 | `		}` |
|       ! 0 |  870 | `		rc = HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|       ! 0 |  871 | `		PH7_MemObjRelease(&sKey);` |
|       ! 0 |  872 | `		return rc;` |
|         - |  873 | `	}` |
|         - |  874 | `	/* Preserve key */` |
|      1083 |  875 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  876 | `		/* Int64 key */` |
|       951 |  877 | `		if( !bPreserve ){` |
|         - |  878 | `			/* Assign an automatic index */` |
|       267 |  879 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       136 |  880 | `		}else{` |
|       687 |  881 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  882 | `		}` |
|       478 |  883 | `	}else{` |
|         - |  884 | `		/* Blob key */` |
|       133 |  885 | `		if( !bPreserve ){` |
|         - |  886 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  887 | `			 * original string key entirely */` |
|        35 |  888 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  889 | `		}else{` |
|       148 |  890 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        49 |  891 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  892 | `		}` |
|         - |  893 | `	}` |
|      1083 |  894 | `	return rc;` |
|       545 |  895 | `}` |
|         - |  896 | `/*` |
|         - |  897 | ` * Compare two node values.` |
|         - |  898 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  899 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  900 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  901 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  902 | ` * documenation.` |
|         - |  903 | ` */` |
|     82500 |  904 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  905 | `{` |
|         - |  906 | `	ph7_value sObj1,sObj2;` |
|         - |  907 | `	sxi32 rc;` |
|     82505 |  908 | `	if( pLeft == pRight ){` |
|         - |  909 | `		/*` |
|         - |  910 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  911 | `		 * below for more information on this sceanario.` |
|         - |  912 | `		 */` |
|       ! 0 |  913 | `		return 0;` |
|         - |  914 | `	}` |
|         - |  915 | `	/* Do the comparison */` |
|     82505 |  916 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     82505 |  917 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     82505 |  918 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     82505 |  919 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     82505 |  920 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     82505 |  921 | `	PH7_MemObjRelease(&sObj1);` |
|     82505 |  922 | `	PH7_MemObjRelease(&sObj2);` |
|     82505 |  923 | `	return rc;` |
|     41182 |  924 | `}` |
|         - |  925 | `/*` |
|         - |  926 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  927 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  928 | ` */` |
|     17552 |  929 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  930 | `{` |
|     17557 |  931 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  932 | `	sxu32 nBucket;` |
|         - |  933 | `	/* Remove old collision links */` |
|     17557 |  934 | `	if( pEntry->pPrevCollide ){` |
|     12412 |  935 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      6091 |  936 | `	}else{` |
|      5150 |  937 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  938 | `	}` |
|     17557 |  939 | `	if( pEntry->pNextCollide ){` |
|      1196 |  940 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       606 |  941 | `	}` |
|     17557 |  942 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  943 | `	/* Compute the new hash */` |
|     17557 |  944 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     17557 |  945 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     17557 |  946 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  947 | `	/* Link to the new bucket */` |
|     17557 |  948 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17557 |  949 | `	if( pMap->apBucket[nBucket] ){` |
|     12743 |  950 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      6264 |  951 | `	}` |
|     17557 |  952 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     17557 |  953 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  954 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  955 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  956 | `	 * the no-overflow invariant uniform). */` |
|     17557 |  957 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     17557 |  958 | `		pMap->iNextIdx++;` |
|      8776 |  959 | `	}` |
|     17557 |  960 | `}` |
|         - |  961 | `/*` |
|         - |  962 | ` * Perform a linear search on a given hashmap.` |
|         - |  963 | ` * Write a pointer to the target node on success.` |
|         - |  964 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  965 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  966 | ` * for more information.` |
|         - |  967 | ` */` |
|     34292 |  968 | `PH7_PRIVATE int HashmapFindValue(` |
|         - |  969 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  970 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  971 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  972 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  973 | `	)` |
|         5 |  974 | `{` |
|         - |  975 | `	ph7_hashmap_node *pEntry;` |
|         - |  976 | `	ph7_value sVal,*pVal;` |
|         - |  977 | `	ph7_value sNeedle;` |
|         - |  978 | `	sxi32 rc;` |
|         - |  979 | `	sxu32 n;` |
|         - |  980 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     34297 |  981 | `	pEntry = pMap->pFirst;` |
|     34297 |  982 | `	n = pMap->nEntry;` |
|     34297 |  983 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     34297 |  984 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     84654 |  985 | `	for(;;){` |
|    169311 |  986 | `		if( n < 1 ){` |
|        91 |  987 | `			break;` |
|         - |  988 | `		}` |
|         - |  989 | `		/* Extract node value */` |
|    169223 |  990 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    169223 |  991 | `		if( pVal ){` |
|         - |  992 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  993 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  994 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  995 | `			 * so null needles/values take the same path as everything else` |
|         - |  996 | `			 * (the historical null-to-null shortcut here made` |
|         - |  997 | `			 * in_array(null, [""]) false where php says true). */` |
|    169223 |  998 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    169223 |  999 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    169223 | 1000 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    169223 | 1001 | `			PH7_MemObjRelease(&sVal);` |
|    169223 | 1002 | `			PH7_MemObjRelease(&sNeedle);` |
|    169223 | 1003 | `			if( rc == 0 ){` |
|     34209 | 1004 | `				if( ppNode ){` |
|        23 | 1005 | `					*ppNode = pEntry;` |
|        11 | 1006 | `				}` |
|         - | 1007 | `				/* Match found*/` |
|     34209 | 1008 | `				return SXRET_OK;` |
|         - | 1009 | `			}` |
|     67508 | 1010 | `		}` |
|         - | 1011 | `		/* Point to the next entry */` |
|    135019 | 1012 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    135019 | 1013 | `		n--;` |
|         5 | 1014 | `	}` |
|         - | 1015 | `	/* No such entry */` |
|        91 | 1016 | `	return SXERR_NOTFOUND;` |
|     17151 | 1017 | `}` |
|         - | 1018 | `/*` |
|         - | 1019 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - | 1020 | ` * for values comparison.` |
|         - | 1021 | ` * Write a pointer to the target node on success.` |
|         - | 1022 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1023 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - | 1024 | ` * for more information.` |
|         - | 1025 | ` */` |
|        28 | 1026 | `PH7_PRIVATE int HashmapFindValueByCallback(` |
|         - | 1027 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - | 1028 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - | 1029 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - | 1030 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - | 1031 | `	)` |
|         2 | 1032 | `{` |
|         - | 1033 | `	ph7_hashmap_node *pEntry;` |
|         - | 1034 | `	ph7_value sResult,*pVal;` |
|         - | 1035 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - | 1036 | `	sxi32 rc;` |
|         - | 1037 | `	sxu32 n;` |
|        30 | 1038 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - | 1039 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - | 1040 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 | 1041 | `		return SXERR_NOTFOUND;` |
|         - | 1042 | `	}` |
|         - | 1043 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        30 | 1044 | `	pEntry = pMap->pFirst;` |
|        30 | 1045 | `	n = pMap->nEntry;` |
|         - | 1046 | `	/* Store callback result here */` |
|        30 | 1047 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - | 1048 | `	/* First argument to the callback */` |
|        30 | 1049 | `	apArg[0] = pNeedle;` |
|        31 | 1050 | `	for(;;){` |
|        64 | 1051 | `		if( n < 1 ){` |
|        12 | 1052 | `			break;` |
|         - | 1053 | `		}` |
|         - | 1054 | `		/* Extract node value */` |
|        54 | 1055 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        54 | 1056 | `		if( pVal ){` |
|         - | 1057 | `			/* Invoke the user callback */` |
|        54 | 1058 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        54 | 1059 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        54 | 1060 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 1061 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1062 | `				 * and report no match for the rest of the run. */` |
|         5 | 1063 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1064 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1065 | `				return SXERR_NOTFOUND;` |
|         - | 1066 | `			}` |
|        50 | 1067 | `			if( rc == SXRET_OK ){` |
|         - | 1068 | `				/* Extract callback result */` |
|        50 | 1069 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1070 | `					/* Perform an int cast */` |
|       ! 0 | 1071 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1072 | `				}` |
|        50 | 1073 | `				rc = (sxi32)sResult.x.iVal;` |
|        50 | 1074 | `				PH7_MemObjRelease(&sResult);` |
|        50 | 1075 | `				if( rc == 0 ){` |
|         - | 1076 | `					/* Match found*/` |
|        16 | 1077 | `					if( ppNode ){` |
|       ! 0 | 1078 | `						*ppNode = pEntry;` |
|       ! 0 | 1079 | `					}` |
|        16 | 1080 | `					return SXRET_OK;` |
|         - | 1081 | `				}` |
|        17 | 1082 | `			}` |
|        17 | 1083 | `		}` |
|         - | 1084 | `		/* Point to the next entry */` |
|        36 | 1085 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        36 | 1086 | `		n--;` |
|         2 | 1087 | `	}` |
|         - | 1088 | `	/* No such entry */` |
|        12 | 1089 | `	return SXERR_NOTFOUND;` |
|        16 | 1090 | `}` |
|         - | 1091 | `/*` |
|         - | 1092 | ` * Compare two hashmaps.` |
|         - | 1093 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1094 | ` * Note on array comparison operators.` |
|         - | 1095 | ` *  According to the PHP language reference manual.` |
|         - | 1096 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1097 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1098 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1099 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1100 | ` *                          order and of the same types.` |
|         - | 1101 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1102 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1103 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1104 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1105 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1106 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1107 | ` * <?php` |
|         - | 1108 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1109 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1110 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1111 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1112 | ` * var_dump($c);` |
|         - | 1113 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1114 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1115 | ` * var_dump($c);` |
|         - | 1116 | ` * ?>` |
|         - | 1117 | ` * When executed, this script will print the following:` |
|         - | 1118 | ` * Union of $a and $b:` |
|         - | 1119 | ` * array(3) {` |
|         - | 1120 | ` *  ["a"]=>` |
|         - | 1121 | ` *  string(5) "apple"` |
|         - | 1122 | ` *  ["b"]=>` |
|         - | 1123 | ` * string(6) "banana"` |
|         - | 1124 | ` *  ["c"]=>` |
|         - | 1125 | ` * string(6) "cherry"` |
|         - | 1126 | ` * }` |
|         - | 1127 | ` * Union of $b and $a:` |
|         - | 1128 | ` * array(3) {` |
|         - | 1129 | ` * ["a"]=>` |
|         - | 1130 | ` * string(4) "pear"` |
|         - | 1131 | ` * ["b"]=>` |
|         - | 1132 | ` * string(10) "strawberry"` |
|         - | 1133 | ` * ["c"]=>` |
|         - | 1134 | ` * string(6) "cherry"` |
|         - | 1135 | ` * }` |
|         - | 1136 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1137 | ` */` |
|        68 | 1138 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1139 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1140 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1141 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1142 | `	)` |
|         3 | 1143 | `{` |
|         - | 1144 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1145 | `	sxi32 rc;` |
|         - | 1146 | `	sxu32 n;` |
|        71 | 1147 | `	if( pLeft == pRight ){` |
|         - | 1148 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1149 | `		 * Unlike the zend engine.` |
|         - | 1150 | `		 */` |
|         7 | 1151 | `		return 0;` |
|         - | 1152 | `	}` |
|        65 | 1153 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1154 | `		/* Must have the same number of entries */` |
|         8 | 1155 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1156 | `	}` |
|        58 | 1157 | `	if( bStrict ){` |
|         - | 1158 | `		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the` |
|         - | 1159 | `		 * same key/value pairs, with identical key types, in the same insertion` |
|         - | 1160 | `		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per` |
|         - | 1161 | `		 * this file's forward-iteration convention) in lockstep and compare each` |
|         - | 1162 | `		 * position's key then value. (Loose '==' below stays order-insensitive,` |
|         - | 1163 | `		 * matching each left key by lookup into the right map.) */` |
|        44 | 1164 | `		ph7_hashmap_node *pLs = pLeft->pFirst;` |
|        44 | 1165 | `		ph7_hashmap_node *pRs = pRight->pFirst;` |
|      1108 | 1166 | `		for( n = pLeft->nEntry ; n > 0 ; n-- ){` |
|         - | 1167 | `			/* Keys must match in type and value at this position */` |
|      1080 | 1168 | `			if( pLs->iType != pRs->iType ){` |
|       ! 0 | 1169 | `				return 1;` |
|         - | 1170 | `			}` |
|      1080 | 1171 | `			if( pLs->iType == HASHMAP_INT_NODE ){` |
|      1050 | 1172 | `				if( pLs->xKey.iKey != pRs->xKey.iKey ){` |
|         3 | 1173 | `					return 1;` |
|         - | 1174 | `				}` |
|       525 | 1175 | `			}else{` |
|        31 | 1176 | `				SyBlob *pLk = &pLs->xKey.sKey;` |
|        31 | 1177 | `				SyBlob *pRk = &pRs->xKey.sKey;` |
|        30 | 1178 | `				if( SyBlobLength(pLk) != SyBlobLength(pRk)` |
|        31 | 1179 | `				 \|\| (SyBlobLength(pLk) > 0` |
|        30 | 1180 | `				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){` |
|         7 | 1181 | `					return 1;` |
|         - | 1182 | `				}` |
|         - | 1183 | `			}` |
|         - | 1184 | `			/* Values must be strictly identical */` |
|      1072 | 1185 | `			if( HashmapNodeCmp(pLs,pRs,TRUE) != 0 ){` |
|         7 | 1186 | `				return 1;` |
|         - | 1187 | `			}` |
|      1066 | 1188 | `			pLs = pLs->pPrev; /* Reverse link = insertion order */` |
|      1066 | 1189 | `			pRs = pRs->pPrev;` |
|       534 | 1190 | `		}` |
|        30 | 1191 | `		return 0; /* Same pairs, same order */` |
|         - | 1192 | `	}` |
|         - | 1193 | `	/* Point to the first inserted entry of the left hashmap */` |
|        16 | 1194 | `	pLe = pLeft->pFirst;` |
|        16 | 1195 | `	pRe = 0; /* cc warning */` |
|         - | 1196 | `	/* Perform the comparison */` |
|        16 | 1197 | `	n = pLeft->nEntry;` |
|        17 | 1198 | `	for(;;){` |
|        36 | 1199 | `		if( n < 1 ){` |
|        13 | 1200 | `			break;` |
|         - | 1201 | `		}` |
|        24 | 1202 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1203 | `			/* Int key */` |
|        16 | 1204 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|         9 | 1205 | `		}else{` |
|         9 | 1206 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1207 | `			/* Blob key */` |
|         9 | 1208 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1209 | `		}` |
|        24 | 1210 | `		if( rc != SXRET_OK ){` |
|         - | 1211 | `			/* No such entry in the right side */` |
|       ! 0 | 1212 | `			return 1;` |
|         - | 1213 | `		}` |
|        24 | 1214 | `		rc = 0;` |
|        24 | 1215 | `		if( bStrict ){` |
|         - | 1216 | `			/* Make sure,the keys are of the same type */` |
|       ! 0 | 1217 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1218 | `				rc = 1;` |
|       ! 0 | 1219 | `			}` |
|       ! 0 | 1220 | `		}` |
|        24 | 1221 | `		if( !rc ){` |
|         - | 1222 | `			/* Compare nodes */` |
|        24 | 1223 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        11 | 1224 | `		}` |
|        24 | 1225 | `		if( rc != 0 ){` |
|         - | 1226 | `			/* Nodes key/value differ */` |
|         3 | 1227 | `			return rc;` |
|         - | 1228 | `		}` |
|         - | 1229 | `		/* Point to the next entry */` |
|        21 | 1230 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        21 | 1231 | `		n--;` |
|         1 | 1232 | `	}` |
|        13 | 1233 | `	return 0; /* Hashmaps are equals */` |
|        37 | 1234 | `}` |
|         - | 1235 | `/*` |
|         - | 1236 | ` * Duplicate a hashmap node.` |
|         - | 1237 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1238 | ` */` |
|    745182 | 1239 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1240 | `	ph7_hashmap *pDest,` |
|         - | 1241 | `	ph7_hashmap_node *pEntry,` |
|         - | 1242 | `	ph7_value *pVal,` |
|         - | 1243 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1244 | `	)` |
|         5 | 1245 | `{` |
|         - | 1246 | `	ph7_value sSafeVal;` |
|         - | 1247 | `	ph7_value sKey;` |
|         - | 1248 | `	sxi32 rc;` |
|         - | 1249 |  |
|    745182 | 1250 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    745184 | 1251 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
|         - | 1252 | ``		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points`` |
|         - | 1253 | `		 * at an outside slot) or, the case PH7 missed, an element somebody took a` |
|         - | 1254 | ``		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through`` |
|         - | 1255 | `		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep` |
|         - | 1256 | ``		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */`` |
|         9 | 1257 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         9 | 1258 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1259 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1260 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1261 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1262 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1263 | `		}else{` |
|         7 | 1264 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         7 | 1265 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         3 | 1266 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1267 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1268 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1269 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1270 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1271 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1272 | `			}` |
|         - | 1273 | `		}` |
|         9 | 1274 | `		return rc;` |
|         - | 1275 | `	}` |
|    745179 | 1276 | `	sSafeVal = *pVal;` |
|         - | 1277 |  |
|    745179 | 1278 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1279 | `		/* Blob key insertion */` |
|      4325 | 1280 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4325 | 1281 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4325 | 1282 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4325 | 1283 | `		PH7_MemObjRelease(&sKey);` |
|      2165 | 1284 | `	}else{` |
|         - | 1285 | `		/* Int key */` |
|    740859 | 1286 | `		if( iAction == 0 ){ /* Merge */` |
|    736911 | 1287 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    372406 | 1288 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1289 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1290 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1291 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1292 | `		}else{ /* Dup */` |
|      3923 | 1293 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1294 | `		}` |
|         - | 1295 | `	}` |
|    745179 | 1296 | `	return rc;` |
|    372596 | 1297 | `}` |
|         - | 1298 | `/*` |
|         - | 1299 | ` * Merge two hashmaps.` |
|         - | 1300 | ` * Note on the merge process` |
|         - | 1301 | ` * According to the PHP language reference manual.` |
|         - | 1302 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1303 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1304 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1305 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1306 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1307 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1308 | ` *  keys starting from zero in the result array.` |
|         - | 1309 | ` */` |
|      2966 | 1310 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1311 | `{` |
|         - | 1312 | `	ph7_hashmap_node *pEntry;` |
|         - | 1313 | `	ph7_value *pVal;` |
|         - | 1314 | `	sxi32 rc;` |
|         - | 1315 | `	sxu32 n;` |
|      2971 | 1316 | `	if( pSrc == pDest ){` |
|         - | 1317 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1318 | `		 * Unlike the zend engine.` |
|         - | 1319 | `		 */` |
|       ! 0 | 1320 | `		return SXRET_OK;` |
|         - | 1321 | `	}` |
|         - | 1322 | `	/* Point to the first inserted entry in the source */` |
|      2971 | 1323 | `	pEntry = pSrc->pFirst;` |
|         - | 1324 | `	/* Perform the merge */` |
|    739937 | 1325 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1326 | `		/* Extract the node value */` |
|    736971 | 1327 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    736971 | 1328 | `		if( pVal ){` |
|         - | 1329 | `			/* Make a local copy of the value.` |
|         - | 1330 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1331 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1332 | `			 * to the old pool.` |
|         - | 1333 | `			 */` |
|    736971 | 1334 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    368488 | 1335 | `		}else{` |
|       ! 0 | 1336 | `			rc = SXRET_OK;` |
|         - | 1337 | `		}` |
|    736971 | 1338 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1339 | `			return rc;` |
|         - | 1340 | `		}` |
|         - | 1341 | `		/* Point to the next entry */` |
|    736971 | 1342 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    368488 | 1343 | `	}` |
|      2971 | 1344 | `	return SXRET_OK;` |
|      1488 | 1345 | `}` |
|         - | 1346 | `/*` |
|         - | 1347 | ` * Overwrite entries with the same key.` |
|         - | 1348 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1349 | ` *  According to the PHP language reference manual.` |
|         - | 1350 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1351 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1352 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1353 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1354 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1355 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1356 | ` *  overwriting the previous values.` |
|         - | 1357 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1358 | ` *  by whatever type is in the second array.` |
|         - | 1359 | ` */` |
|        34 | 1360 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1361 | `{` |
|         - | 1362 | `	ph7_hashmap_node *pEntry;` |
|         - | 1363 | `	ph7_value *pVal;` |
|         - | 1364 | `	sxi32 rc;` |
|         - | 1365 | `	sxu32 n;` |
|        36 | 1366 | `	if( pSrc == pDest ){` |
|         - | 1367 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1368 | `		 * Unlike the zend engine.` |
|         - | 1369 | `		 */` |
|       ! 0 | 1370 | `		return SXRET_OK;` |
|         - | 1371 | `	}` |
|         - | 1372 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1373 | `	pEntry = pSrc->pFirst;` |
|         - | 1374 | `	/* Perform the merge */` |
|        80 | 1375 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1376 | `		/* Extract the node value */` |
|        46 | 1377 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1378 | `		if( pVal ){` |
|        46 | 1379 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1380 | `		}else{` |
|       ! 0 | 1381 | `			rc = SXRET_OK;` |
|         - | 1382 | `		}` |
|        46 | 1383 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1384 | `			return rc;` |
|         - | 1385 | `		}` |
|         - | 1386 | `		/* Point to the next entry */` |
|        46 | 1387 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1388 | `	}` |
|        36 | 1389 | `	return SXRET_OK;` |
|        19 | 1390 | `}` |
|         - | 1391 | `/*` |
|         - | 1392 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1393 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1394 | ` */` |
|      7824 | 1395 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1396 | `{` |
|         - | 1397 | `	ph7_hashmap_node *pEntry;` |
|         - | 1398 | `	ph7_value *pVal;` |
|         - | 1399 | `	sxi32 rc;` |
|         - | 1400 | `	sxu32 n;` |
|      7829 | 1401 | `	if( pSrc == pDest ){` |
|         - | 1402 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1403 | `		 * Unlike the zend engine.` |
|         - | 1404 | `		 */` |
|       ! 0 | 1405 | `		return SXRET_OK;` |
|         - | 1406 | `	}` |
|         - | 1407 | `	/* Point to the first inserted entry in the source */` |
|      7829 | 1408 | `	pEntry = pSrc->pFirst;` |
|         - | 1409 | `	/* Perform the duplication */` |
|     16001 | 1410 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1411 | `		/* Extract the node value */` |
|      8177 | 1412 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      8177 | 1413 | `		if( pVal ){` |
|      8177 | 1414 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      4091 | 1415 | `		}else{` |
|       ! 0 | 1416 | `			rc = SXRET_OK;` |
|         - | 1417 | `		}` |
|      8177 | 1418 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1419 | `			return rc;` |
|         - | 1420 | `		}` |
|         - | 1421 | `		/* Point to the next entry */` |
|      8177 | 1422 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      4091 | 1423 | `	}` |
|      7829 | 1424 | `	return SXRET_OK;` |
|      3917 | 1425 | `}` |
|         - | 1426 | `/*` |
|         - | 1427 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1428 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1429 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1430 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1431 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1432 | ` * this instead of PH7_HashmapDup.` |
|         - | 1433 | ` */` |
|        12 | 1434 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1435 | `{` |
|         - | 1436 | `	ph7_hashmap_node *pEntry;` |
|         - | 1437 | `	ph7_value *pVal;` |
|         - | 1438 | `	sxi32 rc;` |
|         - | 1439 | `	sxu32 n;` |
|        13 | 1440 | `	if( pSrc == pDest ){` |
|       ! 0 | 1441 | `		return SXRET_OK;` |
|         - | 1442 | `	}` |
|        13 | 1443 | `	pEntry = pSrc->pFirst;` |
|       893 | 1444 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1445 | `		/* Extract the node value (resolves foreign references) */` |
|       881 | 1446 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       880 | 1447 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       574 | 1448 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1449 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1450 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1451 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1452 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1453 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1454 | `			pVal = 0;` |
|         2 | 1455 | `		}` |
|       881 | 1456 | `		if( pVal ){` |
|       877 | 1457 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1309 | 1458 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       436 | 1459 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       437 | 1460 | `			}else{` |
|         5 | 1461 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1462 | `			}` |
|       877 | 1463 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1464 | `				return rc;` |
|         - | 1465 | `			}` |
|       438 | 1466 | `		}` |
|         - | 1467 | `		/* Point to the next entry */` |
|       881 | 1468 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       441 | 1469 | `	}` |
|        13 | 1470 | `	return SXRET_OK;` |
|         7 | 1471 | `}` |
|         - | 1472 | `/*` |
|         - | 1473 | ` * Count the map references held by BY-REFERENCE foreach steps iterating the` |
|         - | 1474 | `` * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —`` |
|         - | 1475 | ` * appends/deletes inside the body are visited — so a by-ref step's retain` |
|         - | 1476 | ` * must not make writes through the source variable COW-separate away from` |
|         - | 1477 | ` * the loop's map. By-VALUE steps are deliberately NOT discounted: their` |
|         - | 1478 | ` * retain is exactly what makes an in-loop write separate, which is php's` |
|         - | 1479 | ` * iterate-a-snapshot semantic.` |
|         - | 1480 | ` */` |
|        50 | 1481 | `static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)` |
|         3 | 1482 | `{` |
|         - | 1483 | `	ph7_foreach_step *pStep;` |
|        53 | 1484 | `	sxi32 nRef = 0;` |
|       103 | 1485 | `	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|        53 | 1486 | `		if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        45 | 1487 | `			nRef++;` |
|        21 | 1488 | `		}` |
|        28 | 1489 | `	}` |
|        53 | 1490 | `	return nRef;` |
|         3 | 1491 | `}` |
|         - | 1492 | `/*` |
|         - | 1493 | ` * Copy-on-write separation for arrays.` |
|         - | 1494 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1495 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1496 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1497 | ` * References held by active by-ref foreach steps do not count as sharers` |
|         - | 1498 | `` * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land`` |
|         - | 1499 | ` * on the live map the loop is walking, like php.` |
|         - | 1500 | ` */` |
|    270488 | 1501 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1502 | `{` |
|    270493 | 1503 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1504 | `	ph7_hashmap *pNew;` |
|         - | 1505 | `	ph7_value *pBacking;` |
|         - | 1506 | `	sxu32 nValIdx;` |
|         - | 1507 | `	int bValueInPool;` |
|    270493 | 1508 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    270493 | 1509 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1510 | `		/* Sole owner, no separation needed */` |
|    267427 | 1511 | `		return pMap;` |
|         - | 1512 | `	}` |
|      3071 | 1513 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1514 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1515 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1516 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       151 | 1517 | `		return pMap;` |
|         - | 1518 | `	}` |
|         - | 1519 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1520 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1521 | `	 * frame is popped. */` |
|      2921 | 1522 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2921 | 1523 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2916 | 1524 | `		if( pBacking && pBacking != pValue` |
|      2891 | 1525 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2871 | 1526 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1527 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2871 | 1528 | `			pMap->iRef--;` |
|      2871 | 1529 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1530 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2823 | 1531 | `				pMap->iRef++;` |
|      2823 | 1532 | `				return pMap;` |
|         - | 1533 | `			}` |
|        49 | 1534 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        49 | 1535 | `			if( pNew == 0 ){` |
|       ! 0 | 1536 | `				pMap->iRef++;` |
|       ! 0 | 1537 | `				return pMap;` |
|         - | 1538 | `			}` |
|        49 | 1539 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1540 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1541 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1542 | `				pMap->iRef++;` |
|       ! 0 | 1543 | `				return pMap;` |
|         - | 1544 | `			}` |
|        49 | 1545 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        49 | 1546 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1547 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1548 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1549 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1550 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1551 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1552 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1553 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        49 | 1554 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        49 | 1555 | `			if( pBacking ){` |
|        49 | 1556 | `				pBacking->x.pOther = pNew;` |
|        24 | 1557 | `			}` |
|         - | 1558 | `			/* Update the stack value to match */` |
|        49 | 1559 | `			pValue->x.pOther = pNew;` |
|        49 | 1560 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        49 | 1561 | `			return pNew;` |
|         - | 1562 | `		}` |
|        25 | 1563 | `	}` |
|         - | 1564 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1565 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1566 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1567 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1568 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1569 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1570 | `	 * pBacking in the backing-variable branch above). */` |
|        52 | 1571 | `	nValIdx = pValue->nIdx;` |
|        77 | 1572 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        50 | 1573 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        52 | 1574 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        52 | 1575 | `	if( pNew == 0 ){` |
|         - | 1576 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1577 | `		return pMap;` |
|         - | 1578 | `	}` |
|        52 | 1579 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1580 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1581 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1582 | `		return pMap;` |
|         - | 1583 | `	}` |
|        52 | 1584 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        52 | 1585 | `	pMap->iRef--;` |
|        52 | 1586 | `	if( bValueInPool ){` |
|         - | 1587 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        52 | 1588 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        52 | 1589 | `		if( pValue == 0 ){` |
|       ! 0 | 1590 | `			return pNew;` |
|         - | 1591 | `		}` |
|        25 | 1592 | `	}` |
|        52 | 1593 | `	pValue->x.pOther = pNew;` |
|        52 | 1594 | `	return pNew;` |
|    135249 | 1595 | `}` |
|         - | 1596 | `/*` |
|         - | 1597 | ` * Perform the union of two hashmaps.` |
|         - | 1598 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1599 | ` * with a variable holding an array as follows:` |
|         - | 1600 | ` * <?php` |
|         - | 1601 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1602 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1603 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1604 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1605 | ` * var_dump($c);` |
|         - | 1606 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1607 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1608 | ` * var_dump($c);` |
|         - | 1609 | ` * ?>` |
|         - | 1610 | ` * When executed, this script will print the following:` |
|         - | 1611 | ` * Union of $a and $b:` |
|         - | 1612 | ` * array(3) {` |
|         - | 1613 | ` *  ["a"]=>` |
|         - | 1614 | ` *  string(5) "apple"` |
|         - | 1615 | ` *  ["b"]=>` |
|         - | 1616 | ` * string(6) "banana"` |
|         - | 1617 | ` *  ["c"]=>` |
|         - | 1618 | ` * string(6) "cherry"` |
|         - | 1619 | ` * }` |
|         - | 1620 | ` * Union of $b and $a:` |
|         - | 1621 | ` * array(3) {` |
|         - | 1622 | ` * ["a"]=>` |
|         - | 1623 | ` * string(4) "pear"` |
|         - | 1624 | ` * ["b"]=>` |
|         - | 1625 | ` * string(10) "strawberry"` |
|         - | 1626 | ` * ["c"]=>` |
|         - | 1627 | ` * string(6) "cherry"` |
|         - | 1628 | ` * }` |
|         - | 1629 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1630 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1631 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1632 | ` */` |
|      4080 | 1633 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1634 | `{` |
|         - | 1635 | `	ph7_hashmap_node *pEntry;` |
|      4085 | 1636 | `	sxi32 rc = SXRET_OK;` |
|         - | 1637 | `	ph7_value *pObj;` |
|         - | 1638 | `	sxu32 n;` |
|      4085 | 1639 | `	if( pLeft == pRight ){` |
|         - | 1640 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1641 | `		 * Unlike the zend engine.` |
|         - | 1642 | `		 */` |
|       ! 0 | 1643 | `		return SXRET_OK;` |
|         - | 1644 | `	}` |
|         - | 1645 | `	/* Perform the union */` |
|      4085 | 1646 | `	pEntry = pRight->pFirst;` |
|      4131 | 1647 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1648 | `		/* Make sure the given key does not exists in the left array */` |
|        50 | 1649 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1650 | `			/* BLOB key */` |
|        23 | 1651 | `			if( SXRET_OK !=` |
|        20 | 1652 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        19 | 1653 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        19 | 1654 | `					if( pObj ){` |
|        19 | 1655 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1656 | `						/* Perform the insertion */` |
|        19 | 1657 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1658 | `							&sSafeVal,0,FALSE);` |
|        19 | 1659 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1660 | `							return rc;` |
|         - | 1661 | `						}` |
|         8 | 1662 | `					}` |
|         8 | 1663 | `			}` |
|        13 | 1664 | `		}else{` |
|         - | 1665 | `			/* INT key */` |
|        28 | 1666 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        15 | 1667 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        15 | 1668 | `				if( pObj ){` |
|        15 | 1669 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1670 | `					/* Perform the insertion */` |
|        15 | 1671 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        15 | 1672 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1673 | `						return rc;` |
|         - | 1674 | `					}` |
|         7 | 1675 | `				}` |
|         7 | 1676 | `			}` |
|         - | 1677 | `		}` |
|         - | 1678 | `		/* Point to the next entry */` |
|        50 | 1679 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        27 | 1680 | `	}` |
|      4085 | 1681 | `	return SXRET_OK;` |
|      2045 | 1682 | `}` |
|         - | 1683 | `/*` |
|         - | 1684 | ` * Allocate a new hashmap.` |
|         - | 1685 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1686 | ` */` |
|   2315684 | 1687 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1688 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1689 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1690 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1691 | `	)` |
|         5 | 1692 | `{` |
|         - | 1693 | `	ph7_hashmap *pMap;` |
|         - | 1694 | `	/* Allocate a new instance */` |
|   2315689 | 1695 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   2315689 | 1696 | `	if( pMap == 0 ){` |
|       ! 0 | 1697 | `		return 0;` |
|         - | 1698 | `	}` |
|         - | 1699 | `	/* Zero the structure */` |
|   2315689 | 1700 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1701 | `	/* Fill in the structure */` |
|   2315689 | 1702 | `	pMap->pVm = &(*pVm);` |
|   2315689 | 1703 | `	pMap->iRef = 1;` |
|         - | 1704 | `	/* Default hash functions */` |
|   2315689 | 1705 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   2315689 | 1706 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   2315689 | 1707 | `	return pMap;` |
|   1157847 | 1708 | `}` |
|         - | 1709 | `/*` |
|         - | 1710 | ` * Install superglobals in the given virtual machine.` |
|         - | 1711 | ` * Note on superglobals.` |
|         - | 1712 | ` *  According to the PHP language reference manual.` |
|         - | 1713 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1714 | `*   Description` |
|         - | 1715 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1716 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1717 | `*   global $variable; to access them within functions or methods.` |
|         - | 1718 | `*   These superglobal variables are:` |
|         - | 1719 | `*    $GLOBALS` |
|         - | 1720 | `*    $_SERVER` |
|         - | 1721 | `*    $_GET` |
|         - | 1722 | `*    $_POST` |
|         - | 1723 | `*    $_FILES` |
|         - | 1724 | `*    $_COOKIE` |
|         - | 1725 | `*    $_SESSION` |
|         - | 1726 | `*    $_REQUEST` |
|         - | 1727 | `*    $_ENV` |
|         - | 1728 | `*/` |
|      3654 | 1729 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1730 | `{` |
|         - | 1731 | `	static const char * azSuper[] = {` |
|         - | 1732 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1733 | `		"_GET",      /* $_GET */` |
|         - | 1734 | `		"_POST",     /* $_POST */` |
|         - | 1735 | `		"_FILES",    /* $_FILES */` |
|         - | 1736 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1737 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1738 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1739 | `		"_ENV",      /* $_ENV */` |
|         - | 1740 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1741 | `		"argv"       /* $argv */` |
|         - | 1742 | `	};` |
|         - | 1743 | `	ph7_hashmap *pMap;` |
|         - | 1744 | `	ph7_value *pObj;` |
|         - | 1745 | `	SyString *pFile;` |
|         - | 1746 | `	sxi32 rc;` |
|         - | 1747 | `	sxu32 n;` |
|         - | 1748 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3659 | 1749 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3659 | 1750 | `	if( pMap == 0 ){` |
|       ! 0 | 1751 | `		return SXERR_MEM;` |
|         - | 1752 | `	}` |
|      3659 | 1753 | `	pVm->pGlobal = pMap;` |
|         - | 1754 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3659 | 1755 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3659 | 1756 | `	if( pObj == 0 ){` |
|       ! 0 | 1757 | `		return SXERR_MEM;` |
|         - | 1758 | `	}` |
|      3659 | 1759 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1760 | `	/* Record object index */` |
|      3659 | 1761 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1762 | `	/* Install the special $GLOBALS array */` |
|      3659 | 1763 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3659 | 1764 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1765 | `		return rc;` |
|         - | 1766 | `	}` |
|         - | 1767 | `	/* Install superglobals now */` |
|     40199 | 1768 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1769 | `		ph7_value *pSuper;` |
|         - | 1770 | `		/* Request an empty array */` |
|     36545 | 1771 | `		pSuper = ph7_new_array(&(*pVm));` |
|     36545 | 1772 | `		if( pSuper == 0 ){` |
|       ! 0 | 1773 | `			return SXERR_MEM;` |
|         - | 1774 | `		}` |
|         - | 1775 | `		/* Install */` |
|     36545 | 1776 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     36545 | 1777 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1778 | `			return rc;` |
|         - | 1779 | `		}` |
|         - | 1780 | `		/* Release the value now it have been installed */` |
|     36545 | 1781 | `		ph7_release_value(&(*pVm),pSuper);` |
|     18275 | 1782 | `	}` |
|         - | 1783 | `	/* Set some $_SERVER entries */` |
|      3659 | 1784 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1785 | `	/*` |
|         - | 1786 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1787 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1788 | `	 */` |
|      7313 | 1789 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1790 | `		"SCRIPT_FILENAME",` |
|      1827 | 1791 | `		pFile ? pFile->zString : ":Memory:",` |
|      3654 | 1792 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1793 | `		);` |
|         - | 1794 | `	/* All done,all super-global are installed now */` |
|      3659 | 1795 | `	return SXRET_OK;` |
|      1832 | 1796 | `}` |
|         - | 1797 | `/*` |
|         - | 1798 | ` * Release a hashmap.` |
|         - | 1799 | ` */` |
|   2164012 | 1800 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1801 | `{` |
|         - | 1802 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   2164017 | 1803 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1804 | `	sxu32 n;` |
|   2164017 | 1805 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1806 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1807 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1808 | `		return SXRET_OK;` |
|         - | 1809 | `	}` |
|   2164017 | 1810 | `	if( pMap->pActiveSteps ){` |
|         - | 1811 | `		/* Every node is about to be freed WITHOUT going through` |
|         - | 1812 | `		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any` |
|         - | 1813 | `		 * live foreach cursor on this map (reachable: array_erase() on the` |
|         - | 1814 | `		 * live map of a by-ref foreach — the CowSeparate discount keeps the` |
|         - | 1815 | `		 * loop's map writable). A NULL cursor ends the loop cleanly at the` |
|         - | 1816 | `		 * next step, or resumes on a fresh insert via the link-time re-arm. */` |
|         - | 1817 | `		ph7_foreach_step *pStep;` |
|        17 | 1818 | `		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){` |
|         9 | 1819 | `			pStep->pCursor = 0;` |
|         5 | 1820 | `		}` |
|         4 | 1821 | `	}` |
|         - | 1822 | `	/* Start the release process */` |
|   2164017 | 1823 | `	n = 0;` |
|   2164017 | 1824 | `	pEntry = pMap->pFirst;` |
|   4820470 | 1825 | `	for(;;){` |
|   9640946 | 1826 | `		if( n >= pMap->nEntry ){` |
|   2164017 | 1827 | `			break;` |
|         - | 1828 | `		}` |
|   7476934 | 1829 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1830 | `		/* Remove the reference from the foreign table */` |
|   7476934 | 1831 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   7476934 | 1832 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1833 | `			/* Restore the ph7_value to the free list */` |
|   7476874 | 1834 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   3738434 | 1835 | `		}` |
|         - | 1836 | `		/* Release the node */` |
|   7476934 | 1837 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   2974280 | 1838 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   1487137 | 1839 | `		}` |
|   7476934 | 1840 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1841 | `		/* Point to the next entry */` |
|   7476934 | 1842 | `		pEntry = pNext;` |
|   7476934 | 1843 | `		n++;` |
|         5 | 1844 | `	}` |
|   2164017 | 1845 | `	if( pMap->nEntry > 0 ){` |
|         - | 1846 | `		/* Release the hash bucket */` |
|   1412732 | 1847 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|    706363 | 1848 | `	}` |
|   2164017 | 1849 | `	if( FreeDS ){` |
|         - | 1850 | `		/* Free the whole instance */` |
|   2163991 | 1851 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   1081998 | 1852 | `	}else{` |
|         - | 1853 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1854 | `		pMap->apBucket = 0;` |
|        28 | 1855 | `		pMap->iNextIdx = 0;` |
|        28 | 1856 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1857 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1858 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1859 | `	}` |
|   2164017 | 1860 | `	return SXRET_OK;` |
|   1082011 | 1861 | `}` |
|         - | 1862 | `/*` |
|         - | 1863 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1864 | ` * If the count reaches zero which mean no more variables` |
|         - | 1865 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1866 | ` */` |
|   5169880 | 1867 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1868 | `{` |
|   5169885 | 1869 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1870 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|   5169885 | 1871 | `	pMap->iRef--;` |
|   5169885 | 1872 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   2163971 | 1873 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   1081983 | 1874 | `	}` |
|   5169885 | 1875 | `}` |
|         - | 1876 | `/*` |
|         - | 1877 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1878 | ` * Write a pointer to the target node on success.` |
|         - | 1879 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1880 | ` */` |
|    188452 | 1881 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1882 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1883 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1884 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1885 | `	)` |
|         5 | 1886 | `{` |
|         - | 1887 | `	sxi32 rc;` |
|    188457 | 1888 | `	if( pMap->nEntry < 1 ){` |
|         - | 1889 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1890 | `		 */` |
|       327 | 1891 | `		return SXERR_NOTFOUND;` |
|         - | 1892 | `	}` |
|    188135 | 1893 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    188135 | 1894 | `	return rc;` |
|     94231 | 1895 | `}` |
|         - | 1896 | `/*` |
|         - | 1897 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1898 | ` * hashmap.` |
|         - | 1899 | ` * If a node with the given key already exists in the database` |
|         - | 1900 | ` * then this function overwrite the old value.` |
|         - | 1901 | ` */` |
|   6803801 | 1902 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1903 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1904 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1905 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1906 | `	)` |
|         5 | 1907 | `{` |
|         - | 1908 | `	sxi32 rc;` |
|         - | 1909 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1910 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1911 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1912 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   6803806 | 1913 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   6803806 | 1914 | `	return rc;` |
|         5 | 1915 | `}` |
|         - | 1916 | `/*` |
|         - | 1917 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1918 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1919 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1920 | ` * This is the same routine that backs array_merge().` |
|         - | 1921 | ` */` |
|       658 | 1922 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1923 | `{` |
|       659 | 1924 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1925 | `}` |
|         - | 1926 | `/*` |
|         - | 1927 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1928 | ` * hashmap.` |
|         - | 1929 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1930 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1931 | ` * The insertion by reference is triggered when the following` |
|         - | 1932 | ` * expression is encountered.` |
|         - | 1933 | ` * $var = 10;` |
|         - | 1934 | ` *  $a = array(&var);` |
|         - | 1935 | ` * OR` |
|         - | 1936 | ` *  $a[] =& $var;` |
|         - | 1937 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1938 | ` * over it's contents.` |
|         - | 1939 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1940 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1941 | ` * Example:` |
|         - | 1942 | ` *  $var = 10;` |
|         - | 1943 | ` *  $a[] =& $var;` |
|         - | 1944 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1945 | ` *  //Unset the foreign ph7_value now` |
|         - | 1946 | ` *  unset($var);` |
|         - | 1947 | ` *  echo count($a); //0` |
|         - | 1948 | ` * Note that this is a PH7 eXtension.` |
|         - | 1949 | ` * Refer to the official documentation for more information.` |
|         - | 1950 | ` * If a node with the given key already exists in the database` |
|         - | 1951 | ` * then this function overwrite the old value.` |
|         - | 1952 | ` */` |
|     49754 | 1953 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1954 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1955 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1956 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1957 | `	)` |
|         5 | 1958 | `{` |
|         - | 1959 | `	sxi32 rc;` |
|     49759 | 1960 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1961 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1962 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1963 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1964 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1965 | `		return PH7_ABORT;` |
|         - | 1966 | `	}` |
|     49759 | 1967 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     49759 | 1968 | `	return rc;` |
|     24882 | 1969 | `}` |
|         - | 1970 | `/*` |
|         - | 1971 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1972 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1973 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1974 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1975 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1976 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1977 | ` */` |
|     25176 | 1978 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1979 | `{` |
|     25181 | 1980 | `	pStep->pCursor = pMap->pFirst;` |
|     25181 | 1981 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     25181 | 1982 | `	pMap->pActiveSteps = pStep;` |
|     25181 | 1983 | `}` |
|         - | 1984 | `/*` |
|         - | 1985 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1986 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1987 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1988 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1989 | ` */` |
|     24996 | 1990 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1991 | `{` |
|     25001 | 1992 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     25001 | 1993 | `	while( *ppLink ){` |
|     25001 | 1994 | `		if( *ppLink == pStep ){` |
|     25001 | 1995 | `			*ppLink = pStep->pNextActive;` |
|     25001 | 1996 | `			pStep->pNextActive = 0;` |
|     25001 | 1997 | `			return;` |
|         - | 1998 | `		}` |
|       ! 0 | 1999 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 2000 | `	}` |
|     12503 | 2001 | `}` |
|         - | 2002 | `/*` |
|         - | 2003 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 2004 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 2005 | ` * return NULL.` |
|         - | 2006 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 2007 | ` */` |
|        64 | 2008 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         1 | 2009 | `{` |
|        65 | 2010 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|        65 | 2011 | `	if( pCur == 0 ){` |
|         - | 2012 | `		/* End of the list,return null */` |
|        27 | 2013 | `		return 0;` |
|         - | 2014 | `	}` |
|         - | 2015 | `	/* Advance the node cursor */` |
|        39 | 2016 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        39 | 2017 | `	return pCur;` |
|        33 | 2018 | `}` |
|         - | 2019 | `/*` |
|         - | 2020 | ` * Extract a node value.` |
|         - | 2021 | ` */` |
|    649280 | 2022 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2023 | `{` |
|    649285 | 2024 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    649285 | 2025 | `	if( pEntry ){` |
|    649285 | 2026 | `		if( bStore ){` |
|    242925 | 2027 | `			PH7_MemObjStore(pEntry,pValue);` |
|    121465 | 2028 | `		}else{` |
|    406365 | 2029 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2030 | `		}` |
|    324499 | 2031 | `	}else{` |
|       ! 0 | 2032 | `		PH7_MemObjRelease(pValue);` |
|         - | 2033 | `	}` |
|    649285 | 2034 | `}` |
|         - | 2035 | `/*` |
|         - | 2036 | ` * Extract a node key.` |
|         - | 2037 | ` */` |
|    187960 | 2038 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2039 | `{` |
|         - | 2040 | `	/* Fill with the current key */` |
|    187965 | 2041 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    181227 | 2042 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2043 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2044 | `		}` |
|    181227 | 2045 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    181227 | 2046 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     90616 | 2047 | `	}else{` |
|      6743 | 2048 | `		SyBlobReset(&pKey->sBlob);` |
|      6743 | 2049 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      6743 | 2050 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2051 | `	}` |
|    187965 | 2052 | `}` |
|         - | 2053 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2054 | `/*` |
|         - | 2055 | ` * Store the address of nodes value in the given container.` |
|         - | 2056 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 2057 | ` * defined in 'builtin.c' for more information.` |
|         - | 2058 | ` */` |
|        14 | 2059 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 2060 | `{` |
|        15 | 2061 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2062 | `	ph7_value *pValue;` |
|         - | 2063 | `	sxu32 n;` |
|         - | 2064 | `	/* Initialize the container */` |
|        15 | 2065 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        41 | 2066 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 2067 | `		/* Extract node value */` |
|        27 | 2068 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        27 | 2069 | `		if( pValue ){` |
|        27 | 2070 | `			SySetPut(pOut,(const void *)&pValue);` |
|        13 | 2071 | `		}` |
|         - | 2072 | `		/* Point to the next entry */` |
|        27 | 2073 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        14 | 2074 | `	}` |
|         - | 2075 | `	/* Total inserted entries */` |
|        15 | 2076 | `	return (int)SySetUsed(pOut);` |
|         1 | 2077 | `}` |
|         - | 2078 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 2079 | `/*` |
|         - | 2080 | ` * Table of hashmap functions.` |
|         - | 2081 | ` */` |
|         - | 2082 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 2083 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 2084 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 2085 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 2086 | `	{"count",             ph7_hashmap_count },` |
|         - | 2087 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 2088 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 2089 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 2090 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 2091 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 2092 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 2093 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 2094 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 2095 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 2096 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 2097 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 2098 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 2099 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 2100 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 2101 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 2102 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 2103 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 2104 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 2105 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 2106 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 2107 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 2108 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 2109 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 2110 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 2111 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 2112 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 2113 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 2114 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 2115 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 2116 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 2117 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 2118 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 2119 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 2120 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 2121 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 2122 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 2123 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 2124 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 2125 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 2126 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 2127 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 2128 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 2129 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 2130 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 2131 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 2132 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 2133 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 2134 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 2135 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 2136 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 2137 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 2138 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 2139 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 2140 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 2141 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 2142 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 2143 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 2144 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 2145 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 2146 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 2147 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 2148 | `	{"range",             ph7_hashmap_range   },` |
|         - | 2149 | `	{"current",           ph7_hashmap_current },` |
|         - | 2150 | `	{"each",              ph7_hashmap_each    },` |
|         - | 2151 | `	{"pos",               ph7_hashmap_current },` |
|         - | 2152 | `	{"next",              ph7_hashmap_next    },` |
|         - | 2153 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 2154 | `	{"end",               ph7_hashmap_end     },` |
|         - | 2155 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 2156 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 2157 | `};` |
|         - | 2158 | `/*` |
|         - | 2159 | ` * Register the built-in hashmap functions defined above.` |
|         - | 2160 | ` */` |
|      3646 | 2161 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 2162 | `{` |
|         - | 2163 | `	sxu32 n;` |
|    273455 | 2164 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    269809 | 2165 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    134907 | 2166 | `	}` |
|      3651 | 2167 | `}` |
|         - | 2168 | `/*` |
|         - | 2169 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 2170 | ` * the BLOB given as the first argument.` |
|         - | 2171 | ` * This function is typically invoked when the user issue a call to` |
|         - | 2172 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 2173 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 2174 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 2175 | ` */` |
|         - | 2176 | `/*` |
|         - | 2177 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 2178 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 2179 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 2180 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 2181 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 2182 | ` */` |
|       248 | 2183 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         5 | 2184 | `{` |
|       253 | 2185 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2186 | `	ph7_value *pObj;` |
|       253 | 2187 | `	sxu32 n = 0;` |
|         - | 2188 | `	int isRef;` |
|       253 | 2189 | `	sxi32 rc = SXRET_OK;` |
|         - | 2190 | `	int i;` |
|       357 | 2191 | `	for(;;){` |
|       719 | 2192 | `		if( n >= pMap->nEntry ){` |
|       253 | 2193 | `			break;` |
|         - | 2194 | `		}` |
|       471 | 2195 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 2196 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 2197 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       929 | 2198 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       466 | 2199 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       471 | 2200 | `		if( ShowType ){` |
|         - | 2201 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 2202 | `			 * on the next line at the same indent (php). */` |
|       683 | 2203 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       461 | 2204 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       233 | 2205 | `			}` |
|       227 | 2206 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       137 | 2207 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        71 | 2208 | `			}else{` |
|       140 | 2209 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|        45 | 2210 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2211 | `			}` |
|       227 | 2212 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       227 | 2213 | `			if( pObj ){` |
|       227 | 2214 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|       227 | 2215 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2216 | `					break;` |
|         - | 2217 | `				}` |
|       111 | 2218 | `			}` |
|       116 | 2219 | `		}else{` |
|         - | 2220 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 2221 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 2222 | `			 * php's extra blank line. References carry no marker. */` |
|      1335 | 2223 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1091 | 2224 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       547 | 2225 | `			}` |
|       247 | 2226 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       130 | 2227 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        66 | 2228 | `			}else{` |
|       176 | 2229 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        58 | 2230 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2231 | `			}` |
|       244 | 2232 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       137 | 2233 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 2234 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 2235 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 2236 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2237 | `					break;` |
|         - | 2238 | `				}` |
|        13 | 2239 | `			}else{` |
|       223 | 2240 | `				if( pObj ){` |
|       223 | 2241 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       110 | 2242 | `				}` |
|       223 | 2243 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 2244 | `			}` |
|         - | 2245 | `		}` |
|         - | 2246 | `		/* Point to the next entry */` |
|       471 | 2247 | `		n++;` |
|       471 | 2248 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         5 | 2249 | `	}` |
|       253 | 2250 | `	return rc;` |
|         5 | 2251 | `}` |
|       244 | 2252 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         5 | 2253 | `{` |
|         - | 2254 | `	sxi32 rc;` |
|         - | 2255 | `	int i;` |
|       249 | 2256 | `	if( nDepth > 31 ){` |
|         - | 2257 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 2258 | `		/* Nesting limit reached */` |
|       ! 0 | 2259 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 2260 | `		return SXERR_LIMIT;` |
|         - | 2261 | `	}` |
|       249 | 2262 | `	if( ShowType ){` |
|         - | 2263 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 2264 | `		 * newline (a nested array is itself an entry value line). */` |
|       137 | 2265 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|       137 | 2266 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       137 | 2267 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|       149 | 2268 | `		for( i = 0 ; i < nTab ; i++ ){` |
|        15 | 2269 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|         9 | 2270 | `		}` |
|       137 | 2271 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|       137 | 2272 | `		return rc;` |
|         - | 2273 | `	}` |
|         - | 2274 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       114 | 2275 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       306 | 2276 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 2277 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 2278 | `	}` |
|       114 | 2279 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       114 | 2280 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       306 | 2281 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 2282 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 2283 | `	}` |
|       114 | 2284 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       114 | 2285 | `	return rc;` |
|       127 | 2286 | `}` |
|         - | 2287 | `/*` |
|         - | 2288 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 2289 | ` * retrieved entry.` |
|         - | 2290 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 2291 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 2292 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 2293 | ` * a value different from PH7_OK.` |
|         - | 2294 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 2295 | ` */` |
|     35768 | 2296 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 2297 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2298 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 2299 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 2300 | `	)` |
|         5 | 2301 | `{` |
|         - | 2302 | `	ph7_hashmap_node *pEntry;` |
|         - | 2303 | `	ph7_value sKey,sValue;` |
|         - | 2304 | `	sxi32 rc;` |
|         - | 2305 | `	sxu32 n;` |
|         - | 2306 | `	/* Initialize walker parameter */` |
|     35773 | 2307 | `	rc = SXRET_OK;` |
|     35773 | 2308 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     35773 | 2309 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     35773 | 2310 | `	n = pMap->nEntry;` |
|     35773 | 2311 | `	pEntry = pMap->pFirst;` |
|         - | 2312 | `	/* Start the iteration process */` |
|    104007 | 2313 | `	for(;;){` |
|    208019 | 2314 | `		if( n < 1 ){` |
|     35767 | 2315 | `			break;` |
|         - | 2316 | `		}` |
|         - | 2317 | `		/* Extract a copy of the key and a copy the current value */` |
|    172257 | 2318 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    172257 | 2319 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 2320 | `		/* Invoke the user callback */` |
|    172257 | 2321 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 2322 | `		/* Release the copy of the key and the value */` |
|    172257 | 2323 | `		PH7_MemObjRelease(&sKey);` |
|    172257 | 2324 | `		PH7_MemObjRelease(&sValue);` |
|    172257 | 2325 | `		if( rc != PH7_OK ){` |
|         - | 2326 | `			/* Callback request an operation abort */` |
|         7 | 2327 | `			return SXERR_ABORT;` |
|         - | 2328 | `		}` |
|         - | 2329 | `		/* Point to the next entry */` |
|    172251 | 2330 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    172251 | 2331 | `		n--;` |
|         5 | 2332 | `	}` |
|         - | 2333 | `	/* All done */` |
|     35767 | 2334 | `	return SXRET_OK;` |
|     17889 | 2335 | `}` |
|         - | 2336 |  |
