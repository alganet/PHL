# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1042/1148 lines (90.77%)

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
|   7935324 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7935329 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7935329 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    515517 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    515522 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    515522 |   35 | `	sxu32 nH = 5381;` |
|    515522 |   36 | `	zEnd = &zIn[nLen];` |
|    592304 |   37 | `	for(;;){` |
|   1184614 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1011252 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    909809 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    787333 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    515522 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      2326 |   52 | `PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      2331 |   54 | `	sxi64 iCount = 0;` |
|      2331 |   55 | `	if( !bRecursive ){` |
|      2157 |   56 | `		iCount = pMap->nEntry;` |
|      1081 |   57 | `	}else{` |
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
|      2331 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3625910 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3625915 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3625915 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3625915 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3625915 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3625915 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3625915 |  112 | `	pNode->nHash = nHash;` |
|   3625915 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3625915 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3625915 |  115 | `	return pNode;` |
|   1812960 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    199513 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    199518 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    199518 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    199518 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    199518 |  133 | `	pNode->pMap  = &(*pMap);` |
|    199518 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    199518 |  135 | `	pNode->nHash = nHash;` |
|    199518 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    199518 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    199518 |  138 | `	pNode->nValIdx = nValIdx;` |
|    199518 |  139 | `	return pNode;` |
|     99761 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3825423 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3825428 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   3370215 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   3370215 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1685108 |  150 | `	}` |
|   3825428 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3825428 |  153 | `	if( pMap->pFirst == 0 ){` |
|     87934 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     87934 |  156 | `		pMap->pCur = pNode;` |
|     43969 |  157 | `	}else{` |
|   3737499 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3825428 |  160 | `	if( pMap->pActiveSteps ){` |
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
|   3825428 |  174 | `	++pMap->nEntry;` |
|   3825428 |  175 | `}` |
|         - |  176 | `/*` |
|         - |  177 | ` * Unlink a node from the hashmap.` |
|         - |  178 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  179 | ` */` |
|      7410 |  180 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  181 | `{` |
|      7415 |  182 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7415 |  183 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  184 | `	/* Unlink from the corresponding bucket */` |
|      7415 |  185 | `	if( pNode->pPrevCollide == 0 ){` |
|      6925 |  186 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3465 |  187 | `	}else{` |
|       492 |  188 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  189 | `	}` |
|      7415 |  190 | `	if( pNode->pNextCollide ){` |
|      4841 |  191 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2419 |  192 | `	}` |
|      7415 |  193 | `	if( pMap->pFirst == pNode ){` |
|       173 |  194 | `		pMap->pFirst = pNode->pPrev;` |
|        84 |  195 | `	}` |
|      7415 |  196 | `	if( pMap->pCur == pNode ){` |
|         - |  197 | `		/* Advance the node cursor */` |
|       209 |  198 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|       102 |  199 | `	}` |
|      7415 |  200 | `	if( pMap->pActiveSteps ){` |
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
|      7415 |  211 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7415 |  212 | `	if( bRestore ){` |
|         - |  213 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       215 |  214 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  215 | `		/* Restore to the freelist */` |
|       215 |  216 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       215 |  217 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|       105 |  218 | `		}` |
|       105 |  219 | `	}` |
|      7415 |  220 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7170 |  221 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3583 |  222 | `	}` |
|      7415 |  223 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7415 |  224 | `	pMap->nEntry--;` |
|      7415 |  225 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  226 | `		/* Free the hash-bucket */` |
|       101 |  227 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|       101 |  228 | `		pMap->apBucket = 0;` |
|       101 |  229 | `		pMap->nSize = 0;` |
|       101 |  230 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        48 |  231 | `	}` |
|      7415 |  232 | `}` |
|         - |  233 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  234 | `/*` |
|         - |  235 | ` * Grow the hash-table and rehash all entries.` |
|         - |  236 | ` */` |
|   3825423 |  237 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  238 | `{` |
|   3825428 |  239 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     93366 |  240 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  241 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     93366 |  242 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  243 | `		sxu32 nBucket;` |
|         - |  244 | `		sxu32 n;` |
|     93366 |  245 | `		if( nNew < 1 ){` |
|     87934 |  246 | `			nNew = 16;` |
|     43964 |  247 | `		}` |
|         - |  248 | `		/* Allocate a new bucket */` |
|     93366 |  249 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     93366 |  250 | `		if( apNew == 0 ){` |
|       ! 0 |  251 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  252 | `				return SXERR_MEM; /* Fatal */` |
|         - |  253 | `			}` |
|         - |  254 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  255 | `			return SXRET_OK;` |
|         - |  256 | `		}` |
|         - |  257 | `		/* Zero the table */` |
|     93366 |  258 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  259 | `		/* Reflect the change */` |
|     93366 |  260 | `		pMap->apBucket = apNew;` |
|     93366 |  261 | `		pMap->nSize = nNew;` |
|     93366 |  262 | `		if( apOld == 0 ){` |
|         - |  263 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     87934 |  264 | `			return SXRET_OK;` |
|         - |  265 | `		}` |
|         - |  266 | `		/* Rehash old entries */` |
|      5437 |  267 | `		pEntry = pMap->pFirst;` |
|      5437 |  268 | `		n = 0;` |
|   2536732 |  269 | `		for( ;; ){` |
|   5073469 |  270 | `			if( n >= pMap->nEntry ){` |
|      5437 |  271 | `				break;` |
|         - |  272 | `			}` |
|         - |  273 | `			/* Clear the old collision link */` |
|   5068037 |  274 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  275 | `			/* Link to the new bucket */` |
|   5068037 |  276 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   5068037 |  277 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   4404417 |  278 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   4404417 |  279 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2202206 |  280 | `			}` |
|   5068037 |  281 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  282 | `			/* Point to the next entry */` |
|   5068037 |  283 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   5068037 |  284 | `			n++;` |
|         5 |  285 | `		}` |
|         - |  286 | `		/* Free the old table */` |
|      5437 |  287 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2716 |  288 | `	}` |
|   3737499 |  289 | `	return SXRET_OK;` |
|   1912716 |  290 | `}` |
|         - |  291 | `/*` |
|         - |  292 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  293 | ` * hashmap.` |
|         - |  294 | ` */` |
|   3625910 |  295 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  296 | `{` |
|         - |  297 | `	ph7_hashmap_node *pNode;` |
|         - |  298 | `	sxu32 nIdx;` |
|         - |  299 | `	sxu32 nHash;` |
|         - |  300 | `	sxi32 rc;` |
|   3625915 |  301 | `	if( !isForeign ){` |
|         - |  302 | `		ph7_value *pObj;` |
|         - |  303 | `		ph7_value sSafeVal;` |
|         - |  304 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  305 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  306 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  307 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  308 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  309 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3625873 |  310 | `		if( pValue ){` |
|   3625863 |  311 | `			sSafeVal = *pValue;` |
|   3625863 |  312 | `			pValue = &sSafeVal;` |
|   1812929 |  313 | `		}` |
|         - |  314 | `		/* Reserve a ph7_value for the value */` |
|   3625873 |  315 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3625873 |  316 | `		if( pObj == 0 ){` |
|       ! 0 |  317 | `			return SXERR_MEM;` |
|         - |  318 | `		}` |
|   3625873 |  319 | `		if( pValue ){` |
|         - |  320 | `			/* Duplicate the value */` |
|   3625863 |  321 | `			PH7_MemObjStore(pValue,pObj);` |
|   1812929 |  322 | `		}` |
|   3625873 |  323 | `		nIdx = pObj->nIdx;` |
|   1812939 |  324 | `	}else{` |
|        43 |  325 | `		nIdx = nRefIdx;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Hash the key */` |
|   3625915 |  328 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  329 | `	/* Allocate a new int node */` |
|   3625915 |  330 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3625915 |  331 | `	if( pNode == 0 ){` |
|       ! 0 |  332 | `		return SXERR_MEM;` |
|         - |  333 | `	}` |
|   3625915 |  334 | `	if( isForeign ){` |
|         - |  335 | `		/* Mark as a foregin entry */` |
|        43 |  336 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        21 |  337 | `	}` |
|         - |  338 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3625915 |  339 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3625915 |  340 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  341 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  342 | `		return rc;` |
|         - |  343 | `	}` |
|         - |  344 | `	/* Perform the insertion */` |
|   3625915 |  345 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  346 | `	/* Install in the reference table */` |
|   3625915 |  347 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  348 | `	/* All done */` |
|   3625915 |  349 | `	return SXRET_OK;` |
|   1812960 |  350 | `}` |
|         - |  351 | `/*` |
|         - |  352 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  353 | ` * hashmap.` |
|         - |  354 | ` */` |
|    199513 |  355 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  356 | `{` |
|         - |  357 | `	ph7_hashmap_node *pNode;` |
|         - |  358 | `	sxu32 nHash;` |
|         - |  359 | `	sxu32 nIdx;` |
|         - |  360 | `	sxi32 rc;` |
|    199518 |  361 | `	if( !isForeign ){` |
|         - |  362 | `		ph7_value *pObj;` |
|         - |  363 | `		ph7_value sSafeVal;` |
|         - |  364 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  365 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  366 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  367 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  368 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  369 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    153186 |  370 | `		if( pValue ){` |
|    152876 |  371 | `			sSafeVal = *pValue;` |
|    152876 |  372 | `			pValue = &sSafeVal;` |
|     76435 |  373 | `		}` |
|         - |  374 | `		/* Reserve a ph7_value for the value */` |
|    153186 |  375 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    153186 |  376 | `		if( pObj == 0 ){` |
|       ! 0 |  377 | `			return SXERR_MEM;` |
|         - |  378 | `		}` |
|    153186 |  379 | `		if( pValue ){` |
|         - |  380 | `			/* Duplicate the value */` |
|    152876 |  381 | `			PH7_MemObjStore(pValue,pObj);` |
|     76435 |  382 | `		}` |
|    153186 |  383 | `		nIdx = pObj->nIdx;` |
|     76595 |  384 | `	}else{` |
|     46337 |  385 | `		nIdx = nRefIdx;` |
|         - |  386 | `	}` |
|         - |  387 | `	/* Hash the key */` |
|    199518 |  388 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  389 | `	/* Allocate a new blob node */` |
|    199518 |  390 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    199518 |  391 | `	if( pNode == 0 ){` |
|       ! 0 |  392 | `		return SXERR_MEM;` |
|         - |  393 | `	}` |
|    199518 |  394 | `	if( isForeign ){` |
|         - |  395 | `		/* Mark as a foregin entry */` |
|     46337 |  396 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23166 |  397 | `	}` |
|         - |  398 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    199518 |  399 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    199518 |  400 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  401 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  402 | `		return rc;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Perform the insertion */` |
|    199518 |  405 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  406 | `	/* Install in the reference table */` |
|    199518 |  407 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  408 | `	/* All done */` |
|    199518 |  409 | `	return SXRET_OK;` |
|     99761 |  410 | `}` |
|         - |  411 | `/*` |
|         - |  412 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  413 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  414 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  415 | ` */` |
|   4293512 |  416 | `PH7_PRIVATE sxi32 HashmapLookupIntKey(` |
|         - |  417 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  418 | `	sxi64 iKey,                /* lookup key */` |
|         - |  419 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  420 | `	)` |
|         5 |  421 | `{` |
|         - |  422 | `	ph7_hashmap_node *pNode;` |
|         - |  423 | `	sxu32 nHash;` |
|   4293517 |  424 | `	if( pMap->nEntry < 1 ){` |
|         - |  425 | `		/* Don't bother hashing,there is no entry anyway */` |
|       863 |  426 | `		return SXERR_NOTFOUND;` |
|         - |  427 | `	}` |
|         - |  428 | `	/* Hash the key first */` |
|   4292659 |  429 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  430 | `	/* Point to the appropriate bucket */` |
|   4292659 |  431 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  432 | `	/* Perform the lookup */` |
| 110568697 |  433 | `	for(;;){` |
| 221137361 |  434 | `		if( pNode == 0 ){` |
|   4285277 |  435 | `			break;` |
|         - |  436 | `		}` |
| 216852084 |  437 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216849068 |  438 | `			&& pNode->nHash == nHash` |
| 108426739 |  439 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  440 | `				/* Node found */` |
|      7387 |  441 | `				if( ppNode ){` |
|      7365 |  442 | `					*ppNode = pNode;` |
|      3680 |  443 | `				}` |
|      7387 |  444 | `				return SXRET_OK;` |
|         - |  445 | `		}` |
|         - |  446 | `		/* Follow the collision link */` |
| 216844704 |  447 | `		pNode = pNode->pNextCollide;` |
|         2 |  448 | `	}` |
|         - |  449 | `	/* No such entry */` |
|   4285277 |  450 | `	return SXERR_NOTFOUND;` |
|   2146763 |  451 | `}` |
|         - |  452 | `/*` |
|         - |  453 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  454 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  455 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  456 | ` */` |
|    344617 |  457 | `PH7_PRIVATE sxi32 HashmapLookupBlobKey(` |
|         - |  458 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  459 | `	const void *pKey,           /* Lookup key */` |
|         - |  460 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  461 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  462 | `	)` |
|         5 |  463 | `{` |
|         - |  464 | `	ph7_hashmap_node *pNode;` |
|         - |  465 | `	sxu32 nHash;` |
|    344622 |  466 | `	if( pMap->nEntry < 1 ){` |
|         - |  467 | `		/* Don't bother hashing,there is no entry anyway */` |
|     28618 |  468 | `		return SXERR_NOTFOUND;` |
|         - |  469 | `	}` |
|         - |  470 | `	/* Hash the key first */` |
|    316009 |  471 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  472 | `	/* Point to the appropriate bucket */` |
|    316009 |  473 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  474 | `	/* Perform the lookup */` |
|    256468 |  475 | `	for(;;){` |
|    512941 |  476 | `		if( pNode == 0 ){` |
|    250441 |  477 | `			break;` |
|         - |  478 | `		}` |
|    262500 |  479 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    260987 |  480 | `			&& pNode->nHash == nHash` |
|    162573 |  481 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     65677 |  482 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  483 | `				/* Node found */` |
|     65573 |  484 | `				if( ppNode ){` |
|     65545 |  485 | `					*ppNode = pNode;` |
|     32770 |  486 | `				}` |
|     65573 |  487 | `				return SXRET_OK;` |
|         - |  488 | `		}` |
|         - |  489 | `		/* Follow the collision link */` |
|    196937 |  490 | `		pNode = pNode->pNextCollide;` |
|         5 |  491 | `	}` |
|         - |  492 | `	/* No such entry */` |
|    250441 |  493 | `	return SXERR_NOTFOUND;` |
|    172313 |  494 | `}` |
|         - |  495 | `/*` |
|         - |  496 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  497 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  498 | ` */` |
|    344997 |  499 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  500 | `{` |
|    345002 |  501 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    345002 |  502 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  503 | `	const char *zDigit;` |
|    345002 |  504 | `	int isNeg = FALSE, nDigit;` |
|    345002 |  505 | `	if( zIn >= zEnd ){` |
|        12 |  506 | `		return FALSE;` |
|         - |  507 | `	}` |
|    344992 |  508 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  509 | `		/* Octal not decimal number */` |
|         5 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|    344988 |  512 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  513 | `		isNeg = (zIn[0] == '-');` |
|         5 |  514 | `		zIn++;` |
|         2 |  515 | `	}` |
|    344988 |  516 | `	zDigit = zIn;` |
|    173130 |  517 | `	for(;;){` |
|    346266 |  518 | `		if( zIn >= zEnd ){` |
|       499 |  519 | `			break;` |
|         - |  520 | `		}` |
|    345768 |  521 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  522 | `			/* Key does not look like a decimal number */` |
|    344490 |  523 | `			return FALSE;` |
|         - |  524 | `		}` |
|      1279 |  525 | `		zIn++;` |
|         1 |  526 | `	}` |
|         - |  527 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  528 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  529 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  530 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       499 |  531 | `	nDigit = (int)(zEnd - zDigit);` |
|       499 |  532 | `	if( nDigit < 1 ){` |
|         - |  533 | `		/* A lone sign ("-"/"+") */` |
|       ! 0 |  534 | `		return FALSE;` |
|         - |  535 | `	}` |
|       503 |  536 | `	if( nDigit > 19 \|\|` |
|       252 |  537 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|         7 |  538 | `		return FALSE;` |
|         - |  539 | `	}` |
|       493 |  540 | `	return TRUE;` |
|    172503 |  541 | `}` |
|         - |  542 | `/*` |
|         - |  543 | ` * Check if a given key exists in the given hashmap.` |
|         - |  544 | ` * Write a pointer to the target node on success.` |
|         - |  545 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  546 | ` */` |
|    152692 |  547 | `static sxi32 HashmapLookup(` |
|         - |  548 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  549 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  550 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  551 | `	)` |
|         5 |  552 | `{` |
|    152697 |  553 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  554 | `	sxi32 rc;` |
|    152697 |  555 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    145331 |  556 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  557 | `			/* Force a string cast (NULL becomes "", php's empty-string key) */` |
|       ! 0 |  558 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  559 | `		}` |
|    145331 |  560 | `		if( !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  561 | `			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert` |
|         - |  562 | `			 * path: reading $a[""] must find what writing $a[""] stored, not fall through` |
|         - |  563 | `			 * to an integer lookup for key 0. */` |
|    145259 |  564 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    145259 |  565 | `			goto result;` |
|         - |  566 | `		}` |
|        36 |  567 | `	}` |
|         - |  568 | `	/* Perform an int lookup */` |
|      7443 |  569 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  570 | `		/* Force an integer cast */` |
|        84 |  571 | `		PH7_MemObjToInteger(pKey);` |
|        41 |  572 | `	}` |
|         - |  573 | `	/* Perform an int lookup */` |
|      7443 |  574 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     76346 |  575 | `result:` |
|    152697 |  576 | `	if( rc == SXRET_OK ){` |
|         - |  577 | `		/* Node found */` |
|     72157 |  578 | `		if( ppNode ){` |
|     72103 |  579 | `			*ppNode = pNode;` |
|     36049 |  580 | `		}` |
|     72157 |  581 | `		return SXRET_OK;` |
|         - |  582 | `	}` |
|         - |  583 | `	/* No such entry */` |
|     80545 |  584 | `	return SXERR_NOTFOUND;` |
|     76351 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  588 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  589 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  590 | ` * via HashmapAppendIndexBusy.` |
|         - |  591 | ` */` |
|   2143048 |  592 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  593 | `{` |
|   2143053 |  594 | `	if( !pMap->bIntKeySeen ){` |
|         - |  595 | `		/* php 8.3: the first integer key sets the auto-index even if it is negative */` |
|       905 |  596 | `		pMap->bIntKeySeen = 1;` |
|       905 |  597 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       905 |  598 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  599 | `			pMap->iNextIdx++;` |
|       ! 0 |  600 | `		}` |
|       905 |  601 | `		return;` |
|         - |  602 | `	}` |
|   2142153 |  603 | `	if( iKey >= pMap->iNextIdx ){` |
|   2141867 |  604 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  605 | `		/* Make sure the automatic index is not reserved */` |
|   2141867 |  606 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  607 | `			pMap->iNextIdx++;` |
|       ! 0 |  608 | `		}` |
|   1070933 |  609 | `	}` |
|   1071529 |  610 | `}` |
|         - |  611 | `/*` |
|         - |  612 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  613 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  614 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  615 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  616 | ` */` |
|   1479050 |  617 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  618 | `{` |
|   1479055 |  619 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  620 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  621 | `		return TRUE;` |
|         - |  622 | `	}` |
|   1479049 |  623 | `	return FALSE;` |
|    739530 |  624 | `}` |
|         - |  625 | `/*` |
|         - |  626 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  627 | ` * hashmap.` |
|         - |  628 | ` * If a node with the given key already exists in the database` |
|         - |  629 | ` * then this function overwrite the old value.` |
|         - |  630 | ` */` |
|   3775153 |  631 | `PH7_PRIVATE sxi32 HashmapInsert(` |
|         - |  632 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  633 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  634 | `	ph7_value *pVal    /* Node value */` |
|         - |  635 | `	)` |
|         5 |  636 | `{` |
|   3775158 |  637 | `	ph7_hashmap_node *pNode = 0;` |
|   3775158 |  638 | `	sxi32 rc = SXRET_OK;` |
|   3775158 |  639 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL) ){` |
|    153332 |  640 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  641 | `			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY` |
|         - |  642 | `			 * STRING key, it does not treat it as an integer (PH7 fell through to the int` |
|         - |  643 | `			 * path and filed it under 0). */` |
|         5 |  644 | `			PH7_MemObjToString(&(*pKey));` |
|         2 |  645 | `		}` |
|    153332 |  646 | `		if( HashmapIsIntKey(&pKey->sBlob) ){` |
|       419 |  647 | `			goto IntKey;` |
|         - |  648 | `		}` |
|         - |  649 | `		/* An empty key is a real key: $a[""] = v stores under "", it does NOT` |
|         - |  650 | `		 * auto-index (PH7 turned it into the next integer slot, silently` |
|         - |  651 | `		 * overwriting nothing and bumping the auto-index). */` |
|    229368 |  652 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     76454 |  653 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|    152426 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  668 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  669 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       144 |  670 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  671 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  672 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  673 | `				return SXRET_OK;` |
|         - |  674 | `			}` |
|       215 |  675 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       142 |  676 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        71 |  677 | `				pVal,SXU32_HIGH);` |
|         - |  678 | `		}` |
|         - |  679 | `		/* Perform a blob-key insertion */` |
|    152284 |  680 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    152284 |  681 | `		return rc;` |
|         - |  682 | `	}` |
|   1810913 |  683 | `IntKey:` |
|   3622249 |  684 | `	if( pKey ){` |
|   2143233 |  685 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  686 | `			/* Force an integer cast */` |
|       438 |  687 | `			PH7_MemObjToInteger(pKey);` |
|       218 |  688 | `		}` |
|   2143233 |  689 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  690 | `			/* Overwrite the old value */` |
|         - |  691 | `			ph7_value *pElem;` |
|       186 |  692 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       186 |  693 | `			if( pElem ){` |
|       186 |  694 | `				if( pVal ){` |
|       186 |  695 | `					PH7_MemObjStore(pVal,pElem);` |
|        94 |  696 | `				}else{` |
|         - |  697 | `					/* Nullify the entry */` |
|       ! 0 |  698 | `					PH7_MemObjToNull(pElem);` |
|         - |  699 | `				}` |
|        92 |  700 | `			}` |
|       186 |  701 | `			return SXRET_OK;` |
|         - |  702 | `		}` |
|   2143049 |  703 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  704 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  705 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  706 | `			char zKey[24];` |
|         3 |  707 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  708 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  709 | `		}` |
|         - |  710 | `		/* Perform a 64-bit-int-key insertion */` |
|   2143047 |  711 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2143047 |  712 | `		if( rc == SXRET_OK ){` |
|   2143047 |  713 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1071521 |  714 | `		}` |
|   1071526 |  715 | `	}else{` |
|   1479021 |  716 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  717 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  718 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  719 | `		}` |
|   1479019 |  720 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  721 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  722 | `		}` |
|         - |  723 | `		/* Assign an automatic index */` |
|   1479013 |  724 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|   1479013 |  725 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|   1479011 |  726 | `			++pMap->iNextIdx;` |
|    739503 |  727 | `		}` |
|         - |  728 | `	}` |
|         - |  729 | `	/* Insertion result */` |
|   3622055 |  730 | `	return rc;` |
|   1887581 |  731 | `}` |
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
|     46384 |  759 | `static sxi32 HashmapInsertByRef(` |
|         - |  760 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  761 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  762 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  763 | `	)` |
|         5 |  764 | `{` |
|     46389 |  765 | `	ph7_hashmap_node *pNode = 0;` |
|     46389 |  766 | `	sxi32 rc = SXRET_OK;` |
|     46389 |  767 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46349 |  768 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  769 | `			/* Force a string cast */` |
|       ! 0 |  770 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  771 | `		}` |
|     46349 |  772 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  773 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  774 | `				/* Automatic index assign */` |
|       ! 0 |  775 | `				pKey = 0;` |
|       ! 0 |  776 | `			}` |
|         3 |  777 | `			goto IntKey;` |
|         - |  778 | `		}` |
|     69518 |  779 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23171 |  780 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  781 | `				/* Overwrite */` |
|        11 |  782 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|        11 |  783 | `				pNode->nValIdx = nRefIdx;` |
|         - |  784 | `				/* Install in the reference table */` |
|        11 |  785 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|        11 |  786 | `				return SXRET_OK;` |
|         - |  787 | `		}` |
|         - |  788 | `		/* Perform a blob-key insertion */` |
|     46337 |  789 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46337 |  790 | `		return rc;` |
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
|     23197 |  823 | `}` |
|         - |  824 | `/*` |
|         - |  825 | ` * Extract node value.` |
|         - |  826 | ` */` |
|   1502214 |  827 | `PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  828 | `{` |
|         - |  829 | `	/* Point to the desired object */` |
|         - |  830 | `	ph7_value *pObj;` |
|   1502219 |  831 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1502219 |  832 | `	return pObj;` |
|         5 |  833 | `}` |
|         - |  834 | `/*` |
|         - |  835 | ` * Insert a node in the given hashmap.` |
|         - |  836 | ` * If a node with the given key already exists in the database` |
|         - |  837 | ` * then this function overwrite the old value.` |
|         - |  838 | ` */` |
|       564 |  839 | `PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  840 | `{` |
|         - |  841 | `	ph7_value *pObj;` |
|         - |  842 | `	sxi32 rc;` |
|         - |  843 | `	/* Extract the node value */` |
|       569 |  844 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       569 |  845 | `	if( pObj == 0 ){` |
|       ! 0 |  846 | `		return SXERR_EMPTY;` |
|         - |  847 | `	}` |
|       564 |  848 | `	if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|       568 |  849 | `	 \|\| PH7_VmSlotIsReferenced(pMap->pVm,pNode->nValIdx) ){` |
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
|       567 |  873 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  874 | `		/* Int64 key */` |
|       435 |  875 | `		if( !bPreserve ){` |
|         - |  876 | `			/* Assign an automatic index */` |
|       263 |  877 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|       134 |  878 | `		}else{` |
|       173 |  879 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  880 | `		}` |
|       220 |  881 | `	}else{` |
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
|       567 |  892 | `	return rc;` |
|       287 |  893 | `}` |
|         - |  894 | `/*` |
|         - |  895 | ` * Compare two node values.` |
|         - |  896 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  897 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  898 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  899 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  900 | ` * documenation.` |
|         - |  901 | ` */` |
|     77664 |  902 | `PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  903 | `{` |
|         - |  904 | `	ph7_value sObj1,sObj2;` |
|         - |  905 | `	sxi32 rc;` |
|     77669 |  906 | `	if( pLeft == pRight ){` |
|         - |  907 | `		/*` |
|         - |  908 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  909 | `		 * below for more information on this sceanario.` |
|         - |  910 | `		 */` |
|       ! 0 |  911 | `		return 0;` |
|         - |  912 | `	}` |
|         - |  913 | `	/* Do the comparison */` |
|     77669 |  914 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     77669 |  915 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     77669 |  916 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     77669 |  917 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     77669 |  918 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     77669 |  919 | `	PH7_MemObjRelease(&sObj1);` |
|     77669 |  920 | `	PH7_MemObjRelease(&sObj2);` |
|     77669 |  921 | `	return rc;` |
|     38714 |  922 | `}` |
|         - |  923 | `/*` |
|         - |  924 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  925 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  926 | ` */` |
|     16760 |  927 | `PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  928 | `{` |
|     16765 |  929 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  930 | `	sxu32 nBucket;` |
|         - |  931 | `	/* Remove old collision links */` |
|     16765 |  932 | `	if( pEntry->pPrevCollide ){` |
|     11797 |  933 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5790 |  934 | `	}else{` |
|      4973 |  935 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  936 | `	}` |
|     16765 |  937 | `	if( pEntry->pNextCollide ){` |
|      1078 |  938 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       551 |  939 | `	}` |
|     16765 |  940 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  941 | `	/* Compute the new hash */` |
|     16765 |  942 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     16765 |  943 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     16765 |  944 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  945 | `	/* Link to the new bucket */` |
|     16765 |  946 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     16765 |  947 | `	if( pMap->apBucket[nBucket] ){` |
|     12121 |  948 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5959 |  949 | `	}` |
|     16765 |  950 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     16765 |  951 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  952 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  953 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  954 | `	 * the no-overflow invariant uniform). */` |
|     16765 |  955 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     16765 |  956 | `		pMap->iNextIdx++;` |
|      8380 |  957 | `	}` |
|     16765 |  958 | `}` |
|         - |  959 | `/*` |
|         - |  960 | ` * Perform a linear search on a given hashmap.` |
|         - |  961 | ` * Write a pointer to the target node on success.` |
|         - |  962 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  963 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  964 | ` * for more information.` |
|         - |  965 | ` */` |
|     32816 |  966 | `PH7_PRIVATE int HashmapFindValue(` |
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
|     32821 |  979 | `	pEntry = pMap->pFirst;` |
|     32821 |  980 | `	n = pMap->nEntry;` |
|     32821 |  981 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     32821 |  982 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     78144 |  983 | `	for(;;){` |
|    156295 |  984 | `		if( n < 1 ){` |
|        85 |  985 | `			break;` |
|         - |  986 | `		}` |
|         - |  987 | `		/* Extract node value */` |
|    156211 |  988 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    156211 |  989 | `		if( pVal ){` |
|         - |  990 | `			/* Compare on duplicates (PH7_MemObjCmp converts its operands in` |
|         - |  991 | `			 * place). PH7_MemObjCmp implements php's full comparison table for` |
|         - |  992 | `			 * null too — loose null == ""/0/false, strict null === null only —` |
|         - |  993 | `			 * so null needles/values take the same path as everything else` |
|         - |  994 | `			 * (the historical null-to-null shortcut here made` |
|         - |  995 | `			 * in_array(null, [""]) false where php says true). */` |
|    156211 |  996 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    156211 |  997 | `			PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    156211 |  998 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    156211 |  999 | `			PH7_MemObjRelease(&sVal);` |
|    156211 | 1000 | `			PH7_MemObjRelease(&sNeedle);` |
|    156211 | 1001 | `			if( rc == 0 ){` |
|     32737 | 1002 | `				if( ppNode ){` |
|        23 | 1003 | `					*ppNode = pEntry;` |
|        11 | 1004 | `				}` |
|         - | 1005 | `				/* Match found*/` |
|     32737 | 1006 | `				return SXRET_OK;` |
|         - | 1007 | `			}` |
|     61736 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|    123479 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    123479 | 1011 | `		n--;` |
|         5 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|        85 | 1014 | `	return SXERR_NOTFOUND;` |
|     16413 | 1015 | `}` |
|         - | 1016 | `/*` |
|         - | 1017 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - | 1018 | ` * for values comparison.` |
|         - | 1019 | ` * Write a pointer to the target node on success.` |
|         - | 1020 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1021 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - | 1022 | ` * for more information.` |
|         - | 1023 | ` */` |
|        22 | 1024 | `PH7_PRIVATE int HashmapFindValueByCallback(` |
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
|         2 | 1141 | `{` |
|         - | 1142 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1143 | `	sxi32 rc;` |
|         - | 1144 | `	sxu32 n;` |
|        56 | 1145 | `	if( pLeft == pRight ){` |
|         - | 1146 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1147 | `		 * Unlike the zend engine.` |
|         - | 1148 | `		 */` |
|         7 | 1149 | `		return 0;` |
|         - | 1150 | `	}` |
|        50 | 1151 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1152 | `		/* Must have the same number of entries */` |
|         5 | 1153 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1154 | `	}` |
|        46 | 1155 | `	if( bStrict ){` |
|         - | 1156 | `		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the` |
|         - | 1157 | `		 * same key/value pairs, with identical key types, in the same insertion` |
|         - | 1158 | `		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per` |
|         - | 1159 | `		 * this file's forward-iteration convention) in lockstep and compare each` |
|         - | 1160 | `		 * position's key then value. (Loose '==' below stays order-insensitive,` |
|         - | 1161 | `		 * matching each left key by lookup into the right map.) */` |
|        34 | 1162 | `		ph7_hashmap_node *pLs = pLeft->pFirst;` |
|        34 | 1163 | `		ph7_hashmap_node *pRs = pRight->pFirst;` |
|        62 | 1164 | `		for( n = pLeft->nEntry ; n > 0 ; n-- ){` |
|         - | 1165 | `			/* Keys must match in type and value at this position */` |
|        44 | 1166 | `			if( pLs->iType != pRs->iType ){` |
|       ! 0 | 1167 | `				return 1;` |
|         - | 1168 | `			}` |
|        44 | 1169 | `			if( pLs->iType == HASHMAP_INT_NODE ){` |
|        22 | 1170 | `				if( pLs->xKey.iKey != pRs->xKey.iKey ){` |
|         3 | 1171 | `					return 1;` |
|         - | 1172 | `				}` |
|        11 | 1173 | `			}else{` |
|        23 | 1174 | `				SyBlob *pLk = &pLs->xKey.sKey;` |
|        23 | 1175 | `				SyBlob *pRk = &pRs->xKey.sKey;` |
|        22 | 1176 | `				if( SyBlobLength(pLk) != SyBlobLength(pRk)` |
|        23 | 1177 | `				 \|\| (SyBlobLength(pLk) > 0` |
|        22 | 1178 | `				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){` |
|         7 | 1179 | `					return 1;` |
|         - | 1180 | `				}` |
|         - | 1181 | `			}` |
|         - | 1182 | `			/* Values must be strictly identical */` |
|        36 | 1183 | `			if( HashmapNodeCmp(pLs,pRs,TRUE) != 0 ){` |
|         7 | 1184 | `				return 1;` |
|         - | 1185 | `			}` |
|        30 | 1186 | `			pLs = pLs->pPrev; /* Reverse link = insertion order */` |
|        30 | 1187 | `			pRs = pRs->pPrev;` |
|        16 | 1188 | `		}` |
|        20 | 1189 | `		return 0; /* Same pairs, same order */` |
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
|        29 | 1232 | `}` |
|         - | 1233 | `/*` |
|         - | 1234 | ` * Duplicate a hashmap node.` |
|         - | 1235 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1236 | ` */` |
|    708954 | 1237 | `static sxi32 HashmapDuplicateNode(` |
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
|    708954 | 1248 | `	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)` |
|    708956 | 1249 | `	 \|\| PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){` |
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
|    708951 | 1274 | `	sSafeVal = *pVal;` |
|         - | 1275 |  |
|    708951 | 1276 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1277 | `		/* Blob key insertion */` |
|      4051 | 1278 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4051 | 1279 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4051 | 1280 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4051 | 1281 | `		PH7_MemObjRelease(&sKey);` |
|      2028 | 1282 | `	}else{` |
|         - | 1283 | `		/* Int key */` |
|    704905 | 1284 | `		if( iAction == 0 ){ /* Merge */` |
|    701245 | 1285 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    354285 | 1286 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1287 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1288 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1289 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1290 | `		}else{ /* Dup */` |
|      3635 | 1291 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1292 | `		}` |
|         - | 1293 | `	}` |
|    708951 | 1294 | `	return rc;` |
|    354482 | 1295 | `}` |
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
|      2902 | 1308 | `PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1309 | `{` |
|         - | 1310 | `	ph7_hashmap_node *pEntry;` |
|         - | 1311 | `	ph7_value *pVal;` |
|         - | 1312 | `	sxi32 rc;` |
|         - | 1313 | `	sxu32 n;` |
|      2907 | 1314 | `	if( pSrc == pDest ){` |
|         - | 1315 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1316 | `		 * Unlike the zend engine.` |
|         - | 1317 | `		 */` |
|       ! 0 | 1318 | `		return SXRET_OK;` |
|         - | 1319 | `	}` |
|         - | 1320 | `	/* Point to the first inserted entry in the source */` |
|      2907 | 1321 | `	pEntry = pSrc->pFirst;` |
|         - | 1322 | `	/* Perform the merge */` |
|    704207 | 1323 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1324 | `		/* Extract the node value */` |
|    701305 | 1325 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    701305 | 1326 | `		if( pVal ){` |
|         - | 1327 | `			/* Make a local copy of the value.` |
|         - | 1328 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1329 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1330 | `			 * to the old pool.` |
|         - | 1331 | `			 */` |
|    701305 | 1332 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    350655 | 1333 | `		}else{` |
|       ! 0 | 1334 | `			rc = SXRET_OK;` |
|         - | 1335 | `		}` |
|    701305 | 1336 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1337 | `			return rc;` |
|         - | 1338 | `		}` |
|         - | 1339 | `		/* Point to the next entry */` |
|    701305 | 1340 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    350655 | 1341 | `	}` |
|      2907 | 1342 | `	return SXRET_OK;` |
|      1456 | 1343 | `}` |
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
|        34 | 1358 | `PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
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
|      7322 | 1393 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1394 | `{` |
|         - | 1395 | `	ph7_hashmap_node *pEntry;` |
|         - | 1396 | `	ph7_value *pVal;` |
|         - | 1397 | `	sxi32 rc;` |
|         - | 1398 | `	sxu32 n;` |
|      7327 | 1399 | `	if( pSrc == pDest ){` |
|         - | 1400 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1401 | `		 * Unlike the zend engine.` |
|         - | 1402 | `		 */` |
|       ! 0 | 1403 | `		return SXRET_OK;` |
|         - | 1404 | `	}` |
|         - | 1405 | `	/* Point to the first inserted entry in the source */` |
|      7327 | 1406 | `	pEntry = pSrc->pFirst;` |
|         - | 1407 | `	/* Perform the duplication */` |
|     14937 | 1408 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1409 | `		/* Extract the node value */` |
|      7615 | 1410 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7615 | 1411 | `		if( pVal ){` |
|      7615 | 1412 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      3810 | 1413 | `		}else{` |
|       ! 0 | 1414 | `			rc = SXRET_OK;` |
|         - | 1415 | `		}` |
|      7615 | 1416 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1417 | `			return rc;` |
|         - | 1418 | `		}` |
|         - | 1419 | `		/* Point to the next entry */` |
|      7615 | 1420 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3810 | 1421 | `	}` |
|      7327 | 1422 | `	return SXRET_OK;` |
|      3666 | 1423 | `}` |
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
|       809 | 1442 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1443 | `		/* Extract the node value (resolves foreign references) */` |
|       797 | 1444 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       796 | 1445 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       532 | 1446 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1447 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1448 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1449 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1450 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1451 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1452 | `			pVal = 0;` |
|         2 | 1453 | `		}` |
|       797 | 1454 | `		if( pVal ){` |
|       793 | 1455 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1183 | 1456 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       394 | 1457 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       395 | 1458 | `			}else{` |
|         5 | 1459 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1460 | `			}` |
|       793 | 1461 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1462 | `				return rc;` |
|         - | 1463 | `			}` |
|       396 | 1464 | `		}` |
|         - | 1465 | `		/* Point to the next entry */` |
|       797 | 1466 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       399 | 1467 | `	}` |
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
|    244964 | 1499 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1500 | `{` |
|    244969 | 1501 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1502 | `	ph7_hashmap *pNew;` |
|         - | 1503 | `	ph7_value *pBacking;` |
|         - | 1504 | `	sxu32 nValIdx;` |
|         - | 1505 | `	int bValueInPool;` |
|    244969 | 1506 | `	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;` |
|    244969 | 1507 | `	if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1508 | `		/* Sole owner, no separation needed */` |
|    242139 | 1509 | `		return pMap;` |
|         - | 1510 | `	}` |
|      2835 | 1511 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1512 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1513 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1514 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       137 | 1515 | `		return pMap;` |
|         - | 1516 | `	}` |
|         - | 1517 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1518 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1519 | `	 * frame is popped. */` |
|      2699 | 1520 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2699 | 1521 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2694 | 1522 | `		if( pBacking && pBacking != pValue` |
|      2669 | 1523 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2649 | 1524 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1525 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2649 | 1526 | `			pMap->iRef--;` |
|      2649 | 1527 | `			if( pMap->iRef - nByRefSteps < 2 ){` |
|         - | 1528 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2599 | 1529 | `				pMap->iRef++;` |
|      2599 | 1530 | `				return pMap;` |
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
|        52 | 1569 | `	nValIdx = pValue->nIdx;` |
|        77 | 1570 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        50 | 1571 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        52 | 1572 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        52 | 1573 | `	if( pNew == 0 ){` |
|         - | 1574 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1575 | `		return pMap;` |
|         - | 1576 | `	}` |
|        52 | 1577 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1578 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1579 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1580 | `		return pMap;` |
|         - | 1581 | `	}` |
|        52 | 1582 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        52 | 1583 | `	pMap->iRef--;` |
|        52 | 1584 | `	if( bValueInPool ){` |
|         - | 1585 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        52 | 1586 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        52 | 1587 | `		if( pValue == 0 ){` |
|       ! 0 | 1588 | `			return pNew;` |
|         - | 1589 | `		}` |
|        25 | 1590 | `	}` |
|        52 | 1591 | `	pValue->x.pOther = pNew;` |
|        52 | 1592 | `	return pNew;` |
|    122487 | 1593 | `}` |
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
|      3820 | 1631 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1632 | `{` |
|         - | 1633 | `	ph7_hashmap_node *pEntry;` |
|      3825 | 1634 | `	sxi32 rc = SXRET_OK;` |
|         - | 1635 | `	ph7_value *pObj;` |
|         - | 1636 | `	sxu32 n;` |
|      3825 | 1637 | `	if( pLeft == pRight ){` |
|         - | 1638 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1639 | `		 * Unlike the zend engine.` |
|         - | 1640 | `		 */` |
|       ! 0 | 1641 | `		return SXRET_OK;` |
|         - | 1642 | `	}` |
|         - | 1643 | `	/* Perform the union */` |
|      3825 | 1644 | `	pEntry = pRight->pFirst;` |
|      3865 | 1645 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1646 | `		/* Make sure the given key does not exists in the left array */` |
|        44 | 1647 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1648 | `			/* BLOB key */` |
|        24 | 1649 | `			if( SXRET_OK !=` |
|        20 | 1650 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1651 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1652 | `					if( pObj ){` |
|        20 | 1653 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1654 | `						/* Perform the insertion */` |
|        20 | 1655 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1656 | `							&sSafeVal,0,FALSE);` |
|        20 | 1657 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1658 | `							return rc;` |
|         - | 1659 | `						}` |
|         8 | 1660 | `					}` |
|         8 | 1661 | `			}` |
|        14 | 1662 | `		}else{` |
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
|      3825 | 1679 | `	return SXRET_OK;` |
|      1915 | 1680 | `}` |
|         - | 1681 | `/*` |
|         - | 1682 | ` * Allocate a new hashmap.` |
|         - | 1683 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1684 | ` */` |
|    142070 | 1685 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1686 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1687 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1688 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1689 | `	)` |
|         5 | 1690 | `{` |
|         - | 1691 | `	ph7_hashmap *pMap;` |
|         - | 1692 | `	/* Allocate a new instance */` |
|    142075 | 1693 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    142075 | 1694 | `	if( pMap == 0 ){` |
|       ! 0 | 1695 | `		return 0;` |
|         - | 1696 | `	}` |
|         - | 1697 | `	/* Zero the structure */` |
|    142075 | 1698 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1699 | `	/* Fill in the structure */` |
|    142075 | 1700 | `	pMap->pVm = &(*pVm);` |
|    142075 | 1701 | `	pMap->iRef = 1;` |
|         - | 1702 | `	/* Default hash functions */` |
|    142075 | 1703 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    142075 | 1704 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    142075 | 1705 | `	return pMap;` |
|     71040 | 1706 | `}` |
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
|      3410 | 1727 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|      3415 | 1747 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3415 | 1748 | `	if( pMap == 0 ){` |
|       ! 0 | 1749 | `		return SXERR_MEM;` |
|         - | 1750 | `	}` |
|      3415 | 1751 | `	pVm->pGlobal = pMap;` |
|         - | 1752 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3415 | 1753 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3415 | 1754 | `	if( pObj == 0 ){` |
|       ! 0 | 1755 | `		return SXERR_MEM;` |
|         - | 1756 | `	}` |
|      3415 | 1757 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1758 | `	/* Record object index */` |
|      3415 | 1759 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1760 | `	/* Install the special $GLOBALS array */` |
|      3415 | 1761 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3415 | 1762 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1763 | `		return rc;` |
|         - | 1764 | `	}` |
|         - | 1765 | `	/* Install superglobals now */` |
|     37515 | 1766 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1767 | `		ph7_value *pSuper;` |
|         - | 1768 | `		/* Request an empty array */` |
|     34105 | 1769 | `		pSuper = ph7_new_array(&(*pVm));` |
|     34105 | 1770 | `		if( pSuper == 0 ){` |
|       ! 0 | 1771 | `			return SXERR_MEM;` |
|         - | 1772 | `		}` |
|         - | 1773 | `		/* Install */` |
|     34105 | 1774 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     34105 | 1775 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1776 | `			return rc;` |
|         - | 1777 | `		}` |
|         - | 1778 | `		/* Release the value now it have been installed */` |
|     34105 | 1779 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17055 | 1780 | `	}` |
|         - | 1781 | `	/* Set some $_SERVER entries */` |
|      3415 | 1782 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1783 | `	/*` |
|         - | 1784 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1785 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1786 | `	 */` |
|      6825 | 1787 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1788 | `		"SCRIPT_FILENAME",` |
|      1705 | 1789 | `		pFile ? pFile->zString : ":Memory:",` |
|      3410 | 1790 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1791 | `		);` |
|         - | 1792 | `	/* All done,all super-global are installed now */` |
|      3415 | 1793 | `	return SXRET_OK;` |
|      1710 | 1794 | `}` |
|         - | 1795 | `/*` |
|         - | 1796 | ` * Release a hashmap.` |
|         - | 1797 | ` */` |
|     94154 | 1798 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1799 | `{` |
|         - | 1800 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     94159 | 1801 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1802 | `	sxu32 n;` |
|     94159 | 1803 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1804 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1805 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1806 | `		return SXRET_OK;` |
|         - | 1807 | `	}` |
|     94159 | 1808 | `	if( pMap->pActiveSteps ){` |
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
|     94159 | 1821 | `	n = 0;` |
|     94159 | 1822 | `	pEntry = pMap->pFirst;` |
|   1905219 | 1823 | `	for(;;){` |
|   3810444 | 1824 | `		if( n >= pMap->nEntry ){` |
|     94159 | 1825 | `			break;` |
|         - | 1826 | `		}` |
|   3716290 | 1827 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1828 | `		/* Remove the reference from the foreign table */` |
|   3716290 | 1829 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3716290 | 1830 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1831 | `			/* Restore the ph7_value to the free list */` |
|   3716230 | 1832 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1858112 | 1833 | `		}` |
|         - | 1834 | `		/* Release the node */` |
|   3716290 | 1835 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    109674 | 1836 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     54834 | 1837 | `		}` |
|   3716290 | 1838 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1839 | `		/* Point to the next entry */` |
|   3716290 | 1840 | `		pEntry = pNext;` |
|   3716290 | 1841 | `		n++;` |
|         5 | 1842 | `	}` |
|     94159 | 1843 | `	if( pMap->nEntry > 0 ){` |
|         - | 1844 | `		/* Release the hash bucket */` |
|     68270 | 1845 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     34132 | 1846 | `	}` |
|     94159 | 1847 | `	if( FreeDS ){` |
|         - | 1848 | `		/* Free the whole instance */` |
|     94133 | 1849 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     47069 | 1850 | `	}else{` |
|         - | 1851 | `		/* Keep the instance but reset it's fields */` |
|        28 | 1852 | `		pMap->apBucket = 0;` |
|        28 | 1853 | `		pMap->iNextIdx = 0;` |
|        28 | 1854 | `	pMap->bIntKeySeen = 0;` |
|        28 | 1855 | `		pMap->nEntry = pMap->nSize = 0;` |
|        28 | 1856 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1857 | `	}` |
|     94159 | 1858 | `	return SXRET_OK;` |
|     47082 | 1859 | `}` |
|         - | 1860 | `/*` |
|         - | 1861 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1862 | ` * If the count reaches zero which mean no more variables` |
|         - | 1863 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1864 | ` */` |
|    876122 | 1865 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1866 | `{` |
|    876127 | 1867 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1868 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    876127 | 1869 | `	pMap->iRef--;` |
|    876127 | 1870 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     94113 | 1871 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     47054 | 1872 | `	}` |
|    876127 | 1873 | `}` |
|         - | 1874 | `/*` |
|         - | 1875 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1876 | ` * Write a pointer to the target node on success.` |
|         - | 1877 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1878 | ` */` |
|    152942 | 1879 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1880 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1881 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1882 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1883 | `	)` |
|         5 | 1884 | `{` |
|         - | 1885 | `	sxi32 rc;` |
|    152947 | 1886 | `	if( pMap->nEntry < 1 ){` |
|         - | 1887 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1888 | `		 */` |
|       255 | 1889 | `		return SXERR_NOTFOUND;` |
|         - | 1890 | `	}` |
|    152697 | 1891 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    152697 | 1892 | `	return rc;` |
|     76476 | 1893 | `}` |
|         - | 1894 | `/*` |
|         - | 1895 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1896 | ` * hashmap.` |
|         - | 1897 | ` * If a node with the given key already exists in the database` |
|         - | 1898 | ` * then this function overwrite the old value.` |
|         - | 1899 | ` */` |
|   3073583 | 1900 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   3073588 | 1911 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   3073588 | 1912 | `	return rc;` |
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
|     46374 | 1951 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1952 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1953 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1954 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1955 | `	)` |
|         5 | 1956 | `{` |
|         - | 1957 | `	sxi32 rc;` |
|     46379 | 1958 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1959 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1960 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1961 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1962 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1963 | `		return PH7_ABORT;` |
|         - | 1964 | `	}` |
|     46379 | 1965 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     46379 | 1966 | `	return rc;` |
|     23192 | 1967 | `}` |
|         - | 1968 | `/*` |
|         - | 1969 | ` * Register a foreach step as an active iterator of the given hashmap.` |
|         - | 1970 | ` * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:` |
|         - | 1971 | ` * nested loops over the same array never disturb each other. The map keeps` |
|         - | 1972 | ` * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor` |
|         - | 1973 | ` * parked on a node being deleted (live-map iteration: by-ref foreach,` |
|         - | 1974 | ` * $GLOBALS, OOM snapshot fallbacks).` |
|         - | 1975 | ` */` |
|     23854 | 1976 | `PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1977 | `{` |
|     23859 | 1978 | `	pStep->pCursor = pMap->pFirst;` |
|     23859 | 1979 | `	pStep->pNextActive = pMap->pActiveSteps;` |
|     23859 | 1980 | `	pMap->pActiveSteps = pStep;` |
|     23859 | 1981 | `}` |
|         - | 1982 | `/*` |
|         - | 1983 | ` * Unregister a foreach step from the map's active-iterator list. Must run` |
|         - | 1984 | ` * before the step is freed AND before the step's map reference is dropped —` |
|         - | 1985 | ` * a step left on the list after its pool slot is recycled is a use-after-free` |
|         - | 1986 | ` * on the next unlink fixup (the SyHash-layout incident class).` |
|         - | 1987 | ` */` |
|     23678 | 1988 | `PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)` |
|         5 | 1989 | `{` |
|     23683 | 1990 | `	ph7_foreach_step **ppLink = &pMap->pActiveSteps;` |
|     23683 | 1991 | `	while( *ppLink ){` |
|     23683 | 1992 | `		if( *ppLink == pStep ){` |
|     23683 | 1993 | `			*ppLink = pStep->pNextActive;` |
|     23683 | 1994 | `			pStep->pNextActive = 0;` |
|     23683 | 1995 | `			return;` |
|         - | 1996 | `		}` |
|       ! 0 | 1997 | `		ppLink = &(*ppLink)->pNextActive;` |
|       ! 0 | 1998 | `	}` |
|     11844 | 1999 | `}` |
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
|    598062 | 2020 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 2021 | `{` |
|    598067 | 2022 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    598067 | 2023 | `	if( pEntry ){` |
|    598067 | 2024 | `		if( bStore ){` |
|    218375 | 2025 | `			PH7_MemObjStore(pEntry,pValue);` |
|    109190 | 2026 | `		}else{` |
|    379697 | 2027 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 2028 | `		}` |
|    298790 | 2029 | `	}else{` |
|       ! 0 | 2030 | `		PH7_MemObjRelease(pValue);` |
|         - | 2031 | `	}` |
|    598067 | 2032 | `}` |
|         - | 2033 | `/*` |
|         - | 2034 | ` * Extract a node key.` |
|         - | 2035 | ` */` |
|    165628 | 2036 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 2037 | `{` |
|         - | 2038 | `	/* Fill with the current key */` |
|    165633 | 2039 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    159561 | 2040 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        33 | 2041 | `			SyBlobRelease(&pKey->sBlob);` |
|        16 | 2042 | `		}` |
|    159561 | 2043 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    159561 | 2044 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     79783 | 2045 | `	}else{` |
|      6077 | 2046 | `		SyBlobReset(&pKey->sBlob);` |
|      6077 | 2047 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      6077 | 2048 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 2049 | `	}` |
|    165633 | 2050 | `}` |
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
|         - | 2077 | `/*` |
|         - | 2078 | ` * Table of hashmap functions.` |
|         - | 2079 | ` */` |
|         - | 2080 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 2081 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 2082 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 2083 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 2084 | `	{"count",             ph7_hashmap_count },` |
|         - | 2085 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 2086 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 2087 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 2088 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 2089 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 2090 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 2091 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 2092 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 2093 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 2094 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 2095 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 2096 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 2097 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 2098 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 2099 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 2100 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 2101 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 2102 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 2103 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 2104 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 2105 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 2106 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 2107 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 2108 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 2109 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 2110 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 2111 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 2112 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 2113 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 2114 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 2115 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 2116 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 2117 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 2118 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 2119 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 2120 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 2121 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 2122 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 2123 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 2124 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 2125 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 2126 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 2127 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 2128 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 2129 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 2130 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 2131 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 2132 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 2133 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 2134 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 2135 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 2136 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 2137 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 2138 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 2139 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 2140 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 2141 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 2142 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 2143 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 2144 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 2145 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 2146 | `	{"range",             ph7_hashmap_range   },` |
|         - | 2147 | `	{"current",           ph7_hashmap_current },` |
|         - | 2148 | `	{"each",              ph7_hashmap_each    },` |
|         - | 2149 | `	{"pos",               ph7_hashmap_current },` |
|         - | 2150 | `	{"next",              ph7_hashmap_next    },` |
|         - | 2151 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 2152 | `	{"end",               ph7_hashmap_end     },` |
|         - | 2153 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 2154 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 2155 | `};` |
|         - | 2156 | `/*` |
|         - | 2157 | ` * Register the built-in hashmap functions defined above.` |
|         - | 2158 | ` */` |
|      3402 | 2159 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 2160 | `{` |
|         - | 2161 | `	sxu32 n;` |
|    255155 | 2162 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    251753 | 2163 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    125879 | 2164 | `	}` |
|      3407 | 2165 | `}` |
|         - | 2166 | `/*` |
|         - | 2167 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 2168 | ` * the BLOB given as the first argument.` |
|         - | 2169 | ` * This function is typically invoked when the user issue a call to` |
|         - | 2170 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 2171 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 2172 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 2173 | ` */` |
|         - | 2174 | `/*` |
|         - | 2175 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 2176 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 2177 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 2178 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 2179 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 2180 | ` */` |
|       134 | 2181 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 2182 | `{` |
|       136 | 2183 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 2184 | `	ph7_value *pObj;` |
|       136 | 2185 | `	sxu32 n = 0;` |
|         - | 2186 | `	int isRef;` |
|       136 | 2187 | `	sxi32 rc = SXRET_OK;` |
|         - | 2188 | `	int i;` |
|       212 | 2189 | `	for(;;){` |
|       426 | 2190 | `		if( n >= pMap->nEntry ){` |
|       136 | 2191 | `			break;` |
|         - | 2192 | `		}` |
|       292 | 2193 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         - | 2194 | `		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))` |
|         - | 2195 | `		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */` |
|       580 | 2196 | `		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)` |
|       290 | 2197 | `			\|\| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);` |
|       292 | 2198 | `		if( ShowType ){` |
|         - | 2199 | ``			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value`` |
|         - | 2200 | `			 * on the next line at the same indent (php). */` |
|       170 | 2201 | `			for( i = 0 ; i < nTab + 2 ; i++ ){` |
|       114 | 2202 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        58 | 2203 | `			}` |
|        58 | 2204 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|        53 | 2205 | `				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);` |
|        27 | 2206 | `			}else{` |
|         7 | 2207 | `				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",` |
|         2 | 2208 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2209 | `			}` |
|        58 | 2210 | `			SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        58 | 2211 | `			if( pObj ){` |
|        58 | 2212 | `				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);` |
|        58 | 2213 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2214 | `					break;` |
|         - | 2215 | `				}` |
|        28 | 2216 | `			}` |
|        30 | 2217 | `		}else{` |
|         - | 2218 | ``			/* print_r entry: `[key] => value` at nTab+4; a container value`` |
|         - | 2219 | `			 * renders its block inline (its parens at nTab+8) followed by` |
|         - | 2220 | `			 * php's extra blank line. References carry no marker. */` |
|      1284 | 2221 | `			for( i = 0 ; i < nTab + 4 ; i++ ){` |
|      1050 | 2222 | `				SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       526 | 2223 | `			}` |
|       236 | 2224 | `			if( pEntry->iType == HASHMAP_INT_NODE){` |
|       123 | 2225 | `				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);` |
|        62 | 2226 | `			}else{` |
|       170 | 2227 | `				SyBlobFormat(&(*pOut),"[%.*s] => ",` |
|        56 | 2228 | `					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 2229 | `			}` |
|       234 | 2230 | `			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ))` |
|       131 | 2231 | `			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        25 | 2232 | `				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);` |
|        25 | 2233 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 2234 | `				if( rc == SXERR_LIMIT ){` |
|       ! 0 | 2235 | `					break;` |
|         - | 2236 | `				}` |
|        13 | 2237 | `			}else{` |
|       212 | 2238 | `				if( pObj ){` |
|       212 | 2239 | `					PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|       105 | 2240 | `				}` |
|       212 | 2241 | `				SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 2242 | `			}` |
|         - | 2243 | `		}` |
|         - | 2244 | `		/* Point to the next entry */` |
|       292 | 2245 | `		n++;` |
|       292 | 2246 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 2247 | `	}` |
|       136 | 2248 | `	return rc;` |
|         2 | 2249 | `}` |
|       130 | 2250 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         1 | 2251 | `{` |
|         - | 2252 | `	sxi32 rc;` |
|         - | 2253 | `	int i;` |
|       131 | 2254 | `	if( nDepth > 31 ){` |
|         - | 2255 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 2256 | `		/* Nesting limit reached */` |
|       ! 0 | 2257 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 2258 | `		return SXERR_LIMIT;` |
|         - | 2259 | `	}` |
|       131 | 2260 | `	if( ShowType ){` |
|         - | 2261 | ``		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final`` |
|         - | 2262 | `		 * newline (a nested array is itself an entry value line). */` |
|        25 | 2263 | `		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);` |
|        25 | 2264 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|        25 | 2265 | `		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);` |
|        25 | 2266 | `		for( i = 0 ; i < nTab ; i++ ){` |
|       ! 0 | 2267 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|       ! 0 | 2268 | `		}` |
|        25 | 2269 | `		SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        25 | 2270 | `		return rc;` |
|         - | 2271 | `	}` |
|         - | 2272 | ``	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */`` |
|       107 | 2273 | `	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);` |
|       299 | 2274 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 2275 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 2276 | `	}` |
|       107 | 2277 | `	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);` |
|       107 | 2278 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);` |
|       299 | 2279 | `	for( i = 0 ; i < nTab ; i++ ){` |
|       193 | 2280 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        97 | 2281 | `	}` |
|       107 | 2282 | `	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       107 | 2283 | `	return rc;` |
|        66 | 2284 | `}` |
|         - | 2285 | `/*` |
|         - | 2286 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 2287 | ` * retrieved entry.` |
|         - | 2288 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 2289 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 2290 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 2291 | ` * a value different from PH7_OK.` |
|         - | 2292 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 2293 | ` */` |
|     34056 | 2294 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 2295 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 2296 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 2297 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 2298 | `	)` |
|         5 | 2299 | `{` |
|         - | 2300 | `	ph7_hashmap_node *pEntry;` |
|         - | 2301 | `	ph7_value sKey,sValue;` |
|         - | 2302 | `	sxi32 rc;` |
|         - | 2303 | `	sxu32 n;` |
|         - | 2304 | `	/* Initialize walker parameter */` |
|     34061 | 2305 | `	rc = SXRET_OK;` |
|     34061 | 2306 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     34061 | 2307 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     34061 | 2308 | `	n = pMap->nEntry;` |
|     34061 | 2309 | `	pEntry = pMap->pFirst;` |
|         - | 2310 | `	/* Start the iteration process */` |
|     96138 | 2311 | `	for(;;){` |
|    192281 | 2312 | `		if( n < 1 ){` |
|     34059 | 2313 | `			break;` |
|         - | 2314 | `		}` |
|         - | 2315 | `		/* Extract a copy of the key and a copy the current value */` |
|    158227 | 2316 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    158227 | 2317 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 2318 | `		/* Invoke the user callback */` |
|    158227 | 2319 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 2320 | `		/* Release the copy of the key and the value */` |
|    158227 | 2321 | `		PH7_MemObjRelease(&sKey);` |
|    158227 | 2322 | `		PH7_MemObjRelease(&sValue);` |
|    158227 | 2323 | `		if( rc != PH7_OK ){` |
|         - | 2324 | `			/* Callback request an operation abort */` |
|         3 | 2325 | `			return SXERR_ABORT;` |
|         - | 2326 | `		}` |
|         - | 2327 | `		/* Point to the next entry */` |
|    158225 | 2328 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    158225 | 2329 | `		n--;` |
|         5 | 2330 | `	}` |
|         - | 2331 | `	/* All done */` |
|     34059 | 2332 | `	return SXRET_OK;` |
|     17033 | 2333 | `}` |
|         - | 2334 |  |
